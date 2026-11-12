// Menus of AGBHUD: pause, inventory (items, weapons, spells, skills), skill trees, death screen.
// Keyboard, gamepad and mouse share one cursor: every clickable drawn in a frame is recorded in MenuHits.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "Player/GBPlayerCharacter.h"
#include "Player/GBPlayerController.h"
#include "Spells/GBSpellbookComponent.h"
#include "Items/GBInventoryComponent.h"
#include "Progression/GBSkillTreeComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBGameMode.h"
#include "Core/GBStatNames.h"

#include "Engine/Canvas.h"
#include "Kismet/KismetSystemLibrary.h"

namespace GBHudMenuLocal
{
	const FLinearColor MenuWhite(1.f, 1.f, 1.f, 1.f);
	const FLinearColor MenuDim(0.f, 0.f, 0.f, 0.6f);
	const FLinearColor MenuGrey(0.55f, 0.55f, 0.55f, 1.f);

	int32 Wrap(int32 Value, int32 Num)
	{
		return Num > 0 ? ((Value % Num) + Num) % Num : 0;
	}

	const TCHAR* SlotKey(int32 Slot)
	{
		static const TCHAR* Keys[] = { TEXT("RMB"), TEXT("1"), TEXT("2"), TEXT("3"), TEXT("4") };
		return (Slot >= 0 && Slot < (int32)UE_ARRAY_COUNT(Keys)) ? Keys[Slot] : TEXT("?");
	}

	FString ItemTypeName(EGBItemType Type)
	{
		switch (Type)
		{
		case EGBItemType::Consumable: return TEXT("Consumable");
		case EGBItemType::Throwable: return TEXT("Throwable");
		case EGBItemType::Armor: return TEXT("Armor");
		case EGBItemType::KeyItem: return TEXT("Key item");
		case EGBItemType::Material: return TEXT("Material");
		case EGBItemType::Ammo: return TEXT("Ammo");
		}
		return FString();
	}

	FString ItemName(const UGBDataSubsystem* Data, FName ItemId)
	{
		const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : ItemId.ToString();
	}

	FString SkillName(const UGBDataSubsystem* Data, FName NodeId)
	{
		const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(NodeId) : nullptr;
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : NodeId.ToString();
	}

	FString SkillResultText(const UGBDataSubsystem* Data, FName NodeId, EGBSkillUnlockResult Result)
	{
		switch (Result)
		{
		case EGBSkillUnlockResult::Ok: return TEXT("Can be unlocked");
		case EGBSkillUnlockResult::MaxRank: return TEXT("Max rank reached");
		case EGBSkillUnlockResult::NotEnoughPoints: return TEXT("Not enough skill points");
		case EGBSkillUnlockResult::UnknownNode: return TEXT("Unknown skill");
		case EGBSkillUnlockResult::MissingRequirement:
		{
			FString Names;
			if (const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(NodeId) : nullptr)
			{
				for (const FName& Req : Def->Requires)
				{
					Names += (Names.IsEmpty() ? TEXT("") : TEXT(", ")) + SkillName(Data, Req);
				}
			}
			return Names.IsEmpty() ? FString(TEXT("Unlock the required skills first")) : FString::Printf(TEXT("Requires: %s"), *Names);
		}
		}
		return FString();
	}

	bool IsWeaponTree(FName Tree)
	{
		// FName comparison ignores case.
		return Tree == FName(TEXT("Weapon")) || Tree == FName(TEXT("Weapons"));
	}

	FString WeaponName(const UGBDataSubsystem* Data, FName WeaponId)
	{
		const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
		return (Def && !Def->DisplayName.IsEmpty()) ? Def->DisplayName : WeaponId.ToString();
	}

	FString SlotName(EGBHeldSlot Slot, bool bPlural)
	{
		switch (Slot)
		{
		case EGBHeldSlot::Staff: return bPlural ? TEXT("Staffs") : TEXT("Staff");
		case EGBHeldSlot::Gun: return bPlural ? TEXT("Guns") : TEXT("Gun");
		case EGBHeldSlot::Sword: return TEXT("Sword");
		}
		return FString();
	}

	/** "Damage 10 x8   Fire interval 0.85 s   Magazine 6" from the base data. */
	TArray<FString> WeaponStatLines(const FGBWeaponDefinition& Def)
	{
		TArray<FString> Lines;
		const float Damage = Def.Stats.FindRef(GBStat::Damage);
		const int32 Pellets = FMath::RoundToInt(Def.Stats.FindRef(GBStat::Pellets));
		if (Damage > 0.f)
		{
			Lines.Add(Pellets > 1 ? FString::Printf(TEXT("Damage %d x%d"), FMath::RoundToInt(Damage), Pellets)
				: FString::Printf(TEXT("Damage %d"), FMath::RoundToInt(Damage)));
		}
		const float Interval = Def.Stats.FindRef(GBStat::FireInterval);
		if (Interval > 0.f)
		{
			Lines.Add(FString::Printf(TEXT("Fire interval %.2f s"), Interval));
		}
		const int32 Magazine = FMath::RoundToInt(Def.Stats.FindRef(GBStat::MagazineSize));
		if (Def.Slot == EGBHeldSlot::Gun && Magazine > 0)
		{
			Lines.Add(FString::Printf(TEXT("Magazine %d"), Magazine));
		}
		return Lines;
	}
}

// ======================================================================== state and input

AGBPlayerController* AGBHUD::GetGBController() const
{
	return Cast<AGBPlayerController>(GetOwningPlayerController());
}

EGBMenu AGBHUD::GetOpenMenu() const
{
	const AGBPlayerController* PC = GetGBController();
	return PC ? PC->GetOpenMenu() : EGBMenu::None;
}

void AGBHUD::OnMenuOpened(EGBMenu Menu)
{
	MenuState = FMenuState();
	ResetMenuMouse();
	if (Menu == EGBMenu::SkillTrees)
	{
		const TArray<FSkillNodeRef> Nodes = GetSkillNodes();
		MenuState.SkillCursor = Nodes.Num() > 0 ? Nodes[0].Id : NAME_None;
	}
}

void AGBHUD::OnMenuClosed(EGBMenu Menu)
{
	MenuState = FMenuState();
	ResetMenuMouse();
}

void AGBHUD::SetInventoryPage(int32 NewPage)
{
	const FName KeepSkill = MenuState.SkillCursor;
	MenuState = FMenuState();
	MenuState.Page = GBHudMenuLocal::Wrap(NewPage, PageCount);
	MenuState.SkillCursor = KeepSkill;
}

// ======================================================================== mouse

void AGBHUD::ResetMenuMouse()
{
	MenuHits.Reset();
	MenuHitsMenu = EGBMenu::None;
	MenuHitsPage = -1;
	bMenuHitsChoosing = false;
	// The first position read after opening is only a baseline: an idle mouse must not steal the keyboard cursor.
	bHasMenuMouse = false;
}

void AGBHUD::AddMenuHit(EMenuHitKind Kind, float X, float Y, float W, float H, int32 Index, FName Id)
{
	FMenuHit Hit;
	Hit.Kind = Kind;
	Hit.Min = FVector2D(X, Y);
	Hit.Max = FVector2D(X + FMath::Max(0.f, W), Y + FMath::Max(0.f, H));
	Hit.Index = Index;
	Hit.Id = Id;
	MenuHits.Add(Hit);
}

