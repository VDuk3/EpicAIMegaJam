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
	MenuOpenTime = GetRealTime();
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
	case EMenuHitKind::Tab:
	case EMenuHitKind::SkillReset:
	case EMenuHitKind::SkillAddPoint:
	default:
		// Tabs, the skill buttons (reset, +1 point) and the death screen only react to clicks.
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
	case EMenuHitKind::SkillReset:
		ResetSkills(Player);
		break;
	case EMenuHitKind::SkillAddPoint:
		AddTestSkillPoint(Player);
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

void AGBHUD::ResetSkills(AGBPlayerCharacter* Player)
{
	UGBSkillTreeComponent* SkillTree = Player ? Player->GetSkillTree() : nullptr;
	if (!SkillTree)
	{
		return;
	}
	const int32 Refunded = SkillTree->ResetAll();
	if (Refunded > 0)
	{
		ShowMessage(FString::Printf(TEXT("Skills reset: %d points refunded"), Refunded), 2.f);
	}
	else
	{
		ShowMessage(TEXT("No skills to reset"), 1.5f);
	}
}

void AGBHUD::AddTestSkillPoint(AGBPlayerCharacter* Player)
{
	UGBSkillTreeComponent* SkillTree = Player ? Player->GetSkillTree() : nullptr;
	if (!SkillTree)
	{
		return;
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const int32 Amount = Data ? FMath::Max(0, FMath::RoundToInt(Data->GetGameValue(GBHudKey::TestSkillPointAmount, 1.f))) : 1;
	SkillTree->AddPoints(Amount);
	ShowMessage(FString::Printf(TEXT("+%d skill point (%d)"), Amount, SkillTree->GetPoints()), 1.5f);
}

void AGBHUD::MenuReset()
{
	const EGBMenu Menu = GetOpenMenu();
	const bool bShowsSkills = (Menu == EGBMenu::SkillTrees) || (Menu == EGBMenu::Inventory && MenuState.Page == PageSkills);
	if (!bShowsSkills)
	{
		return;
	}
	ResetSkills(GetPlayer());
}

// ======================================================================== drawing: shared widgets

namespace GBHudMenuDraw
{
	/** Spell tree columns, in the order of GDD 18 "Spell tree" (fire, lightning, ice, casting): presentation only. */
	const TCHAR* SpellColumnName(int32 Column)
	{
		static const TCHAR* Names[] = { TEXT("Fire"), TEXT("Lightning"), TEXT("Ice"), TEXT("Casting") };
		return (Column >= 0 && Column < 4) ? Names[Column] : TEXT("");
	}
	EGBElement SpellColumnElement(int32 Column)
	{
		static const EGBElement Elements[] = { EGBElement::Fire, EGBElement::Lightning, EGBElement::Ice, EGBElement::Arcane };
		return (Column >= 0 && Column < 4) ? Elements[Column] : EGBElement::Arcane;
	}
	/** Splits a name in two lines at the space nearest the middle (or keeps one line). */
	void SplitName(const FString& Name, FString& A, FString& B)
	{
		int32 Best = INDEX_NONE;
		for (int32 i = 0; i < Name.Len(); ++i)
		{
			if (Name[i] == TEXT(' ') && (Best == INDEX_NONE || FMath::Abs(i - Name.Len() / 2) < FMath::Abs(Best - Name.Len() / 2)))
			{
				Best = i;
			}
		}
		if (Best == INDEX_NONE)
		{
			A = Name;
			B.Reset();
			return;
		}
		A = Name.Left(Best);
		B = Name.Mid(Best + 1);
	}
}

bool AGBHUD::IsMouseOver(float X, float Y, float W, float H) const
{
	return bHasMenuMouse && LastMenuMouse.X >= X && LastMenuMouse.X <= X + W && LastMenuMouse.Y >= Y && LastMenuMouse.Y <= Y + H;
}

void AGBHUD::DebugSetMenuCursor(int32 Page, int32 Cursor, FName SkillNode, bool bChoosingSpell)
{
	MenuState.Page = GBHudMenuLocal::Wrap(Page, PageCount);
	MenuState.Cursor = FMath::Max(0, Cursor);
	MenuState.bChoosingSpell = bChoosingSpell;
	MenuState.SubCursor = bChoosingSpell ? 1 : 0;
	if (SkillNode != NAME_None)
	{
		MenuState.SkillCursor = SkillNode;
	}
}

void AGBHUD::MenuBackdrop(EGBHudSkin Skin)
{
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		FillRect(0.f, 0.f, W, H, GBAlpha(GBCy::Ink, 0.9f));
		DrawGlow(W * 0.12f, H * 0.05f, W * 0.55f, H * 0.6f, GBAlpha(GBCy::Red, 0.10f));
		DrawGlow(W * 0.95f, H * 1.0f, W * 0.45f, H * 0.5f, GBAlpha(GBCy::Cyan, 0.05f));
		// Faint layout grid.
		const float Step = 120.f * S;
		for (float GX = Step; GX < W; GX += Step)
		{
			FillRect(GX, 0.f, 1.f, H, GBAlpha(GBCy::Red, 0.035f));
		}
		CyScanlines(0.f, 0.f, W, H, 0.22f);
		return;
	}
	FillRect(0.f, 0.f, W, H, FLinearColor(0.01f, 0.008f, 0.012f, 0.72f));
	DrawGlow(W * 0.5f, H * 0.45f, W * 0.6f, H * 0.65f, FLinearColor(0.25f, 0.12f, 0.05f, 0.18f));
}

namespace GBHudMenuDraw
{
	struct FScreen
	{
		float X = 0.f;
		float Y = 0.f;
		float W = 0.f;
		float H = 0.f;
		float FooterY = 0.f;
	};
}

float AGBHUD::MenuRow(EGBHudSkin Skin, float X, float Y, float W, float H, bool bSelected, bool bMarked)
{
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		if (bSelected)
		{
			CyBox(X, Y, W, H, 8.f * S, GBAlpha(GBCy::Red, 0.16f), GBAlpha(GBCy::Red, 0.95f), 0x4);
			FillRect(X, Y, 4.f * S, H, GBCy::Red);
			DrawFade(X + 4.f * S, Y, W * 0.5f, H, GBAlpha(GBCy::Red, 0.14f), true);
		}
		else
		{
			FillRect(X, Y + H - 1.f, W, 1.f, GBAlpha(GBCy::RedDeep, 0.9f));
		}
		if (bMarked)
		{
			CyBox(X - 3.f * S, Y - 3.f * S, W + 6.f * S, H + 6.f * S, 8.f * S, FLinearColor::Transparent, GBCy::Cyan, 0x4);
		}
		return 16.f * S;
	}
	const float P = ArtPixel();
	if (bSelected)
	{
		FillRect(X, Y, W, H, GBAlpha(GBRpg::Ember, 0.10f));
		DrawFade(X, Y, W * 0.7f, H, GBAlpha(GBRpg::Gold2, 0.14f), true);
		DrawOutline(X, Y, W, H, GBRpg::Gold2, P);
		FillRect(X + P, Y + P, W - 2.f * P, P, GBAlpha(GBRpg::Gold4, 0.35f));
		RpgCursor(X - 7.f * P, Y + H * 0.5f);
	}
	else
	{
		FillRect(X + 2.f * P, Y + H - P, W - 4.f * P, P, GBAlpha(GBRpg::Stone2, 0.8f));
	}
	if (bMarked)
	{
		DrawOutline(X - P, Y - P, W + 2.f * P, H + 2.f * P, GBRpg::ArcaneLight, P);
	}
	return 4.f * P;
}

void AGBHUD::MenuButton(EGBHudSkin Skin, float X, float Y, float W, float H, const FString& Label, bool bEnabled, bool bHot)
{
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		const FLinearColor Line = !bEnabled ? GBCy::Grey : (bHot ? GBCy::Cyan : GBCy::Red);
		CyBox(X, Y, W, H, 8.f * S, bHot && bEnabled ? GBAlpha(GBCy::Cyan, 0.18f) : GBAlpha(GBCy::Ink, 0.7f), Line, 0x4);
		FCyTextStyle Style;
		Style.Face = ECyFace::Condensed;
		Style.Px = 17.f;
		Style.Spacing = 100;
		Style.bUpper = true;
		Style.Color = !bEnabled ? GBCy::Grey : (bHot ? GBCy::Pale : GBCy::RedBright);
		const FVector2D Size = MeasureCyText(Label, Style);
		DrawCyText(Label, X + W * 0.5f, Y + (H - Size.Y) * 0.5f, Style, 0.5f);
		return;
	}
	const float P = ArtPixel();
	FillRect(X, Y, W, H, GBRpg::Stone1);
	RpgStone(X, Y, W, H, bHot && bEnabled ? FLinearColor(1.2f, 1.1f, 1.f, 1.f) : FLinearColor::White);
	RpgFrame(X, Y, W, H, bEnabled ? (bHot ? ERpgFrame::Gold : ERpgFrame::Iron) : ERpgFrame::Inset);
	FPixelTextStyle Style;
	Style.Scale = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	Style.Ink = bEnabled ? (bHot ? &GBRpg::InkGold : &GBRpg::InkBone) : nullptr;
	Style.Color = bEnabled ? FLinearColor::White : GBRpg::BoneDark;
	Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	const FVector2D Size = MeasurePixelText(Label, Style.Scale);
	DrawPixelText(Label, X + W * 0.5f, Y + FMath::RoundToFloat((H - Size.Y) * 0.5f), Style, 0.5f);
}

void AGBHUD::MenuHints(EGBHudSkin Skin, const TArray<TPair<FString, FString>>& Hints, float X, float Y, float AlignX)
{
	// Measure first, then draw from the left.
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const float S = UIScale();
	const float P = ArtPixel();
	const float KeyGap = bCyber ? 8.f * S : 2.f * P;
	const float PairGap = bCyber ? 28.f * S : 8.f * P;
	FCyTextStyle CyStyle;
	CyStyle.Face = ECyFace::Condensed;
	CyStyle.Px = 16.f;
	CyStyle.Spacing = 100;
	CyStyle.bUpper = true;
	CyStyle.Color = GBCy::RedBright;
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	auto KeyWidth = [&](const FString& Key)
	{
		if (bCyber)
		{
			FCyTextStyle K;
			K.Face = ECyFace::Condensed;
			K.Px = 22.f * 0.72f;
			K.bUpper = true;
			return FMath::Max(FMath::RoundToFloat(22.f * S), FMath::RoundToFloat(MeasureCyText(Key, K).X + 10.f * S));
		}
		const FVector2D Size = MeasurePixelText(Key, Small);
		return FMath::Max(FMath::CeilToFloat((Size.X + 6 * P) / P) * P, 9 * P);
	};
	auto TextWidth = [&](const FString& Text) { return bCyber ? MeasureCyText(Text, CyStyle).X : MeasurePixelText(Text, Small).X; };
	float Total = 0.f;
	for (int32 i = 0; i < Hints.Num(); ++i)
	{
		TArray<FString> Keys;
		Hints[i].Key.ParseIntoArray(Keys, TEXT(" "));
		for (const FString& Key : Keys)
		{
			Total += KeyWidth(Key) + KeyGap * 0.5f;
		}
		Total += KeyGap + TextWidth(Hints[i].Value) + (i + 1 < Hints.Num() ? PairGap : 0.f);
	}
	float PX = FMath::RoundToFloat(X - Total * AlignX);
	for (const TPair<FString, FString>& Hint : Hints)
	{
		TArray<FString> Keys;
		Hint.Key.ParseIntoArray(Keys, TEXT(" "));
		for (const FString& Key : Keys)
		{
			if (bCyber)
			{
				PX += CyKey(PX, Y, Key, GBCy::Cyan, 22.f) + KeyGap * 0.5f;
			}
			else
			{
				float KW = 0.f;
				RpgKeyCap(PX, Y, Key, &KW);
				PX += KW + KeyGap * 0.5f;
			}
		}
		PX += KeyGap * 0.5f;
		if (bCyber)
		{
			const FVector2D Size = DrawCyText(Hint.Value, PX, Y + 1.f * S, CyStyle, 0.f);
			PX += Size.X + PairGap;
		}
		else
		{
			FPixelTextStyle Style;
			Style.Scale = Small;
			Style.Ink = &GBRpg::InkBone;
			Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			const FVector2D Size = DrawPixelText(Hint.Value, PX, Y + 2.f * P + FMath::RoundToFloat(0.5f * Small), Style, 0.f);
			PX += Size.X + PairGap;
		}
	}
}

