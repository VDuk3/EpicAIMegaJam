#include "Spells/GBSpell_Beam.h"
#include "Combat/GBCombatLibrary.h"
#include "Components/BillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Core/GBStatNames.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "TimerManager.h"
#include "UI/GBHUD.h"
#include "UI/GBPixelSpriteLibrary.h"

namespace
{
	// Visual only: how many sprites draw the cone. They bunch up near the start (squared spacing) so the stream
	// visibly leaves the staff's head as a small flame and widens from there (owner, 2026-11-15).
	constexpr int32 BeamSpriteCount = 7;
	/** The flame nearest the start is at least this share of the spell's SpriteSize. */
	constexpr float BeamNearSizeShare = 0.25f;
}

// The flame stream (owner, 2026-11-15: a real animated pixel-art spell, not one picture repeated). A small emitter of
// GBPixelVfx clips: each puff is one clip (Strings "StreamVfx" + A/B/C, drawn by Tools/Jam/make_flame_stream.js)
// whose frames are the puff's age, from a white-hot core to red rags and smoke. The puffs leave the staff's head one
// after another, speed up along the cone and grow with it, so seen from first person they overlap into one stream.
// Everything is a pure function of the time since the cast and the puff's index: no per-puff state is kept.
// The same numbers are in make_flame_stream.js (STREAM), which renders the first-person preview.
namespace GBBeamStream
{
	const TCHAR* const VariantSuffixes[] = { TEXT("A"), TEXT("B"), TEXT("C") };
	const FName PuffName(TEXT("FlamePuff"));
	const FName LightName(TEXT("FlameStreamLight"));
	constexpr int32 PuffCount = 16;
	/** Share of the spell's Range the puffs travel before they are gone. */
	constexpr float Reach = 0.92f;
	/** Travel = age ^ this: above 1 the puffs bunch up at the staff, which is what fills the screen near the camera. */
	constexpr float TravelPower = 2.f;
	/** Size (cm) of a puff as it leaves the staff. */
	constexpr float NearSize = 9.f;
	/** Half-angle of the drawn stream; narrower than the damage cone so the target stays visible. */
	constexpr float VisualHalfAngleDeg = 10.f;
	/** Sideways scatter of a puff, as a share of its size. */
	constexpr float Jitter = 0.2f;
	/** One unshadowed light for the whole stream, a quarter of the way along it. */
	constexpr float LightCandelas = 30.f;
	constexpr float LightRadius = 900.f;

	float Hash01(int32 A, int32 B, int32 C)
	{
		return float(HashCombineFast(HashCombineFast(GetTypeHash(A), GetTypeHash(B)), GetTypeHash(C)) & 0xFFFF) / 65535.f;
	}

	const FGBPixelVfxClip* FindClip(const UObject* WorldContext, const FString& ClipBase, int32 Variant)
	{
		if (ClipBase.IsEmpty())
		{
			return nullptr;
		}
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		return Vfx ? Vfx->FindClip(FName(*(ClipBase + VariantSuffixes[Variant % UE_ARRAY_COUNT(VariantSuffixes)]))) : nullptr;
	}

	/** Seconds one puff lives = the length of its clip. 0 when the spell has no stream clips. */
	float PuffLife(const UObject* WorldContext, const FString& ClipBase)
	{
		const FGBPixelVfxClip* Clip = FindClip(WorldContext, ClipBase, 0);
		return (Clip && Clip->Fps > 0 && Clip->FrameCount > 0 && Clip->WorldHeightCm > 0.f) ? float(Clip->FrameCount) / float(Clip->Fps) : 0.f;
	}

