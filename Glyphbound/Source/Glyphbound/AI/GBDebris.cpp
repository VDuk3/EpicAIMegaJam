#include "AI/GBDebris.h"

#include "CollisionQueryParams.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"

namespace GBDebrisLocal
{
	constexpr float SkinCm = 0.1f;             // a resting piece floats this far over the surface (a sweep from inside it would depenetrate)
	constexpr float MaxSubStep = 1.f / 60.f;
	constexpr int32 MaxSubSteps = 6;
	constexpr float MinRadius = 6.f;
	constexpr float MaxRadius = 80.f;
	constexpr float GroundNormalZ = 0.5f;      // a surface this upward counts as the ground
	constexpr float SettleProbeUp = 60.f;      // cm above the bounds centre the settle trace starts
	constexpr float SettleProbeDown = 200.f;   // and how far below it looks for the floor
}

bool GBDebris::Step(FGBDebrisState& State, float Dt, const FGBDebrisParams& Params, FSweep Sweep)
{
	if (State.bAtRest || Dt <= 0.f)
	{
		return false;
	}
	State.Velocity.Z -= Params.Gravity * Dt;

	const float Spin = State.AngularVelocity.Size();
	if (Spin > KINDA_SMALL_NUMBER)
	{
		State.Rotation = (FQuat(State.AngularVelocity / Spin, FMath::DegreesToRadians(Spin * Dt)) * State.Rotation).GetNormalized();
	}

	const FVector To = State.Position + State.Velocity * Dt;
	FGBDebrisHit Hit;
	if (!Sweep(State.Position, To, Hit))
	{
		State.Position = To;
		return false;
	}

	FVector Normal = Hit.Normal.GetSafeNormal();
	if (Normal.IsNearlyZero())
	{
		Normal = FVector::UpVector;
	}
	State.Position = Hit.Position + Normal * GBDebrisLocal::SkinCm;
	const float AlongNormal = FVector::DotProduct(State.Velocity, Normal);
	State.LastNormal = Normal;
	State.LastImpactPoint = Hit.ImpactPoint;
	State.LastImpactSpeed = FMath::Max(0.f, -AlongNormal);
	if (AlongNormal < 0.f)
	{
		// Into the surface: the normal part comes back scaled by the restitution, the sliding part loses to friction.
		const FVector Normal3 = Normal * AlongNormal;
		const FVector Tangent = State.Velocity - Normal3;
		State.Velocity = Tangent * (1.f - Params.Friction) - Normal3 * Params.Restitution;
	}
	State.AngularVelocity *= (1.f - Params.SpinDamping);
	if (State.LastImpactSpeed > Params.RestSpeed * 0.5f)
	{
		++State.Bounces;
	}
	if (State.Velocity.Size() < Params.RestSpeed && Normal.Z >= Params.RestNormalZ)
	{
		State.Velocity = FVector::ZeroVector;
		State.AngularVelocity = FVector::ZeroVector;
		State.bAtRest = true;
	}
	return true;
}

FGBDebrisPiece& FGBDebrisField::Add(USceneComponent* Component, const FVector& Velocity, const FVector& SpinDegrees, float Radius, const FGBDebrisParams& Params)
{
	FGBDebrisPiece& Piece = Pieces.AddDefaulted_GetRef();
	Piece.Component = Component;
	Piece.Params = Params;
	Piece.Radius = FMath::Max(Radius, 1.f);
	if (Component)
	{
		Component->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Piece.State.Position = Component->GetComponentLocation();
		Piece.State.Rotation = Component->GetComponentQuat();
	}
	Piece.State.Velocity = Velocity;
	Piece.State.AngularVelocity = SpinDegrees;
	return Piece;
}