float AGBHUD::MenuDetailPanel(EGBHudSkin Skin, float X, float Y, float W, float H, const FString& Title, const FString& Subtitle, const FLinearColor* SubtitleColor)
{
	if (Skin == EGBHudSkin::Cyberpunk)
	{
		const float S = UIScale();
		CyBox(X, Y, W, H, 18.f * S, GBAlpha(GBCy::RedBlack, 0.55f), GBAlpha(GBCy::RedDim, 0.9f), 0x5);
		CyBrackets(X - 5.f * S, Y - 5.f * S, W + 10.f * S, H + 10.f * S, 14.f * S, GBCy::Red, 2.f * S);
		float CY = Y + 22.f * S;
		const FVector2D TS = UiText(Skin, EGBTextSize::Heading, EGBTextTone::Accent, Title, X + 24.f * S, CY);
		CY += TS.Y + 10.f * S;
		CyRuler(X + 24.f * S, CY, W - 48.f * S, GBAlpha(GBCy::RedDim, 0.8f));
		CY += 14.f * S;
		if (!Subtitle.IsEmpty())
		{
			FCyTextStyle Sub;
			Sub.Face = ECyFace::Condensed;
			Sub.Px = 18.f;
			Sub.Spacing = 120;
			Sub.bUpper = true;
			Sub.Color = SubtitleColor ? *SubtitleColor : GBCy::Cyan;
			CY += DrawCyText(Subtitle, X + 24.f * S, CY, Sub, 0.f).Y + 12.f * S;
		}
		return CY;
	}
	const float P = ArtPixel();
	RpgWell(X, Y, W, H);
	float CY = Y + 6.f * P;
	const FVector2D TS = UiText(Skin, EGBTextSize::Heading, EGBTextTone::Accent, Title, X + 7.f * P, CY);
	CY += TS.Y + 4.f * P;
	RpgDivider(X + 6.f * P, CY, W - 12.f * P);
	CY += 6.f * P;
	if (!Subtitle.IsEmpty())
	{
		CY += UiText(Skin, EGBTextSize::Small, EGBTextTone::Muted, Subtitle, X + 7.f * P, CY, 0.f, SubtitleColor).Y + 5.f * P;
	}
	return CY;
}

// ======================================================================== drawing: screens

namespace GBHudMenuDraw
{
	FScreen Layout(EGBHudSkin Skin, float W, float H, float S, float P)
	{
		FScreen Screen;
		if (Skin == EGBHudSkin::Cyberpunk)
		{
			const float M = 64.f * S;
			Screen.X = M;
			Screen.Y = 158.f * S;
			Screen.W = W - 2.f * M;
			Screen.FooterY = H - 62.f * S;
			Screen.H = Screen.FooterY - 22.f * S - Screen.Y;
			return Screen;
		}
		const float WinX = FMath::RoundToFloat(W * 0.05f / P) * P;
		const float WinY = FMath::RoundToFloat(H * 0.085f / P) * P;
		const float WinW = FMath::RoundToFloat((W - 2.f * WinX) / P) * P;
		const float WinH = FMath::RoundToFloat((H - WinY - 4.f * P) / P) * P;
		Screen.X = WinX;
		Screen.Y = WinY;
		Screen.W = WinW;
		Screen.H = WinH;
		Screen.FooterY = WinY + WinH - 15.f * P;
		return Screen;
	}
}

void AGBHUD::DrawMenus(AGBPlayerCharacter* Player)
{
	const EGBHudSkin Skin = GetCurrentSkin();
	// Clickables are rebuilt while drawing, for the screen as it is now.
	MenuHits.Reset();
	MenuHitsMenu = GetOpenMenu();
	MenuHitsPage = MenuState.Page;
	bMenuHitsChoosing = MenuState.bChoosingSpell;
	switch (GetOpenMenu())
	{
	case EGBMenu::Pause:
		DrawPauseMenu(Skin);
		break;
	case EGBMenu::Death:
		DrawDeathScreen(Skin);
		break;
	case EGBMenu::Inventory:
		DrawInventory(Player, Skin);
		break;
	case EGBMenu::SkillTrees:
	{
		const float S = UIScale();
		const float P = ArtPixel();
		const float W = Canvas->ClipX;
		const float H = Canvas->ClipY;
		MenuBackdrop(Skin);
		const GBHudMenuDraw::FScreen Screen = GBHudMenuDraw::Layout(Skin, W, H, S, P);
		float ContentY = Screen.Y;
		float ContentX = Screen.X;
		float ContentW = Screen.W;
		if (Skin == EGBHudSkin::Cyberpunk)
		{
			FCyTextStyle Title;
			Title.Face = ECyFace::Condensed;
			Title.Px = 58.f;
			Title.Spacing = 80;
			Title.bUpper = true;
			Title.Color = GBCy::Red;
			Title.Split = FMath::Max(0.f, 1.f - (float)(GetRealTime() - MenuOpenTime) / 0.3f) * 6.f * S;
			DrawCyText(TEXT("Skill trees"), Screen.X, 50.f * S, Title, 0.f);
			CyRuler(Screen.X, 134.f * S, Screen.W, GBAlpha(GBCy::RedDim, 0.8f));
			FillRect(Screen.X, Screen.FooterY - 14.f * S, Screen.W, 1.f, GBAlpha(GBCy::RedDim, 0.6f));
		}
		else
		{
			RpgWindow(Screen.X, Screen.Y, Screen.W, Screen.H);
			const int32 TitleScale = FMath::Max(3, FMath::RoundToInt(6.f * S));
			const FBox2D Plaque = RpgPlaque(TEXT("SKILL TREES"), Screen.X + Screen.W * 0.5f, Screen.Y - 7.f * P, TitleScale);
			ContentY = Plaque.Max.Y + 4.f * P;
			ContentX = Screen.X + 9.f * P;
			ContentW = Screen.W - 18.f * P;
		}
		if (Player)
		{
			DrawSkillTrees(Player, Skin, ContentX, ContentY, ContentW, Screen.FooterY - 4.f * P - ContentY);
		}
		TArray<TPair<FString, FString>> Hints;
		Hints.Add(TPair<FString, FString>(TEXT("Arrows"), TEXT("Move")));
		Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Unlock")));
		Hints.Add(TPair<FString, FString>(TEXT("Q E"), TEXT("Other tree")));
		Hints.Add(TPair<FString, FString>(TEXT("R"), TEXT("Reset skills")));
		Hints.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Close")));
		MenuHints(Skin, Hints, Skin == EGBHudSkin::Cyberpunk ? Screen.X : W * 0.5f, Screen.FooterY, Skin == EGBHudSkin::Cyberpunk ? 0.f : 0.5f);
		break;
	}
	default:
		break;
	}
}

void AGBHUD::DrawPauseMenu(EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	MenuBackdrop(Skin);
	static const TCHAR* Entries[] = { TEXT("Resume"), TEXT("Quit to desktop") };

	if (Skin == EGBHudSkin::Cyberpunk)
	{
		// Left-aligned like a system screen: big title, two big entries, the selected one filled.
		const float X = 140.f * S;
		FCyTextStyle Sub;
		Sub.Face = ECyFace::Mono;
		Sub.Px = 15.f;
		Sub.Color = GBCy::RedDim;
		DrawCyText(TEXT("// SESSION SUSPENDED"), X, H * 0.30f, Sub, 0.f);
		FCyTextStyle Title;
		Title.Face = ECyFace::Condensed;
		Title.Px = 96.f;
		Title.Spacing = 60;
		Title.bUpper = true;
		Title.Color = GBCy::Red;
		Title.Split = FMath::Max(0.f, 1.f - (float)(GetRealTime() - MenuOpenTime) / 0.3f) * 8.f * S;
		const FVector2D TS = DrawCyText(TEXT("Paused"), X, H * 0.30f + 18.f * S, Title, 0.f);
		CyRuler(X, H * 0.30f + 18.f * S + TS.Y + 8.f * S, 520.f * S, GBAlpha(GBCy::RedDim, 0.8f));
		for (int32 i = 0; i < 2; ++i)
		{
			const float RowY = H * 0.30f + TS.Y + 52.f * S + i * 66.f * S;
			const float RowW = 460.f * S;
			const float RowH = 54.f * S;
			const bool bSelected = MenuState.Cursor == i;
			if (bSelected)
			{
				CyBox(X, RowY, RowW, RowH, 12.f * S, GBCy::Red, FLinearColor::Transparent, 0x4);
				DrawGlow(X + RowW * 0.3f, RowY + RowH * 0.5f, RowW * 0.6f, RowH, GBAlpha(GBCy::Red, 0.25f));
			}
			else
			{
				CyBox(X, RowY, RowW, RowH, 12.f * S, GBAlpha(GBCy::Ink, 0.4f), GBAlpha(GBCy::RedDim, 0.7f), 0x4);
			}
			AddMenuHit(EMenuHitKind::PauseEntry, X, RowY, RowW, RowH, i);
			FCyTextStyle Entry;
			Entry.Face = ECyFace::Condensed;
			Entry.Px = 30.f;
			Entry.Spacing = 120;
			Entry.bUpper = true;
			Entry.Color = bSelected ? GBCy::Ink : GBCy::RedBright;
			const FVector2D ES = MeasureCyText(Entries[i], Entry);
			DrawCyText(Entries[i], X + 26.f * S, RowY + (RowH - ES.Y) * 0.5f, Entry, 0.f);
		}
		TArray<TPair<FString, FString>> Hints;
		Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Select")));
		Hints.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Resume")));
		MenuHints(Skin, Hints, X, H - 62.f * S, 0.f);
		return;
	}

	// RPG: a small iron-framed window with a plaque, two carved entries and the gold arrow.
	const float PW = FMath::RoundToFloat(150.f * P / P) * P;
	const float PH = 84.f * P;
	const float PX = FMath::RoundToFloat((W - PW) * 0.5f / P) * P;
	const float PY = FMath::RoundToFloat((H - PH) * 0.5f / P) * P;
	RpgWindow(PX, PY, PW, PH);
	const int32 TitleScale = FMath::Max(3, FMath::RoundToInt(6.f * S));
	RpgPlaque(TEXT("PAUSED"), PX + PW * 0.5f, PY - 7.f * P, TitleScale);
	for (int32 i = 0; i < 2; ++i)
	{
		const float RowX = PX + 20.f * P;
		const float RowW = PW - 40.f * P;
		const float RowH = 16.f * P;
		const float RowY = PY + 22.f * P + i * 20.f * P;
		const bool bSelected = MenuState.Cursor == i;
		MenuRow(Skin, RowX, RowY, RowW, RowH, bSelected);
		AddMenuHit(EMenuHitKind::PauseEntry, RowX, RowY, RowW, RowH, i);
		const float LH = UiMeasure(Skin, EGBTextSize::Heading, Entries[i]).Y;
		UiText(Skin, EGBTextSize::Heading, bSelected ? EGBTextTone::Selected : EGBTextTone::Muted, Entries[i], RowX + RowW * 0.5f, RowY + FMath::RoundToFloat((RowH - LH) * 0.5f), 0.5f,
			bSelected ? nullptr : &GBRpg::BoneDim);
	}
	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Select")));
	Hints.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Resume")));
	MenuHints(Skin, Hints, PX + PW * 0.5f, PY + PH - 16.f * P, 0.5f);
}

