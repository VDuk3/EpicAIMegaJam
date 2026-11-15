#include "AI/GBValkyrieEnemy.h"

#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AI/GBEnemy3D.h"
#include "AI/GBSpriteAnimComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Core/GBPlaceholder.h"
#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"
#include "Engine/GameInstance.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "EngineUtils.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "TimerManager.h"
#include "NiagaraComponent.h"
#include "NiagaraDataInterfaceArrayFunctionLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "UObject/StrongObjectPtr.h"

// The final boss (Enemies.json Entity_Phase2, the plague reaper) fights with this class too; its Niagara look is opt-in
// through Strings, so the Valkyries are unchanged when the keys are absent:
//   Fx.LanceNiagara / Fx.LanceImpactNiagara   the lances become plague balls (system attached to each projectile, its meshes
//                                              hidden, the bolt light kept) and burst with the impact system where they end.
//   Fx.JudgementNiagara                        the circles are drawn by this one-shot system (user floats Radius, Telegraph:
//                                              seconds until that circle strikes); the mark's own shapes are hidden.
//   Fx.LiquidTag                               actors with this tag hold the arena's floor liquid (a Niagara component); while
//                                              she fights, the player, she, her swings, impacts and circle strikes stir it
//                                              (user Vector4 array Fx.LiquidParam, default "Disturbers": xyz world, w radius in cm).
namespace GBReaperFx
{
	struct FPulse { FVector Location; float Strength; double At; };
	static TArray<FPulse> Pulses;
	static TArray<TStrongObjectPtr<UNiagaraSystem>> Keep;
	constexpr double PulseLife = 0.12;          // a hit is a short push (the liquid's contract: 4 to 6 frames)
	constexpr int32 MaxDisturbers = 8;          // the floor wants exactly 8 entries every time
	constexpr int32 MaxPulses = 32;
	const FVector4 Parked(0.0, 0.0, -100000.0, 0.0);   // an unused slot: far under the floor, radius 0

	UNiagaraSystem* Load(const FGBEnemyDefinition& Def, const TCHAR* Key, FName Owner)
	{
		const FString Path = GBEnemy3D::Str(Def, FName(Key));
		if (Path.IsEmpty())
		{
			return nullptr;
		}
		UNiagaraSystem* System = GBEnemy3D::LoadAsset<UNiagaraSystem>(Path, Owner);
		if (System && !Keep.ContainsByPredicate([System](const TStrongObjectPtr<UNiagaraSystem>& K) { return K.Get() == System; }))
		{
			Keep.Emplace(System);
		}
		return System;
	}

	void AddPulse(const UWorld* World, const FVector& Location, float Strength, float Delay = 0.f)
	{
		if (World && Pulses.Num() < MaxPulses)
		{
			Pulses.Add({ Location, Strength, World->GetTimeSeconds() + Delay });
		}
	}

	/** Sends the movers and the live pulses to every floor liquid tagged Tag (no movers and no pulses calm it). */
	void Feed(UWorld* World, FName Tag, FName Param, const TArray<FVector4>& Movers)
	{
		if (!World || Tag.IsNone())
		{
			return;
		}
		const double Now = World->GetTimeSeconds();
		Pulses.RemoveAll([Now](const FPulse& P) { return Now - P.At > PulseLife || P.At > Now + 10.0; });
		TArray<FVector4> Values(Movers);
		for (const FPulse& P : Pulses)
		{
			if (P.At <= Now && Values.Num() < MaxDisturbers)
			{
				const float Fade = 1.f - static_cast<float>((Now - P.At) / PulseLife);
				Values.Add(FVector4(P.Location.X, P.Location.Y, P.Location.Z, P.Strength * Fade));
			}
		}
		while (Values.Num() < MaxDisturbers)
		{
			Values.Add(Parked);
		}
		Values.SetNum(MaxDisturbers);
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (It->ActorHasTag(Tag))
			{
				TInlineComponentArray<UNiagaraComponent*> Comps(*It);
				for (UNiagaraComponent* Comp : Comps)
				{
					UNiagaraDataInterfaceArrayFunctionLibrary::SetNiagaraArrayVector4(Comp, Param, Values);
				}
			}
		}
	}
}


namespace GBValkyrieLocal
{
	// Visual-only constants (the numbers that tune the fight are in Enemies.json).
	constexpr int32 RingSegments = 24;
	constexpr float RingThickness = 10.f;
	constexpr float DiscThickness = 2.f;
	constexpr float PillarHeight = 3200.f;
	constexpr float PillarRadiusShare = 0.9f;      // of the circle's radius
	constexpr float StrikeLightIntensity = 12000.f;
	constexpr float StrikeLightRadius = 1500.f;
	constexpr float FloorProbeUp = 150.f;
	constexpr float FloorProbeDown = 700.f;
	constexpr float HopClearanceProbe = 450.f;     // cm behind her that must be free for the hop
	constexpr int32 ActionClipSwing0 = 0;          // Anim.Action0..2 are the three swings, 3 the Judgement cast, 4 the lances
	constexpr int32 ActionClipJudgement = 3;
	constexpr int32 ActionClipLances = 4;
	constexpr int32 ActionClipHop = 5;

	FVector ParseAxis(const FString& Text, const FVector& Default)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, TEXT(","), true);
		if (Parts.Num() < 3)
		{
			return Default;
		}
		return FVector(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// One circle of the Judgement spell
// ---------------------------------------------------------------------------------------------------------------------

AGBJudgementMark::AGBJudgementMark()
{
	PrimaryActorTick.bCanEverTick = true;
	SetActorEnableCollision(false);

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetCastShadow(false);
	Pillar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Pillar"));
	Pillar->SetupAttachment(Root);
	Pillar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Pillar->SetCastShadow(false);
	Pillar->SetVisibility(false);
	Light = CreateDefaultSubobject<UPointLightComponent>(TEXT("Light"));
	Light->SetupAttachment(Root);
	Light->SetMobility(EComponentMobility::Movable);
	Light->CastShadows = false;
	Light->IntensityUnits = ELightUnits::Candelas;
	Light->Intensity = 0.f;
	Light->SetVisibility(false);
}

void AGBJudgementMark::Init(AGBValkyrieEnemy* InOwner, AActor* InTarget, const FVector& InCenter, float InRadius, float InTelegraph, float InStrikeDelay,
	float InStrikeTime, float InDamage, const FLinearColor& InColor, UNiagaraSystem* InImpactFx)
{
	using namespace GBValkyrieLocal;
	Owner = InOwner;
	Target = InTarget;
	Center = InCenter;
	Radius = FMath::Max(InRadius, 20.f);
	TelegraphTime = FMath::Max(InTelegraph, 0.05f);
	StrikeDelay = FMath::Max(InStrikeDelay, 0.f);
	StrikeTime = FMath::Max(InStrikeTime, 0.05f);
	Damage = InDamage;
	Color = InColor;
	ImpactFx = InImpactFx;
	SetActorLocation(Center);

	bPixel = InOwner && InOwner->HasSpriteLook();
	if (bPixel)
	{
		// The castle is pixel art: the circle is a flat pixel sigil on the floor (its ring at the reach, a cross in the
		// middle) over a dim pixel disc that grows to it; the strike is a baked pixel blast instead of the 3D pillar.
		GBPlaceholder::SetupFlatSprite(Disc, FName(TEXT("judgement_fill")), 100.f, Color * 0.45f, 0.6f);
		Disc->SetRelativeLocation(FVector(0.f, 0.f, 2.f));
		Disc->SetRelativeScale3D(FVector(0.01f, 0.01f, 1.f));
		UStaticMeshComponent* Sigil = NewObject<UStaticMeshComponent>(this);
		GBPlaceholder::SetupFlatSprite(Sigil, FName(TEXT("judgement_sigil")), Radius * 2.f, Color, 2.5f);
		Sigil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sigil->SetCastShadow(false);
		Sigil->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Sigil->SetRelativeLocation(FVector(0.f, 0.f, 4.f));
		Sigil->RegisterComponent();
		RingParts.Add(Sigil);
		Light->SetLightColor(Color);
		Light->SetAttenuationRadius(StrikeLightRadius);
		Light->SetRelativeLocation(FVector(0.f, 0.f, 260.f));
		bInitialized = true;
		return;
	}
	GBPlaceholder::SetupShape(Disc, EGBShape::Cylinder, FVector(100.f, 100.f, DiscThickness), Color * 0.35f, true);
	Disc->SetRelativeLocation(FVector(0.f, 0.f, DiscThickness));
	Disc->SetRelativeScale3D(FVector(0.01f, 0.01f, DiscThickness / 100.f));
	GBPlaceholder::SetupShape(Pillar, EGBShape::Cylinder, FVector(100.f, 100.f, 100.f), FLinearColor::LerpUsingHSV(Color, FLinearColor::White, 0.6f), true);
	Pillar->SetRelativeLocation(FVector(0.f, 0.f, PillarHeight * 0.5f));
	Pillar->SetRelativeScale3D(FVector(Radius * 2.f * PillarRadiusShare / 100.f, Radius * 2.f * PillarRadiusShare / 100.f, PillarHeight / 100.f));
	Pillar->SetVisibility(false);
	Light->SetLightColor(Color);
	Light->SetAttenuationRadius(StrikeLightRadius);
	Light->SetRelativeLocation(FVector(0.f, 0.f, 260.f));

	// The ring marks the final reach: thin bars round the circle, bright, so the edge reads from far away.
	const float SegmentLength = 2.f * PI * Radius / RingSegments * 1.06f;
	for (int32 Index = 0; Index < RingSegments; ++Index)
	{
		const float Angle = 2.f * PI * Index / RingSegments;
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(this);
		GBPlaceholder::SetupShape(Part, EGBShape::Cube, FVector(SegmentLength, RingThickness, 4.f), Color, true);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetCastShadow(false);
		Part->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Part->SetRelativeLocation(FVector(FMath::Cos(Angle) * Radius, FMath::Sin(Angle) * Radius, 3.f));
		Part->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f));
		Part->RegisterComponent();
		RingParts.Add(Part);
	}
	bInitialized = true;
}

