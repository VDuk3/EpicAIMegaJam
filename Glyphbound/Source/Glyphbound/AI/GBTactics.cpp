#include "AI/GBTactics.h"

const TCHAR* GBTactics::RoleName(EGBSquadRole Role)
{
	switch (Role)
	{
	case EGBSquadRole::Suppress: return TEXT("Suppress");
	case EGBSquadRole::FlankLeft: return TEXT("FlankLeft");
	case EGBSquadRole::FlankRight: return TEXT("FlankRight");
	case EGBSquadRole::Assault: return TEXT("Assault");
	case EGBSquadRole::Anchor: return TEXT("Anchor");
	default: return TEXT("None");
	}
}

float GBTactics::AddBlockedHit(TArray<FBlockedHit>& Hits, float Now, float Amount, float WindowSeconds)
{
	Hits.RemoveAll([Now, WindowSeconds](const FBlockedHit& Hit) { return Now - Hit.Time > WindowSeconds; });
	FBlockedHit Added;
	Added.Time = Now;
	Added.Amount = Amount;
	Hits.Add(Added);
	float Sum = 0.f;
	for (const FBlockedHit& Hit : Hits)
	{
		Sum += Hit.Amount;
	}
	return Sum;
}

FVector GBTactics::ClampToBox(const FBox& Box, const FVector& P)
{
	if (!Box.IsValid)
	{
		return P;
	}
	return FVector(FMath::Clamp(P.X, Box.Min.X, Box.Max.X), FMath::Clamp(P.Y, Box.Min.Y, Box.Max.Y), FMath::Clamp(P.Z, Box.Min.Z, Box.Max.Z));
}

bool GBTactics::IsInsideBox(const FBox& Box, const FVector& P)
{
	return !Box.IsValid || Box.IsInside(P);
}

FBox GBTactics::MakeZone(const FBox& TriggerBounds, TConstArrayView<FVector> Points, float Padding, float ZMargin)
{
	FBox Zone(ForceInit);
	if (TriggerBounds.IsValid)
	{
		Zone += TriggerBounds;
	}
	for (const FVector& Point : Points)
	{
		Zone += Point;
	}
	if (!Zone.IsValid)
	{
		return Zone;
	}
	Zone.Min -= FVector(Padding, Padding, ZMargin);
	Zone.Max += FVector(Padding, Padding, ZMargin);
	return Zone;
}

TArray<EGBSquadRole> GBTactics::AssignRoles(TConstArrayView<FRoleInput> Members)
{
	TArray<EGBSquadRole> Roles;
	Roles.Init(EGBSquadRole::None, Members.Num());
	if (Members.Num() == 0)
	{
		return Roles;
	}

	bool bHasAnchor = false;
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		if (Members[Index].bAnchor)
		{
			Roles[Index] = EGBSquadRole::Anchor;
			bHasAnchor = true;
		}
	}

	// The others, nearest first by distance scaled up with aggression (an aggressive member is never the one that holds back).
	TArray<int32> Order;
	for (int32 Index = 0; Index < Members.Num(); ++Index)
	{
		if (!Members[Index].bAnchor)
		{
			Order.Add(Index);
		}
	}
	Order.Sort([&Members](int32 A, int32 B)
	{
		const float KeyA = Members[A].DistanceToPlayer * (1.f + Members[A].Aggression);
		const float KeyB = Members[B].DistanceToPlayer * (1.f + Members[B].Aggression);
		return KeyA < KeyB;
	});

	int32 Slot = 0;
	for (const int32 Index : Order)
	{
		EGBSquadRole Role;
		if (bHasAnchor)
		{
			// The chief is the base of fire: the rest flank, then push.
			static const EGBSquadRole AfterAnchor[] = { EGBSquadRole::FlankLeft, EGBSquadRole::FlankRight, EGBSquadRole::Assault };
			Role = AfterAnchor[Slot % 3];
		}
		else if (Slot == 0)
		{
			Role = EGBSquadRole::Suppress;
		}
		else if (Slot == 1)
		{
			Role = EGBSquadRole::FlankLeft;
		}
		else if (Slot == 2)
		{
			Role = EGBSquadRole::FlankRight;
		}
		else
		{
			// Extras alternate; an aggressive one always pushes.
			Role = (Members[Index].Aggression > 0.6f || (Slot % 2) == 1) ? EGBSquadRole::Assault : EGBSquadRole::Suppress;
		}
		Roles[Index] = Role;
		++Slot;
	}
	return Roles;
}

