#include "Spells/GBSpell_Trap.h"
#include "Combat/GBCombatLibrary.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "TimerManager.h"

namespace
{
	// Visual only.
	constexpr float TrapDiscThicknessCm = 3.f;
	constexpr float TrapDiscLiftCm = 2.f;
	/** Glow brightness of the trigger area while the rune is not armed yet. */
	constexpr float TrapUnarmedBrightness = 0.3f;
}

AGBSpell_Trap::AGBSpell_Trap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Disc = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Disc"));
	Disc->SetupAttachment(Root);
	Disc->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Disc->SetGenerateOverlapEvents(false);
	Disc->SetCastShadow(false);

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
}

void AGBSpell_Trap::StartSpell()
{
	TriggerRadius = FMath::Max(1.f, ReadParam(GBSpellParam::TriggerRadius, 150.f));
	TriggerHeight = FMath::Max(1.f, SpellCast.Get(GBStat::Height, 200.f));
	const float ArmTime = ReadParam(GBSpellParam::ArmTime, 0.5f);
	const float Duration = ReadParam(GBStat::Duration, 20.f);

	SetActorLocationAndRotation(FindAimedGroundPoint(ReadParam(GBStat::Range, 1500.f)), FRotator(0.f, SpellCast.Direction.Rotation().Yaw, 0.f));

	Disc->SetRelativeLocation(FVector(0.f, 0.f, TrapDiscLiftCm + TrapDiscThicknessCm * 0.5f));
	GBPlaceholder::SetupShape(Disc, EGBShape::Cylinder, FVector(TriggerRadius * 2.f, TriggerRadius * 2.f, TrapDiscThicknessCm),
		GetSpellColor() * TrapUnarmedBrightness, true);

	const float SpriteHeight = SpellCast.Get(GBSpellParam::SpriteSize, 50.f);
	Sprite->SetRelativeLocation(FVector(0.f, 0.f, SpriteHeight * 0.5f));
	SetupSpellSprite(Sprite, 50.f);

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
	GBPlaceholder::SetShapeColor(Disc, GetSpellColor());
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