	/** Builds the puffs and the light. False (nothing built) when the clips are missing. */
	bool Build(AActor* Beam, USceneComponent* Root, const FString& ClipBase)
	{
		const float Life = PuffLife(Beam, ClipBase);
		if (Life <= 0.f)
		{
			return false;
		}
		const FName FirstId = FindClip(Beam, ClipBase, 0)->Id;
		int32 Built = 0;
		for (int32 Index = 0; Index < PuffCount; ++Index)
		{
			const FGBPixelVfxClip* Variant = FindClip(Beam, ClipBase, Index);
			UGBPixelVfxComponent* Puff = NewObject<UGBPixelVfxComponent>(Beam, FName(PuffName, Index + 1), RF_Transient);
			Puff->FacingOverride = TEXT("Billboard");
			Puff->bEnableLight = false;
			Puff->SizeScale = 0.01f;
			// The puff's frame is its age: the clip starts at the same phase its position uses.
			Puff->StartTimeOffset = Index * Life / PuffCount;
			Puff->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Puff->RegisterComponent();
			// The clip places its quad in world space in its own tick: after the beam has followed the aim, or the
			// stream would trail the staff by a frame when the player turns.
			Puff->AddTickPrerequisiteActor(Beam);
			if (Puff->Play(Variant ? Variant->Id : FirstId))
			{
				Puff->SetVisibility(false, true);
				++Built;
			}
			else
			{
				Puff->DestroyComponent();
			}
		}
		if (Built == 0)
		{
			return false;
		}
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Beam, LightName, RF_Transient);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(FColor(0xF3, 0x61, 0x0B)));
		Light->SetAttenuationRadius(LightRadius);
		Light->SetCastShadows(false);
		Light->SetIntensity(LightCandelas);
		Light->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Light->RegisterComponent();
		UE_LOG(LogGB, Display, TEXT("Beam: flame stream '%s' with %d puffs (life %.2fs)"), *ClipBase, Built, Life);
		return true;
	}

	/** Places every puff for this moment. Elapsed = seconds since the cast, Life = seconds one puff lives. */
	void Update(AActor* Beam, float Elapsed, float Range, float Life)
	{
		if (Life <= 0.f)
		{
			return;
		}
		const float TanHalf = FMath::Tan(FMath::DegreesToRadians(VisualHalfAngleDeg));
		TInlineComponentArray<UGBPixelVfxComponent*> Puffs(Beam);
		for (UGBPixelVfxComponent* Puff : Puffs)
		{
			if (!Puff || Puff->GetFName().GetComparisonIndex() != PuffName.GetComparisonIndex())
			{
				continue;
			}
			const int32 Index = Puff->GetFName().GetNumber() - 1;
			const float Time = FMath::Max(0.f, Elapsed) + Index * Life / PuffCount;
			const int32 Cycle = FMath::FloorToInt(Time / Life);
			const float Age = (Time - Cycle * Life) / Life;
			// Not emitted yet (the stream grows out of the staff when the cast starts), or in its last, empty frame:
			// hidden, so a puff is never seen jumping back to the staff.
			const bool bShown = (Index == 0 || Cycle >= 1) && Age < 0.93f;
			if (Puff->IsVisible() != bShown)
			{
				Puff->SetVisibility(bShown, true);
			}
			if (!bShown)
			{
				continue;
			}
			const float Along = Range * Reach * FMath::Pow(Age, TravelPower);
			const float Size = NearSize + 2.f * Along * TanHalf;
			// A new scatter every time the puff leaves; none at the staff's head, so the stream stays on it.
			const float Spread = Jitter * Size * FMath::Min(1.f, Along / 60.f);
			Puff->SizeScale = Size / 100.f;
			Puff->SetRelativeLocation(FVector(Along,
				(Hash01(Index, Cycle, 1) * 2.f - 1.f) * Spread, (Hash01(Index, Cycle, 2) * 2.f - 1.f) * Spread));
		}
		TInlineComponentArray<UPointLightComponent*> Lights(Beam);
		for (UPointLightComponent* Light : Lights)
		{
			if (Light && Light->GetFName() == LightName)
			{
				// Flicker in steps (12 per second), like the frames.
				const int32 Step = FMath::FloorToInt(Elapsed * 12.f);
				const float Grow = FMath::Clamp(Elapsed / Life, 0.2f, 1.f);
				Light->SetRelativeLocation(FVector(Range * 0.25f * Grow, 0.f, 0.f));
				Light->SetIntensity(LightCandelas * Grow * (0.75f + 0.25f * Hash01(Step, 7, 3)));
			}
		}
	}
}

