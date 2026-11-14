// Base class of the spell behaviors. A spell in Spells.json names one of these classes in "ActorClass".
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Spells/GBSpellTypes.h"
#include "GBSpellActor.generated.h"

class UBillboardComponent;
class UStaticMeshComponent;
struct FCollisionShape;

/** Keys of Game.json "Values" that control how ground spell figures stack (GDD 18). */
namespace GBGroundFigure
{
	inline const FName Lift(TEXT("GroundFigureLift"));               // cm above the ground for a figure with no overlap
	inline const FName LayerStep(TEXT("GroundFigureLayerStep"));     // cm per overlapping figure placed earlier
	inline const FName LayerMaxRise(TEXT("GroundFigureLayerMaxRise")); // cm: figures farther above/below the ground point are ignored
}

/**
 * Spell parameter names that are not in GBStat (Core/GBStatNames.h). Shared ones live here; the ones only one
 * behavior reads live at the top of that behavior's header. All of them are keys of Spells.json "Params".
 */
namespace GBSpellParam
{
	/** World height (cm) of the spell's pixel-art sprite (Strings "Sprite"). Every behavior with a sprite reads it. */
	inline const FName SpriteSize(TEXT("SpriteSize"));
	/** cm from the caster: where a wall is raised, how far in front of the eyes the shield sprite sits. */
	inline const FName Distance(TEXT("Distance"));
}

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
 *   AGBSpell_Shield      ice shield                    (summoned shield, raised with the Block input)
 *   AGBSpell_Enchant     weapon enchant                (timed status effect on the caster that buffs the gun)
 * To add a spell that fits an existing behavior: only add an entry to Spells.json.
 * To add a new behavior: subclass this, document its params, and reference the class from Spells.json.
 *
 * Subclasses put their logic in StartSpell(), which runs once both InitSpell and BeginPlay have happened (in
 * whichever order), so the actor is in the world with its final cast data.
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

	/**
	 * World Z for a new ground figure (GDD 18): above the ground point, and above every
	 * overlapping live figure. Existing entries are (center X, center Y, figure world Z, radius).
	 * Overlap = horizontal center distance < sum of radii, and the existing figure Z within
	 * MaxRise above or below GroundPoint. Returns max(GroundPoint.Z + Lift, highest overlap + LayerStep).
	 */
	static float ComputeGroundFigureZ(const FVector& GroundPoint, float Radius, const TArray<FVector4>& Existing, float Lift, float LayerStep, float MaxRise);

	/** True when this actor placed a ground figure with PlaceGroundFigure and has not ended. */
	bool HasGroundFigure() const { return bHasGroundFigure; }
	float GetGroundFigureRadius() const { return GroundFigureRadius; }
	float GetGroundFigureWorldZ() const { return GroundFigureZ; }

	const FGBSpellCast& GetSpellCast() const { return SpellCast; }
	AActor* GetCaster() const { return SpellCast.Caster; }

	/** Fired once when the spell is over, just before the actor is destroyed. */
	FSimpleMulticastDelegate OnSpellEnded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The behavior itself. Runs once, after InitSpell and BeginPlay. */
	virtual void StartSpell() {}

	/** Broadcasts OnSpellEnded and destroys the actor. Safe to call twice. */
	virtual void EndSpell();

	/** Finds the ground under a point (line trace down, world static). False if nothing within MaxDistance. */
	bool FindGround(const FVector& From, FVector& OutGround, float MaxDistance = 2000.f) const;

	/**
	 * The ground under Point, for a spell that must stand on it. The search starts up to Lift above Point (so a
	 * step or a slope there is found) but never through a ceiling: started above a low roof it would find the roof,
	 * and the spell would appear on top of it, out of sight. MaxDistance is measured down from Point.
	 */
	bool FindGroundUnder(const FVector& Point, float Lift, FVector& OutGround, float MaxDistance = 2000.f) const;

	// ---- Helpers shared by the behaviors ----

	/** SpellCast.Get with a warning when the spell's data does not define the param (the default is then used). */
	float ReadParam(FName Param, float DefaultValue) const;

	/** Strings "Color", or the element color when it is missing. */
	FLinearColor GetSpellColor() const;

	/** Strings "Sprite" as an id (NAME_None when missing). */
	FName GetSpriteId() const;

	/** Sets a billboard to the spell sprite (Strings "Sprite"), SpriteSize cm tall (DefaultHeight if not in data). Hides it if there is no sprite. */
	void SetupSpellSprite(UBillboardComponent* Billboard, float DefaultHeight) const;

	/** Where the caster looks from and towards right now (player aim, pawn eyes, or the cast's origin/direction). */
	void GetCasterAim(FVector& OutOrigin, FVector& OutDirection) const;

	/** The point the caster aims at within MaxRange, dropped to the ground. Falls back to the caster's feet. */
	FVector FindAimedGroundPoint(float MaxRange) const;

	/** Alive actors with attributes that are hostile to the caster. */
	bool IsHostileTarget(const AActor* Actor) const;

	/** Hostile targets whose root is within Radius of Center. */
	void GatherHostilesInSphere(const FVector& Center, float Radius, TArray<AActor*>& OutActors) const;

	/** Hostile targets standing on a disc: within Radius horizontally of GroundCenter and up to Height above it. */
	void GatherHostilesOnDisc(const FVector& GroundCenter, float Radius, float Height, TArray<AActor*>& OutActors) const;

	/** Hostile targets touching an oriented box (Extent = half size). */
	void GatherHostilesInBox(const FVector& Center, const FQuat& Rotation, const FVector& Extent, TArray<AActor*>& OutActors) const;

	/** Ends the spell after Seconds (<= 0: next tick, never inside the current call). */
	void EndSpellAfter(float Seconds);

	/** One tick of area damage: DealDamage(Amount) with the spell's element, on-hit effect and its scales. */
	void DamageTarget(AActor* Target, float Amount, const FVector& SourceLocation);

	/** Applies the spell's on-hit effect (Strings "OnHitEffect") without damage, with EffectDuration / EffectStrength. */
	void ApplyOnHitEffect(AActor* Target) const;

	/** Clamps a tick interval read from data so a 0 or negative value cannot flood the timer. */
	static float SafeInterval(float Interval);

	/**
	 * Places a ground figure (trap, zone): the figure keeps the actor's X/Y and its world Z
	 * comes from ComputeGroundFigureZ over the live ground figures in the world. Marks this
	 * actor as a live ground figure with Radius until it ends.
	 */
	void PlaceGroundFigure(UStaticMeshComponent* Figure, float Radius);

	UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Root;

	/** Named SpellCast, not Cast: a member called Cast would hide the engine Cast<T>() template. */
	UPROPERTY() FGBSpellCast SpellCast;
	bool bEnded = false;
	bool bSpellInitialized = false;
	bool bSpellStarted = false;

	/** Ground figure state for layering (GDD 18): set by PlaceGroundFigure, live until the spell ends. */
	bool bHasGroundFigure = false;
	float GroundFigureRadius = 0.f;
	float GroundFigureZ = 0.f;

private:
	void TryStartSpell();
	void QueryHostiles(const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape, TArray<AActor*>& OutActors) const;

	FTimerHandle EndTimer;
};
