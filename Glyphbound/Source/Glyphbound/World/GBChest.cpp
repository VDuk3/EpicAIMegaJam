#include "World/GBChest.h"

#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInterface.h"
#include "Core/GBPlaceholder.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "Weapons/GBEquipmentComponent.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

namespace GBChestLocal
{
	// Look (cm), visuals only. The chest is long along X and faces +Y: the lid hinges on its long back edge (-Y).
	// Owner, 2026-11-15: it opened on a short end, and it was small; now 1.3 times the first size.
	const FVector BodySize(140.f, 90.f, 76.f);
	const FVector LidSize(146.f, 96.f, 18.f);
	const FVector LockSize(22.f, 4.f, 22.f);
	const FVector LidClosedLocation(0.f, 0.f, 85.f);
	const FVector LidOpenLocation(0.f, -44.f, 125.f);
	const FRotator LidOpenRotation(0.f, 0.f, -75.f);   // roll: the front (+Y) edge goes up
	const FLinearColor BodyColor(0.32f, 0.18f, 0.07f);
	const FLinearColor LidColor(0.55f, 0.4f, 0.12f);
	const FLinearColor OpenBodyColor(0.16f, 0.09f, 0.04f);
	const FLinearColor OpenLidColor(0.3f, 0.22f, 0.08f);
	// Where things the player could not carry are left: in front of the chest, spread sideways.
	constexpr float LeftoverForward = 140.f;
	constexpr float LeftoverSpacing = 60.f;
	constexpr float ChestInteractRadius = 150.f;

	/**
	 * A chest can wear a mesh from the map instead of the pixel chest (owner, 2026-11-15: a small sci-fi crate at the
	 * future base's start that holds the katana). Set on the placed actor's Tags: "ChestMesh=<static mesh path>" and
	 * optionally "ChestScale=<uniform scale>". The mesh stands on the actor's origin; no lid, no lock.
	 */
	FString TagValue(const AActor* Actor, const TCHAR* Key)
	{
		const FString Prefix = FString(Key) + TEXT("=");
		for (const FName& Tag : Actor->Tags)
		{
			const FString Text = Tag.ToString();
			if (Text.StartsWith(Prefix))
			{
				return Text.Mid(Prefix.Len());
			}
		}
		return FString();
	}
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

	// Pixel-art look (owner, 2026-11-15): when the two chest materials exist (Tools/Jam/castle_chest.py makes them),
	// they replace the plain tinted boxes. A cube's faces carry 0..1 UVs, so each face shows the picture once.
	UMaterialInterface* BodyArt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Glyphbound/Castle/Pixel/MI_Px_chest_body.MI_Px_chest_body"), FStringView(), LOAD_NoWarn | LOAD_Quiet);
	UMaterialInterface* LidArt = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Glyphbound/Castle/Pixel/MI_Px_chest_lid.MI_Px_chest_lid"), FStringView(), LOAD_NoWarn | LOAD_Quiet);
	if (BodyArt && LidArt)
	{
		Mesh->SetMaterial(0, BodyArt);
		Lid->SetMaterial(0, LidArt);
	}

	// Crate look: the map's mesh replaces the body, and there is no lid or lock.
	const FString CratePath = GBChestLocal::TagValue(this, TEXT("ChestMesh"));
	UStaticMesh* CrateMesh = CratePath.IsEmpty() ? nullptr : LoadObject<UStaticMesh>(nullptr, *CratePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (!CratePath.IsEmpty() && !CrateMesh)
	{
		UE_LOG(LogGB, Warning, TEXT("%s: ChestMesh '%s' not found; showing the plain chest."), *GetName(), *CratePath);
	}
	if (CrateMesh)
	{
		const FString ScaleText = GBChestLocal::TagValue(this, TEXT("ChestScale"));
		const float Scale = ScaleText.IsEmpty() ? 1.f : FMath::Max(0.01f, FCString::Atof(*ScaleText));
		Mesh->SetStaticMesh(CrateMesh);
		Mesh->EmptyOverrideMaterials();
		Mesh->SetRelativeScale3D(FVector(Scale));
		const FBox Bounds = CrateMesh->GetBoundingBox();
		Mesh->SetRelativeLocation(FVector(0.f, 0.f, -Bounds.Min.Z * Scale));
		Mesh->SetVisibility(true);
		Mesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		Mesh->SetCastShadow(true);
		Lid->SetVisibility(false);
		InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, Bounds.GetSize().Z * Scale * 0.5f));
	}

	// The lock: a brass plate on the front face (+Y), under the lid's edge. On the body, not on the lid (owner).
	if (UMaterialInterface* LockArt = CrateMesh ? nullptr : LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Glyphbound/Castle/Pixel/MI_Px_chest_lock.MI_Px_chest_lock"), FStringView(), LOAD_NoWarn | LOAD_Quiet))
	{
		UStaticMeshComponent* Lock = NewObject<UStaticMeshComponent>(this, TEXT("Lock"));
		Lock->SetupAttachment(Root);
		Lock->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Lock->SetGenerateOverlapEvents(false);
		Lock->RegisterComponent();
		GBPlaceholder::SetupShape(Lock, EGBShape::Cube, GBChestLocal::LockSize, GBChestLocal::LidColor, false);
		Lock->SetRelativeLocation(FVector(0.f, GBChestLocal::BodySize.Y * 0.5f + GBChestLocal::LockSize.Y * 0.5f - 1.f, GBChestLocal::BodySize.Z - GBChestLocal::LockSize.Z * 0.5f - 6.f));
		Lock->SetMaterial(0, LockArt);
	}

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
	if (GBChestLocal::TagValue(this, TEXT("ChestMesh")).IsEmpty())
	{
		Lid->SetRelativeLocationAndRotation(bInOpen ? GBChestLocal::LidOpenLocation : GBChestLocal::LidClosedLocation, bInOpen ? GBChestLocal::LidOpenRotation : FRotator::ZeroRotator);
		GBPlaceholder::SetShapeColor(Mesh, bInOpen ? GBChestLocal::OpenBodyColor : GBChestLocal::BodyColor);
		GBPlaceholder::SetShapeColor(Lid, bInOpen ? GBChestLocal::OpenLidColor : GBChestLocal::LidColor);
	}
	InteractVolume->SetCollisionEnabled(bInOpen ? ECollisionEnabled::NoCollision : ECollisionEnabled::QueryOnly);
}

bool AGBChest::CanInteract(const AGBPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && !bOpen && Player && !Player->IsDead();
}

FString AGBChest::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return GBChestLocal::TagValue(this, TEXT("ChestMesh")).IsEmpty() ? TEXT("Open chest") : TEXT("Open crate");
}

void AGBChest::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	{
		const FVector SoundAt = GetActorLocation();
		GBAudio::Play(this, FName(TEXT("chest_open")), &SoundAt, 1.f, 1.f);
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
			const FVector Location = GetActorLocation() + GetActorRightVector() * GBChestLocal::LeftoverForward + GetActorForwardVector() * Side;
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
