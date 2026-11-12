// The elemental sword (GDD 11): the only melee weapon. 3D hilt (future), pixel-art blade drawn by the HUD.
#pragma once

#include "CoreMinimal.h"
#include "Weapons/GBHeldItem.h"
#include "GBSword.generated.h"

class UStaticMeshComponent;

/**
 * Primary = swing (FireInterval): damages every hostile actor with attributes within MeleeRange and inside the
 * MeleeArc cone in front of the camera, unless a wall is in between. Damage uses the sword's current element
 * (the gem's, changeable from the inventory with SetElement). No ammo, no secondary. The enchant never applies.
 */
UCLASS()
class GLYPHBOUND_API AGBSword : public AGBHeldItem
{
	GENERATED_BODY()

public:
	virtual void InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner) override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void StartPrimary() override { PressTrigger(); }
	virtual void StopPrimary() override { ReleaseTrigger(); }

protected:
	virtual bool PerformAttack() override;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> Grip;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Guard;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> Gem;

	/** Element the gem is currently tinted with (SetElement is not virtual, so the color follows in Tick). */
	EGBElement ShownElement = EGBElement::Physical;
};