void AGBJudgementMark::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bInitialized)
	{
		return;
	}
	Age += DeltaSeconds;
	if (!bStruck)
	{
		// The disc grows to the ring over the telegraph and waits there until its turn to strike.
		const float Grow = FMath::Clamp(Age / TelegraphTime, 0.f, 1.f);
		const float Size = FMath::Max(Radius * 2.f * Grow, 1.f) / 100.f;
		Disc->SetRelativeScale3D(FVector(Size, Size, bPixel ? 1.f : GBValkyrieLocal::DiscThickness / 100.f));
		if (Age >= TelegraphTime + StrikeDelay)
		{
			Strike();
		}
		return;
	}
	StruckAge += DeltaSeconds;
	const float Done = FMath::Clamp(StruckAge / StrikeTime, 0.f, 1.f);
	// The pillar thins out and the flash dies away.
	const float Width = Radius * 2.f * GBValkyrieLocal::PillarRadiusShare * (1.f - Done * Done) / 100.f;
	Pillar->SetRelativeScale3D(FVector(FMath::Max(Width, 0.01f), FMath::Max(Width, 0.01f), GBValkyrieLocal::PillarHeight / 100.f));
	Light->SetIntensity(GBValkyrieLocal::StrikeLightIntensity * (bPixel ? 0.25f : 1.f) * (1.f - Done));
	if (Done >= 1.f)
	{
		Destroy();
	}
}

void AGBJudgementMark::Strike()
{
	bStruck = true;
	StruckAge = 0.f;
	Pillar->SetVisibility(!bPixel);
	Light->SetVisibility(true);
	// The pixel blast carries its own glow: a quarter of the light, or three strikes at once wash the castle out.
	Light->SetIntensity(GBValkyrieLocal::StrikeLightIntensity * (bPixel ? 0.25f : 1.f));
	Disc->SetVisibility(false);
	for (UStaticMeshComponent* Part : RingParts)
	{
		if (Part)
		{
			Part->SetVisibility(false);
		}
	}
	if (bPixel)
	{
		// A baked pixel blast as wide as the circle, its foot on the floor (the clip's pivot is at its base).
		const UGameInstance* GameInstance = GetGameInstance();
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		const FName ClipId(TEXT("FluidExplosion"));
		const FGBPixelVfxClip* Clip = Vfx ? Vfx->FindClip(ClipId) : nullptr;
		const float Scale = Clip && Clip->WorldWidthCm > 0.f ? Radius * 2.2f / Clip->WorldWidthCm : 1.f;
		AGBPixelVfxActor::SpawnPixelVfx(this, ClipId, FTransform(Center), Scale);
	}
	else if (ImpactFx)
	{
		GBEnemy3D::SpawnFx(this, ImpactFx, Center + FVector(0.f, 0.f, 10.f), FRotator::ZeroRotator);
	}

	AGBValkyrieEnemy* OwnerEnemy = Owner.Get();
	AActor* Victim = Target.Get();
	bool bHit = false;
	if (OwnerEnemy && !OwnerEnemy->IsDead() && Victim && UGBCombatLibrary::IsAlive(Victim))
	{
		const FVector Where = Victim->GetActorLocation();
		bHit = FVector::Dist2D(Where, Center) <= Radius && FMath::Abs(Where.Z - Center.Z) <= Radius * 0.5f + 250.f;
		if (bHit)
		{
			FGBDamageInfo Info;
			Info.Amount = Damage;
			Info.Instigator = OwnerEnemy;
			Info.Causer = this;
			Info.SourceId = FName(TEXT("Valkyrie_Judgement"));
			Info.HitLocation = Where;
			Info.HitDirection = FVector::DownVector;
			UGBCombatLibrary::DealDamage(Victim, Info);
		}
	}
	if (OwnerEnemy)
	{
		OwnerEnemy->NotifyMarkStruck(bHit);
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The Valkyrie
// ---------------------------------------------------------------------------------------------------------------------

AGBValkyrieEnemy::AGBValkyrieEnemy()
{
}

void AGBValkyrieEnemy::BeginPlay()
{
	Super::BeginPlay();
	// Her hand closes round the sword (the clips are unarmed): only for a weapon held in the right hand, and only with the data key.
	const FGBEnemyDefinition* Def = GetDefinition();
	const float Curl = S(TEXT("GripCurlDeg"), 0.f);
	if (AnimInst && WeaponComp && Def && Curl > 0.f && GBEnemy3D::Str(*Def, GBEnemy3DKey::WeaponSocket, TEXT("hand_r")) == TEXT("hand_r"))
	{
		const FVector FingerAxis = GBValkyrieLocal::ParseAxis(GBEnemy3D::Str(*Def, FName(TEXT("GripCurlAxis")), TEXT("0,0,1")), FVector(0.f, 0.f, 1.f));
		const FVector ThumbAxis = GBValkyrieLocal::ParseAxis(GBEnemy3D::Str(*Def, FName(TEXT("GripThumbAxis")), TEXT("0,0,1")), FVector(0.f, 0.f, 1.f));
		AnimInst->SetGripCurl(Curl, FingerAxis, S(TEXT("GripThumbDeg"), Curl * 0.5f), ThumbAxis);
	}
}

float AGBValkyrieEnemy::V(const TCHAR* Key, float Default) const
{
	if (bPhaseTwo)
	{
		constexpr float Missing = -1.0e9f;
		const FString PhaseTwoKey = FString(Key) + TEXT("P2");
		const float Value = S(*PhaseTwoKey, Missing);
		if (Value > Missing * 0.5f)
		{
			return Value;
		}
	}
	return S(Key, Default);
}

FString AGBValkyrieEnemy::DescribeTactics() const
{
	return FString::Printf(TEXT("act=%d phase2=%d swings=%d/%d combos=%d dashes=%d hops=%d spells=%d circles=%d/%d hit=%d lances=%d volleys=%d grip=%d lift=%.0f"),
		static_cast<int32>(Act), bPhaseTwo ? 1 : 0, SwingHits, SwingCount, ComboCount, DashCount, HopCount, JudgementCasts, CirclesStruck, CirclesMade, CircleHits,
		LancesFired, VolleyCount, HasGripCurl() ? 1 : 0, LiftApplied);
}

void GBHoldOverlay(const UGBEnemyAnimInstance* Inst, float AtFrac, float Seconds);   // GBEnemyAnimInstance.cpp

namespace GBReaperStagger
{
	// seconds of stagger left per fighter (file-local: no header change)
	static TMap<TWeakObjectPtr<const AGBValkyrieEnemy>, float> Left;
}

void AGBValkyrieEnemy::EnterPhaseTwo()
{
	if (bPhaseTwo)
	{
		return;
	}
	bPhaseTwo = true;
	// PhaseTwoStagger (the final boss, owner): at half health it reels, stunned, for that many seconds; it cannot move or
	// attack, the player's window to hit it hard. The clip is Anim.Action5 (its hop slot, unused by it), held part-way.
	if (const float Stagger = S(TEXT("PhaseTwoStagger"), 0.f); Stagger > 0.f)
	{
		ResetFightState();
		StopMoving();
		GBReaperStagger::Left.Add(this, Stagger);
		float Length = 0.f;
		if (PlayActionClip(5, Length) && AnimInst && Length > 0.f)
		{
			GBHoldOverlay(AnimInst, S(TEXT("StaggerHoldFrac"), 0.55f), FMath::Max(0.f, Stagger - Length));
		}
	}
	// She opens the second phase with her spell.
	SpellCooldown = FMath::Min(SpellCooldown, V(TEXT("PhaseTwoSpellDelay"), 0.8f));
	UE_LOG(LogGB, Display, TEXT("VALKYRIE phase two begins"));
}

void AGBValkyrieEnemy::DebugPhaseTwo()
{
	EnterPhaseTwo();
}

int32 AGBValkyrieEnemy::GetLiveMarks() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AGBJudgementMark>& Mark : Marks)
	{
		Count += Mark.IsValid() ? 1 : 0;
	}
	return Count;
}