const AGBHUD::FMenuHit* AGBHUD::FindMenuHit(const FVector2D& CanvasPosition) const
{
	// Last drawn is on top.
	for (int32 i = MenuHits.Num() - 1; i >= 0; --i)
	{
		const FMenuHit& Hit = MenuHits[i];
		if (CanvasPosition.X >= Hit.Min.X && CanvasPosition.X <= Hit.Max.X && CanvasPosition.Y >= Hit.Min.Y && CanvasPosition.Y <= Hit.Max.Y)
		{
			return &Hit;
		}
	}
	return nullptr;
}

void AGBHUD::UpdateMenuMouse()
{
	APlayerController* PC = GetOwningPlayerController();
	float MouseX = 0.f;
	float MouseY = 0.f;
	if (!PC || !PC->GetMousePosition(MouseX, MouseY))
	{
		return;
	}
	const FVector2D Mouse(MouseX * MouseToCanvas, MouseY * MouseToCanvas);
	const bool bMoved = bHasMenuMouse && !Mouse.Equals(LastMenuMouse, 0.5f);
	LastMenuMouse = Mouse;
	bHasMenuMouse = true;
	// Hover uses last frame's clickables, so the highlight moves in the same frame it is drawn.
	if (bMoved && MenuHitsMenu == GetOpenMenu())
	{
		if (const FMenuHit* Hit = FindMenuHit(Mouse))
		{
			ApplyMenuHover(*Hit);
		}
	}
}

void AGBHUD::ApplyMenuHover(const FMenuHit& Hit)
{
	switch (Hit.Kind)
	{
	case EMenuHitKind::PauseEntry:
	case EMenuHitKind::ItemRow:
	case EMenuHitKind::WeaponRow:
		MenuState.Cursor = Hit.Index;
		break;
	case EMenuHitKind::SpellSlot:
		if (!MenuState.bChoosingSpell)
		{
			MenuState.Cursor = Hit.Index;
		}
		break;
	case EMenuHitKind::SpellChoice:
		MenuState.SubCursor = Hit.Index;
		break;
	case EMenuHitKind::SkillNode:
		MenuState.SkillCursor = Hit.Id;
		break;
	default:
		// Tabs and the death screen only react to clicks.
		break;
	}
}

void AGBHUD::ActivateMenuHit(const FMenuHit& Hit)
{
	AGBPlayerCharacter* Player = GetPlayer();
	switch (Hit.Kind)
	{
	case EMenuHitKind::PauseEntry:
		MenuState.Cursor = Hit.Index;
		MenuConfirm();
		break;
	case EMenuHitKind::Tab:
		if (Hit.Index != MenuState.Page)
		{
			SetInventoryPage(Hit.Index);
		}
		break;
	case EMenuHitKind::ItemRow:
		MenuState.Cursor = Hit.Index;
		ConfirmItem(Player);
		break;
	case EMenuHitKind::WeaponRow:
		MenuState.Cursor = Hit.Index;
		ConfirmWeapon(Player);
		break;
	case EMenuHitKind::SpellSlot:
		// A click on another slot while choosing leaves the chooser and opens it for that slot.
		MenuState.bChoosingSpell = false;
		MenuState.Cursor = Hit.Index;
		ConfirmSpellSlot(Player);
		break;
	case EMenuHitKind::SpellChoice:
		MenuState.SubCursor = Hit.Index;
		ConfirmSpellSlot(Player);
		break;
	case EMenuHitKind::SkillNode:
		MenuState.SkillCursor = Hit.Id;
		ConfirmSkill(Player);
		break;
	case EMenuHitKind::DeathScreen:
		// The controller handles the respawn confirm (AGBPlayerController::InputMenuClick): nothing here,
		// so a click respawns exactly once.
		break;
	}
}

bool AGBHUD::MenuClick(const FVector2D& ScreenPosition)
{
	const EGBMenu Menu = GetOpenMenu();
	// Only the clickables of the screen that is showing right now (not of a page left this frame).
	if (Menu == EGBMenu::None || Menu != MenuHitsMenu || MenuHitsPage != MenuState.Page || bMenuHitsChoosing != MenuState.bChoosingSpell)
	{
		return false;
	}
	const FVector2D Position = ScreenPosition * MouseToCanvas;
	const FMenuHit* Found = FindMenuHit(Position);
	if (!Found)
	{
		return false;
	}
	// Copy: the action may change the menu, and the list is rebuilt on the next draw.
	const FMenuHit Hit = *Found;
	ActivateMenuHit(Hit);
	return true;
}

void AGBHUD::MenuNavigate(FIntPoint Direction)
{
	using namespace GBHudMenuLocal;
	AGBPlayerCharacter* Player = GetPlayer();
	switch (GetOpenMenu())
	{
	case EGBMenu::Pause:
		if (Direction.Y != 0)
		{
			MenuState.Cursor = Wrap(MenuState.Cursor + Direction.Y, 2);
		}
		break;
	case EGBMenu::SkillTrees:
		NavigateSkills(Direction);
		break;
	case EGBMenu::Inventory:
		if (MenuState.Page == PageSkills)
		{
			NavigateSkills(Direction);
		}
		else if (MenuState.Page == PageSpells)
		{
			if (Direction.Y == 0)
			{
				break;
			}
			if (MenuState.bChoosingSpell)
			{
				MenuState.SubCursor = Wrap(MenuState.SubCursor + Direction.Y, GetAssignableSpells(Player).Num());
			}
			else
			{
				MenuState.Cursor = Wrap(MenuState.Cursor + Direction.Y, UGBSpellbookComponent::NumSlots + 1);
			}
		}
		else if (MenuState.Page == PageWeapons)
		{
			if (Direction.Y != 0)
			{
				MenuState.Cursor = Wrap(MenuState.Cursor + Direction.Y, GetWeaponList(Player).Num());
			}
		}
		else if (Direction.Y != 0)
		{
			MenuState.Cursor = Wrap(MenuState.Cursor + Direction.Y, GetInventoryList(Player).Num());
		}
		break;
	default:
		break;
	}
}

void AGBHUD::MenuConfirm()
{
	AGBPlayerCharacter* Player = GetPlayer();
	switch (GetOpenMenu())
	{
	case EGBMenu::Pause:
		if (MenuState.Cursor == 0)
		{
			if (AGBPlayerController* PC = GetGBController())
			{
				PC->SetMenu(EGBMenu::None);
			}
		}
		else
		{
			UKismetSystemLibrary::QuitGame(this, GetOwningPlayerController(), EQuitPreference::Quit, false);
		}
		break;
	case EGBMenu::SkillTrees:
		ConfirmSkill(Player);
		break;
	case EGBMenu::Inventory:
		if (MenuState.Page == PageItems)
		{
			ConfirmItem(Player);
		}
		else if (MenuState.Page == PageWeapons)
		{
			ConfirmWeapon(Player);
		}
		else if (MenuState.Page == PageSpells)
		{
			ConfirmSpellSlot(Player);
		}
		else
		{
			ConfirmSkill(Player);
		}
		break;
	default:
		// Death: the controller handles the respawn confirm.
		break;
	}
}

