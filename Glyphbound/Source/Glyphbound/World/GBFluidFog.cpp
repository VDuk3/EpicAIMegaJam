#include "World/GBFluidFog.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBProjectile.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessContext.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraSystem.h"
#include "Player/GBPlayerCharacter.h"


namespace
{
	/** Where the gas emitters read what the game writes (NiagaraFluids plugin, global data channels). */
	const TCHAR* CollisionChannelPath = TEXT("/NiagaraFluids/DataChannels/NDC_FluidCollision.NDC_FluidCollision");
	const TCHAR* SourceChannelPath = TEXT("/NiagaraFluids/NDC_Grid3D_Gas.NDC_Grid3D_Gas");

	/** NDC_Grid3D_Gas: a sphere that adds density, velocity and temperature to the gas. */
	struct FGBSourceWriter : public FNDCWriterBase
	{
		NDCVarWriter(FNiagaraPosition, Position);
		// The channel stores Vector3f for the GPU, but its game-side layout is the LWC FVector (Sol review): written as FVector.
		NDCVarWriter(FVector, Velocity);
		NDCVarWriter(float, Radius);
		NDCVarWriterOptional(float, PreviousRadius);
		NDCVarWriterOptional(float, Density);
		NDCVarWriterOptional(float, Temperature);
		NDCVarWriterOptional(FLinearColor, Color);
		NDCVarWriterOptional(float, Divergence);
	};

	// Built on first use: their variable types need the reflection system, which is not up during static initialization.
	FGBSourceWriter& GetSourceWriter() { static FGBSourceWriter Writer; return Writer; }

	/** Directions of a blast ring: eight around the tunnel's axis plane, one up, one down. */
	void BlastDirections(TArray<FVector>& Out)
	{
		for (int32 Index = 0; Index < 8; ++Index)
		{
			const float Angle = Index * UE_TWO_PI / 8.f;
			Out.Add(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f));
		}
		Out.Add(FVector::UpVector);
		Out.Add(-FVector::UpVector);
	}
}

bool GBFluidFog::ClipSegment(const FVector& Start, const FVector& End, const FBox& Box, float& OutT0, float& OutT1)
{
	const FVector Delta = End - Start;
	float T0 = 0.f;
	float T1 = 1.f;
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const float Min = Box.Min[Axis];
		const float Max = Box.Max[Axis];
		if (FMath::IsNearlyZero(Delta[Axis]))
		{
			// Parallel to this pair of faces: inside the slab all along, or never.
			if (Start[Axis] < Min || Start[Axis] > Max)
			{
				return false;
			}
			continue;
		}
		float A = (Min - Start[Axis]) / Delta[Axis];
		float B = (Max - Start[Axis]) / Delta[Axis];
		if (A > B)
		{
			Swap(A, B);
		}
		T0 = FMath::Max(T0, A);
		T1 = FMath::Min(T1, B);
		if (T0 > T1)
		{
			return false;
		}
	}
	OutT0 = T0;
	OutT1 = T1;
	return true;
}

int32 GBFluidFog::ClipSegmentPoints(const FVector& Start, const FVector& End, const FBox& Box, float Spacing, int32 MaxPoints, TArray<FVector>& OutPoints)
{
	float T0 = 0.f;
	float T1 = 0.f;
	if (MaxPoints <= 0 || !ClipSegment(Start, End, Box, T0, T1))
	{
		return 0;
	}
	const FVector From = FMath::Lerp(Start, End, T0);
	const FVector To = FMath::Lerp(Start, End, T1);
	const float Length = FVector::Dist(From, To);
	const float Step = FMath::Max(Spacing, 1.f);
	if (Length <= KINDA_SMALL_NUMBER || MaxPoints == 1)
	{
		OutPoints.Add(From);
		return 1;
	}
	int32 Count = FMath::FloorToInt(Length / Step) + 1;
	float UsedStep = Step;
	if (Count > MaxPoints)
	{
		Count = MaxPoints;
		UsedStep = Length / (MaxPoints - 1);
	}
	const FVector Direction = (To - From) / Length;
	for (int32 Index = 0; Index < Count; ++Index)
	{
		OutPoints.Add(From + Direction * (UsedStep * Index));
	}
	return Count;
}

