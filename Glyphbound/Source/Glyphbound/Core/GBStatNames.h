// Names of the stats used in the JSON data files and in the modifier system.
// Every number that tunes gameplay is a named stat: base value from data, then modifiers
// (skill tree, armor, status effects, air passives, weapon enchant) through UGBStatsComponent.
// Use these constants instead of typing strings, so a typo is a compile error.
// Adding a stat: add the constant here, read it where it is used, and put its value in the JSON.
#pragma once

#include "CoreMinimal.h"
#include "Core/GBTypes.h"

namespace GBStat
{
	// ---- Actor stats (Player.json "Stats", Enemies.json "Stats") ----
	inline const FName MaxHealth(TEXT("MaxHealth"));
	inline const FName HealthRegen(TEXT("HealthRegen"));            // per second
	inline const FName HealthRegenDelay(TEXT("HealthRegenDelay"));  // seconds after taking damage
	inline const FName MaxMana(TEXT("MaxMana"));
	inline const FName ManaRegen(TEXT("ManaRegen"));                // per second
	inline const FName ManaRegenDelay(TEXT("ManaRegenDelay"));      // seconds after spending mana
	inline const FName MoveSpeed(TEXT("MoveSpeed"));                // cm/s
	inline const FName JumpVelocity(TEXT("JumpVelocity"));          // cm/s
	inline const FName MaxJumps(TEXT("MaxJumps"));                  // 1 = single jump, 2 = double jump
	inline const FName AirControl(TEXT("AirControl"));              // 0 = jumps commit to their direction (GDD 18)
	inline const FName JumpTakeoffMinSpeedMult(TEXT("JumpTakeoffMinSpeedMult")); // min takeoff speed as a fraction of MoveSpeed (ground jump)
	inline const FName AirJumpMinSpeedMult(TEXT("AirJumpMinSpeedMult"));         // min takeoff speed as a fraction of MoveSpeed (air jump)
	inline const FName GravityScale(TEXT("GravityScale"));
	inline const FName DashDistance(TEXT("DashDistance"));          // cm
	inline const FName DashDuration(TEXT("DashDuration"));          // seconds
	inline const FName DashCooldown(TEXT("DashCooldown"));          // seconds
	inline const FName InteractRange(TEXT("InteractRange"));        // cm
	inline const FName DamageTaken(TEXT("DamageTaken"));            // multiplier, base 1
	inline const FName DamageDealt(TEXT("DamageDealt"));            // multiplier, base 1
	inline const FName CapsuleRadius(TEXT("CapsuleRadius"));
	inline const FName CapsuleHalfHeight(TEXT("CapsuleHalfHeight"));
	inline const FName EyeHeight(TEXT("EyeHeight"));

	// ---- Enemy stats (Enemies.json "Stats") ----
	inline const FName AttackDamage(TEXT("AttackDamage"));
	inline const FName AttackRange(TEXT("AttackRange"));
	inline const FName AttackInterval(TEXT("AttackInterval"));
	inline const FName AttackWindup(TEXT("AttackWindup"));
	inline const FName AggroRange(TEXT("AggroRange"));
	inline const FName HeadFraction(TEXT("HeadFraction"));          // top fraction of the body that counts as head (GDD 3)

	// ---- Weapon stats (Weapons.json "Stats"); modifier keys are Weapon.<Id>.<Stat> and Weapon.All.<Stat> ----
	inline const FName Damage(TEXT("Damage"));
	inline const FName FireInterval(TEXT("FireInterval"));          // seconds between shots
	inline const FName MagazineSize(TEXT("MagazineSize"));
	inline const FName SpareMagazines(TEXT("SpareMagazines"));      // spare magazines after a full refill
	inline const FName ReloadTime(TEXT("ReloadTime"));
	inline const FName Range(TEXT("Range"));
	inline const FName Spread(TEXT("Spread"));                      // degrees, hip fire
	inline const FName AimSpread(TEXT("AimSpread"));                // degrees, aiming
	inline const FName MoveSpread(TEXT("MoveSpread"));              // extra degrees at full speed, hip fire
	inline const FName AimMoveSpread(TEXT("AimMoveSpread"));        // extra degrees at full speed, aiming
	inline const FName RecoilPitch(TEXT("RecoilPitch"));            // degrees per shot
	inline const FName RecoilYaw(TEXT("RecoilYaw"));                // +- degrees per shot
	inline const FName RecoilRecovery(TEXT("RecoilRecovery"));      // degrees per second
	inline const FName Pellets(TEXT("Pellets"));
	inline const FName FalloffStart(TEXT("FalloffStart"));          // cm: up to here, FalloffNearMult
	inline const FName FalloffEnd(TEXT("FalloffEnd"));              // cm: from here on, FalloffFarMult
	inline const FName FalloffNearMult(TEXT("FalloffNearMult"));
	inline const FName FalloffFarMult(TEXT("FalloffFarMult"));
	inline const FName HeadshotMult(TEXT("HeadshotMult"));
	inline const FName ProjectileSpeed(TEXT("ProjectileSpeed"));
	inline const FName ProjectileRadius(TEXT("ProjectileRadius"));
	inline const FName ProjectileGravity(TEXT("ProjectileGravity"));
	inline const FName AimFOV(TEXT("AimFOV"));
	inline const FName AimMoveSpeedMult(TEXT("AimMoveSpeedMult"));
	inline const FName MeleeRange(TEXT("MeleeRange"));
	inline const FName MeleeArc(TEXT("MeleeArc"));                  // degrees

