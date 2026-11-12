#include "World/GBPickup.h"

#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Items/GBInventoryComponent.h"
#include "Player/GBPlayerCharacter.h"
#include "Spells/GBSpellbookComponent.h"
#include "UI/GBHUD.h"
#include "Weapons/GBEquipmentComponent.h"
#include "World/GBProgressSubsystem.h"

namespace GBPickupLocal
{
	// Placeholder look sizes and scatter offsets (cm): visuals only.
	const FVector ItemSpriteSize(30.f, 30.f, 40.f);
	const FVector ItemBoxSize(25.f, 25.f, 25.f);
	const FVector GunBoxSize(45.f, 12.f, 20.f);
	const FVector StaffSpriteSize(20.f, 20.f, 100.f);
	const FVector BookSpriteSize(40.f, 40.f, 45.f);
	const FLinearColor DefaultGunColor(0.08f, 0.08f, 0.1f);
	const FLinearColor DefaultItemColor(0.6f, 0.6f, 0.6f);
	constexpr float ScatterRadius = 90.f;
	constexpr float GroundTraceUp = 100.f;
	constexpr float GroundTraceDown = 2000.f;
	const FName BookSpriteId(TEXT("book"));
	const FName StaffSpriteId(TEXT("staff"));

	FName SpriteFromStrings(const TMap<FName, FString>& Strings, FName Fallback)
	{
		const FString* Found = Strings.Find(GBString::Sprite);
		return (Found && !Found->IsEmpty()) ? FName(**Found) : Fallback;
	}

	void ShowHudMessage(const UObject* WorldContextObject, const FString& Text)
	{
		if (AGBHUD* Hud = AGBHUD::Get(WorldContextObject))
		{
			Hud->ShowMessage(Text);
		}
	}
}

AGBPickup::AGBPickup()
{
}

AGBPickup* AGBPickup::SpawnPickup(UWorld* World, const FGBDropDefinition& InDrop, const FVector& Location, FName InFlagOnPickup)
{
	if (!World)
	{
		return nullptr;
	}
	if (!IsDropValid(World, InDrop))
	{
		UE_LOG(LogGB, Warning, TEXT("SpawnPickup: empty or unknown drop (item %s, weapon %s, spell group %s)."),
			*InDrop.ItemId.ToString(), *InDrop.WeaponId.ToString(), *InDrop.SpellGroup.ToString());
		return nullptr;
	}

	// Snap to the ground below (or slightly above) the requested point.
	FVector SpawnLocation = Location;
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBPickupGround), false);
	if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, GBPickupLocal::GroundTraceUp), Location - FVector(0.f, 0.f, GBPickupLocal::GroundTraceDown),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		SpawnLocation = Hit.ImpactPoint;
	}

	const FTransform Transform(FRotator::ZeroRotator, SpawnLocation);
	AGBPickup* Pickup = World->SpawnActorDeferred<AGBPickup>(AGBPickup::StaticClass(), Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		return nullptr;
	}
	Pickup->InitPickup(InDrop, InFlagOnPickup);
	Pickup->FinishSpawning(Transform);
	return Pickup;
}

int32 AGBPickup::SpawnDrops(UWorld* World, TConstArrayView<FGBDropDefinition> Drops, const FVector& Location)
{
	int32 Spawned = 0;
	for (const FGBDropDefinition& Drop : Drops)
	{
		if (Drop.Chance < 1.f && FMath::FRand() > Drop.Chance)
		{
			continue;
		}
		const FVector2D Offset = FMath::RandPointInCircle(GBPickupLocal::ScatterRadius);
		if (SpawnPickup(World, Drop, Location + FVector(Offset.X, Offset.Y, 0.f)))
		{
			++Spawned;
		}
	}
	return Spawned;
}

bool AGBPickup::IsDropValid(const UObject* WorldContextObject, const FGBDropDefinition& InDrop)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(WorldContextObject);
	if (!Data)
	{
		return false;
	}
	if (!InDrop.ItemId.IsNone())
	{
		return InDrop.Count > 0 && Data->FindItem(InDrop.ItemId) != nullptr;
	}
	if (!InDrop.WeaponId.IsNone())
	{
		return Data->FindWeapon(InDrop.WeaponId) != nullptr;
	}
	if (!InDrop.SpellGroup.IsNone())
	{
		for (const FGBSpellDefinition& Spell : Data->GetSpells())
		{
			if (Spell.UnlockGroup == InDrop.SpellGroup)
			{
				return true;
			}
		}
	}
	return false;
}