TArray<int32> GBTactics::GrantTokens(TConstArrayView<FTokenRequest> Waiting, float HeldWeight, float MaxShooters, int32 TotalMembers, int32 BigSquadSize, float Now, float MinRest)
{
	TArray<int32> Granted;
	const float Capacity = MaxShooters + (TotalMembers >= BigSquadSize ? 1.f : 0.f);
	float Used = HeldWeight;

	TArray<FTokenRequest> Sorted(Waiting);
	Sorted.Sort([Now, MinRest](const FTokenRequest& A, const FTokenRequest& B)
	{
		const bool bRestedA = Now - A.LastBurstEnd >= MinRest;
		const bool bRestedB = Now - B.LastBurstEnd >= MinRest;
		if (bRestedA != bRestedB)
		{
			return bRestedA;
		}
		return A.WaitingSince < B.WaitingSince;
	});
	for (const FTokenRequest& Request : Sorted)
	{
		if (Used + Request.Weight <= Capacity + KINDA_SMALL_NUMBER)
		{
			Granted.Add(Request.Id);
			Used += Request.Weight;
		}
	}
	return Granted;
}

float GBTactics::ScorePosition(const FPositionInput& In, const FPositionWeights& W)
{
	if (In.DistanceToPlayer < In.RangeMin * 0.5f)
	{
		return -1.0e6f;
	}

	float Score = 0.f;
	if (In.bCovered && In.bFiringSpot)
	{
		Score += W.CoverWithFiringSpot;
	}
	else if (!In.bCovered && In.bOpenLineOfFire)
	{
		Score += W.OpenWithLineOfFire;
	}
	else if (In.bCovered)
	{
		Score += W.CoverWithoutFiringSpot;
	}

	// The band: full inside it, falling off linearly to nothing at half a band width outside.
	const float BandWidth = FMath::Max(In.RangeMax - In.RangeMin, 100.f);
	float Outside = 0.f;
	if (In.DistanceToPlayer < In.RangeMin)
	{
		Outside = In.RangeMin - In.DistanceToPlayer;
	}
	else if (In.DistanceToPlayer > In.RangeMax)
	{
		Outside = In.DistanceToPlayer - In.RangeMax;
	}
	Score += W.InBand * (1.f - FMath::Clamp(Outside / (BandWidth * 0.5f), 0.f, 2.f));

	Score += W.Spread * FMath::Clamp(In.MinAngleToOthers / FMath::Max(W.SpreadFullAngle, 1.f), 0.f, 1.f);

	if (In.Role == EGBSquadRole::FlankLeft || In.Role == EGBSquadRole::FlankRight)
	{
		// 60 to 120 degrees round the player from the base of fire is ideal; the reward falls off linearly outside it.
		float Miss = 0.f;
		if (In.AngleFromBase < 60.f)
		{
			Miss = 60.f - In.AngleFromBase;
		}
		else if (In.AngleFromBase > 120.f)
		{
			Miss = In.AngleFromBase - 120.f;
		}
		Score += W.Flank * (1.f - FMath::Clamp(Miss / 60.f, 0.f, 1.f));
		if (In.bInViewCone && In.DistanceToPlayer < In.RangeMin * 1.5f)
		{
			Score -= W.ViewCone;
		}
	}

	Score -= W.Path * FMath::Clamp(In.PathLength / FMath::Max(In.RangeMax, 1.f), 0.f, 2.f);
	if (In.bCrossesFireLine)
	{
		Score -= W.CrossFire;
	}
	// Cornering: a point on a side where the player has room to run cuts that escape (0 weight = off, the default).
	Score += W.Corner * FMath::Clamp(In.EscapeOpenness, 0.f, 1.f);
	return Score;
}

bool GBTactics::IsTooClose(const FVector& P, TConstArrayView<FVector> Others, float Spacing)
{
	for (const FVector& Other : Others)
	{
		if (FVector::Dist2D(P, Other) < Spacing)
		{
			return true;
		}
	}
	return false;
}