void AGBValkyrieEnemy::NotifyMarkStruck(bool bHitTarget)
{
	++CirclesStruck;
	CircleHits += bHitTarget ? 1 : 0;
}

// ---------------------------------------------------------------------------------------------------------------------
// The loop
// ---------------------------------------------------------------------------------------------------------------------

bool AGBValkyrieEnemy::TickRanged(AActor* Goal, float DeltaSeconds)
{
	if (!Goal)
	{
		return false;
	}
	bSight = bSightCached;
	if (const FGBEnemyDefinition* LiquidDef = GetDefinition())
	{
		const FString Tag = GBEnemy3D::Str(*LiquidDef, FName(TEXT("Fx.LiquidTag")));
		if (!Tag.IsEmpty())
		{
			const FVector Me = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FVector Them = Goal->GetActorLocation();
			const float MySpeed = FMath::Min(GetVelocity().Size2D() / 600.f, 1.f);
			const float TheirSpeed = FMath::Min(Goal->GetVelocity().Size2D() / 600.f, 1.f);
			GBReaperFx::Feed(GetWorld(), FName(*Tag), FName(*GBEnemy3D::Str(*LiquidDef, FName(TEXT("Fx.LiquidParam")), TEXT("Disturbers"))),
				{ FVector4(Me.X, Me.Y, Me.Z, V(TEXT("LiquidBossStrength"), 55.f) * (0.35f + MySpeed)),
				  FVector4(Them.X, Them.Y, Them.Z, V(TEXT("LiquidPlayerStrength"), 45.f) * (0.35f + TheirSpeed)) });
		}
	}
	if (!bFightInit)
	{
		bFightInit = true;
		ResetFightState();
		BurstPhase = EBurstPhase::Cooldown;
		LastBurstPhase = EBurstPhase::Cooldown;
		BurstCooldown = S(TEXT("FirstShotDelay"), 0.4f);
		SpellCooldown = S(TEXT("FirstSpellDelay"), 3.f);
		DashCooldown = S(TEXT("FirstDashDelay"), 2.f);
		LanceCooldown = S(TEXT("FirstLanceDelay"), 4.f);
		EmpowerCooldown = S(TEXT("FirstEmpowerDelay"), 8.f);
		ComboCooldown = 0.f;
		ActionPause = S(TEXT("FirstActionDelay"), 0.8f);
		DecisionTimer = 0.f;
		StrafeSide = Random.FRand() < 0.5f ? 1.f : -1.f;
	}

	DecisionTimer -= DeltaSeconds;
	ActionPause = FMath::Max(0.f, ActionPause - DeltaSeconds);
	ComboCooldown = FMath::Max(0.f, ComboCooldown - DeltaSeconds);
	DashCooldown = FMath::Max(0.f, DashCooldown - DeltaSeconds);
	SpellCooldown = FMath::Max(0.f, SpellCooldown - DeltaSeconds);
	LanceCooldown = FMath::Max(0.f, LanceCooldown - DeltaSeconds);
	BurstCooldown = FMath::Max(0.f, BurstCooldown - DeltaSeconds);
	EmpowerCooldown = FMath::Max(0.f, EmpowerCooldown - DeltaSeconds);
	if (EmpowerTimeLeft > 0.f)
	{
		EmpowerTimeLeft -= DeltaSeconds;
		if (EmpowerTimeLeft <= 0.f)
		{
			SetEmpowered(false);
		}
	}

	if (!bPhaseTwo && Attributes && Attributes->GetMaxHealth() > 0.f && Attributes->GetHealth() < Attributes->GetMaxHealth() * S(TEXT("PhaseTwoHealthFraction"), 0.5f))
	{
		EnterPhaseTwo();
	}
	if (float* Stagger = GBReaperStagger::Left.Find(this))
	{
		*Stagger -= DeltaSeconds;
		if (*Stagger > 0.f)
		{
			StopMoving();
			return true;
		}
		GBReaperStagger::Left.Remove(this);
		FinishAction(0.f);
	}

	const float Distance = FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation());
	switch (Act)
	{
	case EAct::Fight:
		TickFight(Goal, DeltaSeconds, Distance);
		break;
	case EAct::Combo:
		TickCombo(Goal, DeltaSeconds);
		break;
	case EAct::Dash:
		TickDash(Goal, DeltaSeconds);
		break;
	case EAct::Hop:
		TickHop(Goal, DeltaSeconds);
		break;
	case EAct::Judgement:
		TickJudgement(Goal, DeltaSeconds);
		break;
	case EAct::Lances:
		TickLances(Goal, DeltaSeconds);
		break;
	case EAct::Empower:
		TickEmpower(Goal, DeltaSeconds);
		break;
	}
	return true;
}

void AGBValkyrieEnemy::TickFight(AActor* Goal, float DeltaSeconds, float Distance)
{
	if (QueuedKind >= 0)
	{
		const EKind Kind = static_cast<EKind>(QueuedKind);
		QueuedKind = -1;
		StartAction(Kind, Goal);
		return;
	}
	if (ActionPause <= 0.f && DecisionTimer <= 0.f)
	{
		// Close in, she decides faster: a player standing at her sword should not wait for the slow rhythm.
		DecisionTimer = Distance <= S(TEXT("MeleeRange"), 280.f) * 1.05f ? V(TEXT("MeleeDecisionInterval"), 0.1f) : V(TEXT("DecisionInterval"), 0.4f);
		ChooseAction(Goal, Distance);
		if (Act != EAct::Fight)
		{
			return;
		}
	}
	if (bFrozen)
	{
		TurnTowards(Goal, DeltaSeconds);
		return;
	}
	TickFightMovement(Goal, DeltaSeconds, Distance);
	TickBursts(Goal, DeltaSeconds, Distance);
}

void AGBValkyrieEnemy::TickFightMovement(AActor* Goal, float DeltaSeconds, float Distance)
{
	const FVector Me = GetActorLocation();
	const FVector Player = Goal->GetActorLocation();
	const FVector Dir = FVector(Player.X - Me.X, Player.Y - Me.Y, 0.f).GetSafeNormal();
	const FVector Tangent(-Dir.Y, Dir.X, 0.f);
	const float RangeMin = S(TEXT("PreferredRangeMin"), 500.f);
	const float RangeMax = FMath::Max(S(TEXT("PreferredRangeMax"), 1500.f), RangeMin + 100.f);
	const float Middle = 0.5f * (RangeMin + RangeMax);

	// Strafing: the side flips now and then, and at once when she is blocked.
	StrafeTimer -= DeltaSeconds;
	if (StrafeTimer <= 0.f)
	{
		StrafeTimer = Random.FRandRange(V(TEXT("StrafeFlipMin"), 1.5f), FMath::Max(V(TEXT("StrafeFlipMax"), 3.5f), V(TEXT("StrafeFlipMin"), 1.5f)));
		StrafeSide = Random.FRand() < 0.35f ? -StrafeSide : StrafeSide;
	}
	StrafeStall = GetVelocity().Size2D() < 25.f ? StrafeStall + DeltaSeconds : 0.f;
	if (StrafeStall > 0.6f)
	{
		StrafeSide = -StrafeSide;
		StrafeStall = 0.f;
	}

	FVector Point = Me;
	EGBGait MoveGait = EGBGait::Combat;
	if (S(TEXT("ChaseAlways"), 0.f) > 0.5f)
	{
		// the final boss: no band, no strafing, no backing off; it runs straight at the player and stays on top of him
		// aimed where the player is going (his velocity times ChaseLead seconds), so circling and backing off do not shake it
		const FVector Lead = Goal->GetVelocity() * S(TEXT("ChaseLead"), 0.f);
		Point = Player + FVector(Lead.X, Lead.Y, 0.f) - Dir * FMath::Min(Distance, S(TEXT("ChaseStopDistance"), 120.f));
		MoveGait = Distance > S(TEXT("ChaseWalkDistance"), 250.f) ? EGBGait::Run : EGBGait::Combat;
	}
	else if (Distance > RangeMax)
	{
		Point = Player - Dir * (RangeMax * 0.8f);
		MoveGait = Distance > RangeMax * S(TEXT("RunBandMult"), 1.6f) ? EGBGait::Run : EGBGait::Combat;
	}
	else if (Distance < RangeMin)
	{
		if (ComboCooldown <= 0.f && bSight)
		{
			// Her combo is ready and he is close: she goes in for it instead of backing off.
			Point = Player - Dir * (S(TEXT("MeleeRange"), 280.f) * 0.7f);
		}
		else
		{
			Point = Me - Dir * 350.f + Tangent * StrafeSide * 150.f;
		}
	}
	else
	{
		Point = Me + Tangent * StrafeSide * 350.f + Dir * (Distance - Middle) * 0.5f;
	}
	SetGait(MoveGait);
	MoveToPoint(Point, Goal, DeltaSeconds);
	TurnTowards(Goal, DeltaSeconds);
	UpdateAnimAim(Goal, bSight);
}