bool AGBHUD::MenuBack()
{
	switch (GetOpenMenu())
	{
	case EGBMenu::Inventory:
		if (MenuState.Page == PageItems && MenuState.PendingCombine != NAME_None)
		{
			MenuState.PendingCombine = NAME_None;
			return true;
		}
		if (MenuState.Page == PageSpells && MenuState.bChoosingSpell)
		{
			MenuState.bChoosingSpell = false;
			return true;
		}
		return false;
	case EGBMenu::Death:
		// The death screen only closes by respawning.
		return true;
	default:
		return false;
	}
}

void AGBHUD::MenuTab(int32 Direction)
{
	if (Direction == 0)
	{
		return;
	}
	const EGBMenu Menu = GetOpenMenu();
	if (Menu == EGBMenu::Inventory)
	{
		SetInventoryPage(MenuState.Page + Direction);
	}
	else if (Menu == EGBMenu::SkillTrees)
	{
		// Jump to the first node of the other tree.
		const TArray<FSkillNodeRef> Nodes = GetSkillNodes();
		const FSkillNodeRef* Current = Nodes.FindByPredicate([this](const FSkillNodeRef& Node) { return Node.Id == MenuState.SkillCursor; });
		const int32 OtherTree = Current ? 1 - Current->Tree : 0;
		if (const FSkillNodeRef* Other = Nodes.FindByPredicate([OtherTree](const FSkillNodeRef& Node) { return Node.Tree == OtherTree; }))
		{
			MenuState.SkillCursor = Other->Id;
		}
	}
}

// ======================================================================== lists

TArray<FName> AGBHUD::GetInventoryList(AGBPlayerCharacter* Player) const
{
	TArray<FName> Result;
	const UGBInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	if (!Inventory)
	{
		return Result;
	}
	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		for (const FGBItemDefinition& Def : Data->GetItems())
		{
			if (Inventory->GetCount(Def.Id) > 0)
			{
				Result.Add(Def.Id);
			}
		}
	}
	// Items no longer in the data (after a reload) are still listed, at the end.
	for (const TPair<FName, int32>& Entry : Inventory->GetItems())
	{
		if (Entry.Value > 0 && !Result.Contains(Entry.Key))
		{
			Result.Add(Entry.Key);
		}
	}
	return Result;
}

TArray<FName> AGBHUD::GetWeaponList(AGBPlayerCharacter* Player) const
{
	TArray<FName> Result;
	const UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
	if (!Equipment)
	{
		return Result;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	static const EGBHeldSlot Order[] = { EGBHeldSlot::Staff, EGBHeldSlot::Gun, EGBHeldSlot::Sword };
	for (const EGBHeldSlot Slot : Order)
	{
		for (const FName& WeaponId : Equipment->GetOwnedWeapons())
		{
			const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr;
			if (Def && Def->Slot == Slot)
			{
				Result.AddUnique(WeaponId);
			}
		}
	}
	// Weapons no longer in the data (after a reload) are still listed, at the end.
	for (const FName& WeaponId : Equipment->GetOwnedWeapons())
	{
		if (WeaponId != NAME_None)
		{
			Result.AddUnique(WeaponId);
		}
	}
	return Result;
}

TArray<FName> AGBHUD::GetAssignableSpells(AGBPlayerCharacter* Player) const
{
	TArray<FName> Result;
	Result.Add(NAME_None);
	const UGBSpellbookComponent* Spellbook = Player ? Player->GetSpellbook() : nullptr;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Spellbook || !Data)
	{
		return Result;
	}
	for (const FName& SpellId : Spellbook->GetKnownSpells())
	{
		const FGBSpellDefinition* Def = Data->FindSpell(SpellId);
		if (Def && !Def->bPassive && !Def->bRequiresGun && SpellId != Spellbook->GetEnchantSpell())
		{
			Result.Add(SpellId);
		}
	}
	return Result;
}

TArray<AGBHUD::FSkillNodeRef> AGBHUD::GetSkillNodes() const
{
	TArray<FSkillNodeRef> Result;
	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		for (const FGBSkillNodeDefinition& Def : Data->GetSkillNodes())
		{
			FSkillNodeRef Ref;
			Ref.Id = Def.Id;
			Ref.Tree = GBHudMenuLocal::IsWeaponTree(Def.Tree) ? 1 : 0;
			Ref.Column = FMath::Max(0, Def.Column);
			Ref.Row = FMath::Max(0, Def.Row);
			Result.Add(Ref);
		}
	}
	return Result;
}

void AGBHUD::NavigateSkills(FIntPoint Direction)
{
	const TArray<FSkillNodeRef> Nodes = GetSkillNodes();
	if (Nodes.Num() == 0 || Direction == FIntPoint::ZeroValue)
	{
		return;
	}
	const FSkillNodeRef* Current = Nodes.FindByPredicate([this](const FSkillNodeRef& Node) { return Node.Id == MenuState.SkillCursor; });
	if (!Current)
	{
		MenuState.SkillCursor = Nodes[0].Id;
		return;
	}

	// Both trees on one grid (the weapon tree to the right of the spell tree); pick the nearest node in the pressed direction.
	int32 MaxSpellColumn = 0;
	for (const FSkillNodeRef& Node : Nodes)
	{
		if (Node.Tree == 0)
		{
			MaxSpellColumn = FMath::Max(MaxSpellColumn, Node.Column);
		}
	}
	auto GridPos = [MaxSpellColumn](const FSkillNodeRef& Node)
	{
		return FVector2D(Node.Tree * (MaxSpellColumn + 2) + Node.Column, Node.Row);
	};
	const FVector2D Dir(Direction.X, Direction.Y);
	const FVector2D From = GridPos(*Current);
	float BestCost = TNumericLimits<float>::Max();
	FName Best = NAME_None;
	for (const FSkillNodeRef& Node : Nodes)
	{
		if (Node.Id == Current->Id)
		{
			continue;
		}
		const FVector2D Delta = GridPos(Node) - From;
		const float Along = FVector2D::DotProduct(Delta, Dir);
		if (Along <= 0.f)
		{
			continue;
		}
		const float Across = FMath::Abs(FVector2D::CrossProduct(Delta, Dir));
		const float Cost = Along + 2.f * Across;
		if (Cost < BestCost)
		{
			BestCost = Cost;
			Best = Node.Id;
		}
	}
	if (Best != NAME_None)
	{
		MenuState.SkillCursor = Best;
	}
}

// ======================================================================== confirm actions