AGBFluidFog::AGBFluidFog()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	RootComponent = Box;
	Box->SetBoxExtent(BoxExtent);
	Box->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Box->SetCanEverAffectNavigation(false);
	Box->SetHiddenInGame(true);

	Fluid = CreateDefaultSubobject<UNiagaraComponent>(TEXT("Fluid"));
	Fluid->SetupAttachment(Box);
	Fluid->SetAutoActivate(false);
	Fluid->SetCanEverAffectNavigation(false);
}

FBox AGBFluidFog::GetBox() const
{
	const FVector Centre = GetActorLocation();
	return FBox(Centre - BoxExtent, Centre + BoxExtent);
}

void AGBFluidFog::LoadSettings()
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	auto Value = [Data](const TCHAR* Key, float Default) { return Data ? Data->GetGameValue(FName(Key), Default) : Default; };
	Resolution = FMath::Max(8, FMath::RoundToInt(Value(TEXT("FluidFog.Resolution"), 192.f)));
	ActiveRange = Value(TEXT("FluidFog.ActiveRange"), 3000.f);
	WarmupSeconds = Value(TEXT("FluidFog.WarmupSeconds"), 2.f);
	PlayerRadius = Value(TEXT("FluidFog.PlayerRadius"), 45.f);
	ShotRadius = Value(TEXT("FluidFog.ShotRadius"), 12.f);
	ShotPush = Value(TEXT("FluidFog.ShotPush"), 1500.f);
	ShotSpacing = FMath::Max(1.f, Value(TEXT("FluidFog.ShotSpacing"), 40.f));
	ShotFrames = FMath::Max(1, FMath::RoundToInt(Value(TEXT("FluidFog.ShotFrames"), 3.f)));
	PuffPush = Value(TEXT("FluidFog.PuffPush"), 500.f);
	PuffRadius = Value(TEXT("FluidFog.PuffRadius"), 40.f);
	ProjectileRadius = Value(TEXT("FluidFog.ProjectileRadius"), 25.f);
	BlastPush = Value(TEXT("FluidFog.BlastPush"), 1200.f);
	BlastRadius = Value(TEXT("FluidFog.BlastRadius"), 90.f);
	EnemyRange = Value(TEXT("FluidFog.EnemyRange"), 1500.f);
	MaxEntries = FMath::Max(4, FMath::RoundToInt(Value(TEXT("FluidFog.MaxEntries"), 64.f)));
	bGridCornered = Value(TEXT("FluidFog.GridCornered"), 0.f) > 0.5f;
	FillDensity = Value(TEXT("FluidFog.FillDensity"), 2.f);
	PushDivergence = Value(TEXT("FluidFog.PushDivergence"), 1.f);
}

void AGBFluidFog::ApplyBox()
{
	Box->SetBoxExtent(BoxExtent);
	if (!Fluid)
	{
		return;
	}
	// The template's grid may be centred on the component or start at its corner: data says which.
	Fluid->SetRelativeLocation(bGridCornered ? -BoxExtent : FVector::ZeroVector);
	Fluid->SetVariableVec3(FName(TEXT("User.WorldSpaceSize")), BoxExtent * 2.0);
	Fluid->SetVariableInt(FName(TEXT("User.ResolutionMaxAxis")), Resolution);
	// The fill: one low, long ellipsoid source along the tunnel floor (the system emits in its local space, the grid is centred
	// on the component). Radius = most of the half width; the long axis is the box's longer horizontal one.
	const bool bAlongX = BoxExtent.X >= BoxExtent.Y;
	const float HalfWidth = FMath::Max(10.f, float(bAlongX ? BoxExtent.Y : BoxExtent.X));
	const float HalfLength = float(bAlongX ? BoxExtent.X : BoxExtent.Y);
	const float FillRadius = HalfWidth * 0.85f;
	const float LongScale = FMath::Max(1.f, HalfLength * 0.9f / FillRadius);
	const float HighScale = FMath::Clamp(float(BoxExtent.Z) * 0.4f / FillRadius, 0.1f, 1.f);
	Fluid->SetVariableFloat(FName(TEXT("User.FillRadius")), FillRadius);
	Fluid->SetVariableVec3(FName(TEXT("User.FillScale")), bAlongX ? FVector(LongScale, 1.0, HighScale) : FVector(1.0, LongScale, HighScale));
	Fluid->SetVariableFloat(FName(TEXT("User.FillDensity")), FillDensity);
	Fluid->SetVariableVec3(FName(TEXT("User.Emit Position")), FVector(0.0, 0.0, -BoxExtent.Z * 0.55));
}