void AGBValkyrieEnemy::TickBursts(AActor* Goal, float DeltaSeconds, float Distance)
{
	const float AttackRange = Stats->GetStat(GBStat::AttackRange, 0.f);
	const bool bCanShoot = Stats->GetStat(GBStat::AttackDamage, 0.f) > 0.f && AttackRange > 0.f;
	if (BurstPhase == EBurstPhase::Cooldown && BurstCooldown <= 0.f && bCanShoot && bSight && Distance <= AttackRange * 1.05f)
	{
		StartBurst();
	}
	const EBurstPhase Before = BurstPhase;
	TickBurst(Goal, DeltaSeconds, bSight, GBEnemy3D::TargetChest(Goal));
	if (Before != EBurstPhase::Cooldown && BurstPhase == EBurstPhase::Cooldown)
	{
		BurstCooldown = FMath::Max(BurstCooldown, V(TEXT("AttackInterval"), 1.f) * Random.FRandRange(1.f, 1.4f));
	}
	LastBurstPhase = BurstPhase;
}

void AGBValkyrieEnemy::ChooseAction(AActor* Goal, float Distance)
{
	const float MeleeRange = S(TEXT("MeleeRange"), 280.f);
	struct FOption { EKind Kind; float Weight; };
	TArray<FOption, TInlineAllocator<5>> Options;
	auto Allowed = [this](EKind Kind) { return (AutoMask & (1 << static_cast<int32>(Kind))) != 0; };
	if (Allowed(EKind::Combo) && Distance <= MeleeRange * 1.05f && ComboCooldown <= 0.f && bSight)
	{
		Options.Add({ EKind::Combo, V(TEXT("WeightCombo"), 4.f) });
	}
	if (Allowed(EKind::Dash) && Distance >= V(TEXT("DashMinRange"), 700.f) && Distance <= V(TEXT("DashMaxRange"), 1700.f) && DashCooldown <= 0.f && bSight)
	{
		Options.Add({ EKind::Dash, V(TEXT("WeightDash"), 2.f) });
	}
	if (Allowed(EKind::Judgement) && Distance >= V(TEXT("JudgementMinRange"), 200.f) && Distance <= V(TEXT("JudgementMaxRange"), 2800.f) && SpellCooldown <= 0.f)
	{
		Options.Add({ EKind::Judgement, V(TEXT("WeightJudgement"), 3.f) });
	}
	if (Allowed(EKind::Lances) && Distance >= V(TEXT("LanceMinRange"), 500.f) && Distance <= V(TEXT("LanceMaxRange"), 2600.f) && LanceCooldown <= 0.f && bSight)
	{
		Options.Add({ EKind::Lances, V(TEXT("WeightLances"), 3.f) });
	}
	if (Allowed(EKind::Empower) && V(TEXT("WeightEmpower"), 0.f) > 0.f && EmpowerCooldown <= 0.f && EmpowerTimeLeft <= 0.f && bSight)
	{
		Options.Add({ EKind::Empower, V(TEXT("WeightEmpower"), 0.f) });
	}
	if (Options.Num() == 0)
	{
		return;
	}
	float Total = V(TEXT("WeightNone"), 2.f);
	for (const FOption& Option : Options)
	{
		Total += FMath::Max(Option.Weight, 0.f);
	}
	float Roll = Random.FRand() * Total;
	for (const FOption& Option : Options)
	{
		Roll -= FMath::Max(Option.Weight, 0.f);
		if (Roll < 0.f)
		{
			StartAction(Option.Kind, Goal);
			return;
		}
	}
}

void AGBValkyrieEnemy::StartAction(EKind Kind, AActor* Goal)
{
	// Every action starts standing still, with no burst half done and the aim bend off.
	StopMoving();
	if (BurstPhase != EBurstPhase::Cooldown)
	{
		AbortBurst(0.5f);
	}
	if (AnimInst)
	{
		AnimInst->SetAim(0.f, 0.f);
	}
	ActTime = 0.f;
	switch (Kind)
	{
	case EKind::Combo:
		StartCombo(Goal);
		break;
	case EKind::Dash:
		VolleysDone = 1;    // dashes in this chain (the lance counters are free during a dash)
		StartDash(Goal, FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()));
		break;
	case EKind::Judgement:
		StartJudgement(Goal);
		break;
	case EKind::Lances:
		StartLances(Goal);
		break;
	case EKind::Empower:
		StartEmpower(Goal);
		break;
	}
}

void AGBValkyrieEnemy::StartEmpower(AActor* Goal)
{
	Act = EAct::Empower;
	EmpowerClipLength = 0.f;
	if (!PlayActionClip(6, EmpowerClipLength) || EmpowerClipLength <= 0.f)
	{
		EmpowerClipLength = V(TEXT("EmpowerTimeFallback"), 1.2f);
	}
	EmpowerCooldown = V(TEXT("EmpowerCooldown"), 22.f);
}

void AGBValkyrieEnemy::TickEmpower(AActor* Goal, float DeltaSeconds)
{
	ActTime += DeltaSeconds;
	if (S(TEXT("MoveWhileCasting"), 0.f) > 0.5f)
	{
		// the final boss never stands to cast: it keeps chasing while the spell goes out
		TickFightMovement(Goal, DeltaSeconds, FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()));
	}

	TurnTowards(Goal, DeltaSeconds);
	// the blade lights up partway through the flourish, and the buff lasts EmpowerTime from then
	if (EmpowerTimeLeft <= 0.f && ActTime >= EmpowerClipLength * V(TEXT("EmpowerLightFrac"), 0.45f))
	{
		EmpowerTimeLeft = V(TEXT("EmpowerTime"), 12.f);
		SetEmpowered(true);
	}
	if (ActTime >= EmpowerClipLength)
	{
		if (EmpowerTimeLeft <= 0.f)
		{
			EmpowerTimeLeft = V(TEXT("EmpowerTime"), 12.f);
			SetEmpowered(true);
		}
		FinishAction(V(TEXT("EmpowerRecover"), 0.2f));
	}
}

