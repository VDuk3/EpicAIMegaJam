#include "Spells/GBSpell_Orb.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"
#include "TimerManager.h"

namespace
{
	/** Visual only: how bright the area sphere is compared with the spell color. */
	constexpr float OrbAreaBrightness = 0.15f;
}

AGBSpell_Orb::AGBSpell_Orb()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);

	Area = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Area"));
	Area->SetupAttachment(Root);
	Area->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Area->SetGenerateOverlapEvents(false);
	Area->SetCastShadow(false);
}

void AGBSpell_Orb::StartSpell()
{
	MoveSpeed = FMath::Max(0.f, ReadParam(GBStat::Speed, 400.f));
	RangeLeft = FMath::Max(0.f, ReadParam(GBStat::Range, 2000.f));
	OrbRadius = FMath::Max(1.f, ReadParam(GBStat::Radius, 250.f));
	const float Duration = ReadParam(GBStat::Duration, 6.f);
	const float Interval = SafeInterval(ReadParam(GBStat::TickInterval, 0.5f));
	DamagePerTick = FMath::Max(0.f, ReadParam(GBStat::DamagePerSecond, 20.f)) * Interval;

	MoveDirection = SpellCast.Direction.GetSafeNormal();
	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = GetActorForwardVector();
	}
	FVector Start = SpellCast.Origin;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetCaster()))
	{
		Start = Player->GetMuzzleLocation();
	}
	SetActorLocation(Start);

	SetupSpellSprite(Sprite, 60.f);
	if (SpellCast.Get(GBSpellParam::ShowArea, 1.f) > 0.5f)
	{
		GBPlaceholder::SetupShape(Area, EGBShape::Sphere, FVector(OrbRadius * 2.f), GetSpellColor() * OrbAreaBrightness, true);
	}
	else
	{
		Area->SetVisibility(false);
	}

	bMoving = MoveSpeed > 0.f && RangeLeft > 0.f;
	SetActorTickEnabled(bMoving);
	GetWorldTimerManager().SetTimer(TickTimer, this, &AGBSpell_Orb::DamageTick, Interval, true, 0.f);
	EndSpellAfter(Duration);
}

void AGBSpell_Orb::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bMoving || bEnded)
	{
		return;
	}

	const float Step = FMath::Min(MoveSpeed * DeltaSeconds, RangeLeft);
	const FVector From = GetActorLocation();
	const FVector To = From + MoveDirection * Step;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellOrbMove), false, this);
	if (AActor* Caster = GetCaster())
	{
		Params.AddIgnoredActor(Caster);
	}
	FHitResult Hit;
	if (GetWorld()->LineTraceSingleByObjectType(Hit, From, To, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		// Stops at the wall and keeps crackling there until its time runs out.
		SetActorLocation(Hit.Location);
		bMoving = false;
		SetActorTickEnabled(false);
		return;
	}

	SetActorLocation(To);
	RangeLeft -= Step;
	if (RangeLeft <= KINDA_SMALL_NUMBER)
	{
		EndSpell();
	}
}

void AGBSpell_Orb::DamageTick()
{
	if (DamagePerTick <= 0.f)
	{
		return;
	}
	TArray<AActor*> Targets;
	GatherHostilesInSphere(GetActorLocation(), OrbRadius, Targets);
	for (AActor* Target : Targets)
	{
		DamageTarget(Target, DamagePerTick, GetActorLocation());
	}
}