AGBSpell_Beam::AGBSpell_Beam()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void AGBSpell_Beam::StartSpell()
{
	BeamRange = FMath::Max(1.f, ReadParam(GBStat::Range, 600.f));
	ConeHalfAngleDeg = FMath::Clamp(ReadParam(GBSpellParam::ConeAngle, 20.f), 1.f, 89.f);
	// Duration <= 0 (or missing) = no time limit: Get, not ReadParam, so no warning is logged.
	BeamMaxDuration = FMath::Max(0.f, SpellCast.Get(GBStat::Duration, 0.f));
	BeamMinDuration = FMath::Max(0.f, SpellCast.Get(GBSpellParam::MinDuration, 0.f));
	bHoldToSustain = SpellCast.Get(GBSpellParam::HoldToSustain, 0.f) > 0.5f;
	BeamStartTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	InterruptFraction = SpellCast.Get(GBSpellParam::InterruptHitFraction, 0.f);
	const float Interval = SafeInterval(ReadParam(GBStat::TickInterval, 0.2f));
	DamagePerTick = FMath::Max(0.f, ReadParam(GBStat::DamagePerSecond, 30.f)) * Interval;
	ManaPerTick = FMath::Max(0.f, SpellCast.Get(GBSpellParam::ManaPerSecond, 0.f)) * Interval;

	// The flame stream (Strings "StreamVfx"); without its clips, the old look: the sprite drawn several times along the
	// cone, growing with its width.
	const bool bStream = GBBeamStream::Build(this, Root, SpellCast.GetString(TEXT("StreamVfx")));
	const FName SpriteId = GetSpriteId();
	UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(this);
	if (!bStream && !SpriteId.IsNone() && Library)
	{
		const float NearHeight = FMath::Max(1.f, SpellCast.Get(GBSpellParam::SpriteSize, 40.f));
		const float TanHalfAngle = FMath::Tan(FMath::DegreesToRadians(ConeHalfAngleDeg));
		for (int32 Index = 0; Index < BeamSpriteCount; ++Index)
		{
			const float T = float(Index + 1) / float(BeamSpriteCount);
			const float Along = BeamRange * 0.85f * T * T;
			UBillboardComponent* Sprite = NewObject<UBillboardComponent>(this);
			Sprite->SetupAttachment(Root);
			Sprite->RegisterComponent();
			AddInstanceComponent(Sprite);
			Sprite->SetRelativeLocation(FVector(Along, 0.f, 0.f));
			Library->SetupBillboard(Sprite, SpriteId, FMath::Max(NearHeight * BeamNearSizeShare, 2.f * Along * TanHalfAngle));
			Sprites.Add(Sprite);
		}
	}

	FollowAim();
	SetActorTickEnabled(true);
	GetWorldTimerManager().SetTimer(TickTimer, this, &AGBSpell_Beam::DamageTick, Interval, true, 0.f);
	SubscribeCasterDamage();
	if (BeamMaxDuration > 0.f)
	{
		EndSpellAfter(BeamMaxDuration);
	}
}

bool AGBSpell_Beam::ShouldBeamContinue(bool bHoldToSustain, bool bKeyHeld, float Elapsed, float MinDuration, float MaxDuration)
{
	if (MaxDuration > 0.f && Elapsed >= MaxDuration)
	{
		return false;
	}
	if (Elapsed < MinDuration)
	{
		return true;
	}
	return !bHoldToSustain || bKeyHeld;
}

bool AGBSpell_Beam::IsInterruptingHit(float DamageTaken, float MaxHealth, float InterruptHitFraction)
{
	if (InterruptHitFraction <= 0.f || MaxHealth <= 0.f)
	{
		return false;
	}
	// Small tolerance so "30% or more" holds in float math (0.3f * 100 is 30.000001, which would miss a hit of 30).
	const float Threshold = InterruptHitFraction * MaxHealth;
	return DamageTaken >= Threshold - KINDA_SMALL_NUMBER * FMath::Max(1.f, Threshold);
}

void AGBSpell_Beam::SubscribeCasterDamage()
{
	if (InterruptFraction <= 0.f)
	{
		return;
	}
	UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetCaster());
	if (!Attributes)
	{
		return;
	}
	UnsubscribeCasterDamage();
	BoundCasterAttributes = Attributes;
	CasterDamagedHandle = Attributes->OnDamaged.AddUObject(this, &AGBSpell_Beam::HandleCasterDamaged);
}

void AGBSpell_Beam::UnsubscribeCasterDamage()
{
	if (UGBAttributeComponent* Attributes = BoundCasterAttributes.Get())
	{
		Attributes->OnDamaged.Remove(CasterDamagedHandle);
	}
	CasterDamagedHandle.Reset();
	BoundCasterAttributes.Reset();
}