void AGBHUD::DrawDeathScreen(EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	const float Since = (float)(GetRealTime() - MenuOpenTime);
	AddMenuHit(EMenuHitKind::DeathScreen, 0.f, 0.f, W, H);
	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Respawn at the last checkpoint")));

	if (Skin == EGBHudSkin::Cyberpunk)
	{
		FillRect(0.f, 0.f, W, H, GBAlpha(GBCy::RedBlack, 0.82f));
		DrawGlow(W * 0.5f, H * 0.45f, W * 0.6f, H * 0.4f, GBAlpha(GBCy::Red, 0.18f));
		CyScanlines(0.f, 0.f, W, H, 0.3f);
		// A flat line across the screen with one heartbeat that dies out.
		const float LineY = H * 0.62f;
		FillRect(0.f, LineY, W, 2.f * S, GBAlpha(GBCy::Red, 0.75f));
		const float BX = W * 0.30f;
		TArray<FVector2D> Beat = { FVector2D(BX, LineY), FVector2D(BX + 14.f * S, LineY - 30.f * S), FVector2D(BX + 26.f * S, LineY + 22.f * S), FVector2D(BX + 38.f * S, LineY - 8.f * S), FVector2D(BX + 48.f * S, LineY) };
		for (int32 i = 1; i < Beat.Num(); ++i)
		{
			DrawLine(Beat[i - 1].X, Beat[i - 1].Y, Beat[i].X, Beat[i].Y, GBAlpha(GBCy::Red, 0.9f), 2.f * S);
		}
		FCyTextStyle Sub;
		Sub.Face = ECyFace::Mono;
		Sub.Px = 16.f;
		Sub.Color = GBCy::RedDim;
		DrawCyText(TEXT("VITAL SIGNS LOST // RESTORE POINT FOUND"), W * 0.5f, H * 0.33f, Sub, 0.5f);
		FCyTextStyle Title;
		Title.Face = ECyFace::Condensed;
		Title.Px = 150.f;
		Title.Spacing = 80;
		Title.bUpper = true;
		Title.Color = GBCy::Red;
		Title.Split = (Since < 0.5f ? (1.f - Since / 0.5f) * 14.f : 2.f + 2.f * FMath::Abs(FMath::Sin(Since * 9.f))) * S;
		DrawCyText(TEXT("Flatlined"), W * 0.5f, H * 0.36f, Title, 0.5f);
		MenuHints(Skin, Hints, W * 0.5f, H * 0.70f, 0.5f);
		return;
	}
	// RPG: the world sinks into a stepped blood vignette, a gold-and-blood epitaph, a divider.
	FillRect(0.f, 0.f, W, H, FLinearColor(0.08f, 0.f, 0.f, 0.62f));
	for (int32 k = 0; k < 8; ++k)
	{
		const float T = 4.f * P;
		const float In = k * T;
		const FLinearColor C = GBAlpha(GBRpg::HealthRamp[0], 0.5f * (1.f - k / 8.f));
		FillRect(In, In, W - 2.f * In, T, C);
		FillRect(In, H - In - T, W - 2.f * In, T, C);
		FillRect(In, In + T, T, H - 2.f * (In + T), C);
		FillRect(W - In - T, In + T, T, H - 2.f * (In + T), C);
	}
	const int32 Scale = FMath::Max(4, FMath::RoundToInt(11.f * S));
	FPixelTextStyle Title;
	Title.Scale = Scale;
	Title.Ink = &GBRpg::InkBlood;
	Title.Outline = GBRpg::Outline;
	Title.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.8f);
	Title.Color = FLinearColor(1.f, 1.f, 1.f, FMath::Clamp(Since / 0.6f, 0.f, 1.f));
	const float TitleY = FMath::RoundToFloat(H * 0.36f);
	DrawPixelText(TEXT("YOU HAVE FALLEN"), W * 0.5f, TitleY, Title, 0.5f);
	const float DW = FMath::RoundToFloat(W * 0.36f / P) * P;
	RpgDivider(FMath::RoundToFloat((W - DW) * 0.5f / P) * P, TitleY + 7.f * Scale + 8.f * P, DW);
	MenuHints(Skin, Hints, W * 0.5f, TitleY + 7.f * Scale + 18.f * P, 0.5f);
}

void AGBHUD::DrawInventory(AGBPlayerCharacter* Player, EGBHudSkin Skin)
{
	const float S = UIScale();
	const float P = ArtPixel();
	const float W = Canvas->ClipX;
	const float H = Canvas->ClipY;
	MenuBackdrop(Skin);
	const GBHudMenuDraw::FScreen Screen = GBHudMenuDraw::Layout(Skin, W, H, S, P);
	static const TCHAR* Tabs[] = { TEXT("Items"), TEXT("Weapons"), TEXT("Spells"), TEXT("Skills") };
	static_assert((int32)UE_ARRAY_COUNT(Tabs) == PageCount, "One tab name per inventory page");
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;

	float CX = Screen.X;
	float CY = Screen.Y;
	float CW = Screen.W;
	if (bCyber)
	{
		// Title, tabs to its right, ruler under both.
		FCyTextStyle Title;
		Title.Face = ECyFace::Condensed;
		Title.Px = 58.f;
		Title.Spacing = 80;
		Title.bUpper = true;
		Title.Color = GBCy::Red;
		Title.Split = FMath::Max(0.f, 1.f - (float)(GetRealTime() - MenuOpenTime) / 0.3f) * 6.f * S;
		const FVector2D TS = DrawCyText(TEXT("Inventory"), Screen.X, 50.f * S, Title, 0.f);
		float TabX = Screen.X + TS.X + 56.f * S;
		const float TabY = 74.f * S;
		const float TabH = 40.f * S;
		CyKey(TabX - 36.f * S, TabY + 9.f * S, TEXT("Q"), GBCy::Cyan, 22.f);
		for (int32 i = 0; i < PageCount; ++i)
		{
			FCyTextStyle Tab;
			Tab.Face = ECyFace::Condensed;
			Tab.Px = 22.f;
			Tab.Spacing = 140;
			Tab.bUpper = true;
			const FVector2D Size = MeasureCyText(Tabs[i], Tab);
			const float TabW = Size.X + 36.f * S;
			const bool bSelected = MenuState.Page == i;
			const bool bHover = IsMouseOver(TabX, TabY, TabW, TabH);
			if (bSelected)
			{
				CyBox(TabX, TabY, TabW, TabH, 10.f * S, GBCy::Red, FLinearColor::Transparent, 0x4);
			}
			else
			{
				CyBox(TabX, TabY, TabW, TabH, 10.f * S, FLinearColor::Transparent, GBAlpha(bHover ? GBCy::RedBright : GBCy::RedDeep, 1.f), 0x4);
			}
			Tab.Color = bSelected ? GBCy::Ink : (bHover ? GBCy::Pale : GBCy::RedDim);
			DrawCyText(Tabs[i], TabX + TabW * 0.5f, TabY + (TabH - Size.Y) * 0.5f, Tab, 0.5f);
			AddMenuHit(EMenuHitKind::Tab, TabX, TabY, TabW, TabH, i);
			TabX += TabW + 8.f * S;
		}
		CyKey(TabX + 4.f * S, TabY + 9.f * S, TEXT("E"), GBCy::Cyan, 22.f);
		CyRuler(Screen.X, 134.f * S, Screen.W, GBAlpha(GBCy::RedDim, 0.8f));
		FillRect(Screen.X, Screen.FooterY - 14.f * S, Screen.W, 1.f, GBAlpha(GBCy::RedDim, 0.6f));
	}
	else
	{
		RpgWindow(Screen.X, Screen.Y, Screen.W, Screen.H);
		const int32 TitleScale = FMath::Max(3, FMath::RoundToInt(6.f * S));
		const FBox2D Plaque = RpgPlaque(TEXT("INVENTORY"), Screen.X + Screen.W * 0.5f, Screen.Y - 7.f * P, TitleScale);
		// Stone tabs hanging under the plaque.
		const int32 TabScale = FMath::Max(2, FMath::RoundToInt(3.f * S));
		float TotalW = 0.f;
		TArray<float> Widths;
		for (int32 i = 0; i < PageCount; ++i)
		{
			Widths.Add(FMath::CeilToFloat((MeasurePixelText(FString(Tabs[i]).ToUpper(), TabScale).X + 16.f * P) / P) * P);
			TotalW += Widths.Last() + 3.f * P;
		}
		float TabX = FMath::RoundToFloat((Screen.X + Screen.W * 0.5f - TotalW * 0.5f) / P) * P;
		const float TabY = Plaque.Max.Y + 3.f * P;
		const float TabH = 7.f * TabScale + 8.f * P;
		for (int32 i = 0; i < PageCount; ++i)
		{
			const bool bSelected = MenuState.Page == i;
			const bool bHover = IsMouseOver(TabX, TabY, Widths[i], TabH);
			FillRect(TabX, TabY, Widths[i], TabH, GBRpg::Stone1);
			RpgStone(TabX, TabY, Widths[i], TabH, bSelected ? FLinearColor(1.3f, 1.15f, 1.f, 1.f) : FLinearColor(0.8f, 0.8f, 0.85f, 1.f));
			RpgFrame(TabX, TabY, Widths[i], TabH, bSelected ? ERpgFrame::Gold : ERpgFrame::Iron);
			if (bSelected)
			{
				DrawGlow(TabX + Widths[i] * 0.5f, TabY + TabH, Widths[i] * 0.7f, TabH, GBAlpha(GBRpg::EmberLight, 0.18f));
			}
			FPixelTextStyle Style;
			Style.Scale = TabScale;
			Style.Ink = bSelected ? &GBRpg::InkGold : (bHover ? &GBRpg::InkBone : nullptr);
			Style.Color = bSelected || bHover ? FLinearColor::White : GBRpg::BoneDim;
			Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
			DrawPixelText(FString(Tabs[i]).ToUpper(), TabX + Widths[i] * 0.5f, TabY + 4.f * P, Style, 0.5f);
			AddMenuHit(EMenuHitKind::Tab, TabX, TabY, Widths[i], TabH, i);
			TabX += Widths[i] + 3.f * P;
		}
		float KW = 0.f;
		RpgKeyCap(FMath::RoundToFloat((Screen.X + Screen.W * 0.5f - TotalW * 0.5f) / P) * P - 13.f * P, TabY + 3.f * P, TEXT("Q"), &KW);
		RpgKeyCap(TabX + 2.f * P, TabY + 3.f * P, TEXT("E"));
		CX = Screen.X + 16.f * P;
		CY = TabY + TabH + 8.f * P;
		CW = Screen.W - 32.f * P;
	}

	TArray<TPair<FString, FString>> Hints;
	Hints.Add(TPair<FString, FString>(TEXT("Q E"), TEXT("Page")));
	Hints.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Select")));
	Hints.Add(TPair<FString, FString>(TEXT("Esc"), MenuState.bChoosingSpell || MenuState.PendingCombine != NAME_None ? TEXT("Back") : TEXT("Close")));
	if (MenuState.Page == PageSkills)
	{
		Hints.Add(TPair<FString, FString>(TEXT("R"), TEXT("Reset skills")));
	}
	MenuHints(Skin, Hints, bCyber ? Screen.X : W * 0.5f, Screen.FooterY, bCyber ? 0.f : 0.5f);

	if (!Player)
	{
		return;
	}
	const float CH = Screen.FooterY - (bCyber ? 26.f * S : 5.f * P) - CY;
	if (MenuState.Page == PageItems)
	{
		DrawItemsPage(Player, Skin, CX, CY, CW, CH);
	}
	else if (MenuState.Page == PageWeapons)
	{
		DrawWeaponsPage(Player, Skin, CX, CY, CW, CH);
	}
	else if (MenuState.Page == PageSpells)
	{
		DrawSpellsPage(Player, Skin, CX, CY, CW, CH);
	}
	else
	{
		DrawSkillTrees(Player, Skin, CX, CY, CW, CH);
	}
}

