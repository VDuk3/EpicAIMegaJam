#include "AI/GBValkyrieEnemy.h"

#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionQueryParams.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

#include "AI/GBEnemy3D.h"
#include "AI/GBSpriteAnimComponent.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"

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
		Disc->SetRelativeScale3D(FVector(Size, Size, GBValkyrieLocal::DiscThickness / 100.f));
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
	Light->SetIntensity(GBValkyrieLocal::StrikeLightIntensity * (1.f - Done));
	if (Done >= 1.f)
	{
		Destroy();
	}
}

void AGBJudgementMark::Strike()
{
	bStruck = true;
	StruckAge = 0.f;
	Pillar->SetVisibility(true);
	Light->SetVisibility(true);
	Light->SetIntensity(GBValkyrieLocal::StrikeLightIntensity);
	Disc->SetVisibility(false);
	for (UStaticMeshComponent* Part : RingParts)
	{
		if (Part)
		{
			Part->SetVisibility(false);
		}
	}
	if (ImpactFx)
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

void AGBValkyrieEnemy::EnterPhaseTwo()
{
	if (bPhaseTwo)
	{
		return;
	}
	bPhaseTwo = true;
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

	if (!bPhaseTwo && Attributes && Attributes->GetMaxHealth() > 0.f && Attributes->GetHealth() < Attributes->GetMaxHealth() * S(TEXT("PhaseTwoHealthFraction"), 0.5f))
	{
		EnterPhaseTwo();
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
	if (Distance > RangeMax)
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
	TArray<FOption, TInlineAllocator<4>> Options;
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
		StartDash(Goal, FVector::Dist2D(GetActorLocation(), Goal->GetActorLocation()));
		break;
	case EKind::Judgement:
		StartJudgement(Goal);
		break;
	case EKind::Lances:
		StartLances(Goal);
		break;
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
	if (!bIn)
	{
		return;
	}
	FGBDamageInfo Info = MakeShotInfo();
	Info.Amount = V(TEXT("SwingDamage"), Info.Amount * S(TEXT("MeleeDamageMult"), 1.5f));
	if (ComboSwingsWanted > 1 && SwingIndex == ComboSwingsWanted - 1)
	{
		Info.Amount *= V(TEXT("FinisherMult"), 1.4f);
	}
	Info.HitLocation = GBEnemy3D::TargetChest(Goal);
	Info.HitDirection = Flat.GetSafeNormal();
	UGBCombatLibrary::DealDamage(Goal, Info);
	++SwingHits;
	if (ShotFx.HitImpact)
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
	if (const FGBEnemyDefinition* Def = GetDefinition())
	{
		Color = UGBDataSubsystem::ParseColor(GBEnemy3D::Str(*Def, FName(TEXT("JudgementColor")), TEXT("#FFE08A")), Color);
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
			Mark->Init(this, Goal, Spots[Index], Radius, Telegraph, Index * Stagger, StrikeTime, Damage, Color, ImpactFx);
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
	VolleysWanted = FMath::Clamp(FMath::RoundToInt(V(TEXT("LanceVolleys"), 1.f)), 1, 4);
	float Unused = 0.f;
	PlayActionClip(GBValkyrieLocal::ActionClipLances, Unused);
}

void AGBValkyrieEnemy::TickLances(AActor* Goal, float DeltaSeconds)
{
	ActTime += DeltaSeconds;
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
	for (int32 Index = 0; Index < Count; ++Index)
	{
		const float Alpha = Count > 1 ? static_cast<float>(Index) / (Count - 1) : 0.5f;
		const float Yaw = FMath::Lerp(-Fan * 0.5f, Fan * 0.5f, Alpha);
		if (AGBProjectile::SpawnProjectile(World, Config, Origin, Aim.RotateAngleAxis(Yaw, FVector::UpVector), this))
		{
			++LancesFired;
		}
	}
	++VolleyCount;
}