	// ---- Spell parameters (Spells.json "Params"); modifier keys are Spell.<Id>.<Param> and Spell.All.<Param> ----
	inline const FName ManaCost(TEXT("ManaCost"));
	inline const FName Cooldown(TEXT("Cooldown"));
	inline const FName Speed(TEXT("Speed"));
	inline const FName Radius(TEXT("Radius"));
	inline const FName Duration(TEXT("Duration"));
	inline const FName Width(TEXT("Width"));
	inline const FName Height(TEXT("Height"));
	inline const FName Thickness(TEXT("Thickness"));
	inline const FName Health(TEXT("Health"));
	inline const FName TickInterval(TEXT("TickInterval"));
	inline const FName DamagePerSecond(TEXT("DamagePerSecond"));
	inline const FName EffectDuration(TEXT("EffectDuration"));      // multiplier on the on-hit status effect duration
	inline const FName EffectStrength(TEXT("EffectStrength"));      // multiplier on the on-hit status effect strength
	inline const FName BlockFraction(TEXT("BlockFraction"));        // ice shield: 0..1 of frontal damage removed
	inline const FName FuseTime(TEXT("FuseTime"));

	// ---- QTE (QTE.json "Stats"); also modifiable by gear (GDD 7) ----
	inline const FName QTETimeDilation(TEXT("QTE.TimeDilation"));   // world speed during the QTE (0.1 = 90% slow)
	inline const FName QTETimeLimit(TEXT("QTE.TimeLimit"));         // real seconds for the whole QTE
	inline const FName QTESequenceLength(TEXT("QTE.SequenceLength"));
	inline const FName QTEGestureCount(TEXT("QTE.GestureCount"));

	// ---- Elemental build-up (GDD 6): spell param / item stat, weapon stat ----
	inline const FName BuildUp(TEXT("BuildUp"));                   // raw build-up per hit or per tick
	inline const FName WeakPointBuildUpMult(TEXT("WeakPointBuildUpMult")); // weapon stat; multiplies BuildUp on weak point hits

	/** "Weapon.<WeaponId>.<Stat>" */
	inline FName WeaponKey(FName WeaponId, FName Stat) { return FName(*FString::Printf(TEXT("Weapon.%s.%s"), *WeaponId.ToString(), *Stat.ToString())); }
	/** "Weapon.Staff.<Stat>", "Weapon.Gun.<Stat>" or "Weapon.Sword.<Stat>" */
	inline FName WeaponSlotKey(EGBHeldSlot Slot, FName Stat)
	{
		const TCHAR* SlotName = TEXT("Gun");
		switch (Slot)
		{
		case EGBHeldSlot::Staff: SlotName = TEXT("Staff"); break;
		case EGBHeldSlot::Gun:   SlotName = TEXT("Gun"); break;
		case EGBHeldSlot::Sword:  SlotName = TEXT("Sword"); break;
		}
		return FName(*FString::Printf(TEXT("Weapon.%s.%s"), SlotName, *Stat.ToString()));
	}
	/** "Weapon.All.<Stat>" */
	inline FName WeaponAllKey(FName Stat) { return FName(*FString::Printf(TEXT("Weapon.All.%s"), *Stat.ToString())); }
	/** "Spell.<SpellId>.<Param>" */
	inline FName SpellKey(FName SpellId, FName Param) { return FName(*FString::Printf(TEXT("Spell.%s.%s"), *SpellId.ToString(), *Param.ToString())); }
	/** "Spell.All.<Param>" */
	inline FName SpellAllKey(FName Param) { return FName(*FString::Printf(TEXT("Spell.All.%s"), *Param.ToString())); }
}