// ======================================================================== drawing: pages

void AGBHUD::DrawItemsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const UGBInventoryComponent* Inventory = Player->GetInventory();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> List = GetInventoryList(Player);
	if (!Inventory || List.Num() == 0)
	{
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Muted, TEXT("Your bag is empty."), X, Y);
		return;
	}
	MenuState.Cursor = FMath::Clamp(MenuState.Cursor, 0, List.Num() - 1);

	const float ListW = FMath::RoundToFloat(W * 0.46f);
	const float RowH = bCyber ? 48.f * S : 16.f * P;
	const float RowGap = bCyber ? 6.f * S : 2.f * P;
	const float ListX = X + (bCyber ? 0.f : 8.f * P);
	const int32 Visible = FMath::Max(1, FMath::FloorToInt(H / (RowH + RowGap)));
	const int32 First = FMath::Clamp(MenuState.Cursor - Visible + 1, 0, FMath::Max(0, List.Num() - Visible));
	if (!bCyber)
	{
		RpgWell(X - 2.f * P, Y - 3.f * P, ListW + 12.f * P, H + 3.f * P);
	}
	for (int32 i = First; i < List.Num() && i < First + Visible; ++i)
	{
		const FName ItemId = List[i];
		const FGBItemDefinition* Def = Data ? Data->FindItem(ItemId) : nullptr;
		const float RowY = Y + (i - First) * (RowH + RowGap);
		const bool bSelected = i == MenuState.Cursor;
		const float Pad = MenuRow(Skin, ListX, RowY, ListW - (bCyber ? 0.f : 8.f * P), RowH, bSelected, MenuState.PendingCombine == ItemId);
		AddMenuHit(EMenuHitKind::ItemRow, ListX, RowY, ListW, RowH, i);
		const float IconS = RowH - (bCyber ? 12.f * S : 4.f * P);
		const FString* Sprite = Def ? Def->Strings.Find(GBString::Sprite) : nullptr;
		DrawSpriteFit(Sprite ? FName(**Sprite) : NAME_None, ListX + Pad - (bCyber ? 4.f * S : 2.f * P), RowY + (RowH - IconS) * 0.5f, IconS, IconS,
			bCyber ? FLinearColor(0.9f, 1.f, 1.f, 1.f) : FLinearColor::White, bCyber);
		const float TextX = ListX + Pad + IconS + (bCyber ? 10.f * S : 3.f * P);
		const float LH = UiMeasure(Skin, EGBTextSize::Body, TEXT("Ag")).Y;
		UiText(Skin, EGBTextSize::Body, bSelected ? EGBTextTone::Selected : EGBTextTone::Normal, ItemName(Data, ItemId), TextX, RowY + FMath::RoundToFloat((RowH - LH) * 0.5f));
		// Count and tags at the right of the row.
		FString TagText;
		if (Inventory->GetEquippedArmor() == ItemId)
		{
			TagText += TEXT("EQUIPPED  ");
		}
		if (Inventory->GetQuickUseItem() == ItemId)
		{
			TagText += TEXT("[F]  ");
		}
		const float RightX = ListX + ListW - (bCyber ? 16.f * S : 14.f * P);
		const FVector2D CountSize = UiText(Skin, EGBTextSize::Body, EGBTextTone::Value, FString::Printf(TEXT("x%d"), Inventory->GetCount(ItemId)), RightX, RowY + FMath::RoundToFloat((RowH - LH) * 0.5f), 1.f);
		if (!TagText.IsEmpty())
		{
			const float SH = UiMeasure(Skin, EGBTextSize::Small, TEXT("A")).Y;
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Warn, TagText.TrimEnd(), RightX - CountSize.X - (bCyber ? 14.f * S : 4.f * P), RowY + FMath::RoundToFloat((RowH - SH) * 0.5f), 1.f);
		}
	}

	// Details of the selected item.
	const FName Selected = List[MenuState.Cursor];
	const FGBItemDefinition* Def = Data ? Data->FindItem(Selected) : nullptr;
	const float DX = X + ListW + (bCyber ? 40.f * S : 14.f * P);
	const float DW = X + W - DX;
	const FLinearColor TypeColor = ToneColor(Skin, EGBTextTone::Value);
	float DY = MenuDetailPanel(Skin, DX, Y - (bCyber ? 0.f : 3.f * P), DW, H + (bCyber ? 0.f : 3.f * P), ItemName(Data, Selected), Def ? ItemTypeName(Def->Type) : FString(), &TypeColor);
	const float Inner = bCyber ? 24.f * S : 7.f * P;
	if (!Def)
	{
		return;
	}
	// Large icon on the right of the panel.
	const float Big = bCyber ? 168.f * S : 40.f * P;
	const FString* Sprite = Def->Strings.Find(GBString::Sprite);
	if (bCyber)
	{
		CyBox(DX + DW - Inner - Big, DY, Big, Big, 14.f * S, GBAlpha(GBCy::CyanDeep, 0.35f), GBAlpha(GBCy::CyanDim, 0.9f), 0x5);
	}
	else
	{
		RpgFrame(DX + DW - Inner - Big, DY, Big, Big, ERpgFrame::Gold);
		FillRect(DX + DW - Inner - Big + 3.f * P, DY + 3.f * P, Big - 6.f * P, Big - 6.f * P, GBRpg::StoneDeep);
		DrawGlow(DX + DW - Inner - Big * 0.5f, DY + Big * 0.5f, Big * 0.45f, Big * 0.45f, GBAlpha(GBRpg::EmberLight, 0.15f));
	}
	DrawSpriteFit(Sprite ? FName(**Sprite) : NAME_None, DX + DW - Inner - Big + Big * 0.15f, DY + Big * 0.15f, Big * 0.7f, Big * 0.7f,
		bCyber ? FLinearColor(0.9f, 1.f, 1.f, 1.f) : FLinearColor::White, bCyber);
	const float TextW = DW - 3.f * Inner - Big;
	DY += UiWrapped(Skin, EGBTextSize::Body, EGBTextTone::Normal, Def->Description, DX + Inner, DY, TextW) + (bCyber ? 20.f * S : 6.f * P);
	DY = FMath::Max(DY, DY);

	TArray<TPair<FString, FString>> Actions;
	switch (Def->Type)
	{
	case EGBItemType::Consumable: Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Use"))); break;
	case EGBItemType::Throwable: Actions.Add(TPair<FString, FString>(TEXT("F"), TEXT("Throw (quick-use)"))); Actions.Add(TPair<FString, FString>(TEXT("Shift"), TEXT("Cycle"))); break;
	case EGBItemType::Armor: Actions.Add(TPair<FString, FString>(TEXT("Enter"), FString(Inventory->GetEquippedArmor() == Selected ? TEXT("Unequip") : TEXT("Equip")))); break;
	case EGBItemType::Material:
	case EGBItemType::KeyItem:
		if (MenuState.PendingCombine == NAME_None)
		{
			Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Combine with another item")));
		}
		else if (MenuState.PendingCombine == Selected)
		{
			Actions.Add(TPair<FString, FString>(TEXT("Esc"), TEXT("Cancel (choose a second item to combine)")));
		}
		else
		{
			Actions.Add(TPair<FString, FString>(TEXT("Enter"), FString::Printf(TEXT("Combine with %s"), *ItemName(Data, MenuState.PendingCombine))));
		}
		break;
	default: break;
	}
	const float HintY = FMath::Max(DY, Y + Big + (bCyber ? 60.f * S : 18.f * P));
	MenuHints(Skin, Actions, DX + Inner, HintY, 0.f);
}

