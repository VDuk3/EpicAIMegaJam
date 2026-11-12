#include "Spells/GBSpell_Zone.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "TimerManager.h"

namespace
{
	// Visual only: thickness of the glowing disc and how far it floats above the ground (no z-fighting).
	constexpr float ZoneDiscThicknessCm = 4.f;
	constexpr float ZoneDiscLiftCm = 2.f;
}

AGBSpell_Zone::AGBSpell_Zone()
{
	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetGenerateOverlapEvents(false);
	Disc->SetCastShadow(false);

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
}

void AGBSpell_Zone::StartSpell()
{
	ZoneRadius = FMath::Max(1.f, ReadParam(GBStat::Radius, 300.f));
	ZoneHeight = FMath::Max(1.f, SpellCast.Get(GBStat::Height, 200.f));
	const float Duration = ReadParam(GBStat::Duration, 5.f);
	const float Interval = SafeInterval(ReadParam(GBStat::TickInterval, 0.5f));
	DamagePerTick = FMath::Max(0.f, SpellCast.Get(GBStat::DamagePerSecond, 0.f)) * Interval;

	SetActorLocationAndRotation(FindAimedGroundPoint(SpellCast.Get(GBStat::Range, 0.f)), FRotator(0.f, SpellCast.Direction.Rotation().Yaw, 0.f));

	Disc->SetRelativeLocation(FVector(0.f, 0.f, ZoneDiscLiftCm + ZoneDiscThicknessCm * 0.5f));
	GBPlaceholder::SetupShape(Disc, EGBShape::Cylinder, FVector(ZoneRadius * 2.f, ZoneRadius * 2.f, ZoneDiscThicknessCm), GetSpellColor(), true);

	const float SpriteHeight = SpellCast.Get(GBSpellParam::SpriteSize, 60.f);
	Sprite->SetRelativeLocation(FVector(0.f, 0.f, SpriteHeight * 0.5f));
	SetupSpellSprite(Sprite, 60.f);

	if (SpellCast.GetString(GBString::OnHitEffect).IsEmpty() && DamagePerTick <= 0.f)
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': zone with no OnHitEffect and no DamagePerSecond does nothing."), *SpellCast.SpellId.ToString());
	}

	GetWorldTimerManager().SetTimer(TickTimer, this, &AGBSpell_Zone::ZoneTick, Interval, true, 0.f);
	EndSpellAfter(Duration);
}

void AGBSpell_Zone::ZoneTick()
{
	TArray<AActor*> Targets;
	GatherHostilesOnDisc(GetActorLocation(), ZoneRadius, ZoneHeight, Targets);
	for (AActor* Target : Targets)
	{
		if (DamagePerTick > 0.f)
		{
			// DealDamage also applies the on-hit effect.
			DamageTarget(Target, DamagePerTick, GetActorLocation());
		}
		else
		{
			ApplyOnHitEffect(Target);
		}
	}
}
