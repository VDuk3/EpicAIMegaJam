#include "Spells/GBSpell_Trap.h"
#include "Combat/GBCombatLibrary.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "TimerManager.h"

namespace
{
	// Visual only: how far the flat figure floats above the ground (no z-fighting), and the fallback
	// disc thickness when the spell has no sprite.
	constexpr float TrapFigureLiftCm = 3.f;
	constexpr float TrapDiscThicknessCm = 3.f;
	/** Glow brightness of the trigger area while the rune is not armed yet. */
	constexpr float TrapUnarmedBrightness = 0.3f;
}

AGBSpell_Trap::AGBSpell_Trap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Figure = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Figure"));
	Figure->SetupAttachment(Root);
	Figure->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Figure->SetGenerateOverlapEvents(false);
	Figure->SetCastShadow(false);
}

void AGBSpell_Trap::StartSpell()
{
	TriggerRadius = FMath::Max(1.f, ReadParam(GBSpellParam::TriggerRadius, 150.f));
	TriggerHeight = FMath::Max(1.f, SpellCast.Get(GBStat::Height, 200.f));
	const float ArmTime = ReadParam(GBSpellParam::ArmTime, 0.5f);
	const float Duration = ReadParam(GBStat::Duration, 20.f);

	SetActorLocationAndRotation(FindAimedGroundPoint(ReadParam(GBStat::Range, 1500.f)), FRotator(0.f, SpellCast.Direction.Rotation().Yaw, 0.f));

	const FString ColorStr = SpellCast.GetString(GBString::Color);
	FigureTint = ColorStr.IsEmpty() ? FLinearColor::White : UGBDataSubsystem::ParseColor(ColorStr, FLinearColor::White);
	FigureGlowValue = SpellCast.Get(GBSpellParam::FigureGlow, 1.f);

	Figure->SetRelativeLocation(FVector(0.f, 0.f, TrapFigureLiftCm));
	const FName SpriteId = GetSpriteId();
	if (SpriteId.IsNone())
	{
		Figure->SetRelativeLocation(FVector(0.f, 0.f, TrapFigureLiftCm + TrapDiscThicknessCm * 0.5f));
		GBPlaceholder::SetupShape(Figure, EGBShape::Cylinder, FVector(TriggerRadius * 2.f, TriggerRadius * 2.f, TrapDiscThicknessCm),
			GetSpellColor() * TrapUnarmedBrightness, true);
	}
	else
	{
		FLinearColor Dimmed = FigureTint * TrapUnarmedBrightness;
		Dimmed.A = 1.f;
		GBPlaceholder::SetupFlatSprite(Figure, SpriteId, TriggerRadius * 2.f, Dimmed, FigureGlowValue * TrapUnarmedBrightness);
	}

	if (ArmTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(ArmTimer, this, &AGBSpell_Trap::Arm, ArmTime, false);
	}
	else
	{
		Arm();
	}
	EndSpellAfter(Duration);
}

void AGBSpell_Trap::Arm()
{
	bArmed = true;
	if (GetSpriteId().IsNone())
	{
		GBPlaceholder::SetShapeColor(Figure, GetSpellColor());
	}
	else
	{
		GBPlaceholder::SetFlatSpriteLook(Figure, FigureTint, FigureGlowValue);
	}
	SetActorTickEnabled(true);
}

void AGBSpell_Trap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bArmed || bEnded)
	{
		return;
	}
	TArray<AActor*> Targets;
	GatherHostilesOnDisc(GetActorLocation(), TriggerRadius, TriggerHeight, Targets);
	if (Targets.Num() > 0)
	{
		Trigger();
	}
}

void AGBSpell_Trap::Trigger()
{
	bArmed = false;
	SetActorTickEnabled(false);

	const FVector BlastCenter = GetActorLocation() + FVector::UpVector * SpellCast.Get(GBSpellParam::BlastHeight, 50.f);
	FGBDamageInfo Info = SpellCast.MakeDamageInfo(GBStat::Damage, this);
	Info.HitLocation = BlastCenter;
	const bool bFalloff = SpellCast.Get(GBSpellParam::Falloff, 0.f) > 0.5f;
	TArray<AActor*> Ignore;
	Ignore.Add(this);
	UGBCombatLibrary::DealRadialDamage(this, BlastCenter, FMath::Max(1.f, ReadParam(GBStat::Radius, 300.f)), Info, Ignore, bFalloff);
	EndSpell();
}