void AGBHUD::DrawWeaponsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	const UGBEquipmentComponent* Equipment = Player->GetEquipment();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const TArray<FName> List = GetWeaponList(Player);
	if (!Equipment || List.Num() == 0)
	{
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Muted, TEXT("You carry no weapons."), X, Y);
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

	const float ListW = FMath::RoundToFloat(W * 0.46f);
	const float ListX = X + (bCyber ? 0.f : 8.f * P);
	const float RowH = bCyber ? 44.f * S : 14.f * P;
	const float HeaderH = bCyber ? 40.f * S : 13.f * P;
	if (!bCyber)
	{
		RpgWell(X - 2.f * P, Y - 3.f * P, ListW + 12.f * P, H + 3.f * P);
	}
	float RowY = Y;
	const int32 Visible = FMath::Max(1, FMath::FloorToInt(H / RowH));
	const int32 First = FMath::Clamp(CursorRow - Visible + 1, 0, FMath::Max(0, Rows.Num() - Visible));
	for (int32 r = First; r < Rows.Num() && RowY + RowH <= Y + H + 1.f; ++r)
	{
		const FRow& Row = Rows[r];
		if (Row.WeaponIndex < 0)
		{
			const float HY = RowY + (bCyber ? 10.f * S : 4.f * P);
			if (bCyber)
			{
				FCyTextStyle Head;
				Head.Face = ECyFace::Mono;
				Head.Px = 14.f;
				Head.Color = GBCy::RedDim;
				const FVector2D HS = DrawCyText(FString::Printf(TEXT("// %s"), *Row.Text.ToUpper()), ListX, HY, Head, 0.f);
				FillRect(ListX + HS.X + 10.f * S, HY + HS.Y * 0.5f, ListW - HS.X - 10.f * S, 1.f, GBAlpha(GBCy::RedDeep, 1.f));
			}
			else
			{
				const FVector2D HS = UiText(Skin, EGBTextSize::Small, EGBTextTone::Accent, Row.Text.ToUpper(), ListX + 2.f * P, HY);
				FillRect(ListX + HS.X + 6.f * P, HY + 3.f * P, ListW - HS.X - 18.f * P, P, GBRpg::Gold1);
			}
			RowY += HeaderH;
			continue;
		}
		const FName WeaponId = List[Row.WeaponIndex];
		const bool bSelected = Row.WeaponIndex == MenuState.Cursor;
		const float RowWidth = ListW - (bCyber ? 0.f : 8.f * P);
		const float Pad = MenuRow(Skin, ListX, RowY, RowWidth, RowH - (bCyber ? 4.f * S : P), bSelected);
		AddMenuHit(EMenuHitKind::WeaponRow, ListX, RowY, RowWidth, RowH, Row.WeaponIndex);
		const bool bEquipped = Equipment->IsWeaponEquipped(WeaponId);
		const float LH = UiMeasure(Skin, EGBTextSize::Body, TEXT("Ag")).Y;
		const float TY = RowY + FMath::RoundToFloat((RowH - (bCyber ? 4.f * S : P) - LH) * 0.5f);
		const FVector2D NS = UiText(Skin, EGBTextSize::Body, bSelected ? EGBTextTone::Selected : EGBTextTone::Normal, Row.Text, ListX + Pad, TY);
		if (bEquipped)
		{
			const float SH = UiMeasure(Skin, EGBTextSize::Small, TEXT("A")).Y;
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Value, TEXT("EQUIPPED"), ListX + Pad + NS.X + (bCyber ? 14.f * S : 4.f * P), RowY + FMath::RoundToFloat((RowH - SH) * 0.5f));
		}
		const EGBElement Element = Equipment->GetWeaponElement(WeaponId);
		const FLinearColor EC = GBHudText::ElementColor(Element);
		const float RightX = ListX + RowWidth - (bCyber ? 16.f * S : 6.f * P);
		if (bCyber)
		{
			FillRect(RightX - 6.f * S, RowY + 12.f * S, 6.f * S, RowH - 28.f * S, EC);
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, GBHudText::ElementName(Element).ToUpper(), RightX - 14.f * S, TY + 3.f * S, 1.f, &EC);
		}
		else
		{
			RpgGem(RightX - 2.f * P, RowY + (RowH - P) * 0.5f, EC, true);
			const float SH = UiMeasure(Skin, EGBTextSize::Small, TEXT("A")).Y;
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, GBHudText::ElementName(Element), RightX - 8.f * P, RowY + FMath::RoundToFloat((RowH - P - SH) * 0.5f), 1.f, &EC);
		}
		RowY += RowH;
	}

	// Details of the selected weapon.
	const FName Selected = List[MenuState.Cursor];
	const FGBWeaponDefinition* Def = Data ? Data->FindWeapon(Selected) : nullptr;
	const float DX = X + ListW + (bCyber ? 40.f * S : 14.f * P);
	const float DW = X + W - DX;
	const float Inner = bCyber ? 24.f * S : 7.f * P;
	const EGBElement Element = Equipment->GetWeaponElement(Selected);
	const FLinearColor EC = GBHudText::ElementColor(Element);
	const FString Subtitle = Def ? FString::Printf(TEXT("%s  -  %s"), *SlotName(Def->Slot, false), *GBHudText::ElementName(Element)) : FString();
	float DY = MenuDetailPanel(Skin, DX, Y - (bCyber ? 0.f : 3.f * P), DW, H + (bCyber ? 0.f : 3.f * P), WeaponName(Data, Selected), Subtitle, &EC);
	if (!Def)
	{
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Muted, TEXT("Unknown weapon"), DX + Inner, DY);
		return;
	}
	// Stats as label / value lines.
	const float LineH = UiLineHeight(Skin, EGBTextSize::Body) + (bCyber ? 4.f * S : P);
	for (const FString& Line : WeaponStatLines(*Def))
	{
		// "Damage 10 x8" -> label "Damage", value "10 x8": split at the first digit.
		int32 Split = INDEX_NONE;
		for (int32 c = 0; c < Line.Len(); ++c)
		{
			if (FChar::IsDigit(Line[c]))
			{
				Split = c;
				break;
			}
		}
		const FString Label = Split == INDEX_NONE ? Line : Line.Left(Split).TrimEnd();
		const FString Value = Split == INDEX_NONE ? FString() : Line.Mid(Split);
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Muted, Label, DX + Inner, DY);
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Value, Value, DX + DW - Inner, DY, 1.f);
		if (bCyber)
		{
			FillRect(DX + Inner, DY + LineH - 6.f * S, DW - 2.f * Inner, 1.f, GBAlpha(GBCy::RedDeep, 1.f));
		}
		else
		{
			FillRect(DX + Inner, DY + LineH - 3.f * P, DW - 2.f * Inner, P, GBAlpha(GBRpg::Stone2, 0.9f));
		}
		DY += LineH;
	}
	DY += bCyber ? 18.f * S : 5.f * P;
	if (Equipment->IsWeaponEquipped(Selected))
	{
		UiText(Skin, EGBTextSize::Small, EGBTextTone::Value, FString::Printf(TEXT("Equipped in the %s slot"), *SlotName(Def->Slot, false).ToLower()), DX + Inner, DY);
	}
	else
	{
		const AGBHeldItem* Current = Equipment->GetItemInSlot(Def->Slot);
		TArray<TPair<FString, FString>> Actions;
		Actions.Add(TPair<FString, FString>(TEXT("Enter"), Current ? FString::Printf(TEXT("Equip (replaces %s)"), *Current->GetDisplayName()) : FString(TEXT("Equip"))));
		MenuHints(Skin, Actions, DX + Inner, DY, 0.f);
	}
}