void AGBHUD::ConfirmItem(AGBPlayerCharacter* Player)
{
	using namespace GBHudMenuLocal;
	UGBInventoryComponent* Inventory = Player ? Player->GetInventory() : nullptr;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> List = GetInventoryList(Player);
	if (!Inventory || List.Num() == 0)
	{
		return;
	}
	MenuState.Cursor = FMath::Clamp(MenuState.Cursor, 0, List.Num() - 1);
	const FName ItemId = List[MenuState.Cursor];
	const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
	if (!Def)
	{
		ShowMessage(TEXT("Unknown item"), 1.5f);
		return;
	}

	switch (Def->Type)
	{
	case EGBItemType::Consumable:
		if (!Inventory->UseItem(ItemId))
		{
			ShowMessage(TEXT("Cannot use that now"), 1.5f);
		}
		break;
	case EGBItemType::Throwable:
		ShowMessage(TEXT("Throw it with [F] (quick-use)"), 2.f);
		break;
	case EGBItemType::Armor:
		if (!Inventory->EquipArmor(Inventory->GetEquippedArmor() == ItemId ? NAME_None : ItemId))
		{
			ShowMessage(TEXT("Cannot equip that"), 1.5f);
		}
		break;
	case EGBItemType::Material:
	case EGBItemType::KeyItem:
		if (MenuState.PendingCombine == NAME_None)
		{
			MenuState.PendingCombine = ItemId;
		}
		else if (MenuState.PendingCombine == ItemId)
		{
			MenuState.PendingCombine = NAME_None;
		}
		else
		{
			const FName Other = MenuState.PendingCombine;
			MenuState.PendingCombine = NAME_None;
			const FGBRecipeDefinition* Recipe = Inventory->FindRecipe(Other, ItemId);
			// Copy what the toast needs before combining: the recipe pointer belongs to the data tables.
			FString ResultName;
			if (Recipe)
			{
				if (Recipe->ResultItem != NAME_None)
				{
					ResultName = ItemName(Data, Recipe->ResultItem);
				}
				else if (Recipe->ResultWeapon != NAME_None)
				{
					const FGBWeaponDefinition* Weapon = Data->FindWeapon(Recipe->ResultWeapon);
					ResultName = (Weapon && !Weapon->DisplayName.IsEmpty()) ? Weapon->DisplayName : Recipe->ResultWeapon.ToString();
				}
			}
			if (Recipe && Inventory->TryCombine(Other, ItemId))
			{
				ShowMessage(ResultName.IsEmpty() ? FString(TEXT("Combined")) : FString::Printf(TEXT("Created %s"), *ResultName), 2.5f);
			}
			else
			{
				ShowMessage(TEXT("These do not combine"), 1.5f);
			}
		}
		break;
	default:
		break;
	}
}

void AGBHUD::ConfirmWeapon(AGBPlayerCharacter* Player)
{
	UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
	const TArray<FName> List = GetWeaponList(Player);
	if (!Equipment || List.Num() == 0)
	{
		return;
	}
	MenuState.Cursor = FMath::Clamp(MenuState.Cursor, 0, List.Num() - 1);
	const FName WeaponId = List[MenuState.Cursor];
	const FString Name = GBHudMenuLocal::WeaponName(UGBDataSubsystem::Get(this), WeaponId);
	if (Equipment->IsWeaponEquipped(WeaponId))
	{
		ShowMessage(FString::Printf(TEXT("%s is already equipped"), *Name), 1.5f);
	}
	else if (Equipment->EquipOwnedWeapon(WeaponId))
	{
		ShowMessage(FString::Printf(TEXT("Equipped %s"), *Name), 2.f);
	}
	else
	{
		ShowMessage(TEXT("Cannot change weapons right now"), 1.5f);
	}
}

void AGBHUD::ConfirmSpellSlot(AGBPlayerCharacter* Player)
{
	UGBSpellbookComponent* Spellbook = Player ? Player->GetSpellbook() : nullptr;
	if (!Spellbook)
	{
		return;
	}
	const int32 NumSlots = UGBSpellbookComponent::NumSlots;
	if (MenuState.bChoosingSpell)
	{
		const TArray<FName> Options = GetAssignableSpells(Player);
		if (Options.IsValidIndex(MenuState.SubCursor) && MenuState.Cursor < NumSlots)
		{
			if (!Spellbook->EquipSpell(MenuState.Cursor, Options[MenuState.SubCursor]))
			{
				ShowMessage(TEXT("Cannot equip that spell"), 1.5f);
			}
		}
		MenuState.bChoosingSpell = false;
	}
	else if (MenuState.Cursor < NumSlots)
	{
		const TArray<FName> Options = GetAssignableSpells(Player);
		MenuState.bChoosingSpell = true;
		MenuState.SubCursor = FMath::Max(0, Options.IndexOfByKey(Spellbook->GetSlotSpell(MenuState.Cursor)));
	}
	else
	{
		// The enchant slot: cycle its element Fire -> Lightning -> Ice.
		EGBElement Next = EGBElement::Fire;
		switch (Spellbook->GetEnchantElement())
		{
		case EGBElement::Fire: Next = EGBElement::Lightning; break;
		case EGBElement::Lightning: Next = EGBElement::Ice; break;
		default: Next = EGBElement::Fire; break;
		}
		Spellbook->SetEnchantElement(Next);
	}
}

void AGBHUD::ConfirmSkill(AGBPlayerCharacter* Player)
{
	UGBSkillTreeComponent* SkillTree = Player ? Player->GetSkillTree() : nullptr;
	const FName NodeId = MenuState.SkillCursor;
	if (!SkillTree || NodeId == NAME_None)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const EGBSkillUnlockResult Result = SkillTree->Unlock(NodeId);
	if (Result == EGBSkillUnlockResult::Ok)
	{
		ShowMessage(FString::Printf(TEXT("Unlocked %s (rank %d)"), *GBHudMenuLocal::SkillName(Data, NodeId), SkillTree->GetRank(NodeId)), 2.f);
	}
	else
	{
		ShowMessage(GBHudMenuLocal::SkillResultText(Data, NodeId, Result), 2.f);
	}
}

// ======================================================================== drawing

float AGBHUD::DrawWrapped(const FString& Text, const FLinearColor& Color, float X, float Y, float MaxWidth, float SizePx, bool bShadow)
{
	TArray<FString> Words;
	Text.ParseIntoArrayWS(Words);
	const float LineH = FMath::Max(MeasureLabel(TEXT("Ag"), SizePx).Y, 1.f);
	float CurY = Y;
	FString Line;
	for (const FString& Word : Words)
	{
		const FString Candidate = Line.IsEmpty() ? Word : Line + TEXT(" ") + Word;
		if (!Line.IsEmpty() && MeasureLabel(Candidate, SizePx).X > MaxWidth)
		{
			DrawLabel(Line, Color, X, CurY, SizePx, 0.f, bShadow);
			CurY += LineH;
			Line = Word;
		}
		else
		{
			Line = Candidate;
		}
	}
	if (!Line.IsEmpty())
	{
		DrawLabel(Line, Color, X, CurY, SizePx, 0.f, bShadow);
		CurY += LineH;
	}
	return CurY - Y;
}

void AGBHUD::DrawMenus(AGBPlayerCharacter* Player)
{
	const FGBHudSkinStyle& Style = GetSkinStyle(GetCurrentSkin());
	// Clickables are rebuilt while drawing, for the screen as it is now.
	MenuHits.Reset();
	MenuHitsMenu = GetOpenMenu();
	MenuHitsPage = MenuState.Page;
	bMenuHitsChoosing = MenuState.bChoosingSpell;
	switch (GetOpenMenu())
	{
	case EGBMenu::Pause:
		DrawPauseMenu(Style);
		break;
	case EGBMenu::Death:
		DrawDeathScreen();
		break;
	case EGBMenu::Inventory:
		DrawInventory(Player, Style);
		break;
	case EGBMenu::SkillTrees:
	{
		const float S = UIScale();
		DrawRect(GBHudMenuLocal::MenuDim, 0.f, 0.f, Canvas->ClipX, Canvas->ClipY);
		DrawLabel(TEXT("SKILL TREES"), GBHudMenuLocal::MenuWhite, Canvas->ClipX * 0.5f, Canvas->ClipY * 0.04f, 34.f, 0.5f, true, true);
		if (Player)
		{
			DrawSkillTrees(Player, Canvas->ClipY * 0.1f);
		}
		DrawLabel(TEXT("Arrows or mouse: move   Click or Enter: unlock   Q / E: other tree   Esc: close"), GBHudMenuLocal::MenuWhite,
			Canvas->ClipX * 0.5f, Canvas->ClipY - 40.f * S, 16.f, 0.5f);
		break;
	}
	default:
		break;
	}
}