void AGBValkyrieEnemy::SetEmpowered(bool bOn)
{
	for (UNiagaraComponent* Fx : EmpowerFx)
	{
		if (Fx)
		{
			Fx->Deactivate();
			Fx->DestroyComponent();
		}
	}
	EmpowerFx.Reset();
	// the blade's own glow: Strings Fx.EmpowerBladeSlot = the weapon's material slot that is only the blade (the
	// reaper's crescent); its HitFlash / HitFlashColor (M_GBEnemy) light it while empowered, nothing else on the weapon
	const FGBEnemyDefinition* GlowDef = GetDefinition();
	const FString SlotText = GlowDef ? GBEnemy3D::Str(*GlowDef, FName(TEXT("Fx.EmpowerBladeSlot"))) : FString();
	if (WeaponComp && !SlotText.IsEmpty())
	{
		const int32 Slot = FCString::Atoi(*SlotText);
		if (Slot >= 0 && Slot < WeaponComp->GetNumMaterials())
		{
			if (UMaterialInstanceDynamic* Blade = WeaponComp->CreateDynamicMaterialInstance(Slot))
			{
				const FLinearColor Glow = UGBDataSubsystem::ParseColor(GBEnemy3D::Str(*GlowDef, FName(TEXT("Fx.EmpowerBladeColor")), TEXT("#7CFF3A")), FLinearColor(0.2f, 1.f, 0.05f));
				Blade->SetVectorParameterValue(FName(TEXT("HitFlashColor")), Glow * V(TEXT("EmpowerBladeGlow"), 3.f));
				Blade->SetScalarParameterValue(FName(TEXT("HitFlash")), bOn ? 1.f : 0.f);
			}
		}
	}
	if (!bOn)
	{
		EmpowerTimeLeft = 0.f;
		return;
	}
	const FGBEnemyDefinition* Def = GetDefinition();
	USkeletalMeshComponent* Body = GetMesh();
	if (!Def || !Body)
	{
		return;
	}
	// Strings Fx.EmpowerTrail<N> = system path, Fx.EmpowerSocket<N> = socket it rides, Fx.EmpowerEnd<N> = "x y z" end of the
	// trail in that socket's space (the system's EndParticle_Position), for N = 0..5
	for (int32 Index = 0; Index < 6; ++Index)
	{
		UNiagaraSystem* System = GBReaperFx::Load(*Def, *FString::Printf(TEXT("Fx.EmpowerTrail%d"), Index), EnemyId);
		if (!System)
		{
			continue;
		}
		const FName Socket(*GBEnemy3D::Str(*Def, FName(*FString::Printf(TEXT("Fx.EmpowerSocket%d"), Index)), TEXT("BladeBase")));
		// the reaper's scythe is its own mesh (WeaponComp) carrying the blade sockets; otherwise the body's sockets
		USceneComponent* Parent = (WeaponComp && WeaponComp->DoesSocketExist(Socket)) ? static_cast<USceneComponent*>(WeaponComp) : static_cast<USceneComponent*>(Body);
		UNiagaraComponent* Fx = UNiagaraFunctionLibrary::SpawnSystemAttached(System, Parent, Socket, FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::SnapToTarget, false);
		if (!Fx)
		{
			continue;
		}
		const FString End = GBEnemy3D::Str(*Def, FName(*FString::Printf(TEXT("Fx.EmpowerEnd%d"), Index)));
		TArray<FString> Parts;
		End.ParseIntoArray(Parts, TEXT(" "));
		if (Parts.Num() == 3)
		{
			const FVector EndAt(FCString::Atof(*Parts[0]), FCString::Atof(*Parts[1]), FCString::Atof(*Parts[2]));
			Fx->SetVariableVec3(FName(TEXT("EndParticle_Position")), EndAt);
			Fx->SetVariableFloat(FName(TEXT("SwordLength")), EndAt.Size());
		}
		const FString Width = GBEnemy3D::Str(*Def, FName(*FString::Printf(TEXT("Fx.EmpowerWidth%d"), Index)));
		if (!Width.IsEmpty())
		{
			Fx->SetVariableFloat(FName(TEXT("Trail Width")), FCString::Atof(*Width));
		}
		EmpowerFx.Add(Fx);
	}
}

void AGBValkyrieEnemy::FinishAction(float Pause)
{
	Act = EAct::Fight;
	ActionPause = FMath::Max(0.f, Pause);
	DecisionTimer = 0.f;
	if (bLifted)
	{
		EndLift();
	}
}

void AGBValkyrieEnemy::ResetFightState()
{
	if (bLifted)
	{
		EndLift();
	}
	DestroyMarks();
	Act = EAct::Fight;
	QueuedKind = -1;
	DashTimeLeft = 0.f;
	if (EmpowerTimeLeft > 0.f || EmpowerFx.Num() > 0)
	{
		SetEmpowered(false);
	}
}

void AGBValkyrieEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyMarks();
	Super::EndPlay(EndPlayReason);
}

void AGBValkyrieEnemy::DestroyMarks()
{
	for (const TWeakObjectPtr<AGBJudgementMark>& Mark : Marks)
	{
		if (AGBJudgementMark* Live = Mark.Get())
		{
			Live->Destroy();
		}
	}
	Marks.Reset();
}

void AGBValkyrieEnemy::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	if (const FGBEnemyDefinition* LiquidDef = GetDefinition())
	{
		const FString Tag = GBEnemy3D::Str(*LiquidDef, FName(TEXT("Fx.LiquidTag")));
		if (!Tag.IsEmpty())
		{
			// her fall is the last splash: one short push, then every slot parked so the floor settles
			GBReaperFx::Pulses.Reset();
			const FVector At = GetActorLocation() - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
			const FName LiquidTag(*Tag);
			const FName Param(*GBEnemy3D::Str(*LiquidDef, FName(TEXT("Fx.LiquidParam")), TEXT("Disturbers")));
			GBReaperFx::Feed(GetWorld(), LiquidTag, Param, { FVector4(At.X, At.Y, At.Z, V(TEXT("LiquidDeathStrength"), 90.f)) });
			if (UWorld* World = GetWorld())
			{
				TWeakObjectPtr<UWorld> WeakWorld(World);
				FTimerHandle Calm;
				World->GetTimerManager().SetTimer(Calm, FTimerDelegate::CreateLambda([WeakWorld, LiquidTag, Param]()
				{
					GBReaperFx::Pulses.Reset();
					GBReaperFx::Feed(WeakWorld.Get(), LiquidTag, Param, {});
				}), 0.1f, false);
			}
		}
	}
	ResetFightState();
	Super::HandleDeath(KillingBlow);
}

void AGBValkyrieEnemy::OnReturnedToUnaware()
{
	Super::OnReturnedToUnaware();
	ResetFightState();
	bFightInit = false;
}

bool AGBValkyrieEnemy::PlayActionClip(int32 Index, float& OutLength)
{
	OutLength = 0.f;
	// The castle's Valkyrie (sprite look): Action0..5 map to her sheet's clips (Strings Clip.Action<N> overrides), Attack when missing.
	if (bSpriteLook && SpriteAnim)
	{
		static const TCHAR* const SheetClips[] = { TEXT("Attack"), TEXT("Attack2"), TEXT("Attack"), TEXT("Cast"), TEXT("Cast"), TEXT("Leap") };
		const TCHAR* const Default = SheetClips[FMath::Clamp(Index, 0, static_cast<int32>(UE_ARRAY_COUNT(SheetClips)) - 1)];
		FName Clip = GetLookClip(*FString::Printf(TEXT("Clip.Action%d"), Index), Default);
		if (!SpriteAnim->PlayAction(Clip))
		{
			Clip = FName(TEXT("Attack"));
			if (!SpriteAnim->PlayAction(Clip))
			{
				return false;
			}
		}
		OutLength = SpriteAnim->GetActionLength();
		++ActionClipsPlayed;
		return true;
	}
	if (!AnimInst)
	{
		return false;
	}
	const EGBEnemyClip Clip = static_cast<EGBEnemyClip>(static_cast<int32>(EGBEnemyClip::Action0) + Index);
	if (const UAnimSequence* Sequence = AnimInst->GetClip(Clip))
	{
		OutLength = Sequence->GetPlayLength();
	}
	const bool bPlayed = AnimInst->PlayOneShot(Clip, false);
	ActionClipsPlayed += bPlayed ? 1 : 0;
	return bPlayed;
}

void AGBValkyrieEnemy::PushHorizontal(const FVector& Direction, float Speed)
{
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		const FVector Flat = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal();
		Move->Velocity = FVector(Flat.X * Speed, Flat.Y * Speed, Move->Velocity.Z);
	}
}

bool AGBValkyrieEnemy::FindFloor(const FVector& Around, float ReferenceZ, FVector& OutPoint) const
{
	if (!GetWorld())
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBValkyrieFloor), false, this);
	FHitResult Hit;
	const FVector Start(Around.X, Around.Y, ReferenceZ + GBValkyrieLocal::FloorProbeUp);
	const FVector End(Around.X, Around.Y, ReferenceZ - GBValkyrieLocal::FloorProbeDown);
	if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		return false;
	}
	OutPoint = Hit.ImpactPoint + FVector(0.f, 0.f, 2.f);
	return true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Close: the combo, and the hop after it
// ---------------------------------------------------------------------------------------------------------------------

void AGBValkyrieEnemy::StartCombo(AActor* Goal)
{
	Act = EAct::Combo;
	++ComboCount;
	ComboSwingsWanted = FMath::Clamp(FMath::RoundToInt(V(TEXT("ComboSwings"), 2.f)), 1, 3);
	StartSwing(0, Goal);
}