/** Keys of the free-form "Strings" maps in the data files. */
namespace GBString
{
	inline const FName Sprite(TEXT("Sprite"));                 // sprite id from Sprites.json
	inline const FName Color(TEXT("Color"));                   // "#RRGGBB"
	inline const FName OnHitEffect(TEXT("OnHitEffect"));       // status effect id
	inline const FName ProjectileSprite(TEXT("ProjectileSprite"));
	inline const FName HandSprite(TEXT("HandSprite"));
}

/** Elemental build-up keys (GDD 6): per-enemy stats, Game.json defaults, weapon stats, reactions. */
namespace GBElemental
{
	constexpr int32 NumElements = 3;

	inline int32 ElementIndex(EGBElement Element)
	{
		switch (Element)
		{
		case EGBElement::Fire: return 0;
		case EGBElement::Lightning: return 1;
		case EGBElement::Ice: return 2;
		default: return -1;
		}
	}

	inline EGBElement IndexToElement(int32 Index)
	{
		switch (Index)
		{
		case 0: return EGBElement::Fire;
		case 1: return EGBElement::Lightning;
		case 2: return EGBElement::Ice;
		default: return EGBElement::Physical;
		}
	}

	inline const FName& DamageTakenKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireDamageTaken")), FName(TEXT("LightningDamageTaken")), FName(TEXT("IceDamageTaken")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& BuildUpTakenKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireBuildUpTaken")), FName(TEXT("LightningBuildUpTaken")), FName(TEXT("IceBuildUpTaken")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& ThresholdKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireBuildUpThreshold")), FName(TEXT("LightningBuildUpThreshold")), FName(TEXT("IceBuildUpThreshold")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& DecayDelayKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireBuildUpDecayDelay")), FName(TEXT("LightningBuildUpDecayDelay")), FName(TEXT("IceBuildUpDecayDelay")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& DecayRateKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireBuildUpDecayRate")), FName(TEXT("LightningBuildUpDecayRate")), FName(TEXT("IceBuildUpDecayRate")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& StatusDurationKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireStatusDuration")), FName(TEXT("LightningStatusDuration")), FName(TEXT("IceStatusDuration")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& StatusImmunityKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireStatusImmunity")), FName(TEXT("LightningStatusImmunity")), FName(TEXT("IceStatusImmunity")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& ReactionKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireStatusReaction")), FName(TEXT("LightningStatusReaction")), FName(TEXT("IceStatusReaction")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName& WeaponBuildUpKey(int32 Index)
	{
		static const FName Keys[3] = { FName(TEXT("FireBuildUp")), FName(TEXT("LightningBuildUp")), FName(TEXT("IceBuildUp")) };
		static const FName None = NAME_None;
		return (Index >= 0 && Index < 3) ? Keys[Index] : None;
	}

	inline const FName IgniteDamageMult(TEXT("IgniteDamageMult"));
	inline const FName ShockDischargeDamage(TEXT("ShockDischargeDamage"));
	inline const FName FrostSlow(TEXT("FrostSlow"));
	inline const FName FrostSource(TEXT("Elemental.Frost"));
	inline const FName IgniteEffect(TEXT("Ignite"));
	inline const FName ShockSource(TEXT("Shock"));

	// Reaction names.
	inline const FName ReactionIgnite(TEXT("Ignite"));
	inline const FName ReactionStun(TEXT("Stun"));
	inline const FName ReactionInterrupt(TEXT("Interrupt"));
	inline const FName ReactionFreeze(TEXT("Freeze"));
	inline const FName ReactionSlow(TEXT("Slow"));
	inline const FName ReactionNone(TEXT("None"));

	/** Default of enemy key Key in Game.json: "ElementalDefault.<Key>". Built once per key. */
	inline FName DefaultKey(FName Key)
	{
		static TMap<FName, FName> Cache;
		if (const FName* Found = Cache.Find(Key))
		{
			return *Found;
		}
		const FName Result(*FString(TEXT("ElementalDefault.")) + Key.ToString());
		Cache.Add(Key, Result);
		return Result;
	}
}