void AGBHUD::DrawPauseMenu(const FGBHudSkinStyle& Style)
{
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(GBHudMenuLocal::MenuDim, 0.f, 0.f, W, H);

	const float PW = 440.f * S;
	const float PH = 260.f * S;
	const float PX = (W - PW) * 0.5f;
	const float PY = (H - PH) * 0.5f;
	const bool bShadow = !Style.bChunky;
	DrawPanel(Style, PX, PY, PW, PH);
	DrawLabel(TEXT("PAUSED"), Style.Text, W * 0.5f, PY + 24.f * S, 34.f, 0.5f, bShadow, true);

	static const TCHAR* Entries[] = { TEXT("Resume"), TEXT("Quit to desktop") };
	for (int32 i = 0; i < 2; ++i)
	{
		const float Y = PY + 100.f * S + i * 54.f * S;
		const bool bSelected = MenuState.Cursor == i;
		if (bSelected)
		{
			DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, 0.35f), PX + 30.f * S, Y - 6.f * S, PW - 60.f * S, 42.f * S);
		}
		AddMenuHit(EMenuHitKind::PauseEntry, PX + 30.f * S, Y - 6.f * S, PW - 60.f * S, 42.f * S, i);
		DrawLabel(bSelected ? FString::Printf(TEXT("> %s <"), Entries[i]) : FString(Entries[i]), Style.Text, W * 0.5f, Y, 26.f, 0.5f, bShadow);
	}
	DrawLabel(TEXT("Click or Enter: select   Esc: resume"), Style.Text, W * 0.5f, PY + PH - 36.f * S, 15.f, 0.5f, bShadow);
}

void AGBHUD::DrawDeathScreen()
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	DrawRect(FLinearColor(0.25f, 0.f, 0.f, 0.65f), 0.f, 0.f, W, H);
	DrawLabel(TEXT("YOU DIED"), FLinearColor(1.f, 0.25f, 0.2f, 1.f), W * 0.5f, H * 0.38f, 72.f, 0.5f, true, true);
	DrawLabel(TEXT("Click, or press Enter / A, to respawn"), GBHudMenuLocal::MenuWhite, W * 0.5f, H * 0.52f, 26.f, 0.5f);
	AddMenuHit(EMenuHitKind::DeathScreen, 0.f, 0.f, W, H);
}

void AGBHUD::DrawInventory(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style)
{
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const bool bShadow = !Style.bChunky;
	DrawRect(GBHudMenuLocal::MenuDim, 0.f, 0.f, W, H);

	const float PX = W * 0.07f;
	const float PY = H * 0.06f;
	const float PW = W * 0.86f;
	const float PH = H * 0.88f;
	DrawPanel(Style, PX, PY, PW, PH);

	// Tabs.
	static const TCHAR* Tabs[] = { TEXT("Items"), TEXT("Weapons"), TEXT("Spells"), TEXT("Skills") };
	static_assert((int32)UE_ARRAY_COUNT(Tabs) == PageCount,"One tab name per inventory page");
	float TabX = PX + 24.f * S;
	for (int32 i = 0; i < PageCount; ++i)
	{
		const FVector2D Size = MeasureLabel(Tabs[i], 24.f);
		const bool bSelected = MenuState.Page == i;
		if (bSelected)
		{
			DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, 0.45f), TabX - 10.f * S, PY + 14.f * S, Size.X + 20.f * S, Size.Y + 8.f * S);
		}
		AddMenuHit(EMenuHitKind::Tab, TabX - 10.f * S, PY + 14.f * S, Size.X + 20.f * S, Size.Y + 8.f * S, i);
		DrawLabel(Tabs[i], Style.Text, TabX, PY + 18.f * S, 24.f, 0.f, bShadow);
		TabX += Size.X + 44.f * S;
	}
	DrawLabel(TEXT("Q / E or click a tab: switch page   Esc: close"), Style.Text, PX + PW - 24.f * S, PY + 22.f * S, 16.f, 1.f, bShadow);

	if (!Player)
	{
		return;
	}
	const float CX = PX + 24.f * S;
	const float CY = PY + 70.f * S;
	const float CW = PW - 48.f * S;
	const float CH = PH - 100.f * S;
	if (MenuState.Page == PageItems)
	{
		DrawItemsPage(Player, Style, CX, CY, CW, CH);
	}
	else if (MenuState.Page == PageWeapons)
	{
		DrawWeaponsPage(Player, Style, CX, CY, CW, CH);
	}
	else if (MenuState.Page == PageSpells)
	{
		DrawSpellsPage(Player, Style, CX, CY, CW, CH);
	}
	else
	{
		DrawSkillTrees(Player, CY);
	}
}

void AGBHUD::DrawItemsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;
	const UGBInventoryComponent* Inventory = Player->GetInventory();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> List = GetInventoryList(Player);
	if (!Inventory || List.Num() == 0)
	{
		DrawLabel(TEXT("Your bag is empty."), Style.Text, X, Y, 22.f, 0.f, bShadow);
		return;
	}
	MenuState.Cursor = FMath::Clamp(MenuState.Cursor, 0, List.Num() - 1);

	const float ListW = W * 0.5f;
	const float RowH = 34.f * S;
	const int32 Visible = FMath::Max(1, FMath::FloorToInt(H / RowH));
	const int32 First = FMath::Clamp(MenuState.Cursor - Visible + 1, 0, FMath::Max(0, List.Num() - Visible));
	for (int32 i = First; i < List.Num() && i < First + Visible; ++i)
	{
		const FName ItemId = List[i];
		const float RowY = Y + (i - First) * RowH;
		if (i == MenuState.Cursor)
		{
			DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, 0.35f), X - 6.f * S, RowY - 3.f * S, ListW, RowH - 2.f * S);
		}
		AddMenuHit(EMenuHitKind::ItemRow, X - 6.f * S, RowY - 3.f * S, ListW, RowH - 2.f * S, i);
		FString Text = FString::Printf(TEXT("%s  x%d"), *ItemName(Data, ItemId), Inventory->GetCount(ItemId));
		if (Inventory->GetEquippedArmor() == ItemId)
		{
			Text += TEXT("  [Equipped]");
		}
		if (Inventory->GetQuickUseItem() == ItemId)
		{
			Text += TEXT("  [F]");
		}
		if (MenuState.PendingCombine == ItemId)
		{
			Text = TEXT("* ") + Text;
		}
		DrawLabel(Text, Style.Text, X, RowY, 20.f, 0.f, bShadow);
	}

	// Details of the selected item.
	const FName Selected = List[MenuState.Cursor];
	const FGBItemDefinition* Def = Data ? Data->FindItem(Selected) : nullptr;
	const float DX = X + ListW + 30.f * S;
	const float DW = W - ListW - 30.f * S;
	float DY = Y;
	DrawLabel(ItemName(Data, Selected), Style.Text, DX, DY, 28.f, 0.f, bShadow, true);
	DY += 44.f * S;
	if (Def)
	{
		DrawLabel(ItemTypeName(Def->Type), Style.Text, DX, DY, 18.f, 0.f, bShadow);
		DY += 32.f * S;
		DY += DrawWrapped(Def->Description, Style.Text, DX, DY, DW, 18.f, bShadow) + 20.f * S;

		FString Hint;
		switch (Def->Type)
		{
		case EGBItemType::Consumable: Hint = TEXT("Click or Enter: use"); break;
		case EGBItemType::Throwable: Hint = TEXT("Throw it with [F] (quick-use, Shift cycles)"); break;
		case EGBItemType::Armor: Hint = Inventory->GetEquippedArmor() == Selected ? TEXT("Click or Enter: unequip") : TEXT("Click or Enter: equip"); break;
		case EGBItemType::Material:
		case EGBItemType::KeyItem:
			if (MenuState.PendingCombine == NAME_None)
			{
				Hint = TEXT("Click or Enter: combine with another item");
			}
			else if (MenuState.PendingCombine == Selected)
			{
				Hint = TEXT("Choose the second item (Esc: cancel)");
			}
			else
			{
				Hint = FString::Printf(TEXT("Click or Enter: combine with %s"), *ItemName(Data, MenuState.PendingCombine));
			}
			break;
		default: break;
		}
		DrawLabel(Hint, Style.Accent, DX, DY, 18.f, 0.f, true);
	}
}