void AGBSpell_Beam::HandleCasterDamaged(const FGBDamageInfo& Info, float ActualDamage)
{
	if (bEnded || InterruptFraction <= 0.f)
	{
		return;
	}
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetCaster());
	const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;
	if (IsInterruptingHit(ActualDamage, MaxHealth, InterruptFraction))
	{
		HandleInterrupt();
	}
}

void AGBSpell_Beam::HandleInterrupt()
{
	if (bEnded)
	{
		return;
	}
	if (AGBHUD* Hud = AGBHUD::Get(this))
	{
		Hud->ShowMessage(TEXT("Interrupted!"), 1.5f);
	}
	if (AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const float Duration = Data ? Data->GetGameValue(GBInterruptShake::Duration, 0.35f) : 0.35f;
		const float Amplitude = Data ? Data->GetGameValue(GBInterruptShake::Amplitude, 2.5f) : 2.5f;
		const float Frequency = Data ? Data->GetGameValue(GBInterruptShake::Frequency, 22.f) : 22.f;
		Player->StartViewShake(Duration, Amplitude, Frequency);
	}
	EndSpell();
}

void AGBSpell_Beam::EndSpell()
{
	UnsubscribeCasterDamage();
	Super::EndSpell();
}

void AGBSpell_Beam::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnsubscribeCasterDamage();
	Super::EndPlay(EndPlayReason);
}

void AGBSpell_Beam::CheckSustain()
{
	if (bEnded)
	{
		return;
	}
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : BeamStartTime;
	const float Elapsed = static_cast<float>(Now - BeamStartTime);
	bool bKeyHeld = true;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		bKeyHeld = Player->IsSpellInputHeld(SpellCast.SpellId);
	}
	if (!ShouldBeamContinue(bHoldToSustain, bKeyHeld, Elapsed, BeamMinDuration, BeamMaxDuration))
	{
		EndSpell();
	}
}

void AGBSpell_Beam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEnded)
	{
		FollowAim();
		CheckSustain();
	}
}

void AGBSpell_Beam::FollowAim()
{
	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);
	FVector Start = AimOrigin;
	FRotator Facing = AimDir.Rotation();
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		// From the muzzle (the staff's head when a staff is held) toward the crosshair at full range, so the stream
		// leaves the staff and meets the aim (owner, 2026-11-15). Damage still uses the camera's cone.
		Start = Player->GetMuzzleLocation();
		const FVector ToAim = AimOrigin + AimDir * BeamRange - Start;
		if (!ToAim.IsNearlyZero())
		{
			Facing = ToAim.Rotation();
		}
	}
	SetActorLocationAndRotation(Start, Facing);
	const FString StreamVfx = SpellCast.GetString(TEXT("StreamVfx"));
	if (!StreamVfx.IsEmpty() && GetWorld())
	{
		GBBeamStream::Update(this, static_cast<float>(GetWorld()->GetTimeSeconds() - BeamStartTime), BeamRange,
			GBBeamStream::PuffLife(this, StreamVfx));
	}
}

void AGBSpell_Beam::DamageTick()
{
	AActor* Caster = GetCaster();
	if (!Caster || !UGBCombatLibrary::IsAlive(Caster))
	{
		EndSpell();
		return;
	}
	CheckSustain();
	if (bEnded)
	{
		return;
	}
	if (ManaPerTick > 0.f)
	{
		UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Caster);
		if (!Attributes || !Attributes->SpendMana(ManaPerTick))
		{
			EndSpell();
			return;
		}
	}
	if (DamagePerTick <= 0.f)
	{
		return;
	}

	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);
	const float MinDot = FMath::Cos(FMath::DegreesToRadians(ConeHalfAngleDeg));

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellBeamSight), false, this);
	Params.AddIgnoredActor(Caster);

	TArray<AActor*> Candidates;
	GatherHostilesInSphere(AimOrigin, BeamRange, Candidates);
	for (AActor* Target : Candidates)
	{
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector ToTarget = TargetLocation - AimOrigin;
		const float Distance = ToTarget.Size();
		if (Distance > KINDA_SMALL_NUMBER && FVector::DotProduct(ToTarget / Distance, AimDir) < MinDot)
		{
			continue;
		}
		if (GetWorld()->LineTraceTestByObjectType(AimOrigin, TargetLocation, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			continue;
		}
		DamageTarget(Target, DamagePerTick, AimOrigin);
	}
}