void AGBValkyrieEnemy::StartSwing(int32 Index, AActor* Goal)
{
	(void)Goal;
	SwingIndex = Index;
	SwingTime = 0.f;
	bSwingHit = false;
	++SwingCount;
	float ClipLength = 0.f;
	const bool bClip = PlayActionClip(GBValkyrieLocal::ActionClipSwing0 + FMath::Min(Index, 2), ClipLength);
	const float Duration = (bClip && ClipLength > 0.1f) ? ClipLength : S(TEXT("SwingTimeFallback"), 0.9f);
	// The windup is the part of the clip before the blow: long enough for the player to read it.
	SwingHitAt = FMath::Max(Duration * V(TEXT("SwingHitFrac"), 0.5f), V(TEXT("SwingMinWindup"), 0.4f));
	SwingNextAt = FMath::Max(Duration * V(TEXT("SwingChainFrac"), 0.92f), SwingHitAt + V(TEXT("SwingMinFollow"), 0.25f));
}

void AGBValkyrieEnemy::TickCombo(AActor* Goal, float DeltaSeconds)
{
	SwingTime += DeltaSeconds;
	const FVector Me = GetActorLocation();
	const FVector Player = Goal->GetActorLocation();
	const FVector Flat(Player.X - Me.X, Player.Y - Me.Y, 0.f);
	if (SwingTime < SwingHitAt)
	{
		// The windup: she faces him, then steps in with the blow.
		if (!Flat.IsNearlyZero())
		{
			TurnYawTowards(Flat.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 480.f) * V(TEXT("SwingTurnMult"), 1.5f));
		}
		if (SwingTime >= SwingHitAt * V(TEXT("SwingStepStartFrac"), 0.3f) && Flat.Size() > V(TEXT("SwingStopDistance"), 150.f))
		{
			PushHorizontal(Flat, V(TEXT("SwingStepSpeed"), 650.f));
		}
	}
	else if (!bSwingHit)
	{
		bSwingHit = true;
		StopMoving();
		SwingHit(Goal);
	}
	if (SwingTime >= SwingNextAt)
	{
		if (SwingIndex + 1 < ComboSwingsWanted)
		{
			StartSwing(SwingIndex + 1, Goal);
		}
		else
		{
			EndCombo(Goal);
		}
	}
}

void AGBValkyrieEnemy::SwingHit(AActor* Goal)
{
	const FVector Me = GetActorLocation();
	const FVector Player = Goal->GetActorLocation();
	const FVector Flat(Player.X - Me.X, Player.Y - Me.Y, 0.f);
	const float FlatDistance = Flat.Size();
	const FVector Forward = FVector(GetActorForwardVector().X, GetActorForwardVector().Y, 0.f).GetSafeNormal();
	const float Cosine = FlatDistance > 1.f ? FVector::DotProduct(Forward, Flat / FlatDistance) : 1.f;
	const float Reach = V(TEXT("SwingReach"), S(TEXT("MeleeRange"), 280.f) * 1.1f);
	const float ConeCosine = FMath::Cos(FMath::DegreesToRadians(V(TEXT("SwingConeDeg"), 70.f)));
	const bool bIn = FlatDistance <= Reach && Cosine >= ConeCosine && FMath::Abs(Player.Z - Me.Z) <= V(TEXT("SwingHeight"), 220.f);
	// the scythe's sweep cuts the floor liquid in front of her (only for a fighter that feeds an arena liquid)
	if (const FGBEnemyDefinition* LiquidDef = GetDefinition(); LiquidDef && !GBEnemy3D::Str(*LiquidDef, FName(TEXT("Fx.LiquidTag"))).IsEmpty())
	{
		GBReaperFx::AddPulse(GetWorld(), Me + Forward * Reach * 0.7f - FVector(0.f, 0.f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),
			V(TEXT("LiquidSwingStrength"), 80.f));
	}
	if (!bIn)
	{
		return;
	}
	FGBDamageInfo Info = MakeShotInfo();
	Info.Amount = V(TEXT("SwingDamage"), Info.Amount * S(TEXT("MeleeDamageMult"), 1.5f));
	if (IsEmpowered())
	{
		Info.Amount *= V(TEXT("EmpowerDamageMult"), 1.6f);
	}
	if (ComboSwingsWanted > 1 && SwingIndex == ComboSwingsWanted - 1)
	{
		Info.Amount *= V(TEXT("FinisherMult"), 1.4f);
	}
	Info.HitLocation = GBEnemy3D::TargetChest(Goal);
	Info.HitDirection = Flat.GetSafeNormal();
	UGBCombatLibrary::DealDamage(Goal, Info);
	++SwingHits;
	UNiagaraSystem* EmpoweredHit = nullptr;
	if (IsEmpowered())
	{
		if (const FGBEnemyDefinition* Def = GetDefinition())
		{
			EmpoweredHit = GBReaperFx::Load(*Def, TEXT("Fx.EmpowerHit"), EnemyId);
		}
	}
	if (EmpoweredHit)
	{
		GBEnemy3D::SpawnFx(this, EmpoweredHit, Info.HitLocation, (-Info.HitDirection).Rotation());
	}
	else if (ShotFx.HitImpact)
	{
		GBEnemy3D::SpawnFx(this, ShotFx.HitImpact, Info.HitLocation, (-Info.HitDirection).Rotation());
	}
}

void AGBValkyrieEnemy::EndCombo(AActor* Goal)
{
	ComboCooldown = V(TEXT("ComboCooldown"), 3.f);
	StopMoving();
	if (Random.FRand() < V(TEXT("HopChance"), 0.7f))
	{
		StartHop(Goal);
	}
	else
	{
		FinishAction(V(TEXT("ActionPause"), 0.6f));
	}
}

void AGBValkyrieEnemy::StartHop(AActor* Goal)
{
	const FVector Me = GetActorLocation();
	const FVector Away = FVector(Me.X - Goal->GetActorLocation().X, Me.Y - Goal->GetActorLocation().Y, 0.f).GetSafeNormal();
	if (Away.IsNearlyZero() || !GetWorld())
	{
		FinishAction(V(TEXT("ActionPause"), 0.6f));
		return;
	}
	// A wall close behind her: no hop (she would land on it).
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBValkyrieHop), false, this);
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByChannel(Hit, Me, Me + Away * GBValkyrieLocal::HopClearanceProbe, ECC_Visibility, Params))
	{
		FinishAction(V(TEXT("ActionPause"), 0.6f));
		return;
	}
	Act = EAct::Hop;
	ActTime = 0.f;
	++HopCount;
	HopDirection = Away;
	float Unused = 0.f;
	PlayActionClip(GBValkyrieLocal::ActionClipHop, Unused);
	LaunchCharacter(Away * V(TEXT("HopSpeed"), 900.f) + FVector::UpVector * V(TEXT("HopUp"), 380.f), true, true);
}

void AGBValkyrieEnemy::TickHop(AActor* Goal, float DeltaSeconds)
{
	ActTime += DeltaSeconds;
	const FVector Flat(Goal->GetActorLocation().X - GetActorLocation().X, Goal->GetActorLocation().Y - GetActorLocation().Y, 0.f);
	if (!Flat.IsNearlyZero())
	{
		TurnYawTowards(Flat.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 480.f));
	}
	const bool bLanded = ActTime > 0.25f && GetCharacterMovement() && GetCharacterMovement()->IsMovingOnGround();
	if (bLanded || ActTime > V(TEXT("HopMaxTime"), 1.2f))
	{
		StopMoving();
		FinishAction(V(TEXT("ActionPause"), 0.6f));
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// Middle range: the dash
// ---------------------------------------------------------------------------------------------------------------------

void AGBValkyrieEnemy::StartDash(AActor* Goal, float Distance)
{
	Act = EAct::Dash;
	++DashCount;
	DashCooldown = V(TEXT("DashCooldown"), 5.f);
	const float Speed = FMath::Max(V(TEXT("DashSpeed"), 2200.f), 100.f);
	DashTimeLeft = FMath::Clamp((Distance - V(TEXT("DashStopDistance"), 180.f)) / Speed, 0.1f, V(TEXT("DashMaxTime"), 0.7f));
	const FVector Flat(Goal->GetActorLocation().X - GetActorLocation().X, Goal->GetActorLocation().Y - GetActorLocation().Y, 0.f);
	if (!Flat.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.f, Flat.Rotation().Yaw, 0.f));
	}
	SetGait(EGBGait::Run);
}