void AGBHUD::DrawWeaponsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;
	const UGBEquipmentComponent* Equipment = Player->GetEquipment();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> List = GetWeaponList(Player);
	if (!Equipment || List.Num() == 0)
	{
		DrawLabel(TEXT("You carry no weapons."), Style.Text, X, Y, 22.f, 0.f, bShadow);
		return;
	}
	MenuState.Cursor = FMath::Clamp(MenuState.Cursor, 0, List.Num() - 1);

	// Display rows: a header before each kind, then its weapons. WeaponIndex -1 = header.
	struct FRow
	{
		FString Text;
		int32 WeaponIndex = -1;
	};
	TArray<FRow> Rows;
	FString LastGroup;
	int32 CursorRow = 0;
	for (int32 i = 0; i < List.Num(); ++i)
	{
		const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(List[i]) : nullptr;
		const FString Group = Def ? SlotName(Def->Slot, true) : FString(TEXT("Other"));
		if (Group != LastGroup)
		{
			Rows.Add(FRow{ Group, -1 });
			LastGroup = Group;
		}
		if (i == MenuState.Cursor)
		{
			CursorRow = Rows.Num();
		}
		Rows.Add(FRow{ WeaponName(Data, List[i]), i });
	}

	const float ListW = W * 0.5f;
	const float RowH = 34.f * S;
	const int32 Visible = FMath::Max(1, FMath::FloorToInt(H / RowH));
	const int32 First = FMath::Clamp(CursorRow - Visible + 1, 0, FMath::Max(0, Rows.Num() - Visible));
	for (int32 r = First; r < Rows.Num() && r < First + Visible; ++r)
	{
		const FRow& Row = Rows[r];
		const float RowY = Y + (r - First) * RowH;
		if (Row.WeaponIndex < 0)
		{
			DrawLabel(Row.Text.ToUpper(), Style.Text, X, RowY + 6.f * S, 16.f, 0.f, bShadow, true);
			continue;
		}
		const FName WeaponId = List[Row.WeaponIndex];
		if (Row.WeaponIndex == MenuState.Cursor)
		{
			DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, 0.35f), X - 6.f * S, RowY - 3.f * S, ListW, RowH - 2.f * S);
		}
		AddMenuHit(EMenuHitKind::WeaponRow, X - 6.f * S, RowY - 3.f * S, ListW, RowH - 2.f * S, Row.WeaponIndex);
		const bool bEquipped = Equipment->IsWeaponEquipped(WeaponId);
		DrawLabel(bEquipped ? FString::Printf(TEXT("   %s  [equipped]"), *Row.Text) : TEXT("   ") + Row.Text, Style.Text, X, RowY, 20.f, 0.f, bShadow);
		if (const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(WeaponId) : nullptr)
		{
			DrawLabel(GBHudText::ElementName(Def->Element), GBHudText::ElementColor(Def->Element), X + ListW - 18.f * S, RowY + 2.f * S, 16.f, 1.f, true);
		}
	}

	// Details of the selected weapon.
	const FName Selected = List[MenuState.Cursor];
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(Selected) : nullptr;
	const float DX = X + ListW + 30.f * S;
	const float DW = W - ListW - 30.f * S;
	float DY = Y;
	DrawLabel(WeaponName(Data, Selected), Style.Text, DX, DY, 28.f, 0.f, bShadow, true);
	DY += 44.f * S;
	if (!Def)
	{
		DrawLabel(TEXT("Unknown weapon"), Style.Text, DX, DY, 18.f, 0.f, bShadow);
		return;
	}
	DrawLabel(FString::Printf(TEXT("%s   %s"), *SlotName(Def->Slot, false), *GBHudText::ElementName(Def->Element)),
		GBHudText::ElementColor(Def->Element), DX, DY, 18.f, 0.f, true);
	DY += 32.f * S;
	for (const FString& Line : WeaponStatLines(*Def))
	{
		DrawLabel(Line, Style.Text, DX, DY, 18.f, 0.f, bShadow);
		DY += 28.f * S;
	}
	DY += 16.f * S;

	if (Equipment->IsWeaponEquipped(Selected))
	{
		DrawLabel(FString::Printf(TEXT("Equipped in the %s slot"), *SlotName(Def->Slot, false).ToLower()), Style.Accent, DX, DY, 18.f, 0.f, true);
	}
	else
	{
		const AGBHeldItem* Current = Equipment->GetItemInSlot(Def->Slot);
		const FString Hint = Current ? FString::Printf(TEXT("Click or Enter: equip (replaces %s)"), *Current->GetDisplayName())
			: FString(TEXT("Click or Enter: equip"));
		DrawWrapped(Hint, Style.Accent, DX, DY, DW, 18.f, true);
	}
}

