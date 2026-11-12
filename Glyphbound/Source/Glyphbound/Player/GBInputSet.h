// Enhanced Input actions and mappings built in C++ (no binary assets): the whole control table of GDD 9.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "GBInputSet.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Created once per player (NewObject) and kept alive by the character. Build() creates every action and the
 * two mapping contexts with keyboard + mouse and gamepad bindings from GDD 9:
 *  - GameplayContext: always active while playing.
 *  - MenuContext: active while a menu is open (navigate, confirm, back).
 * Context-dependent keys are resolved by the character, not by the mapping: right click / L1 means "spell slot 1"
 * with the staff and "aim" / "enchant" with a gun; W A S D and the face buttons go to the QTE while it runs;
 * gamepad L2 held + face button = spell slots 2-5 with the staff.
 */
UCLASS()
class GLYPHBOUND_API UGBInputSet : public UObject
{
	GENERATED_BODY()

public:
	void Build();

	UPROPERTY() TObjectPtr<UInputMappingContext> GameplayContext;
	UPROPERTY() TObjectPtr<UInputMappingContext> MenuContext;

	// Axis2D (x = right, y = forward / up)
	UPROPERTY() TObjectPtr<UInputAction> Move;
	UPROPERTY() TObjectPtr<UInputAction> Look;         // mouse: per-frame delta
	UPROPERTY() TObjectPtr<UInputAction> LookStick;    // right stick: a rate, scaled per second by the character
	// Axis1D (+1 next, -1 previous)
	UPROPERTY() TObjectPtr<UInputAction> SwitchWeapon;
	UPROPERTY() TObjectPtr<UInputAction> CycleQuickUse;
	// Buttons
	UPROPERTY() TObjectPtr<UInputAction> Jump;
	UPROPERTY() TObjectPtr<UInputAction> Dash;
	UPROPERTY() TObjectPtr<UInputAction> Primary;      // left click / R2
	UPROPERTY() TObjectPtr<UInputAction> Secondary;    // right click / L2 (aim with gun, spell modifier with staff on gamepad)
	UPROPERTY() TObjectPtr<UInputAction> SpellSlot1;   // right click (KB/M) / L1
	UPROPERTY() TObjectPtr<UInputAction> SpellSlot2;   // 1
	UPROPERTY() TObjectPtr<UInputAction> SpellSlot3;   // 2
	UPROPERTY() TObjectPtr<UInputAction> SpellSlot4;   // 3
	UPROPERTY() TObjectPtr<UInputAction> SpellSlot5;   // 4
	UPROPERTY() TObjectPtr<UInputAction> Enchant;      // T / L1 (gun in hand)
	UPROPERTY() TObjectPtr<UInputAction> Reload;       // R / Square (X)
	UPROPERTY() TObjectPtr<UInputAction> Interact;     // G / Triangle (Y)
	UPROPERTY() TObjectPtr<UInputAction> QuickUse;     // F / R1
	UPROPERTY() TObjectPtr<UInputAction> Inventory;    // Tab / View
	UPROPERTY() TObjectPtr<UInputAction> SkillTrees;   // K
	UPROPERTY() TObjectPtr<UInputAction> Pause;        // Esc / Menu
	// Gamepad face buttons, raw: the character decides if they mean jump / dash / reload / interact,
	// a QTE key, or (with L2 held and the staff) spell slots 2-5.
	UPROPERTY() TObjectPtr<UInputAction> FaceUp;
	UPROPERTY() TObjectPtr<UInputAction> FaceLeft;
	UPROPERTY() TObjectPtr<UInputAction> FaceDown;
	UPROPERTY() TObjectPtr<UInputAction> FaceRight;
	// QTE part 1 on keyboard: W A S D (also mapped to Move; the character uses one or the other)
	UPROPERTY() TObjectPtr<UInputAction> QTEUp;
	UPROPERTY() TObjectPtr<UInputAction> QTELeft;
	UPROPERTY() TObjectPtr<UInputAction> QTEDown;
	UPROPERTY() TObjectPtr<UInputAction> QTERight;
	// Menus
	UPROPERTY() TObjectPtr<UInputAction> MenuNavigate; // Axis2D: arrows / WASD / D-pad / left stick
	UPROPERTY() TObjectPtr<UInputAction> MenuConfirm;  // Enter / Space / Cross (A)
	UPROPERTY() TObjectPtr<UInputAction> MenuBack;     // Esc / Tab / Circle (B)
	UPROPERTY() TObjectPtr<UInputAction> MenuTab;      // Axis1D: Q-E / L1-R1, switches menu page
};