void AGBValkyrieEnemy::TickDash(AActor* Goal, float DeltaSeconds)
{
	DashTimeLeft -= DeltaSeconds;
	const FVector Flat(Goal->GetActorLocation().X - GetActorLocation().X, Goal->GetActorLocation().Y - GetActorLocation().Y, 0.f);
	if (!Flat.IsNearlyZero())
	{
		TurnYawTowards(Flat.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 480.f) * 2.f);
	}
	if (DashTimeLeft <= 0.f || Flat.Size() <= V(TEXT("DashStopDistance"), 180.f))
	{
		// DashChain: still out of reach when the dash runs out, it dashes again at once, as many times as it takes
		if (S(TEXT("DashChain"), 0.f) > 0.5f && Flat.Size() > S(TEXT("MeleeRange"), 280.f)
			&& VolleysDone < FMath::RoundToInt(S(TEXT("DashChainMax"), 2.f)))
		{
			++VolleysDone;
			StartDash(Goal, Flat.Size());
			return;
		}
		StopMoving();
		StartCombo(Goal);
		return;
	}
	PushHorizontal(Flat, V(TEXT("DashSpeed"), 2200.f));
}

// ---------------------------------------------------------------------------------------------------------------------
// The spell: Judgement
// ---------------------------------------------------------------------------------------------------------------------

void AGBValkyrieEnemy::StartJudgement(AActor* Goal)
{
	(void)Goal;
	Act = EAct::Judgement;
	++JudgementCasts;
	bMarksSpawned = false;
	// the circle casts in a row (JudgementRepeatsMin..JudgementRepeats; the lance counters are free during Judgement)
	{
		const int32 MaxCasts = FMath::Clamp(FMath::RoundToInt(V(TEXT("JudgementRepeats"), 1.f)), 1, 4);
		const int32 MinCasts = FMath::Clamp(FMath::RoundToInt(V(TEXT("JudgementRepeatsMin"), float(MaxCasts))), 1, MaxCasts);
		VolleysDone = 0;
		VolleysWanted = FMath::RandRange(MinCasts, MaxCasts);
	}
	MarksWanted = FMath::Clamp(FMath::RoundToInt(V(TEXT("JudgementCircles"), 3.f)), 1, 8);
	JudgementTotal = V(TEXT("JudgementRiseTime"), 0.6f) + V(TEXT("JudgementTelegraph"), 1.2f) + (MarksWanted - 1) * V(TEXT("JudgementStagger"), 0.15f)
		+ V(TEXT("JudgementStrikeTime"), 0.45f) + V(TEXT("JudgementHold"), 0.5f) + V(TEXT("JudgementLowerTime"), 0.4f);
	float Unused = 0.f;
	PlayActionClip(GBValkyrieLocal::ActionClipJudgement, Unused);
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		Move->StopMovementImmediately();
		Move->SetMovementMode(MOVE_Flying);
	}
	bLifted = true;
	LiftBaseZ = GetActorLocation().Z;
	LiftApplied = 0.f;
}

void AGBValkyrieEnemy::TickJudgement(AActor* Goal, float DeltaSeconds)
{
	ActTime += DeltaSeconds;
	if (S(TEXT("MoveWhileCasting"), 0.f) > 0.5f)
	{
		// the final boss never stands to cast: it keeps chasing while the spell goes out
		TickFightMovement(Goal, DeltaSeconds, FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()));
	}

	const FVector Flat(Goal->GetActorLocation().X - GetActorLocation().X, Goal->GetActorLocation().Y - GetActorLocation().Y, 0.f);
	if (!Flat.IsNearlyZero())
	{
		TurnYawTowards(Flat.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 480.f));
	}
	const float Rise = FMath::Max(V(TEXT("JudgementRiseTime"), 0.6f), 0.05f);
	const float Lower = FMath::Max(V(TEXT("JudgementLowerTime"), 0.4f), 0.05f);
	const float Height = V(TEXT("JudgementRise"), 70.f);
	float Wanted = Height;
	if (ActTime < Rise)
	{
		Wanted = Height * FMath::SmoothStep(0.f, 1.f, ActTime / Rise);
	}
	else if (ActTime > JudgementTotal - Lower)
	{
		Wanted = Height * (1.f - FMath::SmoothStep(0.f, 1.f, (ActTime - (JudgementTotal - Lower)) / Lower));
	}
	SetLift(Wanted);
	if (!bMarksSpawned && ActTime >= Rise)
	{
		bMarksSpawned = true;
		SpawnMarks(Goal);
		if (++VolleysDone < VolleysWanted)
		{
			// another set of circles under the player's new place after JudgementRepeatGap
			bMarksSpawned = false;
			ActTime = Rise - V(TEXT("JudgementRepeatGap"), 0.5f);
			float Unused = 0.f;
			PlayActionClip(GBValkyrieLocal::ActionClipJudgement, Unused);
			return;
		}
		if (S(TEXT("MoveWhileCasting"), 0.f) > 0.5f)
		{
			// the circles strike on their own: it does not wait for them
			SpellCooldown = V(TEXT("SpellCooldown"), 11.f);
			FinishAction(0.f);
			return;
		}
	}
	if (ActTime >= JudgementTotal)
	{
		SpellCooldown = V(TEXT("SpellCooldown"), 11.f);
		FinishAction(V(TEXT("ActionPause"), 0.6f));
	}
}

void AGBValkyrieEnemy::SetLift(float Wanted)
{
	const float Delta = Wanted - LiftApplied;
	if (FMath::Abs(Delta) > 0.01f)
	{
		AddActorWorldOffset(FVector(0.f, 0.f, Delta), true);
		LiftApplied = GetActorLocation().Z - LiftBaseZ;
	}
}

void AGBValkyrieEnemy::EndLift()
{
	bLifted = false;
	LiftApplied = 0.f;
	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		if (Move->MovementMode == MOVE_Flying)
		{
			Move->SetMovementMode(MOVE_Falling);
		}
	}
}

void AGBValkyrieEnemy::SpawnMarks(AActor* Goal)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	const FVector Player = Goal->GetActorLocation();
	const float Radius = V(TEXT("JudgementRadius"), 230.f);
	const float Scatter = V(TEXT("JudgementScatter"), 550.f);
	const float Telegraph = V(TEXT("JudgementTelegraph"), 1.2f);
	const float Stagger = V(TEXT("JudgementStagger"), 0.15f);
	const float StrikeTime = V(TEXT("JudgementStrikeTime"), 0.45f);
	const float Damage = V(TEXT("JudgementDamage"), 30.f);
	FLinearColor Color(1.f, 0.88f, 0.54f);
	UNiagaraSystem* ImpactFx = ShotFx.Impact;
	UNiagaraSystem* CircleFx = nullptr;
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		Color = UGBDataSubsystem::ParseColor(GBEnemy3D::Str(*Def, FName(TEXT("JudgementColor")), TEXT("#FFE08A")), Color);
		CircleFx = GBReaperFx::Load(*Def, TEXT("Fx.JudgementNiagara"), EnemyId);
	}

	TArray<FVector> Spots;
	FVector Floor;
	if (FindFloor(Player, Player.Z, Floor))
	{
		Spots.Add(Floor);   // the first circle is always on the player: standing still is not an option
	}
	const float BaseAngle = Random.FRandRange(0.f, 360.f);
	for (int32 Index = 1; Index < MarksWanted; ++Index)
	{
		for (int32 Try = 0; Try < 8; ++Try)
		{
			const float Angle = BaseAngle + (Index - 1) * 360.f / FMath::Max(MarksWanted - 1, 1) + Random.FRandRange(-25.f, 25.f);
			const float Distance = Random.FRandRange(Scatter * 0.45f, Scatter);
			const FVector Candidate = Player + FVector(FMath::Cos(FMath::DegreesToRadians(Angle)), FMath::Sin(FMath::DegreesToRadians(Angle)), 0.f) * Distance;
			bool bTooClose = false;
			for (const FVector& Other : Spots)
			{
				bTooClose |= FVector::Dist2D(Other, Candidate) < Radius * 1.3f;
			}
			if (!bTooClose && FindFloor(Candidate, Player.Z, Floor))
			{
				Spots.Add(Floor);
				break;
			}
		}
	}
	for (int32 Index = 0; Index < Spots.Num(); ++Index)
	{
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AGBJudgementMark* Mark = World->SpawnActor<AGBJudgementMark>(Spots[Index], FRotator::ZeroRotator, Params))
		{
			Mark->Init(this, Goal, Spots[Index], Radius, Telegraph, Index * Stagger, StrikeTime, Damage, Color, CircleFx ? nullptr : ImpactFx);
			if (CircleFx)
			{
				// the reaper's circles are the floor liquid churning, then erupting: the system draws both, the mark keeps the
				// timing and the damage with its own shapes hidden
				Mark->SetActorHiddenInGame(true);
				if (UNiagaraComponent* Fx = UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, CircleFx, Spots[Index]))
				{
					Fx->SetVariableFloat(FName(TEXT("Radius")), Radius);
					Fx->SetVariableFloat(FName(TEXT("Telegraph")), Telegraph + Index * Stagger);
					// over the arena's black glass floor (translucency sort priority 1), or the glass dims the column
					Fx->SetTranslucentSortPriority(static_cast<int32>(V(TEXT("CircleFxSortPriority"), 2.f)));
				}
				GBReaperFx::AddPulse(World, Spots[Index], V(TEXT("LiquidStrikeStrength"), 90.f), Telegraph + Index * Stagger);
			}
			Marks.Add(Mark);
			++CirclesMade;
		}
	}
}