void AGBHUD::DrawSpellsPage(AGBPlayerCharacter* Player, const FGBHudSkinStyle& Style, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const bool bShadow = !Style.bChunky;
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Spellbook)
	{
		return;
	}
	const int32 NumSlots = UGBSpellbookComponent::NumSlots;
	const float ListW = W * 0.45f;
	const float RowH = 42.f * S;

	DrawLabel(TEXT("Loadout"), Style.Text, X, Y, 22.f, 0.f, bShadow, true);
	const float ListY = Y + 44.f * S;
	for (int32 i = 0; i <= NumSlots; ++i)
	{
		const float RowY = ListY + i * RowH;
		if (i == MenuState.Cursor)
		{
			DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, MenuState.bChoosingSpell ? 0.2f : 0.4f), X - 6.f * S, RowY - 4.f * S, ListW, RowH - 4.f * S);
		}
		AddMenuHit(EMenuHitKind::SpellSlot, X - 6.f * S, RowY - 4.f * S, ListW, RowH - 4.f * S, i);
		FString Text;
		if (i < NumSlots)
		{
			const FName SpellId = Spellbook->GetSlotSpell(i);
			Text = FString::Printf(TEXT("[%s]  %s"), SlotKey(i), SpellId == NAME_None ? TEXT("(empty)") : *GBHudText::SpellName(this, SpellId));
		}
		else
		{
			const EGBElement Element = Spellbook->GetEnchantElement();
			Text = FString::Printf(TEXT("[T]  Weapon enchant: %s"), *GBHudText::ElementName(Element));
			DrawRect(GBHudText::ElementColor(Element), X - 6.f * S, RowY - 4.f * S, 4.f * S, RowH - 4.f * S);
		}
		DrawLabel(Text, Style.Text, X, RowY, 20.f, 0.f, bShadow);
	}

	// Known passives (always active, never equipped).
	FString Passives;
	if (Data)
	{
		for (const FName& SpellId : Spellbook->GetKnownSpells())
		{
			const FGBSpellDefinition* Def = Data->FindSpell(SpellId);
			if (Def && Def->bPassive)
			{
				Passives += (Passives.IsEmpty() ? TEXT("") : TEXT(", ")) + GBHudText::SpellName(this, SpellId);
			}
		}
	}
	const float PassY = ListY + (NumSlots + 1) * RowH + 20.f * S;
	DrawLabel(TEXT("Passives (always active)"), Style.Text, X, PassY, 18.f, 0.f, bShadow);
	DrawWrapped(Passives.IsEmpty() ? FString(TEXT("None yet")) : Passives, Style.Text, X, PassY + 28.f * S, ListW, 16.f, bShadow);

	// Right side: the spell chooser, or details of the selected slot.
	const float DX = X + ListW + 30.f * S;
	const float DW = W - ListW - 30.f * S;
	float DY = Y;
	if (MenuState.bChoosingSpell)
	{
		const TArray<FName> Options = GetAssignableSpells(Player);
		MenuState.SubCursor = FMath::Clamp(MenuState.SubCursor, 0, FMath::Max(0, Options.Num() - 1));
		DrawLabel(FString::Printf(TEXT("Spell for slot [%s]"), SlotKey(MenuState.Cursor)), Style.Text, DX, DY, 22.f, 0.f, bShadow, true);
		DY += 44.f * S;
		const float OptH = 32.f * S;
		const int32 Visible = FMath::Max(1, FMath::FloorToInt((H - 140.f * S) / OptH));
		const int32 First = FMath::Clamp(MenuState.SubCursor - Visible + 1, 0, FMath::Max(0, Options.Num() - Visible));
		for (int32 i = First; i < Options.Num() && i < First + Visible; ++i)
		{
			const FName SpellId = Options[i];
			if (i == MenuState.SubCursor)
			{
				DrawRect(Style.Accent * FLinearColor(1.f, 1.f, 1.f, 0.4f), DX - 6.f * S, DY - 3.f * S, DW * 0.8f, OptH - 2.f * S);
			}
			AddMenuHit(EMenuHitKind::SpellChoice, DX - 6.f * S, DY - 3.f * S, DW * 0.8f, OptH - 2.f * S, i);
			FString Text = SpellId == NAME_None ? FString(TEXT("(empty the slot)")) : GBHudText::SpellName(this, SpellId);
			for (int32 Slot = 0; Slot < NumSlots; ++Slot)
			{
				if (SpellId != NAME_None && Spellbook->GetSlotSpell(Slot) == SpellId)
				{
					Text += FString::Printf(TEXT("  (in [%s])"), SlotKey(Slot));
				}
			}
			DrawLabel(Text, Style.Text, DX, DY, 19.f, 0.f, bShadow);
			DY += OptH;
		}
		const FName Hovered = Options.IsValidIndex(MenuState.SubCursor) ? Options[MenuState.SubCursor] : NAME_None;
		const FGBSpellDefinition* Def = (Data && Hovered != NAME_None) ? Data->FindSpell(Hovered) : nullptr;
		if (Def)
		{
			DY += 12.f * S;
			DrawWrapped(Def->Description, Style.Text, DX, DY, DW, 16.f, bShadow);
		}
		DrawLabel(TEXT("Click or Enter: equip   Esc: back"), Style.Accent, DX, Y + H - 30.f * S, 16.f, 0.f, true);
		return;
	}

	if (MenuState.Cursor < NumSlots)
	{
		const FName SpellId = Spellbook->GetSlotSpell(MenuState.Cursor);
		const FGBSpellDefinition* Def = (Data && SpellId != NAME_None) ? Data->FindSpell(SpellId) : nullptr;
		if (Def)
		{
			DrawLabel(GBHudText::SpellName(this, SpellId), Style.Text, DX, DY, 28.f, 0.f, bShadow, true);
			DY += 44.f * S;
			DrawLabel(FString::Printf(TEXT("%s   Mana %d"), *GBHudText::ElementName(Def->Element), FMath::RoundToInt(Spellbook->GetManaCost(SpellId))),
				GBHudText::ElementColor(Def->Element), DX, DY, 18.f, 0.f, true);
			DY += 32.f * S;
			DY += DrawWrapped(Def->Description, Style.Text, DX, DY, DW, 18.f, bShadow) + 16.f * S;
		}
		else
		{
			DrawLabel(TEXT("Empty slot"), Style.Text, DX, DY, 24.f, 0.f, bShadow);
			DY += 44.f * S;
		}
		DrawLabel(TEXT("Click or Enter: choose a spell"), Style.Accent, DX, DY, 18.f, 0.f, true);
	}
	else
	{
		const FName EnchantId = Spellbook->GetEnchantSpell();
		DrawLabel(EnchantId != NAME_None ? GBHudText::SpellName(this, EnchantId) : FString(TEXT("Weapon enchant")), Style.Text, DX, DY, 28.f, 0.f, bShadow, true);
		DY += 44.f * S;
		const FGBSpellDefinition* Def = (Data && EnchantId != NAME_None) ? Data->FindSpell(EnchantId) : nullptr;
		if (Def)
		{
			DY += DrawWrapped(Def->Description, Style.Text, DX, DY, DW, 18.f, bShadow) + 16.f * S;
		}
		DY += DrawWrapped(TEXT("Only with a gun in hand (T). Enchants the gun with the chosen element."), Style.Text, DX, DY, DW, 16.f, bShadow) + 16.f * S;
		DrawLabel(TEXT("Click or Enter: change element (Fire, Lightning, Ice)"), Style.Accent, DX, DY, 18.f, 0.f, true);
	}
}