void AGBFluidFog::BeginPlay()
{
	Super::BeginPlay();
	LoadSettings();

	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FString SystemPath = Data ? Data->GetGameString(FName(TEXT("FluidFog.System")), FString()) : FString();
	UNiagaraSystem* System = SystemPath.IsEmpty() ? nullptr : LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
	if (System)
	{
		Fluid->SetAsset(System);
		bHasSystem = true;
		ApplyBox();
	}
	else if (!bWarned)
	{
		bWarned = true;
		UE_LOG(LogGB, Warning, TEXT("GBFluidFog %s: the Niagara system '%s' does not exist yet; the fog does nothing"), *GetName(), *SystemPath);
	}

	// Kept alive by the editor-visible references below (the assets are small, loading them once is enough).
	CollisionChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, CollisionChannelPath);
	SourceChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, SourceChannelPath);
	if (bHasSystem && (!CollisionChannel || !SourceChannel))
	{
		UE_LOG(LogGB, Warning, TEXT("GBFluidFog %s: a Niagara Fluids data channel is missing (collision %d, source %d); the gas will not react to the game"),
			*GetName(), CollisionChannel ? 1 : 0, SourceChannel ? 1 : 0);
	}

	ShotHandle = UGBCombatLibrary::OnShotTraced().AddUObject(this, &AGBFluidFog::OnShotTraced);
}

void AGBFluidFog::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UGBCombatLibrary::OnShotTraced().Remove(ShotHandle);
	ShotHandle.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGBFluidFog::SetSimulating(bool bOn)
{
	if (bOn == bSimulating)
	{
		return;
	}
	bSimulating = bOn;
	if (bOn)
	{
		ApplyBox();
		Fluid->Activate(true);
		if (WarmupSeconds > 0.f)
		{
			// A fixed-step catch-up, so the smoke is already spread when the player arrives.
			Fluid->AdvanceSimulationByTime(WarmupSeconds, 1.f / 15.f);
		}
	}
	else
	{
		Fluid->DeactivateImmediate();
		PendingCollision.Reset();
		PendingSources.Reset();
		Tracked.Reset();
		LastCollisionEntries = LastPlayerEntries = LastShotEntries = LastSourceEntries = 0;
	}
}

void AGBFluidFog::AddShot(const FVector& Start, const FVector& End)
{
	const FBox Inside = GetBox();
	const FVector Direction = (End - Start).GetSafeNormal();
	if (Direction.IsNearlyZero())
	{
		return;
	}
	TArray<FVector> Points;
	// A quarter of the cap per shot at most: a shotgun's pellets must not push the player out of the list.
	GBFluidFog::ClipSegmentPoints(Start, End, Inside.ExpandBy(ShotRadius), ShotSpacing, FMath::Max(2, MaxEntries / 4), Points);
	for (const FVector& Point : Points)
	{
		if (PendingCollision.Num() >= MaxEntries * 2)
		{
			break;
		}
		FEntry Entry;
		Entry.Position = Point;
		Entry.Velocity = Direction * ShotPush;
		Entry.Radius = ShotRadius;
		Entry.FramesLeft = ShotFrames;
		PendingCollision.Add(Entry);
	}
	// A puff where it ends inside the box (the impact): outward velocity in the six axis directions.
	if (Inside.IsInside(End) && PendingSources.Num() < MaxEntries * 2)
	{
		const FVector Axes[6] = { FVector::ForwardVector, -FVector::ForwardVector, FVector::RightVector, -FVector::RightVector, FVector::UpVector, -FVector::UpVector };
		for (const FVector& Axis : Axes)
		{
			FEntry Entry;
			Entry.Position = End + Axis * PuffRadius * 0.5f;
			Entry.Velocity = Axis * PuffPush;
			Entry.Radius = PuffRadius;
			Entry.FramesLeft = ShotFrames;
			PendingSources.Add(Entry);
		}
	}
}

void AGBFluidFog::OnShotTraced(const FVector& Start, const FVector& End, float Speed)
{
	if (bSimulating)
	{
		AddShot(Start, End);
	}
}