void AGBHUD::DrawSpellsPage(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;
	UGBSpellbookComponent* Spellbook = Player->GetSpellbook();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Spellbook)
	{
		return;
	}
	const int32 NumSlots = UGBSpellbookComponent::NumSlots;
	const float ListW = FMath::RoundToFloat(W * 0.46f);
	const float ListX = X + (bCyber ? 0.f : 8.f * P);
	const float RowH = bCyber ? 52.f * S : 17.f * P;
	const float RowGap = bCyber ? 6.f * S : 2.f * P;
	if (!bCyber)
	{
		RpgWell(X - 2.f * P, Y - 3.f * P, ListW + 12.f * P, H + 3.f * P);
	}
	const FVector2D HeadSize = UiText(Skin, EGBTextSize::Small, EGBTextTone::Accent, TEXT("LOADOUT"), ListX + (bCyber ? 0.f : 2.f * P), Y + (bCyber ? 0.f : 2.f * P));
	const float ListY = Y + HeadSize.Y + (bCyber ? 14.f * S : 6.f * P);
	for (int32 i = 0; i <= NumSlots; ++i)
	{
		const float RowY = ListY + i * (RowH + RowGap);
		const float RowWidth = ListW - (bCyber ? 0.f : 8.f * P);
		const bool bSelected = i == MenuState.Cursor;
		const float Pad = MenuRow(Skin, ListX, RowY, RowWidth, RowH, bSelected && !MenuState.bChoosingSpell, bSelected && MenuState.bChoosingSpell);
		AddMenuHit(EMenuHitKind::SpellSlot, ListX, RowY, RowWidth, RowH, i);
		const FName SpellId = i < NumSlots ? Spellbook->GetSlotSpell(i) : Spellbook->GetEnchantSpell();
		const FString Key = i < NumSlots ? FString(SlotKey(i)) : FString(TEXT("T"));
		// Key, icon, name.
		float KX = ListX + Pad;
		if (bCyber)
		{
			KX += CyKey(KX, RowY + (RowH - 24.f * S) * 0.5f, Key, GBCy::Cyan, 24.f) + 12.f * S;
		}
		else
		{
			float KW = 0.f;
			RpgKeyCap(KX, RowY + FMath::RoundToFloat((RowH - 11.f * P) * 0.5f / P) * P, Key, &KW);
			KX += KW + 3.f * P;
		}
		const float IconS = RowH - (bCyber ? 14.f * S : 5.f * P);
		if (SpellId != NAME_None)
		{
			if (bCyber)
			{
				CyBox(KX, RowY + (RowH - IconS) * 0.5f, IconS, IconS, 6.f * S, GBAlpha(GBCy::Ink, 0.8f), GBAlpha(GBCy::RedDim, 1.f), 0x4);
			}
			else
			{
				FillRect(KX, RowY + (RowH - IconS) * 0.5f, IconS, IconS, GBRpg::StoneDeep);
				DrawOutline(KX, RowY + (RowH - IconS) * 0.5f, IconS, IconS, GBRpg::Gold1, P);
			}
			DrawSpriteFit(GBHudText::SpellIconId(SpellId), KX + 2.f, RowY + (RowH - IconS) * 0.5f + 2.f, IconS - 4.f, IconS - 4.f);
		}
		KX += IconS + (bCyber ? 12.f * S : 3.f * P);
		const float LH = UiMeasure(Skin, EGBTextSize::Body, TEXT("Ag")).Y;
		FString Name;
		if (i < NumSlots)
		{
			Name = SpellId == NAME_None ? FString(TEXT("(empty)")) : GBHudText::SpellName(this, SpellId);
		}
		else
		{
			Name = FString::Printf(TEXT("Enchant: %s"), *GBHudText::ElementName(Spellbook->GetEnchantElement()));
		}
		UiText(Skin, EGBTextSize::Body, SpellId == NAME_None && i < NumSlots ? EGBTextTone::Disabled : (bSelected ? EGBTextTone::Selected : EGBTextTone::Normal), Name, KX, RowY + FMath::RoundToFloat((RowH - LH) * 0.5f));
		if (i == NumSlots)
		{
			const FLinearColor EC = GBHudText::ElementColor(Spellbook->GetEnchantElement());
			FillRect(ListX + RowWidth - (bCyber ? 10.f * S : 4.f * P), RowY + (bCyber ? 8.f * S : 3.f * P), bCyber ? 4.f * S : P, RowH - (bCyber ? 16.f * S : 6.f * P), EC);
		}
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
	float PassY = ListY + (NumSlots + 1) * (RowH + RowGap) + (bCyber ? 18.f * S : 5.f * P);
	PassY += UiText(Skin, EGBTextSize::Small, EGBTextTone::Accent, TEXT("PASSIVES (ALWAYS ACTIVE)"), ListX + (bCyber ? 0.f : 2.f * P), PassY).Y + (bCyber ? 10.f * S : 4.f * P);
	UiWrapped(Skin, EGBTextSize::Small, EGBTextTone::Normal, Passives.IsEmpty() ? FString(TEXT("None yet")) : Passives, ListX + (bCyber ? 0.f : 2.f * P), PassY, ListW - 12.f * P);

	// Right side: the spell chooser, or details of the selected slot.
	const float DX = X + ListW + (bCyber ? 40.f * S : 14.f * P);
	const float DW = X + W - DX;
	const float Inner = bCyber ? 24.f * S : 7.f * P;
	const float PanelY = Y - (bCyber ? 0.f : 3.f * P);
	const float PanelH = H + (bCyber ? 0.f : 3.f * P);
	if (MenuState.bChoosingSpell)
	{
		const TArray<FName> Options = GetAssignableSpells(Player);
		MenuState.SubCursor = FMath::Clamp(MenuState.SubCursor, 0, FMath::Max(0, Options.Num() - 1));
		float DY = MenuDetailPanel(Skin, DX, PanelY, DW, PanelH, FString::Printf(TEXT("Spell for slot %s"), SlotKey(MenuState.Cursor)), TEXT("Choose a spell"), nullptr);
		const float OptH = bCyber ? 46.f * S : 13.f * P;
		const float DescReserve = bCyber ? 110.f * S : 34.f * P;
		const int32 Visible = FMath::Max(1, FMath::FloorToInt((PanelY + PanelH - DY - DescReserve) / OptH));
		const int32 First = FMath::Clamp(MenuState.SubCursor - Visible + 1, 0, FMath::Max(0, Options.Num() - Visible));
		for (int32 i = First; i < Options.Num() && i < First + Visible; ++i)
		{
			const FName SpellId = Options[i];
			const bool bSel = i == MenuState.SubCursor;
			const float RX = DX + Inner + (bCyber ? 0.f : 6.f * P);
			const float RW = DW - 2.f * Inner - (bCyber ? 0.f : 6.f * P);
			const float Pad = MenuRow(Skin, RX, DY, RW, OptH - (bCyber ? 4.f * S : P), bSel);
			AddMenuHit(EMenuHitKind::SpellChoice, RX, DY, RW, OptH, i);
			FString Text = SpellId == NAME_None ? FString(TEXT("(empty the slot)")) : GBHudText::SpellName(this, SpellId);
			for (int32 Slot = 0; Slot < NumSlots; ++Slot)
			{
				if (SpellId != NAME_None && Spellbook->GetSlotSpell(Slot) == SpellId)
				{
					Text += FString::Printf(TEXT("  (in %s)"), SlotKey(Slot));
				}
			}
			const float IconS = bCyber ? 34.f * S : OptH - 5.f * P;
			DrawSpriteFit(GBHudText::SpellIconId(SpellId), RX + Pad, DY + (OptH - IconS) * 0.5f - (bCyber ? 2.f * S : 0.f), IconS, IconS);
			const float LH = UiMeasure(Skin, EGBTextSize::Body, TEXT("Ag")).Y;
			UiText(Skin, EGBTextSize::Body, bSel ? EGBTextTone::Selected : EGBTextTone::Normal, Text, RX + Pad + IconS + (bCyber ? 10.f * S : 3.f * P), DY + FMath::RoundToFloat((OptH - (bCyber ? 4.f * S : P) - LH) * 0.5f));
			DY += OptH;
		}
		const FName Hovered = Options.IsValidIndex(MenuState.SubCursor) ? Options[MenuState.SubCursor] : NAME_None;
		const FGBSpellDefinition* Def = (Data && Hovered != NAME_None) ? Data->FindSpell(Hovered) : nullptr;
		if (Def)
		{
			UiWrapped(Skin, EGBTextSize::Small, EGBTextTone::Normal, Def->Description, DX + Inner, PanelY + PanelH - DescReserve + (bCyber ? 10.f * S : 3.f * P), DW - 2.f * Inner);
		}
		return;
	}

	if (MenuState.Cursor < NumSlots)
	{
		const FName SpellId = Spellbook->GetSlotSpell(MenuState.Cursor);
		const FGBSpellDefinition* Def = (Data && SpellId != NAME_None) ? Data->FindSpell(SpellId) : nullptr;
		if (Def)
		{
			const FLinearColor EC = GBHudText::ElementColor(Def->Element);
			float DY = MenuDetailPanel(Skin, DX, PanelY, DW, PanelH, GBHudText::SpellName(this, SpellId),
				FString::Printf(TEXT("%s  -  %d mana"), *GBHudText::ElementName(Def->Element), FMath::RoundToInt(Spellbook->GetManaCost(SpellId))), &EC);
			const float Big = bCyber ? 112.f * S : 30.f * P;
			if (bCyber)
			{
				CyBox(DX + DW - Inner - Big, DY, Big, Big, 12.f * S, GBAlpha(GBCy::Ink, 0.7f), GBAlpha(EC, 0.9f), 0x5);
			}
			else
			{
				RpgFrame(DX + DW - Inner - Big, DY, Big, Big, ERpgFrame::Gold);
				FillRect(DX + DW - Inner - Big + 3.f * P, DY + 3.f * P, Big - 6.f * P, Big - 6.f * P, GBRpg::StoneDeep);
			}
			DrawGlow(DX + DW - Inner - Big * 0.5f, DY + Big * 0.5f, Big * 0.5f, Big * 0.5f, GBAlpha(EC, 0.25f));
			DrawSpriteFit(GBHudText::SpellIconId(SpellId), DX + DW - Inner - Big + Big * 0.15f, DY + Big * 0.15f, Big * 0.7f, Big * 0.7f);
			DY += UiWrapped(Skin, EGBTextSize::Body, EGBTextTone::Normal, Def->Description, DX + Inner, DY, DW - 3.f * Inner - Big);
			TArray<TPair<FString, FString>> Actions;
			Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Choose a spell")));
			MenuHints(Skin, Actions, DX + Inner, FMath::Max(DY + (bCyber ? 20.f * S : 6.f * P), PanelY + Big + (bCyber ? 150.f * S : 40.f * P)), 0.f);
		}
		else
		{
			const float DY = MenuDetailPanel(Skin, DX, PanelY, DW, PanelH, TEXT("Empty slot"), FString(), nullptr);
			TArray<TPair<FString, FString>> Actions;
			Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Choose a spell")));
			MenuHints(Skin, Actions, DX + Inner, DY, 0.f);
		}
		return;
	}
	const FName EnchantId = Spellbook->GetEnchantSpell();
	const EGBElement Element = Spellbook->GetEnchantElement();
	const FLinearColor EC = GBHudText::ElementColor(Element);
	float DY = MenuDetailPanel(Skin, DX, PanelY, DW, PanelH, EnchantId != NAME_None ? GBHudText::SpellName(this, EnchantId) : FString(TEXT("Weapon enchant")),
		FString::Printf(TEXT("Element: %s"), *GBHudText::ElementName(Element)), &EC);
	if (const FGBSpellDefinition* Def = (Data && EnchantId != NAME_None) ? Data->FindSpell(EnchantId) : nullptr)
	{
		DY += UiWrapped(Skin, EGBTextSize::Body, EGBTextTone::Normal, Def->Description, DX + Inner, DY, DW - 2.f * Inner) + (bCyber ? 12.f * S : 4.f * P);
	}
	DY += UiWrapped(Skin, EGBTextSize::Small, EGBTextTone::Muted, TEXT("Only with a gun in hand (T). Enchants the gun with the chosen element."), DX + Inner, DY, DW - 2.f * Inner) + (bCyber ? 16.f * S : 5.f * P);
	// The three elements as swatches, the chosen one lit.
	const EGBElement Choices[3] = { EGBElement::Fire, EGBElement::Lightning, EGBElement::Ice };
	float SX = DX + Inner;
	for (const EGBElement Choice : Choices)
	{
		const bool bOn = Choice == Element;
		const FLinearColor C = GBHudText::ElementColor(Choice);
		const FString Name = GBHudText::ElementName(Choice);
		if (bCyber)
		{
			const float SW = 140.f * S;
			CyBox(SX, DY, SW, 40.f * S, 8.f * S, bOn ? GBAlpha(C, 0.25f) : GBAlpha(GBCy::Ink, 0.6f), bOn ? C : GBAlpha(C, 0.35f), 0x4);
			UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, Name.ToUpper(), SX + SW * 0.5f, DY + 12.f * S, 0.5f, bOn ? &C : &GBCy::Grey);
			SX += SW + 10.f * S;
		}
		else
		{
			RpgGem(SX + 4.f * P, DY + 4.f * P, C, bOn);
			const FLinearColor TextColor = bOn ? C : GBRpg::BoneDark;
			const FVector2D NS = UiText(Skin, EGBTextSize::Small, EGBTextTone::Normal, Name, SX + 10.f * P, DY + 2.f * P, 0.f, &TextColor);
			SX += NS.X + 18.f * P;
		}
	}
	DY += bCyber ? 60.f * S : 14.f * P;
	TArray<TPair<FString, FString>> Actions;
	Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Change element")));
	MenuHints(Skin, Actions, DX + Inner, DY, 0.f);
}

// ======================================================================== drawing: skill trees

