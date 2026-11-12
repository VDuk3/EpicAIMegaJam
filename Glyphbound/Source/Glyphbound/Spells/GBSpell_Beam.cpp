#include "Spells/GBSpell_Beam.h"
#include "Combat/GBCombatLibrary.h"
#include "Components/BillboardComponent.h"
#include "Core/GBStatNames.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "TimerManager.h"
#include "UI/GBPixelSpriteLibrary.h"

namespace
{
	// Visual only: how many sprites draw the cone.
	constexpr int32 BeamSpriteCount = 4;
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
	const float Duration = ReadParam(GBStat::Duration, 4.f);
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
			const float Along = BeamRange * (Index + 1) / (BeamSpriteCount + 1);
			UBillboardComponent* Sprite = NewObject<UBillboardComponent>(this);
			Sprite->SetupAttachment(Root);
			Sprite->RegisterComponent();
			AddInstanceComponent(Sprite);
			Sprite->SetRelativeLocation(FVector(Along, 0.f, 0.f));
			Library->SetupBillboard(Sprite, SpriteId, FMath::Max(NearHeight, 2.f * Along * TanHalfAngle));
			Sprites.Add(Sprite);
		}
	}

	FollowAim();
	SetActorTickEnabled(true);
	GetWorldTimerManager().SetTimer(TickTimer, this, &AGBSpell_Beam::DamageTick, Interval, true, 0.f);
	EndSpellAfter(Duration);
}

void AGBSpell_Beam::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEnded)
	{
		FollowAim();
	}
}

void AGBSpell_Beam::FollowAim()
{
	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);
	FVector Start = AimOrigin;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		Start = Player->GetMuzzleLocation();
	}
	SetActorLocationAndRotation(Start, AimDir.Rotation());
}

void AGBSpell_Beam::DamageTick()
{
	AActor* Caster = GetCaster();
	if (!Caster || !UGBCombatLibrary::IsAlive(Caster))
	{
		EndSpell();
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