void AGBFluidFog::AddBlast(const FVector& Where)
{
	TArray<FVector> Directions;
	BlastDirections(Directions);
	for (const FVector& Direction : Directions)
	{
		if (PendingSources.Num() >= MaxEntries * 2)
		{
			break;
		}
		FEntry Entry;
		Entry.Position = Where + Direction * BlastRadius * 0.5f;
		Entry.Velocity = Direction * BlastPush;
		Entry.Radius = BlastRadius;
		Entry.FramesLeft = ShotFrames;
		PendingSources.Add(Entry);
	}
}

void AGBFluidFog::GatherProjectiles(TArray<FEntry>& Collision)
{
	UWorld* World = GetWorld();
	const FBox Reach = GetBox().ExpandBy(ProjectileRadius);
	TSet<const AActor*> Seen;
	for (TActorIterator<AGBProjectile> It(World); It; ++It)
	{
		AGBProjectile* Projectile = *It;
		if (!IsValid(Projectile) || !Reach.IsInside(Projectile->GetActorLocation()))
		{
			continue;
		}
		Seen.Add(Projectile);
		const UProjectileMovementComponent* Movement = Projectile->FindComponentByClass<UProjectileMovementComponent>();
		if (Collision.Num() < MaxEntries)
		{
			FEntry Entry;
			Entry.Position = Projectile->GetActorLocation();
			Entry.Velocity = Movement ? Movement->Velocity : FVector::ZeroVector;
			Entry.Radius = ProjectileRadius;
			Collision.Add(Entry);
		}
		FTrackedProjectile* Existing = Tracked.FindByPredicate([Projectile](const FTrackedProjectile& T) { return T.Projectile.Get() == Projectile; });
		if (Existing)
		{
			Existing->LastPosition = Projectile->GetActorLocation();
		}
		else
		{
			FTrackedProjectile NewTracked;
			NewTracked.Projectile = Projectile;
			NewTracked.LastPosition = Projectile->GetActorLocation();
			Tracked.Add(NewTracked);
		}
	}
	// A tracked projectile that vanished inside the box exploded there; one that left the box is just forgotten.
	for (int32 Index = Tracked.Num() - 1; Index >= 0; --Index)
	{
		const AActor* Actor = Tracked[Index].Projectile.Get();
		if (!Actor)
		{
			AddBlast(Tracked[Index].LastPosition);
			Tracked.RemoveAtSwap(Index);
		}
		else if (!Seen.Contains(Actor))
		{
			Tracked.RemoveAtSwap(Index);
		}
	}
}

void AGBFluidFog::GatherEnemies(const FVector& PlayerLocation, TArray<FEntry>& Collision)
{
	const FBox Inside = GetBox();
	const float RangeSquared = FMath::Square(EnemyRange);
	for (TActorIterator<AGBEnemyCharacter> It(GetWorld()); It; ++It)
	{
		const AGBEnemyCharacter* Enemy = *It;
		if (Collision.Num() >= MaxEntries)
		{
			break;
		}
		if (!IsValid(Enemy) || Enemy->IsDead() || FVector::DistSquared(Enemy->GetActorLocation(), PlayerLocation) > RangeSquared)
		{
			continue;
		}
		const float Radius = Enemy->GetCapsuleComponent() ? Enemy->GetCapsuleComponent()->GetScaledCapsuleRadius() * 0.9f : 40.f;
		if (!Inside.ExpandBy(Radius).IsInside(Enemy->GetActorLocation()))
		{
			continue;
		}
		FEntry Entry;
		Entry.Position = Enemy->GetActorLocation();
		Entry.Velocity = Enemy->GetVelocity();
		Entry.Radius = Radius;
		Collision.Add(Entry);
	}
}

