#include "World/GBChest.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Core/GBPlaceholder.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "Weapons/GBEquipmentComponent.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"

namespace GBChestLocal
{
	// Placeholder look (cm), visuals only. The chest faces +X.
	const FVector BodySize(110.f, 70.f, 60.f);
	const FVector LidSize(114.f, 74.f, 14.f);
	const FVector LidClosedLocation(0.f, 0.f, 67.f);
	const FVector LidOpenLocation(-42.f, 0.f, 100.f);
	const FRotator LidOpenRotation(75.f, 0.f, 0.f);
	const FLinearColor BodyColor(0.32f, 0.18f, 0.07f);
	const FLinearColor LidColor(0.55f, 0.4f, 0.12f);
	const FLinearColor OpenBodyColor(0.16f, 0.09f, 0.04f);
	const FLinearColor OpenLidColor(0.3f, 0.22f, 0.08f);
	// Where things the player could not carry are left: in front of the chest, spread sideways.
	constexpr float LeftoverForward = 120.f;
	constexpr float LeftoverSpacing = 60.f;
	constexpr float ChestInteractRadius = 120.f;
}

AGBChest::AGBChest()
{
	bSolid = true;

	Lid = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Lid"));
	Lid->SetupAttachment(Root);
	Lid->SetMobility(EComponentMobility::Movable);
	Lid->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Lid->SetGenerateOverlapEvents(false);
	Lid->SetRelativeLocation(GBChestLocal::LidClosedLocation);

	// The body blocks the player, so the volume must reach past it.
	InteractVolume->InitSphereRadius(GBChestLocal::ChestInteractRadius);
}

void AGBChest::Configure(TConstArrayView<FGBDropDefinition> InContents, FName InOpenedFlag)
{
	Contents.Reset();
	Contents.Append(InContents.GetData(), InContents.Num());
	OpenedFlag = InOpenedFlag;
	if (HasActorBegunPlay())
	{
		const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
		SetOpenLook(Progress && Progress->HasFlag(GetOpenedFlag()));
	}
}

void AGBChest::BeginPlay()
{
	Super::BeginPlay();

	SetPlaceholderLook(false, NAME_None, GBChestLocal::BodyColor, GBChestLocal::BodySize);
	GBPlaceholder::SetupShape(Lid, EGBShape::Cube, GBChestLocal::LidSize, GBChestLocal::LidColor, false);

	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	SetOpenLook(Progress && Progress->HasFlag(GetOpenedFlag()));
}

FName AGBChest::GetOpenedFlag() const
{
	return OpenedFlag.IsNone() ? FName(*FString::Printf(TEXT("Chest.%s"), *GetName())) : OpenedFlag;
}

void AGBChest::SetOpenLook(bool bInOpen)
{
	bOpen = bInOpen;
	Lid->SetRelativeLocationAndRotation(bInOpen ? GBChestLocal::LidOpenLocation : GBChestLocal::LidClosedLocation, bInOpen ? GBChestLocal::LidOpenRotation : FRotator::ZeroRotator);
	GBPlaceholder::SetShapeColor(Mesh, bInOpen ? GBChestLocal::OpenBodyColor : GBChestLocal::BodyColor);
	GBPlaceholder::SetShapeColor(Lid, bInOpen ? GBChestLocal::OpenLidColor : GBChestLocal::LidColor);
	InteractVolume->SetCollisionEnabled(bInOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

bool AGBChest::CanInteract(const AGBPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && !bOpen && Player && !Player->IsDead();
}

FString AGBChest::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return TEXT("Open chest");
}

void AGBChest::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}

	TArray<FString> Found;
	int32 Leftovers = 0;
	for (const FGBDropDefinition& Entry : Contents)
	{
		if (Entry.Chance < 1.f && FMath::FRand() > Entry.Chance)
		{
			continue;
		}
		if (!AGBPickup::IsDropValid(this, Entry))
		{
			UE_LOG(LogGB, Warning, TEXT("%s: skipping empty or unknown content (item %s, weapon %s, spell group %s)."), *GetName(),
				*Entry.ItemId.ToString(), *Entry.WeaponId.ToString(), *Entry.SpellGroup.ToString());
			continue;
		}

		FGBDropDefinition Remaining = Entry;
		bool bAllTaken = false;
		if (AGBPickup::GiveDrop(Player, Remaining, bAllTaken))
		{
			FGBDropDefinition TakenPart = Entry;
			if (!Entry.ItemId.IsNone())
			{
				TakenPart.Count = FMath::Max(1, Entry.Count) - FMath::Max(0, Remaining.Count);
			}
			if (!Entry.WeaponId.IsNone())
			{
				// A weapon that did not make it into the hands is stored: say so (owner's rule, 2026-11-12).
				const UGBEquipmentComponent* Equipment = Player->GetEquipment();
				if (Equipment && Equipment->OwnsWeapon(Entry.WeaponId) && !Equipment->IsWeaponEquipped(Entry.WeaponId))
				{
					Found.Add(FString::Printf(TEXT("%s added to the inventory"), *AGBPickup::GetDropDisplayName(this, TakenPart, true)));
				}
				else
				{
					Found.Add(AGBPickup::GetDropDisplayName(this, TakenPart, true));
				}
			}
			else
			{
				Found.Add(AGBPickup::GetDropDisplayName(this, TakenPart, true));
			}
		}
		if (!bAllTaken)
		{
			const float Side = (Leftovers % 2 == 0 ? 1.f : -1.f) * GBChestLocal::LeftoverSpacing * ((Leftovers + 1) / 2);
			const FVector Location = GetActorLocation() + GetActorForwardVector() * GBChestLocal::LeftoverForward + GetActorRightVector() * Side;
			AGBPickup::SpawnPickup(GetWorld(), Remaining, Location);
			++Leftovers;
		}
	}

	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->SetFlag(GetOpenedFlag());
	}
	SetOpenLook(true);

	if (AGBHUD* Hud = AGBHUD::Get(this))
	{
		Hud->ShowMessage(Found.Num() > 0 ? FString::Printf(TEXT("Found: %s"), *FString::Join(Found, TEXT(", "))) : FString(TEXT("The chest is empty")));
	}
}