FString AGBPickup::GetDropDisplayName(const UObject* WorldContextObject, const FGBDropDefinition& InDrop, bool bWithCount)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(WorldContextObject);
	if (!InDrop.ItemId.IsNone())
	{
		const FGBItemDefinition* Def = Data ? Data->FindItem(InDrop.ItemId) : nullptr;
		const FString Name = (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : InDrop.ItemId.ToString();
		return (bWithCount && InDrop.Count > 1) ? FString::Printf(TEXT("%dx %s"), InDrop.Count, *Name) : Name;
	}
	if (!InDrop.WeaponId.IsNone())
	{
		const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(InDrop.WeaponId) : nullptr;
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : InDrop.WeaponId.ToString();
	}
	if (!InDrop.SpellGroup.IsNone())
	{
		return FString::Printf(TEXT("%s spellbook"), *InDrop.SpellGroup.ToString());
	}
	return TEXT("nothing");
}

bool AGBPickup::GiveDrop(AGBPlayerCharacter* Player, FGBDropDefinition& InOutDrop, bool& bOutAllTaken)
{
	bOutAllTaken = false;
	if (!Player)
	{
		return false;
	}

	if (!InOutDrop.ItemId.IsNone())
	{
		UGBInventoryComponent* Inventory = Player->GetInventory();
		if (!Inventory)
		{
			return false;
		}
		const int32 Wanted = FMath::Max(1, InOutDrop.Count);
		const int32 Taken = Inventory->AddItem(InOutDrop.ItemId, Wanted);
		if (Taken <= 0)
		{
			return false;
		}
		InOutDrop.Count = Wanted - Taken;
		bOutAllTaken = InOutDrop.Count <= 0;
		return true;
	}

	if (!InOutDrop.WeaponId.IsNone())
	{
		UGBEquipmentComponent* Equipment = Player->GetEquipment();
		if (Equipment && Equipment->GiveWeapon(InOutDrop.WeaponId, true, true))
		{
			bOutAllTaken = true;
			return true;
		}
		return false;
	}

	if (!InOutDrop.SpellGroup.IsNone())
	{
		UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
		if (!Spellbook)
		{
			return false;
		}
		// Knowledge already owned is still "taken": the book is used up either way.
		Spellbook->LearnGroup(InOutDrop.SpellGroup);
		bOutAllTaken = true;
		return true;
	}

	return false;
}

void AGBPickup::InitPickup(const FGBDropDefinition& InDrop, FName InFlagOnPickup)
{
	Drop = InDrop;
	FlagOnPickup = InFlagOnPickup;
	if (HasActorBegunPlay())
	{
		ApplyLook();
	}
}

void AGBPickup::BeginPlay()
{
	Super::BeginPlay();
	InteractVolume->OnComponentBeginOverlap.AddDynamic(this, &AGBPickup::HandleOverlap);
	ApplyLook();
}

bool AGBPickup::IsWalkOver() const
{
	return !Drop.ItemId.IsNone() && Drop.WeaponId.IsNone() && Drop.SpellGroup.IsNone();
}

