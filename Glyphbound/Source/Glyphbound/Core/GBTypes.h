// Shared enums and small structs used by every Glyphbound system.
#pragma once

#include "CoreMinimal.h"
#include "GBTypes.generated.h"

GLYPHBOUND_API DECLARE_LOG_CATEGORY_EXTERN(LogGB, Log, All);

/** The two times of the map (GDD 4). */
UENUM(BlueprintType)
enum class EGBEra : uint8
{
	Past,
	Future
};

/** In which era(s) an actor exists. */
UENUM(BlueprintType)
enum class EGBEraPresence : uint8
{
	Both,
	PastOnly,
	FutureOnly
};

UENUM(BlueprintType)
enum class EGBElement : uint8
{
	Physical,
	Arcane,
	Fire,
	Lightning,
	Ice,
	Air
};

/** Held slots (GDD 8): one staff, one gun, and later the elemental sword. */
UENUM(BlueprintType)
enum class EGBHeldSlot : uint8
{
	Staff,
	Gun,
	Sword
};

UENUM(BlueprintType)
enum class EGBTeam : uint8
{
	Neutral,
	Player,
	Enemy
};

/** Which QTE part scales a spell parameter (GDD 7): part 1 = power, part 2 = reach. */
UENUM(BlueprintType)
enum class EGBScaleSource : uint8
{
	None,
	Power,
	Reach
};

UENUM(BlueprintType)
enum class EGBModOp : uint8
{
	Add,
	Multiply,
	Override,
	MultiplyMin
};

/** HUD skin, chosen by the held item (GDD 13). */
UENUM(BlueprintType)
enum class EGBHudSkin : uint8
{
	Rpg90s,
	Cyberpunk
};

UENUM(BlueprintType)
enum class EGBItemType : uint8
{
	Consumable,
	Throwable,
	Armor,
	KeyItem,
	Material,
	Ammo
};

/** One hit. Everything that hurts something goes through UGBCombatLibrary::DealDamage with one of these. */
USTRUCT(BlueprintType)
struct GLYPHBOUND_API FGBDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float Amount = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	EGBElement Element = EGBElement::Physical;

	/** Who is responsible (the player pawn, an enemy). Used for teams and kill credit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> Instigator = nullptr;

	/** The thing that touched the target (projectile, wall, trap). May be null. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TObjectPtr<AActor> Causer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector HitLocation = FVector::ZeroVector;

	/** Direction the damage travels (from the source towards the target), normalized or zero. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector HitDirection = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bHeadshot = false;

	/** Spell, weapon or effect id that produced the hit (for logs, HUD and synergies). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FName SourceId = NAME_None;

	/** Optional status effect applied to the target on hit (id from StatusEffects.json). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FName OnHitEffect = NAME_None;

	/** Scales the duration / strength of OnHitEffect (1 = as defined in data). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float EffectDurationScale = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float EffectStrengthScale = 1.f;

	/** Raw elemental build-up this hit adds to the target's bar of Element (before the target's resistance and the weak point bonus). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BuildUp = 0.f;

	/** Multiplies BuildUp once when bHeadshot is true (weapon hits only). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float WeakPointBuildUpMult = 1.f;

	/** Hits that share an id add build-up to one target only once (shotgun pellets of one shot); 0 = no dedupe. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	int32 BuildUpEventId = 0;
};
