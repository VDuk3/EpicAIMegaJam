// A small debris simulation of our own for the wrecks of the flying enemies (no physics engine: their meshes have no
// simple collision, which is why the simulated-body death never ran). A piece is a scene component moved by a sphere sweep:
// gravity, a bounce with restitution and friction on the first thing hit, a damped spin, and a rest state.
// GBDebris::Step is pure (the sweep is passed in) so a test can run it against analytic planes; FGBDebrisField drives real
// components in a world.
#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"

class UWorld;
class AActor;

/** Enemies.json "Strings" keys of the death effects of the flyers. */
namespace GBDebrisKey
{
	inline const FName FxDeathSparks(TEXT("Fx.DeathSparks"));
	inline const FName FxDeathExplosion(TEXT("Fx.DeathExplosion"));
}

/** How a piece bounces. Every number is read from the enemy's Stats (DebrisRestitution...), these are the fallbacks. */
struct FGBDebrisParams
{
	float Gravity = 980.f;          // cm/s^2, downwards
	float Restitution = 0.35f;      // share of the speed along the normal kept by a bounce
	float Friction = 0.6f;          // share of the speed along the surface lost by a bounce
	float SpinDamping = 0.5f;       // share of the spin lost by a bounce
	float RestSpeed = 60.f;         // cm/s: slower than this after a bounce on an upward surface and the piece rests
	float RestNormalZ = 0.7f;       // how upward a surface has to be to rest on it
};

struct FGBDebrisState
{
	FVector Position = FVector::ZeroVector;     // centre of the swept sphere
	FVector Velocity = FVector::ZeroVector;     // cm/s
	FVector AngularVelocity = FVector::ZeroVector;   // degrees/s about the world axes
	FQuat Rotation = FQuat::Identity;
	bool bAtRest = false;
	int32 Bounces = 0;                          // impacts harder than half the rest speed
	// The last contact, for effects.
	FVector LastNormal = FVector::UpVector;
	FVector LastImpactPoint = FVector::ZeroVector;
	float LastImpactSpeed = 0.f;
};

/** What a sweep found: where the sphere centre stopped and the surface normal there. */
struct FGBDebrisHit
{
	FVector Position = FVector::ZeroVector;
	FVector Normal = FVector::UpVector;
	FVector ImpactPoint = FVector::ZeroVector;
};

namespace GBDebris
{
	using FSweep = TFunctionRef<bool(const FVector& From, const FVector& To, FGBDebrisHit& OutHit)>;

	/**
	 * One step: gravity, then the move from Position to Position + Velocity * Dt. When Sweep reports a hit the piece stops at
	 * the hit position and its velocity is reflected on the normal: the normal part times Restitution (away from the
	 * surface), the tangential part times (1 - Friction), the spin times (1 - SpinDamping). It rests when the speed after the
	 * bounce is under RestSpeed and the normal points up. Returns true when it hit something this step.
	 */
	GLYPHBOUND_API bool Step(FGBDebrisState& State, float Dt, const FGBDebrisParams& Params, FSweep Sweep);
}

/** One piece of a wreck: a component and its state. */
struct FGBDebrisPiece
{
	TWeakObjectPtr<USceneComponent> Component;
	FGBDebrisState State;
	FGBDebrisParams Params;
	float Radius = 10.f;
	int32 GroundBounces = 0;
	bool bSettled = false;          // the resting pose has been lifted onto the floor (see FGBDebrisField::Settle)
};

/** The pieces of one wreck, moved against the world's static geometry. Owned by the enemy actor, not a UObject. */
class GLYPHBOUND_API FGBDebrisField
{
public:
	/** Detaches the component (keeping its world transform) and starts moving it. Returns the piece. */
	FGBDebrisPiece& Add(USceneComponent* Component, const FVector& Velocity, const FVector& SpinDegrees, float Radius, const FGBDebrisParams& Params);

	/** Moves every piece that is not at rest by Dt (in sub-steps). The enemy actor and pawns are ignored by the sweeps. */
	void Tick(UWorld* World, const AActor* Ignore, float Dt);

	/** Stops everything where it is (the wreck is sinking). */
	void Freeze();

	int32 NumPieces() const { return Pieces.Num(); }
	FGBDebrisPiece* GetPiece(int32 Index) { return Pieces.IsValidIndex(Index) ? &Pieces[Index] : nullptr; }
	int32 TotalBounces() const;
	bool AllAtRest() const;

	/** The first time a piece hit something that points up (the ground). The point is on the surface. */
	bool HasGroundContact() const { return bGroundContact; }
	const FVector& GetGroundContactPoint() const { return GroundContactPoint; }
	const FVector& GetGroundContactNormal() const { return GroundContactNormal; }
	/** Set once on the tick that made the first ground contact, cleared by ConsumeGroundContact. */
	bool ConsumeGroundContact();

	/** The radius of a sphere that stands for a static mesh component: its smallest half extent, scaled, kept in a sane range. */
	static float RadiusOf(const USceneComponent* Component);

private:
	/** A piece that came to rest tilted: lifts it so the lowest corner of its rotated mesh bounds sits on the floor under it. */
	static void Settle(UWorld* World, const AActor* Ignore, FGBDebrisPiece& Piece);

	TArray<FGBDebrisPiece> Pieces;
	bool bGroundContact = false;
	bool bGroundContactFresh = false;
	FVector GroundContactPoint = FVector::ZeroVector;
	FVector GroundContactNormal = FVector::UpVector;
};