int32 AGBFluidFog::WriteChannels(const TArray<FEntry>& Collision, const TArray<FEntry>& Sources)
{
	int32 Written = 0;
	UWorld* World = GetWorld();
	// No gas template reads NDC_FluidCollision (checked in the plugin's content), so the moving spheres go to the gas
	// channel too, as sources with outward divergence and the body's velocity: the smoke is pushed out of them and dragged along.
	const int32 Total = Collision.Num() + Sources.Num();
	if (Total > 0 && SourceChannel && SourceChannel->Get())
	{
		FGBSourceWriter& Writer = GetSourceWriter();
		Writer.DebugSource = TEXT("GBFluidFog");
		if (Writer.BeginWrite(World, SourceChannel->Get(), FNiagaraDataChannelSearchParameters(), Total, false, false, true))
		{
			for (int32 Index = 0; Index < Total; ++Index)
			{
				// A pure push: no density and no heat, only velocity (and, for a moving body, outward divergence) inside the sphere.
				const bool bBody = Index < Collision.Num();
				const FEntry& Entry = bBody ? Collision[Index] : Sources[Index - Collision.Num()];
				const bool bPlaced = Writer.WritePosition(Index, Entry.Position);
				const bool bPushed = Writer.WriteVelocity(Index, Entry.Velocity);
				Writer.WriteRadius(Index, Entry.Radius);
				Writer.WritePreviousRadius(Index, Entry.Radius);
				Writer.WriteDensity(Index, 0.f);
				Writer.WriteTemperature(Index, 0.f);
				Writer.WriteColor(Index, FLinearColor::Black);
				// Bodies push the gas out of their sphere; puffs and blasts burst outwards.
				Writer.WriteDivergence(Index, PushDivergence);
				Written += (bPlaced && bPushed) ? 1 : 0;
			}
		}
		Writer.EndWrite();
	}
	return Written;
}

void AGBFluidFog::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bHasSystem)
	{
		return;
	}

	const AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	const FBox Inside = GetBox();
	const FVector PlayerLocation = Player ? Player->GetActorLocation() : FVector::ZeroVector;
	const float Distance = Player ? FMath::Sqrt(Inside.ComputeSquaredDistanceToPoint(PlayerLocation)) : TNumericLimits<float>::Max();
	// A little hysteresis, so standing on the range does not switch the simulation on and off.
	SetSimulating(Distance <= (bSimulating ? ActiveRange * 1.1f : ActiveRange));
	if (!bSimulating)
	{
		return;
	}

	TArray<FEntry> Collision;
	TArray<FEntry> Sources;
	Collision.Reserve(MaxEntries);

	// The player: two spheres (chest and feet), moving with the character.
	LastPlayerEntries = 0;
	if (Player)
	{
		const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
		const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 90.f;
		const FVector Velocity = Player->GetVelocity();
		const FVector Spots[2] = { PlayerLocation, PlayerLocation - FVector::UpVector * HalfHeight * 0.6f };
		const FBox Reach = Inside.ExpandBy(PlayerRadius);
		for (const FVector& Spot : Spots)
		{
			if (Reach.IsInside(Spot))
			{
				FEntry Entry;
				Entry.Position = Spot;
				Entry.Velocity = Velocity;
				Entry.Radius = PlayerRadius;
				Collision.Add(Entry);
				++LastPlayerEntries;
			}
		}
	}

	// Shots and blasts written on earlier frames (and this one) stay for ShotFrames frames.
	LastShotEntries = 0;
	for (const FEntry& Pending : PendingCollision)
	{
		if (Collision.Num() < MaxEntries)
		{
			Collision.Add(Pending);
			++LastShotEntries;
		}
	}
	GatherProjectiles(Collision);
	GatherEnemies(PlayerLocation, Collision);
	// GatherProjectiles may have queued a blast: it is written from this frame on.
	for (const FEntry& Pending : PendingSources)
	{
		if (Sources.Num() < MaxEntries)
		{
			Sources.Add(Pending);
		}
	}

	// The counters say what reached the channel (the smoke checks them): nothing when the channel or the write failed.
	const int32 Written = WriteChannels(Collision, Sources);
	if (Written < Collision.Num() + Sources.Num())
	{
		LastPlayerEntries = LastShotEntries = 0;
	}
	LastCollisionEntries = FMath::Min(Written, Collision.Num());
	LastSourceEntries = FMath::Max(0, Written - Collision.Num());
	TotalShotEntries += LastShotEntries;

	for (int32 Index = PendingCollision.Num() - 1; Index >= 0; --Index)
	{
		if (--PendingCollision[Index].FramesLeft <= 0)
		{
			PendingCollision.RemoveAtSwap(Index);
		}
	}
	for (int32 Index = PendingSources.Num() - 1; Index >= 0; --Index)
	{
		if (--PendingSources[Index].FramesLeft <= 0)
		{
			PendingSources.RemoveAtSwap(Index);
		}
	}
}