void AGBHUD::DrawSkillTrees(AGBPlayerCharacter* Player, float Top)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree();
	if (!SkillTree)
	{
		return;
	}
	const TArray<FSkillNodeRef> Nodes = GetSkillNodes();

	DrawLabel(FString::Printf(TEXT("Skill points: %d"), SkillTree->GetPoints()), MenuWhite, W * 0.5f, Top, 22.f, 0.5f, true, true);
	if (Nodes.Num() == 0)
	{
		DrawLabel(TEXT("No skills defined (Skills.json)."), MenuWhite, W * 0.5f, Top + 50.f * S, 20.f, 0.5f);
		return;
	}
	const FSkillNodeRef* Selected = Nodes.FindByPredicate([this](const FSkillNodeRef& Node) { return Node.Id == MenuState.SkillCursor; });
	if (!Selected)
	{
		MenuState.SkillCursor = Nodes[0].Id;
		Selected = &Nodes[0];
	}

	// GDD 13: the spell tree has the 90s RPG look, the weapon tree the cyberpunk look.
	const float DetailH = 120.f * S;
	const float AreaX0 = W * 0.09f;
	const float Gap = W * 0.02f;
	const float AreaW = (W * 0.82f - Gap) * 0.5f;
	const float AreaY = Top + 40.f * S;
	const float Bottom = H * 0.93f - DetailH - 10.f * S;
	const float AreaH = FMath::Max(100.f * S, Bottom - AreaY);
	static const TCHAR* Titles[] = { TEXT("SPELLS"), TEXT("WEAPONS") };

	for (int32 Tree = 0; Tree < 2; ++Tree)
	{
		const FGBHudSkinStyle& Style = GetSkinStyle(Tree == 0 ? EGBHudSkin::Rpg90s : EGBHudSkin::Cyberpunk);
		const float AX = AreaX0 + Tree * (AreaW + Gap);
		DrawPanel(Style, AX, AreaY, AreaW, AreaH);
		DrawLabel(Titles[Tree], Style.Text, AX + AreaW * 0.5f, AreaY + 10.f * S, 22.f, 0.5f, !Style.bChunky, true);

		int32 MaxColumn = 0;
		int32 MaxRow = 0;
		for (const FSkillNodeRef& Node : Nodes)
		{
			if (Node.Tree == Tree)
			{
				MaxColumn = FMath::Max(MaxColumn, Node.Column);
				MaxRow = FMath::Max(MaxRow, Node.Row);
			}
		}
		const float Pad = 14.f * S;
		const float GridY = AreaY + 50.f * S;
		const float CellW = (AreaW - 2.f * Pad) / (MaxColumn + 1);
		const float CellH = FMath::Min(96.f * S, (AreaH - 60.f * S) / (MaxRow + 1));
		const float BoxW = FMath::Max(20.f * S, CellW - 12.f * S);
		const float BoxH = FMath::Max(20.f * S, CellH - 14.f * S);
		auto BoxPos = [&](const FSkillNodeRef& Node)
		{
			return FVector2D(AX + Pad + Node.Column * CellW + (CellW - BoxW) * 0.5f, GridY + Node.Row * CellH);
		};

		// Requirement links first, under the boxes.
		for (const FSkillNodeRef& Node : Nodes)
		{
			const FGBSkillNodeDefinition* Def = (Node.Tree == Tree && Data) ? Data->FindSkillNode(Node.Id) : nullptr;
			if (!Def)
			{
				continue;
			}
			for (const FName& Req : Def->Requires)
			{
				const FSkillNodeRef* ReqNode = Nodes.FindByPredicate([&Req](const FSkillNodeRef& Other) { return Other.Id == Req; });
				if (ReqNode && ReqNode->Tree == Tree)
				{
					const FVector2D A = BoxPos(*ReqNode) + FVector2D(BoxW * 0.5f, BoxH);
					const FVector2D B = BoxPos(Node) + FVector2D(BoxW * 0.5f, 0.f);
					const bool bOwned = SkillTree->GetRank(Req) > 0;
					DrawLine(A.X, A.Y, B.X, B.Y, bOwned ? Style.Accent : MenuGrey, 2.f * S);
				}
			}
		}

		for (const FSkillNodeRef& Node : Nodes)
		{
			if (Node.Tree != Tree)
			{
				continue;
			}
			const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(Node.Id) : nullptr;
			const int32 Rank = SkillTree->GetRank(Node.Id);
			const int32 MaxRank = Def ? Def->MaxRank : 1;
			const bool bCan = SkillTree->CanUnlock(Node.Id) == EGBSkillUnlockResult::Ok;
			const bool bSelected = Node.Id == MenuState.SkillCursor;
			const FVector2D Pos = BoxPos(Node);

			FLinearColor Fill(0.f, 0.f, 0.f, 0.7f);
			if (Rank >= MaxRank)
			{
				Fill = Style.Accent * FLinearColor(0.45f, 0.45f, 0.45f, 0.9f);
			}
			else if (Rank > 0)
			{
				Fill = Style.Mana * FLinearColor(0.5f, 0.5f, 0.5f, 0.85f);
			}
			DrawRect(Fill, Pos.X, Pos.Y, BoxW, BoxH);
			AddMenuHit(EMenuHitKind::SkillNode, Pos.X, Pos.Y, BoxW, BoxH, 0, Node.Id);
			const FLinearColor Edge = bSelected ? Style.Accent : (bCan ? (Style.bChunky ? FLinearColor(1.f, 0.85f, 0.5f, 1.f) : Style.Border) : MenuGrey);
			DrawOutline(Pos.X, Pos.Y, BoxW, BoxH, Edge, (bSelected ? 4.f : (bCan ? 2.f : 1.f)) * S);

			const int32 MaxChars = FMath::Max(4, FMath::FloorToInt(BoxW / (9.f * S)));
			DrawLabel(GBHudText::Abbreviate(SkillName(Data, Node.Id), MaxChars), MenuWhite, Pos.X + BoxW * 0.5f, Pos.Y + 6.f * S, 15.f, 0.5f);
			DrawLabel(FString::Printf(TEXT("%d/%d   %d pt"), Rank, MaxRank, Def ? Def->Cost : 0), bCan ? MenuWhite : MenuGrey,
				Pos.X + BoxW * 0.5f, Pos.Y + BoxH - 22.f * S, 13.f, 0.5f);
		}
	}

	// Details of the selected node, in its tree's style.
	if (Selected)
	{
		const FGBHudSkinStyle& Style = GetSkinStyle(Selected->Tree == 0 ? EGBHudSkin::Rpg90s : EGBHudSkin::Cyberpunk);
		const bool bShadow = !Style.bChunky;
		const float DY = AreaY + AreaH + 10.f * S;
		const float DW = 2.f * AreaW + Gap;
		DrawPanel(Style, AreaX0, DY, DW, DetailH);
		const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(Selected->Id) : nullptr;
		const int32 Rank = SkillTree->GetRank(Selected->Id);
		const FString Header = FString::Printf(TEXT("%s   Rank %d/%d   Cost %d"), *SkillName(Data, Selected->Id), Rank, Def ? Def->MaxRank : 1, Def ? Def->Cost : 0);
		DrawLabel(Header, Style.Text, AreaX0 + 16.f * S, DY + 12.f * S, 20.f, 0.f, bShadow);
		if (Def)
		{
			DrawWrapped(Def->Description, Style.Text, AreaX0 + 16.f * S, DY + 42.f * S, DW * 0.65f, 16.f, bShadow);
		}
		const EGBSkillUnlockResult Can = SkillTree->CanUnlock(Selected->Id);
		const FString Status = Can == EGBSkillUnlockResult::Ok ? FString(TEXT("Click or Enter: unlock")) : SkillResultText(Data, Selected->Id, Can);
		DrawLabel(Status, Style.Accent, AreaX0 + DW - 16.f * S, DY + 42.f * S, 18.f, 1.f, true);
	}
}
