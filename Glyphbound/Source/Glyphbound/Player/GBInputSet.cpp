#include "Player/GBInputSet.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "InputCoreTypes.h"

namespace
{
	// Stick dead zones (input feel, not gameplay numbers).
	constexpr float GameplayStickDeadZone = 0.2f;
	constexpr float MenuStickDeadZone = 0.5f;

	/** Where a digital key pushes a 2D axis. */
	enum class EDir2D : uint8 { Right, Left, Up, Down };

	UInputAction* MakeAction(UObject* Outer, const TCHAR* Name, EInputActionValueType Type, bool bTriggerWhenPaused = false)
	{
		UInputAction* Action = NewObject<UInputAction>(Outer, MakeUniqueObjectName(Outer, UInputAction::StaticClass(), FName(Name)));
		Action->ValueType = Type;
		Action->bTriggerWhenPaused = bTriggerWhenPaused;
		return Action;
	}

	UInputModifierNegate* MakeNegate(UObject* Outer)
	{
		return NewObject<UInputModifierNegate>(Outer);
	}

	UInputModifierSwizzleAxis* MakeSwizzleYXZ(UObject* Outer)
	{
		UInputModifierSwizzleAxis* Swizzle = NewObject<UInputModifierSwizzleAxis>(Outer);
		Swizzle->Order = EInputAxisSwizzle::YXZ;
		return Swizzle;
	}

	UInputModifierDeadZone* MakeDeadZone(UObject* Outer, float Lower)
	{
		UInputModifierDeadZone* DeadZone = NewObject<UInputModifierDeadZone>(Outer);
		DeadZone->LowerThreshold = Lower;
		DeadZone->UpperThreshold = 1.f;
		DeadZone->Type = EDeadZoneType::Radial;
		return DeadZone;
	}

	// MapKey returns a reference into the context's array: every helper adds its modifiers before the next MapKey.

	void MapButton(UInputMappingContext* Context, UInputAction* Action, const FKey& Key)
	{
		Context->MapKey(Action, Key);
	}

	/** Axis1D: +1 or -1. */
	void MapAxis1D(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, bool bNegative = false)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (bNegative)
		{
			Mapping.Modifiers.Add(MakeNegate(Context));
		}
	}

	/** Axis2D from a digital key: x = right, y = up / forward. */
	void MapDir2D(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, EDir2D Dir)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		if (Dir == EDir2D::Up || Dir == EDir2D::Down)
		{
			Mapping.Modifiers.Add(MakeSwizzleYXZ(Context));
		}
		if (Dir == EDir2D::Left || Dir == EDir2D::Down)
		{
			Mapping.Modifiers.Add(MakeNegate(Context));
		}
	}

	/** Axis2D from a stick, with a radial dead zone. */
	void MapStick(UInputMappingContext* Context, UInputAction* Action, const FKey& Key, float DeadZone)
	{
		FEnhancedActionKeyMapping& Mapping = Context->MapKey(Action, Key);
		Mapping.Modifiers.Add(MakeDeadZone(Context, DeadZone));
	}

	void MapWASD(UInputMappingContext* Context, UInputAction* Action)
	{
		MapDir2D(Context, Action, EKeys::W, EDir2D::Up);
		MapDir2D(Context, Action, EKeys::S, EDir2D::Down);
		MapDir2D(Context, Action, EKeys::A, EDir2D::Left);
		MapDir2D(Context, Action, EKeys::D, EDir2D::Right);
	}
}