void AGBPickup::ApplyLook()
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);

	if (!Drop.ItemId.IsNone())
	{
		const FGBItemDefinition* Def = Data ? Data->FindItem(Drop.ItemId) : nullptr;
		const FName SpriteId = Def ? GBPickupLocal::SpriteFromStrings(Def->Strings, NAME_None) : NAME_None;
		const bool bForce3D = Def && Def->Strings.FindRef(GBPickupString::PickupLook).Equals(TEXT("3D"), ESearchCase::IgnoreCase);
		if (!SpriteId.IsNone() && !bForce3D)
		{
			// Medieval / magic items (potions, gems) are pixel art.
			SetPlaceholderLook(true, SpriteId, FLinearColor::White, GBPickupLocal::ItemSpriteSize);
		}
		else
		{
			// Cyberpunk items (grenade, injection, ammo) are 3D.
			const FLinearColor Fallback = Def ? GBPlaceholder::ElementColor(static_cast<uint8>(Def->Element)) : GBPickupLocal::DefaultItemColor;
			const FLinearColor Color = Def ? UGBDataSubsystem::ParseColor(Def->Strings.FindRef(GBString::Color), Fallback) : Fallback;
			SetPlaceholderLook(false, NAME_None, Color, GBPickupLocal::ItemBoxSize);
		}
		return;
	}

	if (!Drop.WeaponId.IsNone())
	{
		const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(Drop.WeaponId) : nullptr;
		if (Def && Def->Slot != EGBHeldSlot::Gun)
		{
			SetPlaceholderLook(true, GBPickupLocal::SpriteFromStrings(Def->Strings, GBPickupLocal::StaffSpriteId), FLinearColor::White, GBPickupLocal::StaffSpriteSize);
		}
		else
		{
			const FLinearColor Color = Def ? UGBDataSubsystem::ParseColor(Def->Strings.FindRef(GBString::Color), GBPickupLocal::DefaultGunColor) : GBPickupLocal::DefaultGunColor;
			SetPlaceholderLook(false, NAME_None, Color, GBPickupLocal::GunBoxSize);
		}
		return;
	}

	if (!Drop.SpellGroup.IsNone())
	{
		SetPlaceholderLook(true, GBPickupLocal::BookSpriteId, FLinearColor::White, GBPickupLocal::BookSpriteSize);
	}
}

bool AGBPickup::CanInteract(const AGBPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && !IsActorBeingDestroyed() && Player && !Player->IsDead();
}

void AGBPickup::Interact(AGBPlayerCharacter* Player)
{
	if (CanInteract(Player))
	{
		TryPickUp(Player);
	}
}

FString AGBPickup::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return FString::Printf(TEXT("Pick up %s"), *GetDropDisplayName(this, Drop, true));
}

void AGBPickup::HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (!IsWalkOver())
	{
		return;
	}
	// Only the capsule counts, so a player with several colliding components triggers one pickup.
	AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(OtherActor);
	if (Player && OtherComp == Player->GetRootComponent() && CanInteract(Player))
	{
		TryPickUp(Player);
	}
}

bool AGBPickup::GiveTo(AGBPlayerCharacter* Player)
{
	bool bAllTaken = false;
	return GiveDrop(Player, Drop, bAllTaken);
}

void AGBPickup::TryPickUp(AGBPlayerCharacter* Player)
{
	if (!Player)
	{
		return;
	}

	const FGBDropDefinition Before = Drop;
	bool bAllTaken = false;
	if (!GiveDrop(Player, Drop, bAllTaken))
	{
		const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
		const FGBItemDefinition* Item = (Data && !Drop.ItemId.IsNone()) ? Data->FindItem(Drop.ItemId) : nullptr;
		const UGBEquipmentComponent* Equipment = Player->GetEquipment();
		if (Item && Item->Type == EGBItemType::Ammo && Equipment && !Equipment->HasWeaponInSlot(EGBHeldSlot::Gun))
		{
			GBPickupLocal::ShowHudMessage(this, FString::Printf(TEXT("No gun for %s"), *GetDropDisplayName(this, Before)));
		}
		else
		{
			GBPickupLocal::ShowHudMessage(this, FString::Printf(TEXT("Cannot carry more %s"), *GetDropDisplayName(this, Before)));
		}
		return;
	}

	if (!FlagOnPickup.IsNone())
	{
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
		{
			Progress->SetFlag(FlagOnPickup);
		}
	}

	FGBDropDefinition TakenPart = Before;
	if (!Before.ItemId.IsNone())
	{
		TakenPart.Count = FMath::Max(1, Before.Count) - FMath::Max(0, Drop.Count);
	}
	GBPickupLocal::ShowHudMessage(this, FString::Printf(TEXT("Picked up %s"), *GetDropDisplayName(this, TakenPart, true)));

	if (bAllTaken)
	{
		Destroy();
	}
}