// ---------------------------------------------------------------------------------------------------------------------
// The second spell: a fan of light lances
// ---------------------------------------------------------------------------------------------------------------------

void AGBValkyrieEnemy::StartLances(AActor* Goal)
{
	(void)Goal;
	Act = EAct::Lances;
	VolleysDone = 0;
	// a random count between LanceVolleysMin and LanceVolleys (the final boss throws its fan 2 or 3 times in a row)
	const int32 MaxVolleys = FMath::Clamp(FMath::RoundToInt(V(TEXT("LanceVolleys"), 1.f)), 1, 4);
	const int32 MinVolleys = FMath::Clamp(FMath::RoundToInt(V(TEXT("LanceVolleysMin"), float(MaxVolleys))), 1, MaxVolleys);
	VolleysWanted = FMath::RandRange(MinVolleys, MaxVolleys);
	float Unused = 0.f;
	PlayActionClip(GBValkyrieLocal::ActionClipLances, Unused);
}

void AGBValkyrieEnemy::TickLances(AActor* Goal, float DeltaSeconds)
{
	ActTime += DeltaSeconds;
	if (S(TEXT("MoveWhileCasting"), 0.f) > 0.5f)
	{
		// the final boss never stands to cast: it keeps chasing while the spell goes out
		TickFightMovement(Goal, DeltaSeconds, FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()));
	}

	const FVector Flat(Goal->GetActorLocation().X - GetActorLocation().X, Goal->GetActorLocation().Y - GetActorLocation().Y, 0.f);
	if (!Flat.IsNearlyZero())
	{
		TurnYawTowards(Flat.Rotation().Yaw, DeltaSeconds, S(TEXT("TurnRate"), 480.f));
	}
	const float Windup = V(TEXT("LanceWindup"), 0.55f);
	const float Gap = V(TEXT("LanceVolleyGap"), 0.8f);
	if (VolleysDone < VolleysWanted && ActTime >= Windup + VolleysDone * Gap)
	{
		FireVolley(Goal);
		++VolleysDone;
		if (VolleysDone < VolleysWanted)
		{
			float Unused = 0.f;
			PlayActionClip(GBValkyrieLocal::ActionClipLances, Unused);
		}
	}
	if (VolleysDone >= VolleysWanted && ActTime >= Windup + (VolleysDone - 1) * Gap + V(TEXT("LanceRecover"), 0.6f))
	{
		LanceCooldown = V(TEXT("LanceCooldown"), 7.f);
		FinishAction(V(TEXT("ActionPause"), 0.6f));
	}
}

void AGBValkyrieEnemy::FireVolley(AActor* Goal)
{
	UWorld* World = GetWorld();
	const FGBEnemyDefinition* Def = GetDefinition();
	if (!World || !Def)
	{
		return;
	}
	const int32 Count = FMath::Clamp(FMath::RoundToInt(V(TEXT("LanceCount"), 5.f)), 1, 15);
	const float Fan = V(TEXT("LanceFanDeg"), 42.f);
	const FVector Origin = GetMuzzleLocation();
	const FVector Base = (GBEnemy3D::TargetChest(Goal) - Origin).GetSafeNormal();
	const FVector Aim = Base.IsNearlyZero() ? GetActorForwardVector() : Base;

	FGBProjectileConfig Config;
	Config.Damage = MakeShotInfo();
	Config.Damage.Amount = V(TEXT("LanceDamage"), 12.f);
	Config.Damage.SourceId = FName(TEXT("Valkyrie_Lance"));
	Config.Damage.Causer = nullptr;
	Config.Speed = V(TEXT("LanceSpeed"), 1700.f);
	Config.Radius = V(TEXT("LanceRadius"), 16.f);
	Config.GravityScale = 0.f;
	Config.MaxRange = V(TEXT("LanceRange"), 4500.f);
	Config.MaxLifetime = FMath::Max(Config.MaxRange / FMath::Max(Config.Speed, 1.f) + 1.f, 2.f);
	Config.bPixelArt = false;
	Config.Color = UGBDataSubsystem::ParseColor(GBEnemy3D::Str(*Def, FName(TEXT("LanceColor")), TEXT("#C8A0FF")), FLinearColor(0.78f, 0.63f, 1.f));
	Config.bEnergyBolt = true;
	Config.BoltLength = V(TEXT("LanceLength"), 170.f);
	Config.BoltThickness = V(TEXT("LanceThickness"), 14.f);
	Config.BoltTrailLength = V(TEXT("LanceTrail"), 260.f);
	Config.BoltLightIntensity = V(TEXT("LanceLight"), 6000.f);
	Config.BoltLightRadius = V(TEXT("LanceLightRadius"), 500.f);
	if (bSpriteLook)
	{
		// The castle's Valkyrie is pixel art (GDD 3): her lances are pixel sprites (Strings LanceSprite, optional baked LanceVfx).
		Config.bPixelArt = true;
		Config.bEnergyBolt = false;
		Config.SpriteId = FName(*GBEnemy3D::Str(*Def, FName(TEXT("LanceSprite")), TEXT("lightning_bolt")));
		Config.VisualScale = V(TEXT("LanceVisualScale"), 2.f);
		Config.FlightVfxId = FName(*GBEnemy3D::Str(*Def, FName(TEXT("LanceVfx"))));
		Config.FlightVfxHeight = V(TEXT("LanceVfxHeight"), 60.f);
	}
	UNiagaraSystem* BallFx = GBReaperFx::Load(*Def, TEXT("Fx.LanceNiagara"), EnemyId);
	UNiagaraSystem* BurstFx = GBReaperFx::Load(*Def, TEXT("Fx.LanceImpactNiagara"), EnemyId);
	const float ImpactStrength = V(TEXT("LiquidImpactStrength"), 80.f);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1 ? static_cast<float>(Index) / (Count - 1) : 0.5f;
		const float Yaw = FMath::Lerp(-Fan * 0.5f, Fan * 0.5f, Alpha);
		if (AGBProjectile* Shot = AGBProjectile::SpawnProjectile(World, Config, Origin, Aim.RotateAngleAxis(Yaw, FVector::UpVector), this))
		{
			++LancesFired;
			if (BallFx)
			{
				// a plague ball: the Niagara system is the look, the bolt's shapes are hidden (its light stays)
				TInlineComponentArray<UStaticMeshComponent*> Shapes(Shot);
				for (UStaticMeshComponent* Shape : Shapes)
				{
					Shape->SetVisibility(false);
				}
				UNiagaraComponent* BallComp = UNiagaraFunctionLibrary::SpawnSystemAttached(BallFx, Shot->GetRootComponent(), NAME_None,
					FVector::ZeroVector, FRotator::ZeroRotator, EAttachLocation::KeepRelativeOffset, true);
				if (BallComp)
				{
					// on impact the ball is gone, but its smoke is not cut: the system is left in the world, stops emitting
					// (Deactivate) and its smoke fades on its own before the component auto-destroys
					TWeakObjectPtr<UNiagaraComponent> WeakBall(BallComp);
					Shot->OnImpact.AddLambda([WeakBall](AGBProjectile*, AActor*, const FVector&)
					{
						if (UNiagaraComponent* Ball = WeakBall.Get())
						{
							Ball->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
							Ball->SetVariableFloat(FName(TEXT("Alive")), 0.f);
							Ball->Deactivate();
						}
					});
				}
			}
			if (BurstFx)
			{
				TWeakObjectPtr<UNiagaraSystem> WeakBurst(BurstFx);
				Shot->OnImpact.AddLambda([WeakBurst, ImpactStrength](AGBProjectile* Projectile, AActor*, const FVector& Location)
				{
					UWorld* ShotWorld = Projectile ? Projectile->GetWorld() : nullptr;
					if (ShotWorld && WeakBurst.IsValid())
					{
						UNiagaraFunctionLibrary::SpawnSystemAtLocation(ShotWorld, WeakBurst.Get(), Location);
					}
					GBReaperFx::AddPulse(ShotWorld, Location, ImpactStrength);
				});
			}
		}
	}
	++VolleyCount;
}