void AGBHUD::DrawSkillNode(AGBPlayerCharacter* Player, int32 Tree, FName NodeId, float X, float Y, float W, float H)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree();
	const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(NodeId) : nullptr;
	const int32 Rank = SkillTree ? SkillTree->GetRank(NodeId) : 0;
	const int32 MaxRank = Def ? FMath::Max(1, Def->MaxRank) : 1;
	const bool bCan = SkillTree && SkillTree->CanUnlock(NodeId) == EGBSkillUnlockResult::Ok;
	const bool bSelected = NodeId == MenuState.SkillCursor;
	const bool bMax = Rank >= MaxRank;
	const FString Name = GBHudMenuLocal::SkillName(Data, NodeId);
	FString LineA;
	FString LineB;
	const float S = UIScale();
	const float P = ArtPixel();

	if (Tree == 1)
	{
		// Cyberpunk node: cut-corner box, condensed caps, rank segments, cost tag.
		const FLinearColor Line = bSelected ? GBCy::Cyan : (bMax ? GBCy::Red : (Rank > 0 ? GBCy::Red : (bCan ? GBAlpha(GBCy::Cyan, 0.75f) : GBAlpha(GBCy::Grey, 0.6f))));
		const FLinearColor Fill = bMax ? GBAlpha(GBCy::Red, 0.28f) : (Rank > 0 ? GBAlpha(GBCy::RedDeep, 0.85f) : GBAlpha(GBCy::Ink, 0.85f));
		if (bSelected)
		{
			DrawGlow(X + W * 0.5f, Y + H * 0.5f, W * 0.75f, H, GBAlpha(GBCy::Cyan, 0.16f));
		}
		CyBox(X, Y, W, H, 10.f * S, Fill, Line, 0x5, bSelected ? 2.f : 1.f);
		if (bSelected)
		{
			CyBrackets(X - 5.f * S, Y - 5.f * S, W + 10.f * S, H + 10.f * S, 9.f * S, GBCy::Cyan, 2.f * S);
		}
		FCyTextStyle NameStyle;
		NameStyle.Face = ECyFace::Condensed;
		NameStyle.Px = 15.f;
		NameStyle.Spacing = 60;
		NameStyle.bUpper = true;
		NameStyle.Color = bSelected ? GBCy::Pale : (Rank > 0 || bCan ? GBCy::RedBright : GBCy::Grey);
		const float TextW = W - 16.f * S;
		if (MeasureCyText(Name, NameStyle).X > TextW)
		{
			GBHudMenuDraw::SplitName(Name, LineA, LineB);
		}
		else
		{
			LineA = Name;
		}
		const float LH = MeasureCyText(TEXT("A"), NameStyle).Y;
		const float TextY = Y + (LineB.IsEmpty() ? H * 0.5f - LH * 0.85f : H * 0.5f - LH * 1.3f);
		DrawCyText(LineA, X + W * 0.5f, TextY, NameStyle, 0.5f);
		if (!LineB.IsEmpty())
		{
			DrawCyText(LineB, X + W * 0.5f, TextY + LH * 0.92f, NameStyle, 0.5f);
		}
		// Rank segments.
		const float SegW = 14.f * S;
		const float SegGap = 4.f * S;
		const float SegsW = MaxRank * SegW + (MaxRank - 1) * SegGap;
		const float SegY = Y + H - 12.f * S;
		for (int32 i = 0; i < MaxRank; ++i)
		{
			FillRect(X + (W - SegsW) * 0.5f + i * (SegW + SegGap), SegY, SegW, 3.f * S, i < Rank ? GBCy::Red : GBAlpha(GBCy::Grey, 0.35f));
		}
		if (Def && Def->Cost > 1)
		{
			FCyTextStyle Cost;
			Cost.Face = ECyFace::Mono;
			Cost.Px = 11.f;
			Cost.Color = GBCy::Yellow;
			DrawCyText(FString::Printf(TEXT("%dPT"), Def->Cost), X + W - 8.f * S, Y + 3.f * S, Cost, 1.f);
		}
		return;
	}

	// RPG node: a stone tablet in a gold (or iron) frame, pixel name, gems for ranks.
	const float NX = FMath::RoundToFloat(X / P) * P;
	const float NY = FMath::RoundToFloat(Y / P) * P;
	const float NW = FMath::RoundToFloat(W / P) * P;
	const float NH = FMath::RoundToFloat(H / P) * P;
	if (bSelected)
	{
		DrawGlow(NX + NW * 0.5f, NY + NH * 0.5f, NW * 0.8f, NH * 1.1f, GBAlpha(GBRpg::EmberLight, 0.30f));
	}
	FillRect(NX, NY, NW, NH, GBRpg::Stone1);
	RpgStone(NX, NY, NW, NH, bMax ? FLinearColor(1.25f, 1.1f, 0.9f, 1.f) : (Rank > 0 || bCan ? FLinearColor::White : FLinearColor(0.6f, 0.6f, 0.65f, 1.f)));
	if (Rank > 0)
	{
		FillRect(NX, NY, NW, NH, GBAlpha(GBRpg::Gold1, bMax ? 0.35f : 0.18f));
	}
	RpgFrame(NX, NY, NW, NH, (Rank > 0 || bCan || bSelected) ? ERpgFrame::Gold : ERpgFrame::Inset);
	if (bSelected)
	{
		DrawOutline(NX - P, NY - P, NW + 2.f * P, NH + 2.f * P, GBRpg::Gold4, P);
	}
	const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
	FPixelTextStyle NameStyle;
	NameStyle.Scale = Small;
	NameStyle.Ink = bSelected || Rank > 0 ? &GBRpg::InkGold : (bCan ? &GBRpg::InkBone : nullptr);
	NameStyle.Color = (bSelected || Rank > 0 || bCan) ? FLinearColor::White : GBRpg::BoneDark;
	NameStyle.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
	// Text area: between the top frame (4 art pixels) and the pip row (6 from the bottom).
	const float LineH = 8.f * Small;
	const float AreaTop = NY + 4.f * P;
	const float AreaH = NH - 10.f * P;
	const float TextMaxW = NW - 6.f * P;
	if (MeasurePixelText(Name, Small).X <= TextMaxW)
	{
		LineA = Name;
	}
	else if (AreaH >= LineH + 7.f * Small)
	{
		GBHudMenuDraw::SplitName(Name, LineA, LineB);
	}
	else
	{
		// Too short for two lines (small screens): one abbreviated line; the detail panel shows the full name.
		const int32 MaxChars = FMath::Max(4, FMath::FloorToInt(TextMaxW / (6.f * Small)));
		LineA = GBHudText::Abbreviate(Name, MaxChars);
	}
	const float BlockH = LineB.IsEmpty() ? 7.f * Small : LineH + 7.f * Small;
	const float TextY = FMath::RoundToFloat((AreaTop + (AreaH - BlockH) * 0.5f) / Small) * Small;
	DrawPixelText(LineA, NX + NW * 0.5f, TextY, NameStyle, 0.5f);
	if (!LineB.IsEmpty())
	{
		DrawPixelText(LineB, NX + NW * 0.5f, TextY + LineH, NameStyle, 0.5f);
	}
	// Rank pips: tiny gems along the bottom.
	const float PipW = 4.f * P;
	const float PipGap = 2.f * P;
	const float PipsW = MaxRank * PipW + (MaxRank - 1) * PipGap;
	const float PipY = NY + NH - 5.f * P;
	for (int32 i = 0; i < MaxRank; ++i)
	{
		const float PX = FMath::RoundToFloat((NX + (NW - PipsW) * 0.5f) / P) * P + i * (PipW + PipGap);
		FillRect(PX, PipY, PipW, 2.f * P, GBRpg::Outline);
		FillRect(PX + P, PipY, PipW - 2.f * P, P, i < Rank ? GBRpg::Gold4 : GBRpg::Iron1);
	}
	if (Def && Def->Cost > 1)
	{
		FPixelTextStyle Cost;
		Cost.Scale = Small;
		Cost.Ink = &GBRpg::InkEmber;
		Cost.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
		// A small cost badge hanging on the bottom-right corner of the tablet (only for nodes that cost more than 1).
		const FString CostText = FString::Printf(TEXT("%dpt"), Def->Cost);
		const FVector2D CS = MeasurePixelText(CostText, Small);
		const float BW = FMath::CeilToFloat((CS.X + 4.f * P) / P) * P;
		const float BH = FMath::CeilToFloat((CS.Y + 4.f * P) / P) * P;
		const float BX = NX + NW - BW + 2.f * P;
		const float BY = NY + NH - FMath::RoundToFloat(BH * 0.6f / P) * P;
		FillRect(BX, BY, BW, BH, GBRpg::Outline);
		FillRect(BX + P, BY + P, BW - 2.f * P, BH - 2.f * P, GBRpg::Gold0);
		FillRect(BX + P, BY + P, BW - 2.f * P, P, GBRpg::Gold1);
		DrawPixelText(CostText, BX + BW * 0.5f, BY + 2.f * P, Cost, 0.5f);
	}
}

