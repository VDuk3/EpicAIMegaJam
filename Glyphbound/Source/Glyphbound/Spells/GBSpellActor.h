// Base class of the spell behaviors. A spell in Spells.json names one of these classes in "ActorClass".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spells/GBSpellTypes.h"
#include "GBSpellActor.generated.h"

/**
 * A behavior is spawned by UGBSpellbookComponent at the cast origin, receives the resolved FGBSpellCast in
 * InitSpell, and does its thing using only SpellCast.Get("<Param>") values. It destroys itself when done.
 *
 * Behaviors (each documents the params and strings it reads at the top of its header):
 *   AGBSpell_Projectile  fireball, lightning bolt      (launches an AGBProjectile)
 *   AGBSpell_Wall        fire wall, ice wall           (a wall in front of the caster; blocks and/or burns)
 *   AGBSpell_Zone        ice floor                     (ground area that applies an effect / damage over time)
 *   AGBSpell_Trap        lightning rune                (arms on the ground, triggers when an enemy steps on it)
 *   AGBSpell_Orb         static-electricity ball       (moves forward and damages everything around it)
 *   AGBSpell_Beam        flamethrower                  (sustained cone from the caster's aim)
 *   AGBSpell_Shield      ice shield                    (sustained frontal block on the caster)
 *   AGBSpell_Enchant     weapon enchant                (timed status effect on the caster that buffs the gun)
 * To add a spell that fits an existing behavior: only add an entry to Spells.json.
 * To add a new behavior: subclass this, document its params, and reference the class from Spells.json.
 */
UCLASS(Abstract)
class GLYPHBOUND_API AGBSpellActor : public AActor
{
	GENERATED_BODY()

public:
	AGBSpellActor();

	/** Called once, right after spawn. Subclasses call Super first (it stores SpellCast). */
	virtual void InitSpell(const FGBSpellCast& InCast);

	/** Sustained spells: the cast input was pressed again, the caster switched weapon, or died. Default: EndSpell(). */
	virtual void OnCastReleased();

	/** True while a sustained spell is running (the spellbook keeps a reference and blocks other casts). */
	virtual bool IsSustaining() const { return false; }

	const FGBSpellCast& GetSpellCast() const { return SpellCast; }
	AActor* GetCaster() const { return SpellCast.Caster; }

	/** Fired once when the spell is over, just before the actor is destroyed. */
	FSimpleMulticastDelegate OnSpellEnded;

protected:
	/** Broadcasts OnSpellEnded and destroys the actor. Safe to call twice. */
	virtual void EndSpell();

	/** Finds the ground under a point (line trace down, world static). False if nothing within MaxDistance. */
	bool FindGround(const FVector& From, FVector& OutGround, float MaxDistance = 2000.f) const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	/** Named SpellCast, not Cast: a member called Cast would hide the engine Cast<T>() template. */
	UPROPERTY() FGBSpellCast SpellCast;
	bool bEnded = false;
};
