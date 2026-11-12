#include "Spells/GBSpell_Shield.h"
#include "Components/BillboardComponent.h"
#include "Core/GBStatNames.h"
#include "Stats/GBAttributeComponent.h"

AGBSpell_Shield::AGBSpell_Shield()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Root);
}

float AGBSpell_Shield::ComputeBlockedDamage(float Amount, const FVector& HitDirection, const FVector& AimDirection, float InBlockFraction, float ShieldHealthLeft)
{
	if (Amount <= 0.f || ShieldHealthLeft <= 0.f || HitDirection.IsNearlyZero() || AimDirection.IsNearlyZero())
	{
		return 0.f;
	}
	// A hit from the front travels against the aim.
	if (FVector::DotProduct(HitDirection.GetSafeNormal(), AimDirection.GetSafeNormal()) >= 0.f)
	{
		return 0.f;
	}
	return FMath::Min(Amount * FMath::Clamp(InBlockFraction, 0.f, 1.f), ShieldHealthLeft);
}

void AGBSpell_Shield::StartSpell()
{
	const float Duration = ReadParam(GBStat::Duration, 5.f);
	BlockFraction = FMath::Clamp(ReadParam(GBStat::BlockFraction, 0.75f), 0.f, 1.f);
	ShieldHealth = FMath::Max(1.f, ReadParam(GBStat::Health, 150.f));
	SpriteDistance = SpellCast.Get(GBSpellParam::Distance, 80.f);

	if (UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(GetCaster()))
	{
		BoundAttributes = Attributes;
		DamageHandle = Attributes->OnModifyIncomingDamage.AddUObject(this, &AGBSpell_Shield::HandleIncomingDamage);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("Spell '%s': the caster has no attributes, the shield cannot block."), *SpellCast.SpellId.ToString());
	}

	SetupSpellSprite(Sprite, 45.f);
	FollowAim();
	SetActorTickEnabled(true);
	EndSpellAfter(Duration);
}

void AGBSpell_Shield::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBAttributeComponent* Attributes = BoundAttributes.Get())
	{
		Attributes->OnModifyIncomingDamage.Remove(DamageHandle);
	}
	BoundAttributes.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGBSpell_Shield::HandleIncomingDamage(FGBDamageInfo& Info)
{
	if (bEnded || ShieldHealth <= 0.f)
	{
		return;
	}
	AActor* Caster = GetCaster();
	FVector HitDirection = Info.HitDirection;
	if (HitDirection.IsNearlyZero() && Caster)
	{
		const AActor* Source = Info.Causer ? Info.Causer.Get() : Info.Instigator.Get();
		if (Source)
		{
			HitDirection = Caster->GetActorLocation() - Source->GetActorLocation();
		}
	}
	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);

	const float Blocked = ComputeBlockedDamage(Info.Amount, HitDirection, AimDir, BlockFraction, ShieldHealth);
	if (Blocked <= 0.f)
	{
		return;
	}
	Info.Amount -= Blocked;
	ShieldHealth -= Blocked;
	if (ShieldHealth <= KINDA_SMALL_NUMBER)
	{
		ShieldHealth = 0.f;
		// Broken: end after this damage call has finished.
		EndSpellAfter(0.f);
	}
}

void AGBSpell_Shield::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bEnded)
	{
		FollowAim();
	}
}

void AGBSpell_Shield::FollowAim()
{
	FVector AimOrigin;
	FVector AimDir;
	GetCasterAim(AimOrigin, AimDir);
	SetActorLocationAndRotation(AimOrigin + AimDir * SpriteDistance, AimDir.Rotation());
}
