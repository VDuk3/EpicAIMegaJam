#include "Spells/GBSpell_Zone.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "TimerManager.h"

namespace
{
	// Visual only: how far the flat figure floats above the ground (no z-fighting), and the fallback
	// disc thickness when the spell has no sprite.
	constexpr float ZoneFigureLiftCm = 3.f;
	constexpr float ZoneDiscThicknessCm = 4.f;
	const FName FigureGlowParam(TEXT("FigureGlow"));
}

AGBSpell_Zone::AGBSpell_Zone()
{
	Figure = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Figure"));
	Figure->SetupAttachment(Root);
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetGenerateOverlapEvents(false);
	Figure->SetCastShadow(false);
}

void AGBSpell_Zone::StartSpell()
{
	ZoneRadius = FMath::Max(1.f, ReadParam(GBStat::Radius, 300.f));
	ZoneHeight = FMath::Max(1.f, SpellCast.Get(GBStat::Height, 200.f));
	const float Duration = ReadParam(GBStat::Duration, 5.f);
	const float Interval = SafeInterval(ReadParam(GBStat::TickInterval, 0.5f));
	DamagePerTick = FMath::Max(0.f, SpellCast.Get(GBStat::DamagePerSecond, 0.f)) * Interval;

	SetActorLocationAndRotation(FindAimedGroundPoint(SpellCast.Get(GBStat::Range, 0.f)), FRotator(0.f, SpellCast.Direction.Rotation().Yaw, 0.f));

	const FString ColorStr = SpellCast.GetString(GBString::Color);
	const FLinearColor Tint = ColorStr.IsEmpty() ? FLinearColor::White : UGBDataSubsystem::ParseColor(ColorStr, FLinearColor::White);
	const float Glow = SpellCast.Get(FigureGlowParam, 1.f);

	Figure->SetRelativeLocation(FVector(0.f, 0.f, ZoneFigureLiftCm));
	const FName SpriteId = GetSpriteId();
	if (SpriteId.IsNone())
	{
		Figure->SetRelativeLocation(FVector(0.f, 0.f, ZoneFigureLiftCm + ZoneDiscThicknessCm * 0.5f));
		GBPlaceholder::SetupShape(Figure, EGBShape::Cylinder, FVector(ZoneRadius * 2.f, ZoneRadius * 2.f, ZoneDiscThicknessCm), GetSpellColor(), true);
	}
	else
	{
		GBPlaceholder::SetupFlatSprite(Figure, SpriteId, ZoneRadius * 2.f, Tint, Glow);
	}

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