void FGBDebrisField::Tick(UWorld* World, const AActor* Ignore, float Dt)
{
	if (!World || Dt <= 0.f)
	{
		return;
	}
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GBDebrisSweep), false, Ignore);
	FCollisionResponseParams Response;
	Response.CollisionResponse.SetResponse(ECC_Pawn, ECR_Ignore);

	const int32 SubSteps = FMath::Clamp(FMath::CeilToInt(Dt / GBDebrisLocal::MaxSubStep), 1, GBDebrisLocal::MaxSubSteps);
	const float SubDt = Dt / SubSteps;
	for (FGBDebrisPiece& Piece : Pieces)
	{
		USceneComponent* Component = Piece.Component.Get();
		if (!Component || Piece.State.bAtRest)
		{
			continue;
		}
		const FCollisionShape Shape = FCollisionShape::MakeSphere(Piece.Radius);
		auto Sweep = [&](const FVector& From, const FVector& To, FGBDebrisHit& Out)
		{
			FHitResult Hit;
			if (!World->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_WorldStatic, Shape, Query, Response))
			{
				return false;
			}
			if (Hit.bStartPenetrating)
			{
				// Inside something: push out along the way out instead of getting stuck.
				Out.Position = From + Hit.Normal * FMath::Max(Hit.PenetrationDepth, 0.f);
				Out.Normal = Hit.Normal;
			}
			else
			{
				Out.Position = Hit.Location;
				Out.Normal = Hit.ImpactNormal;
			}
			Out.ImpactPoint = Hit.ImpactPoint;
			return true;
		};
		for (int32 Index = 0; Index < SubSteps && !Piece.State.bAtRest; ++Index)
		{
			if (GBDebris::Step(Piece.State, SubDt, Piece.Params, Sweep) && Piece.State.LastNormal.Z >= GBDebrisLocal::GroundNormalZ)
			{
				++Piece.GroundBounces;
				if (!bGroundContact)
				{
					bGroundContact = true;
					bGroundContactFresh = true;
					GroundContactPoint = Piece.State.LastImpactPoint;
					GroundContactNormal = Piece.State.LastNormal;
				}
			}
		}
		Component->SetWorldLocationAndRotation(Piece.State.Position, Piece.State.Rotation);
		if (Piece.State.bAtRest && !Piece.bSettled)
		{
			Settle(World, Ignore, Piece);
		}
	}
}

void FGBDebrisField::Settle(UWorld* World, const AActor* Ignore, FGBDebrisPiece& Piece)
{
	Piece.bSettled = true;
	USceneComponent* Component = Piece.Component.Get();
	const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component);
	if (!Mesh || !Mesh->GetStaticMesh())
	{
		return;
	}
	// The rotated mesh bounds: the eight corners of the local box in world space.
	const FBox Local = Mesh->GetStaticMesh()->GetBounds().GetBox();
	const FTransform Transform = Mesh->GetComponentTransform();
	FVector Centre = FVector::ZeroVector;
	float LowestZ = TNumericLimits<float>::Max();
	for (int32 Corner = 0; Corner < 8; ++Corner)
	{
		const FVector Point(Corner & 1 ? Local.Max.X : Local.Min.X, Corner & 2 ? Local.Max.Y : Local.Min.Y, Corner & 4 ? Local.Max.Z : Local.Min.Z);
		const FVector World = Transform.TransformPosition(Point);
		LowestZ = FMath::Min(LowestZ, static_cast<float>(World.Z));
		Centre += World / 8.0;
	}
	// The floor under the bounds' centre (from a little above it, so a piece resting on the floor does not start inside it).
	FCollisionQueryParams Query(SCENE_QUERY_STAT(GBDebrisSettle), false, Ignore);
	FHitResult Hit;
	const FVector Start = Centre + FVector::UpVector * GBDebrisLocal::SettleProbeUp;
	if (!World->LineTraceSingleByObjectType(Hit, Start, Centre - FVector::UpVector * GBDebrisLocal::SettleProbeDown, FCollisionObjectQueryParams(ECC_WorldStatic), Query))
	{
		return;
	}
	const float Lift = static_cast<float>(Hit.ImpactPoint.Z) - LowestZ;
	if (FMath::Abs(Lift) > KINDA_SMALL_NUMBER && FMath::Abs(Lift) < GBDebrisLocal::SettleProbeUp + GBDebrisLocal::SettleProbeDown)
	{
		Piece.State.Position.Z += Lift;
		Component->SetWorldLocationAndRotation(Piece.State.Position, Piece.State.Rotation);
	}
}

void FGBDebrisField::Freeze()
{
	for (FGBDebrisPiece& Piece : Pieces)
	{
		Piece.State.bAtRest = true;
		Piece.State.Velocity = FVector::ZeroVector;
		Piece.State.AngularVelocity = FVector::ZeroVector;
	}
}

int32 FGBDebrisField::TotalBounces() const
{
	int32 Total = 0;
	for (const FGBDebrisPiece& Piece : Pieces)
	{
		Total += Piece.State.Bounces;
	}
	return Total;
}

bool FGBDebrisField::AllAtRest() const
{
	for (const FGBDebrisPiece& Piece : Pieces)
	{
		if (Piece.Component.IsValid() && !Piece.State.bAtRest)
		{
			return false;
		}
	}
	return true;
}

bool FGBDebrisField::ConsumeGroundContact()
{
	const bool bWas = bGroundContactFresh;
	bGroundContactFresh = false;
	return bWas;
}

float FGBDebrisField::RadiusOf(const USceneComponent* Component)
{
	float Radius = 10.f;
	if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Component))
	{
		if (const UStaticMesh* Asset = Mesh->GetStaticMesh())
		{
			const FVector Extent = Asset->GetBounds().BoxExtent * Mesh->GetComponentScale().GetAbs();
			Radius = Extent.GetMin();
		}
	}
	return FMath::Clamp(Radius, GBDebrisLocal::MinRadius, GBDebrisLocal::MaxRadius);
}
