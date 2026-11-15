#include "Spells/GBSpell_Beam.h"
#include "Combat/GBCombatLibrary.h"
#include "Components/BillboardComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
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

	// Sprites along the cone, growing with its width.
	const FName SpriteId = GetSpriteId();
	UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(this);
	if (!SpriteId.IsNone() && Library)
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