void AGBHUD::DrawSkillTrees(AGBPlayerCharacter* Player, EGBHudSkin Skin, float X, float Y, float W, float H)
{
	using namespace GBHudMenuLocal;
	const float S = UIScale();
	const float P = ArtPixel();
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const UGBSkillTreeComponent* SkillTree = Player->GetSkillTree();
	if (!SkillTree)
	{
		return;
	}
	const TArray<FSkillNodeRef> Nodes = GetSkillNodes();
	const bool bCyber = Skin == EGBHudSkin::Cyberpunk;

	// Top row: points in the middle, "+1 skill point" (test) on the left, reset on the right.
	{
		const int32 Points = SkillTree->GetPoints();
		const FString PointsText = FString::Printf(TEXT("%d"), Points);
		const float RowY = Y;
		const float BtnH = bCyber ? 36.f * S : 13.f * P;
		const float CX = X + W * 0.5f;
		const FVector2D LabelSize = UiMeasure(Skin, EGBTextSize::Small, TEXT("SKILL POINTS"));
		const FVector2D NumSize = UiMeasure(Skin, EGBTextSize::Heading, PointsText);
		const float GroupW = LabelSize.X + (bCyber ? 14.f * S : 4.f * P) + NumSize.X;
		UiText(Skin, EGBTextSize::Small, EGBTextTone::Muted, TEXT("SKILL POINTS"), CX - GroupW * 0.5f, RowY + (BtnH - LabelSize.Y) * 0.5f);
		UiText(Skin, EGBTextSize::Heading, Points > 0 ? EGBTextTone::Warn : EGBTextTone::Accent, PointsText, CX + GroupW * 0.5f - NumSize.X, RowY + (BtnH - NumSize.Y) * 0.5f);
		const int32 Spent = SkillTree->GetPointsSpent();
		const FString ResetText = FString::Printf(TEXT("Reset skills (+%d)"), Spent);
		const FString AddText(TEXT("+1 skill point"));
		const float ResetW = UiMeasure(Skin, EGBTextSize::Small, ResetText).X + (bCyber ? 40.f * S : 14.f * P);
		const float AddW = UiMeasure(Skin, EGBTextSize::Small, AddText).X + (bCyber ? 40.f * S : 14.f * P);
		const float Margin = bCyber ? 40.f * S : 12.f * P;
		const float ResetX = CX + GroupW * 0.5f + Margin;
		const float AddX = CX - GroupW * 0.5f - Margin - AddW;
		MenuButton(Skin, ResetX, RowY, ResetW, BtnH, ResetText, Spent > 0, IsMouseOver(ResetX, RowY, ResetW, BtnH));
		AddMenuHit(EMenuHitKind::SkillReset, ResetX, RowY, ResetW, BtnH);
		MenuButton(Skin, AddX, RowY, AddW, BtnH, AddText, true, IsMouseOver(AddX, RowY, AddW, BtnH));
		AddMenuHit(EMenuHitKind::SkillAddPoint, AddX, RowY, AddW, BtnH);
		Y += BtnH + (bCyber ? 18.f * S : 6.f * P);
		H -= BtnH + (bCyber ? 18.f * S : 6.f * P);
	}
	if (Nodes.Num() == 0)
	{
		UiText(Skin, EGBTextSize::Body, EGBTextTone::Muted, TEXT("No skills defined (Skills.json)."), X + W * 0.5f, Y + 20.f * S, 0.5f);
		return;
	}
	const FSkillNodeRef* Selected = Nodes.FindByPredicate([this](const FSkillNodeRef& Node) { return Node.Id == MenuState.SkillCursor; });
	if (!Selected)
	{
		MenuState.SkillCursor = Nodes[0].Id;
		Selected = &Nodes[0];
	}

	// GDD 13: the spell tree has the 90s RPG look, the weapon tree the cyberpunk look, whatever the held item.
	const float DetailH = FMath::RoundToFloat(FMath::Max(110.f * S, 26.f * P));
	const float Gap = FMath::RoundToFloat(W * 0.02f);
	const float AreaW = FMath::RoundToFloat((W - Gap) * 0.5f);
	const float AreaY = Y;
	const float AreaH = FMath::Max(100.f * S, H - DetailH - 12.f * S);
	static const TCHAR* Titles[] = { TEXT("SPELLS"), TEXT("WEAPONS") };

	for (int32 Tree = 0; Tree < 2; ++Tree)
	{
		const float AX = X + Tree * (AreaW + Gap);
		const bool bTreeCyber = Tree == 1;
		const EGBHudSkin TreeSkin = bTreeCyber ? EGBHudSkin::Cyberpunk : EGBHudSkin::Rpg90s;
		float GridTop = AreaY;
		if (bTreeCyber)
		{
			CyBox(AX, AreaY, AreaW, AreaH, 22.f * S, GBAlpha(GBCy::Ink, 0.92f), GBAlpha(GBCy::RedDim, 0.9f), 0x5);
			CyScanlines(AX, AreaY, AreaW, AreaH, 0.25f);
			DrawGlow(AX + AreaW * 0.5f, AreaY, AreaW * 0.6f, AreaH * 0.4f, GBAlpha(GBCy::Red, 0.08f));
			FCyTextStyle TitleStyle;
			TitleStyle.Face = ECyFace::Condensed;
			TitleStyle.Px = 26.f;
			TitleStyle.Spacing = 200;
			TitleStyle.bUpper = true;
			TitleStyle.Color = GBCy::Red;
			const FVector2D TS = DrawCyText(Titles[Tree], AX + 26.f * S, AreaY + 16.f * S, TitleStyle, 0.f);
			FCyTextStyle Sub;
			Sub.Face = ECyFace::Mono;
			Sub.Px = 12.f;
			Sub.Color = GBCy::RedDim;
			DrawCyText(TEXT("// FIREARMS PERKS"), AX + 26.f * S + TS.X + 14.f * S, AreaY + 26.f * S, Sub, 0.f);
			CyRuler(AX + 26.f * S, AreaY + 16.f * S + TS.Y + 8.f * S, AreaW - 52.f * S, GBAlpha(GBCy::RedDim, 0.7f));
			GridTop = AreaY + 16.f * S + TS.Y + 30.f * S;
		}
		else
		{
			const float PX = FMath::RoundToFloat(AX / P) * P;
			const float PY = FMath::RoundToFloat(AreaY / P) * P;
			const float PW = FMath::RoundToFloat(AreaW / P) * P;
			const float PH = FMath::RoundToFloat(AreaH / P) * P;
			FillRect(PX, PY, PW, PH, GBRpg::Stone1);
			RpgStone(PX, PY, PW, PH);
			DrawGlow(PX + PW * 0.5f, PY + PH * 0.35f, PW * 0.6f, PH * 0.6f, FLinearColor(0.35f, 0.2f, 0.1f, 0.12f));
			RpgFrame(PX, PY, PW, PH, ERpgFrame::Iron);
			DrawOutline(PX + 4.f * P, PY + 4.f * P, PW - 8.f * P, PH - 8.f * P, GBRpg::Gold1, P);
			const int32 HeadScale = FMath::Max(2, FMath::RoundToInt(4.f * S));
			FPixelTextStyle Head;
			Head.Scale = HeadScale;
			Head.Ink = &GBRpg::InkGold;
			Head.Outline = GBRpg::Outline;
			Head.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.7f);
			DrawPixelText(Titles[Tree], PX + PW * 0.5f, PY + 8.f * P, Head, 0.5f);
			GridTop = PY + 8.f * P + 7.f * HeadScale + 6.f * P;
		}

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
		const float Pad = bTreeCyber ? 22.f * S : 9.f * P;
		// The spell tree names its four element columns above the nodes.
		const float ColumnHeadH = bTreeCyber ? 0.f : 8.f * P;
		const float GridY = GridTop + ColumnHeadH;
		const float CellW = (AreaW - 2.f * Pad) / (MaxColumn + 1);
		const float CellH = FMath::Min(bTreeCyber ? 104.f * S : 29.f * P, (AreaY + AreaH - Pad - GridY) / (MaxRow + 1));
		const float BoxW = FMath::Max(20.f * S, CellW - (bTreeCyber ? 18.f * S : 5.f * P));
		const float BoxH = FMath::Max(20.f * S, CellH - (bTreeCyber ? 22.f * S : 4.f * P));
		auto BoxPos = [&](const FSkillNodeRef& Node)
		{
			return FVector2D(AX + Pad + Node.Column * CellW + (CellW - BoxW) * 0.5f, GridY + Node.Row * CellH);
		};
		if (!bTreeCyber)
		{
			for (int32 Column = 0; Column <= MaxColumn; ++Column)
			{
				const float ColX = AX + Pad + Column * CellW + CellW * 0.5f;
				const FLinearColor EC = GBHudText::ElementColor(GBHudMenuDraw::SpellColumnElement(Column));
				const FString Label = FString(GBHudMenuDraw::SpellColumnName(Column)).ToUpper();
				const int32 Small = FMath::Max(2, FMath::RoundToInt(P * 0.5f));
				const float LW = MeasurePixelText(Label, Small).X;
				RpgGem(ColX - LW * 0.5f - 5.f * P, GridTop + 2.5f * P, EC, true);
				FPixelTextStyle Lab;
				Lab.Scale = Small;
				Lab.Color = EC;
				Lab.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.9f);
				DrawPixelText(Label, ColX + 2.f * P, GridTop, Lab, 0.5f);
			}
		}

		// Requirement links first, under the boxes: down from the parent, across in the gap above the child, down.
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
				if (!ReqNode || ReqNode->Tree != Tree)
				{
					continue;
				}
				const FVector2D A = BoxPos(*ReqNode) + FVector2D(BoxW * 0.5f, BoxH);
				const FVector2D B = BoxPos(Node) + FVector2D(BoxW * 0.5f, 0.f);
				const bool bOwned = SkillTree->GetRank(Req) > 0;
				const float ElbowY = FMath::RoundToFloat(B.Y - (CellH - BoxH) * 0.5f);
				if (bTreeCyber)
				{
					const FLinearColor LinkColor = bOwned ? GBCy::Red : GBAlpha(GBCy::Grey, 0.45f);
					const float T = FMath::Max(1.f, FMath::RoundToFloat((bOwned ? 2.f : 1.f) * S));
					FillRect(A.X - T * 0.5f, A.Y, T, ElbowY - A.Y, LinkColor);
					FillRect(FMath::Min(A.X, B.X) - T * 0.5f, ElbowY - T * 0.5f, FMath::Abs(B.X - A.X) + T, T, LinkColor);
					FillRect(B.X - T * 0.5f, ElbowY, T, B.Y - ElbowY, LinkColor);
					FillRect(B.X - 3.f * S, B.Y - 6.f * S, 6.f * S, 6.f * S, LinkColor);
					if (bOwned)
					{
						// A data pulse running down the owned link.
						const float Len = (ElbowY - A.Y) + FMath::Abs(B.X - A.X) + (B.Y - ElbowY);
						const float D = FMath::Frac((float)GetRealTime() * 0.6f + A.X * 0.001f) * Len;
						FVector2D Dot;
						if (D < ElbowY - A.Y) { Dot = FVector2D(A.X, A.Y + D); }
						else if (D < ElbowY - A.Y + FMath::Abs(B.X - A.X)) { Dot = FVector2D(A.X + FMath::Sign(B.X - A.X) * (D - (ElbowY - A.Y)), ElbowY); }
						else { Dot = FVector2D(B.X, ElbowY + (D - (ElbowY - A.Y) - FMath::Abs(B.X - A.X))); }
						DrawGlow(Dot.X, Dot.Y, 8.f * S, 8.f * S, GBAlpha(GBCy::RedBright, 0.9f));
					}
				}
				else
				{
					// A gilt chain two art pixels wide with a dark keyline.
					const float T = 2.f * P;
					const float AX2 = FMath::RoundToFloat((A.X - P) / P) * P;
					const float BX2 = FMath::RoundToFloat((B.X - P) / P) * P;
					const float EY = FMath::RoundToFloat(ElbowY / P) * P;
					const FLinearColor Core = bOwned ? GBRpg::Gold3 : GBRpg::Iron1;
					auto Seg = [&](float SX, float SY, float SW, float SH)
					{
						FillRect(SX - P, SY - P, SW + 2.f * P, SH + 2.f * P, GBRpg::Outline);
					};
					auto SegCore = [&](float SX, float SY, float SW, float SH) { FillRect(SX, SY, SW, SH, Core); };
					Seg(AX2, A.Y, T, EY - A.Y);
					Seg(FMath::Min(AX2, BX2), EY, FMath::Abs(BX2 - AX2) + T, T);
					Seg(BX2, EY, T, B.Y - EY);
					SegCore(AX2, A.Y, T, EY - A.Y);
					SegCore(FMath::Min(AX2, BX2), EY, FMath::Abs(BX2 - AX2) + T, T);
					SegCore(BX2, EY, T, B.Y - EY);
				}
			}
		}
		for (const FSkillNodeRef& Node : Nodes)
		{
			if (Node.Tree != Tree)
			{
				continue;
			}
			const FVector2D Pos = BoxPos(Node);
			DrawSkillNode(Player, Tree, Node.Id, Pos.X, Pos.Y, BoxW, BoxH);
			AddMenuHit(EMenuHitKind::SkillNode, Pos.X, Pos.Y, BoxW, BoxH, 0, Node.Id);
		}
		(void)TreeSkin;
	}

	// Details of the selected node, in its tree's skin.
	if (Selected)
	{
		const EGBHudSkin DetailSkin = Selected->Tree == 0 ? EGBHudSkin::Rpg90s : EGBHudSkin::Cyberpunk;
		const float DY = AreaY + AreaH + 12.f * S;
		const FGBSkillNodeDefinition* Def = Data ? Data->FindSkillNode(Selected->Id) : nullptr;
		const int32 Rank = SkillTree->GetRank(Selected->Id);
		const EGBSkillUnlockResult Can = SkillTree->CanUnlock(Selected->Id);
		const FString Status = Can == EGBSkillUnlockResult::Ok ? FString() : SkillResultText(Data, Selected->Id, Can);
		const FString Meta = FString::Printf(TEXT("Rank %d / %d    Cost %d"), Rank, Def ? Def->MaxRank : 1, Def ? Def->Cost : 0);
		if (DetailSkin == EGBHudSkin::Cyberpunk)
		{
			CyBox(X, DY, W, DetailH, 16.f * S, GBAlpha(GBCy::Ink, 0.92f), GBAlpha(GBCy::Red, 0.9f), 0x5);
			FillRect(X, DY + 14.f * S, 4.f * S, DetailH - 28.f * S, GBCy::Red);
			const FVector2D NS = UiText(DetailSkin, EGBTextSize::Heading, EGBTextTone::Accent, SkillName(Data, Selected->Id), X + 26.f * S, DY + 14.f * S);
			UiText(DetailSkin, EGBTextSize::Small, EGBTextTone::Value, Meta, X + 26.f * S + NS.X + 20.f * S, DY + 24.f * S);
			if (Def)
			{
				UiWrapped(DetailSkin, EGBTextSize::Body, EGBTextTone::Normal, Def->Description, X + 26.f * S, DY + 22.f * S + NS.Y, W * 0.62f);
			}
			if (Can == EGBSkillUnlockResult::Ok)
			{
				TArray<TPair<FString, FString>> Actions;
				Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Unlock")));
				MenuHints(DetailSkin, Actions, X + W - 26.f * S, DY + DetailH - 40.f * S, 1.f);
			}
			else
			{
				UiText(DetailSkin, EGBTextSize::Small, EGBTextTone::Warn, Status.ToUpper(), X + W - 26.f * S, DY + DetailH - 38.f * S, 1.f);
			}
		}
		else
		{
			const float PX = FMath::RoundToFloat(X / P) * P;
			const float PY = FMath::RoundToFloat(DY / P) * P;
			const float PW = FMath::RoundToFloat(W / P) * P;
			const float PH = FMath::RoundToFloat(DetailH / P) * P;
			FillRect(PX, PY, PW, PH, GBRpg::Stone1);
			RpgStone(PX, PY, PW, PH);
			RpgFrame(PX, PY, PW, PH, ERpgFrame::Gold);
			const FVector2D NS = UiText(DetailSkin, EGBTextSize::Heading, EGBTextTone::Accent, SkillName(Data, Selected->Id), PX + 7.f * P, PY + 6.f * P);
			UiText(DetailSkin, EGBTextSize::Small, EGBTextTone::Muted, Meta, PX + 7.f * P + NS.X + 8.f * P, PY + 6.f * P + NS.Y - 7.f * FMath::Max(2, FMath::RoundToInt(P * 0.5f)));
			if (Def)
			{
				UiWrapped(DetailSkin, EGBTextSize::Body, EGBTextTone::Normal, Def->Description, PX + 7.f * P, PY + 10.f * P + NS.Y, PW * 0.62f);
			}
			if (Can == EGBSkillUnlockResult::Ok)
			{
				TArray<TPair<FString, FString>> Actions;
				Actions.Add(TPair<FString, FString>(TEXT("Enter"), TEXT("Unlock")));
				MenuHints(DetailSkin, Actions, PX + PW - 7.f * P, PY + PH - 15.f * P, 1.f);
			}
			else
			{
				UiText(DetailSkin, EGBTextSize::Small, EGBTextTone::Warn, Status, PX + PW - 7.f * P, PY + PH - 10.f * P, 1.f);
			}
		}
	}
}