void UGBInputSet::Build()
{
	if (GameplayContext)
	{
		return; // built once per player
	}

	using EType = EInputActionValueType;

	// ---- Actions ----
	Move = MakeAction(this, TEXT("IA_Move"), EType::Axis2D);
	Look = MakeAction(this, TEXT("IA_Look"), EType::Axis2D);
	LookStick = MakeAction(this, TEXT("IA_LookStick"), EType::Axis2D);
	SwitchWeapon = MakeAction(this, TEXT("IA_SwitchWeapon"), EType::Axis1D);
	CycleQuickUse = MakeAction(this, TEXT("IA_CycleQuickUse"), EType::Axis1D);

	Jump = MakeAction(this, TEXT("IA_Jump"), EType::Boolean);
	Dash = MakeAction(this, TEXT("IA_Dash"), EType::Boolean);
	Primary = MakeAction(this, TEXT("IA_Primary"), EType::Boolean);
	Secondary = MakeAction(this, TEXT("IA_Secondary"), EType::Boolean);
	SpellSlot1 = MakeAction(this, TEXT("IA_SpellSlot1"), EType::Boolean);
	SpellSlot2 = MakeAction(this, TEXT("IA_SpellSlot2"), EType::Boolean);
	SpellSlot3 = MakeAction(this, TEXT("IA_SpellSlot3"), EType::Boolean);
	SpellSlot4 = MakeAction(this, TEXT("IA_SpellSlot4"), EType::Boolean);
	SpellSlot5 = MakeAction(this, TEXT("IA_SpellSlot5"), EType::Boolean);
	Enchant = MakeAction(this, TEXT("IA_Enchant"), EType::Boolean);
	Reload = MakeAction(this, TEXT("IA_Reload"), EType::Boolean);
	Interact = MakeAction(this, TEXT("IA_Interact"), EType::Boolean);
	QuickUse = MakeAction(this, TEXT("IA_QuickUse"), EType::Boolean);
	Block = MakeAction(this, TEXT("IA_Block"), EType::Boolean);
	// These three open / close menus, so they must also fire while the game is paused.
	Inventory = MakeAction(this, TEXT("IA_Inventory"), EType::Boolean, true);
	SkillTrees = MakeAction(this, TEXT("IA_SkillTrees"), EType::Boolean, true);
	Pause = MakeAction(this, TEXT("IA_Pause"), EType::Boolean, true);

	FaceUp = MakeAction(this, TEXT("IA_FaceUp"), EType::Boolean);
	FaceLeft = MakeAction(this, TEXT("IA_FaceLeft"), EType::Boolean);
	FaceDown = MakeAction(this, TEXT("IA_FaceDown"), EType::Boolean);
	FaceRight = MakeAction(this, TEXT("IA_FaceRight"), EType::Boolean);

	QTEUp = MakeAction(this, TEXT("IA_QTEUp"), EType::Boolean);
	QTELeft = MakeAction(this, TEXT("IA_QTELeft"), EType::Boolean);
	QTEDown = MakeAction(this, TEXT("IA_QTEDown"), EType::Boolean);
	QTERight = MakeAction(this, TEXT("IA_QTERight"), EType::Boolean);

	MenuNavigate = MakeAction(this, TEXT("IA_MenuNavigate"), EType::Axis2D, true);
	MenuConfirm = MakeAction(this, TEXT("IA_MenuConfirm"), EType::Boolean, true);
	MenuBack = MakeAction(this, TEXT("IA_MenuBack"), EType::Boolean, true);
	MenuTab = MakeAction(this, TEXT("IA_MenuTab"), EType::Axis1D, true);
	MenuClick = MakeAction(this, TEXT("IA_MenuClick"), EType::Boolean, true);

	// ---- Gameplay context (GDD 9) ----
	GameplayContext = NewObject<UInputMappingContext>(this, MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), TEXT("IMC_Gameplay")));
	UInputMappingContext* G = GameplayContext;

	MapWASD(G, Move);
	MapStick(G, Move, EKeys::Gamepad_Left2D, GameplayStickDeadZone);

	G->MapKey(Look, EKeys::Mouse2D);
	MapStick(G, LookStick, EKeys::Gamepad_Right2D, GameplayStickDeadZone);

	MapAxis1D(G, SwitchWeapon, EKeys::MouseWheelAxis);
	MapAxis1D(G, SwitchWeapon, EKeys::LeftAlt);
	MapAxis1D(G, SwitchWeapon, EKeys::Gamepad_DPad_Up);

	MapAxis1D(G, CycleQuickUse, EKeys::LeftShift);
	MapAxis1D(G, CycleQuickUse, EKeys::Gamepad_DPad_Right);
	MapAxis1D(G, CycleQuickUse, EKeys::Gamepad_DPad_Left, true);

	MapButton(G, Jump, EKeys::SpaceBar);
	MapButton(G, Dash, EKeys::LeftControl);

	MapButton(G, Primary, EKeys::LeftMouseButton);
	MapButton(G, Primary, EKeys::Gamepad_RightTrigger);
	MapButton(G, Secondary, EKeys::RightMouseButton);
	MapButton(G, Secondary, EKeys::Gamepad_LeftTrigger);

	MapButton(G, SpellSlot1, EKeys::RightMouseButton);
	MapButton(G, SpellSlot1, EKeys::Gamepad_LeftShoulder);
	MapButton(G, SpellSlot2, EKeys::One);
	MapButton(G, SpellSlot3, EKeys::Two);
	MapButton(G, SpellSlot4, EKeys::Three);
	MapButton(G, SpellSlot5, EKeys::Four);

	MapButton(G, Enchant, EKeys::T);
	MapButton(G, Enchant, EKeys::Gamepad_LeftShoulder);
	MapButton(G, Reload, EKeys::R);
	MapButton(G, Interact, EKeys::G);
	MapButton(G, QuickUse, EKeys::F);
	MapButton(G, QuickUse, EKeys::Gamepad_RightShoulder);
	// L2 is also Secondary (aim with a gun, spell modifier with the staff): both meanings coexist, the character
	// and the spellbook decide (block is refused with a gun in hand).
	MapButton(G, Block, EKeys::Q);
	MapButton(G, Block, EKeys::Gamepad_LeftTrigger);

	MapButton(G, Inventory, EKeys::Tab);
	MapButton(G, Inventory, EKeys::Gamepad_Special_Left);
	MapButton(G, SkillTrees, EKeys::K);
	MapButton(G, Pause, EKeys::Escape);
	MapButton(G, Pause, EKeys::Gamepad_Special_Right);

	MapButton(G, FaceUp, EKeys::Gamepad_FaceButton_Top);
	MapButton(G, FaceLeft, EKeys::Gamepad_FaceButton_Left);
	MapButton(G, FaceDown, EKeys::Gamepad_FaceButton_Bottom);
	MapButton(G, FaceRight, EKeys::Gamepad_FaceButton_Right);

	MapButton(G, QTEUp, EKeys::W);
	MapButton(G, QTELeft, EKeys::A);
	MapButton(G, QTEDown, EKeys::S);
	MapButton(G, QTERight, EKeys::D);

	// ---- Menu context ----
	MenuContext = NewObject<UInputMappingContext>(this, MakeUniqueObjectName(this, UInputMappingContext::StaticClass(), TEXT("IMC_Menu")));
	UInputMappingContext* M = MenuContext;

	MapDir2D(M, MenuNavigate, EKeys::Up, EDir2D::Up);
	MapDir2D(M, MenuNavigate, EKeys::Down, EDir2D::Down);
	MapDir2D(M, MenuNavigate, EKeys::Left, EDir2D::Left);
	MapDir2D(M, MenuNavigate, EKeys::Right, EDir2D::Right);
	MapWASD(M, MenuNavigate);
	MapDir2D(M, MenuNavigate, EKeys::Gamepad_DPad_Up, EDir2D::Up);
	MapDir2D(M, MenuNavigate, EKeys::Gamepad_DPad_Down, EDir2D::Down);
	MapDir2D(M, MenuNavigate, EKeys::Gamepad_DPad_Left, EDir2D::Left);
	MapDir2D(M, MenuNavigate, EKeys::Gamepad_DPad_Right, EDir2D::Right);
	MapStick(M, MenuNavigate, EKeys::Gamepad_Left2D, MenuStickDeadZone);

	MapButton(M, MenuConfirm, EKeys::Enter);
	MapButton(M, MenuConfirm, EKeys::SpaceBar);
	MapButton(M, MenuConfirm, EKeys::Gamepad_FaceButton_Bottom);

	MapButton(M, MenuBack, EKeys::Escape);
	MapButton(M, MenuBack, EKeys::Tab);
	MapButton(M, MenuBack, EKeys::Gamepad_FaceButton_Right);

	MapAxis1D(M, MenuTab, EKeys::Q, true);
	MapAxis1D(M, MenuTab, EKeys::E);
	MapAxis1D(M, MenuTab, EKeys::Gamepad_LeftShoulder, true);
	MapAxis1D(M, MenuTab, EKeys::Gamepad_RightShoulder);

	MapButton(M, MenuClick, EKeys::LeftMouseButton);

	// Inside menus: the gamepad Menu / View buttons and K still toggle their screens (Esc and Tab are "back" here).
	MapButton(M, Pause, EKeys::Gamepad_Special_Right);
	MapButton(M, Inventory, EKeys::Gamepad_Special_Left);
	MapButton(M, SkillTrees, EKeys::K);
}
