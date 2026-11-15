#include "World/GBMimic.h"

#include "Combat/GBCombatLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "World/GBPickup.h"
#include "World/GBProgressSubsystem.h"

namespace GBMimicLocal
{
	// The measures of the chest (GBChest.cpp): body 140 x 90 x 76, lid 146 x 96 x 18, hinge on the long back edge (-Y).
	constexpr float BodyHalfDepth = 45.f;
	constexpr float BodyHeight = 76.f;
	constexpr float LidHalfDepth = 48.f;
	constexpr float LidHalfThickness = 9.f;
	constexpr float LidDepth = 96.f;
	// Jaw angles (degrees): wide open, the resting gape while it hops.
	constexpr float JawOpen = 62.f;
	constexpr float JawGape = 38.f;
	constexpr float SnapTime = 0.22f;
	constexpr float HopHeight = 70.f;
	constexpr float KeepFromPlayer = 150.f;      // it lands this far from the centre of the player, never on top
	constexpr float FloorProbeUp = 120.f;
	constexpr float FloorProbeDown = 260.f;
	constexpr float DeathSeconds = 0.6f;
	constexpr int32 TeethPerJaw = 6;
	const FVector ToothSize(13.f, 9.f, 17.f);
	constexpr float ToothSpacing = 21.f;
	const FLinearColor Bone(0.92f, 0.9f, 0.72f);
	const FLinearColor Gum(0.29f, 0.03f, 0.03f);
	const FLinearColor Tongue(0.87f, 0.1f, 0.07f);
	const FLinearColor Eye(1.f, 0.75f, 0.1f);
}

AGBMimic::AGBMimic()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;      // asleep it costs nothing
}

FVector AGBMimic::LidPoint(const FVector& HingeFrame, float AngleDeg)
{
	const float Rad = FMath::DegreesToRadians(AngleDeg);
	const float C = FMath::Cos(Rad);
	const float S = FMath::Sin(Rad);
	return FVector(HingeFrame.X,
		-GBMimicLocal::LidHalfDepth + HingeFrame.Y * C - HingeFrame.Z * S,
		GBMimicLocal::BodyHeight + HingeFrame.Y * S + HingeFrame.Z * C);
}

FName AGBMimic::GetDeadFlag() const
{
	return OpenedFlag.IsNone() ? FName(*FString::Printf(TEXT("Mimic.%s"), *GetName())) : OpenedFlag;
}

void AGBMimic::BeginPlay()
{
	Super::BeginPlay();

	const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (Progress && Progress->HasFlag(GetDeadFlag()))
	{
		Destroy();          // killed in an earlier visit
		return;
	}

	UGBStatsComponent* Stats = NewObject<UGBStatsComponent>(this, TEXT("MimicStats"));
	Stats->RegisterComponent();
	AddInstanceComponent(Stats);
	Stats->SetBaseStat(GBStat::MaxHealth, MaxHealth);
	Stats->SetBaseStat(GBStat::DamageTaken, 1.f);
	Stats->SetBaseStat(GBStat::HealthRegen, 0.f);
	Stats->SetBaseStat(GBStat::MaxMana, 0.f);

	Attributes = NewObject<UGBAttributeComponent>(this, TEXT("MimicAttributes"));
	Attributes->Team = EGBTeam::Neutral;      // asleep nothing marks it as an enemy; a hit still reaches it and wakes it
	Attributes->RegisterComponent();
	AddInstanceComponent(Attributes);
	Attributes->InitializeFromStats();
	Attributes->OnDamaged.AddUObject(this, &AGBMimic::HandleDamaged);
	Attributes->OnDeath.AddUObject(this, &AGBMimic::HandleDeath);

	// Projectiles only overlap WorldDynamic things: the body must be one for spells to land on it.
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	Mesh->SetGenerateOverlapEvents(true);
	Mesh->SetCanEverAffectNavigation(false);
}

UStaticMeshComponent* AGBMimic::AddBlock(const TCHAR* Name, const FVector& SizeCm, const FLinearColor& Color, bool bGlow)
{
	UStaticMeshComponent* Block = NewObject<UStaticMeshComponent>(this, Name);
	Block->SetupAttachment(Root);
	Block->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Block->SetGenerateOverlapEvents(false);
	Block->SetCanEverAffectNavigation(false);
	Block->RegisterComponent();
	GBPlaceholder::SetupShape(Block, EGBShape::Cube, SizeCm, Color, bGlow);
	return Block;
}

void AGBMimic::Wake()
{
	if (bAwake || bDying)
	{
		return;
	}
	bAwake = true;
	InteractVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (Attributes)
	{
		Attributes->Team = EGBTeam::Enemy;
	}

	using namespace GBMimicLocal;
	// Lower jaw: gums over the top of the body, teeth along its front edge, the tongue lolling out over them.
	UStaticMeshComponent* Gums = AddBlock(TEXT("Gums"), FVector(132.f, 82.f, 3.f), Gum, false);
	Gums->SetRelativeLocation(FVector(0.f, 0.f, BodyHeight + 1.5f));
	const float First = -ToothSpacing * (TeethPerJaw - 1) * 0.5f;
	for (int32 Index = 0; Index < TeethPerJaw; ++Index)
	{
		UStaticMeshComponent* Lower = AddBlock(*FString::Printf(TEXT("LowerTooth%d"), Index), ToothSize, Bone, false);
		Lower->SetRelativeLocation(FVector(First + ToothSpacing * Index, BodyHalfDepth - ToothSize.Y * 0.5f - 2.f, BodyHeight + ToothSize.Z * 0.5f));
		UpperTeeth.Add(AddBlock(*FString::Printf(TEXT("UpperTooth%d"), Index), ToothSize, Bone, false));
	}
	UStaticMeshComponent* TongueRoot = AddBlock(TEXT("Tongue"), FVector(46.f, 70.f, 8.f), Tongue, false);
	TongueRoot->SetRelativeLocation(FVector(0.f, 8.f, BodyHeight + 7.f));
	UStaticMeshComponent* TongueTip = AddBlock(TEXT("TongueTip"), FVector(38.f, 34.f, 8.f), Tongue, false);
	TongueTip->SetRelativeLocationAndRotation(FVector(0.f, BodyHalfDepth + 10.f, BodyHeight - 4.f), FRotator(0.f, 0.f, 40.f));
	Palate = AddBlock(TEXT("Palate"), FVector(132.f, 82.f, 3.f), Gum, false);
	Eyes.Add(AddBlock(TEXT("EyeL"), FVector(15.f, 4.f, 9.f), Eye, true));
	Eyes.Add(AddBlock(TEXT("EyeR"), FVector(15.f, 4.f, 9.f), Eye, true));

	SetJaw(JawOpen);
	bHopping = false;
	StateSeconds = 0.f;
	BiteSeconds = 0.35f;          // a beat to see what it is before the first bite
	AwakeSeconds = 0.f;
	SetActorTickEnabled(true);
	UE_LOG(LogGB, Display, TEXT("Mimic %s: awake at %s"), *GetName(), *GetActorLocation().ToCompactString());
}

void AGBMimic::SetJaw(float AngleDeg)
{
	using namespace GBMimicLocal;
	const FRotator LidRotation(0.f, 0.f, -AngleDeg);          // roll: the front edge goes up
	Lid->SetRelativeLocationAndRotation(LidPoint(FVector(0.f, LidHalfDepth, LidHalfThickness), AngleDeg), LidRotation);
	if (Palate)
	{
		Palate->SetRelativeLocationAndRotation(LidPoint(FVector(0.f, LidHalfDepth, -1.5f), AngleDeg), LidRotation);
	}
	// The upper row sits half a step aside, so the two rows mesh when the jaw shuts.
	const float First = -ToothSpacing * (TeethPerJaw - 1) * 0.5f + ToothSpacing * 0.5f;
	for (int32 Index = 0; Index < UpperTeeth.Num(); ++Index)
	{
		if (UStaticMeshComponent* Tooth = UpperTeeth[Index])
		{
			const float X = FMath::Min(First + ToothSpacing * Index, 62.f);
			Tooth->SetRelativeLocationAndRotation(LidPoint(FVector(X, LidDepth - ToothSize.Y * 0.5f - 5.f, -ToothSize.Z * 0.5f), AngleDeg), LidRotation);
		}
	}
	for (int32 Index = 0; Index < Eyes.Num(); ++Index)
	{
		if (UStaticMeshComponent* EyeBlock = Eyes[Index])
		{
			EyeBlock->SetRelativeLocationAndRotation(LidPoint(FVector(Index == 0 ? -36.f : 36.f, LidDepth + 1.f, LidHalfThickness), AngleDeg), LidRotation);
		}
	}
}

void AGBMimic::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	Wake();          // no loot: the "chest" opens its mouth instead
}

void AGBMimic::HandleDamaged(const FGBDamageInfo& Info, float ActualDamage)
{
	Wake();
}

void AGBMimic::HandleDeath(const FGBDamageInfo& KillingBlow)
{
	if (bDying)
	{
		return;
	}
	bDying = true;
	bAwake = false;
	SetActorTickEnabled(false);
	SetJaw(0.f);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	int32 Dropped = 0;
	for (const FGBDropDefinition& Entry : Contents)
	{
		if (AGBPickup::IsDropValid(this, Entry))
		{
			AGBPickup::SpawnPickup(GetWorld(), Entry, GetActorLocation() + FVector(40.f * Dropped, 0.f, 40.f));
			++Dropped;
		}
	}
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->SetFlag(GetDeadFlag());
	}
	UE_LOG(LogGB, Display, TEXT("Mimic %s: dead, %d drops"), *GetName(), Dropped);
	SetLifeSpan(GBMimicLocal::DeathSeconds);          // never destroyed inside the damage call that killed it
}

void AGBMimic::Bite(AActor* Target)
{
	FGBDamageInfo Info;
	Info.Amount = BiteDamage;
	Info.Element = EGBElement::Physical;
	Info.Instigator = this;
	Info.Causer = this;
	Info.HitLocation = Target->GetActorLocation();
	Info.HitDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	Info.SourceId = TEXT("MimicBite");
	const float Dealt = UGBCombatLibrary::DealDamage(Target, Info);
	SnapSeconds = GBMimicLocal::SnapTime;
	BiteSeconds = BiteCooldown;
	UE_LOG(LogGB, Display, TEXT("Mimic %s: bite for %.0f"), *GetName(), Dealt);
}

void AGBMimic::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	using namespace GBMimicLocal;
	UWorld* World = GetWorld();
	if (!bAwake || !World)
	{
		return;
	}
	AwakeSeconds += DeltaSeconds;
	BiteSeconds = FMath::Max(0.f, BiteSeconds - DeltaSeconds);

	// Jaw: snaps shut on a bite, otherwise chews the air a little.
	if (SnapSeconds > 0.f)
	{
		SnapSeconds = FMath::Max(0.f, SnapSeconds - DeltaSeconds);
		const float Alpha = SnapSeconds / SnapTime;                         // 1 -> 0
		SetJaw(FMath::Lerp(JawOpen, 4.f, FMath::Sin(Alpha * PI)));          // shut in the middle of the snap
	}
	else
	{
		SetJaw(JawGape + (JawOpen - JawGape) * 0.5f * (1.f + FMath::Sin(AwakeSeconds * 7.f)));
	}

	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player || !UGBCombatLibrary::IsAlive(Player))
	{
		return;
	}
	const FVector Here = GetActorLocation();
	FVector ToPlayer = Player->GetActorLocation() - Here;
	ToPlayer.Z = 0.f;
	const float Distance = ToPlayer.Size();
	const FVector Dir = Distance > 1.f ? ToPlayer / Distance : GetActorRightVector();

	if (bHopping)
	{
		StateSeconds += DeltaSeconds;
		const float Alpha = FMath::Clamp(StateSeconds / FMath::Max(0.05f, HopSeconds), 0.f, 1.f);
		FVector At = FMath::Lerp(HopFrom, HopTo, Alpha);
		At.Z += HopHeight * 4.f * Alpha * (1.f - Alpha);
		SetActorLocation(At);
		if (Alpha >= 1.f)
		{
			bHopping = false;
			StateSeconds = 0.f;
		}
		return;
	}

	// On the ground: face the player (the front of the chest is +Y, so its right vector looks at them).
	SetActorRotation(FRotator(0.f, Dir.Rotation().Yaw - 90.f, 0.f));

	if (Distance <= BiteRange)
	{
		if (BiteSeconds <= 0.f)
		{
			Bite(Player);
		}
		return;
	}
	if (Distance > ChaseRange)
	{
		return;
	}
	StateSeconds += DeltaSeconds;
	if (StateSeconds < HopRestSeconds)
	{
		return;
	}

	// Next hop: towards the player, never onto them, stopped by walls, landing on a floor.
	const float Step = FMath::Min(HopDistance, Distance - KeepFromPlayer);
	if (Step < 20.f)
	{
		return;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBMimicHop), false, this);
	Params.AddIgnoredActor(Player);
	const FVector ChestHigh(0.f, 0.f, BodyHeight * 0.6f);
	FVector Target = Here + Dir * Step;
	FHitResult Wall;
	if (World->SweepSingleByChannel(Wall, Here + ChestHigh, Target + ChestHigh, FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(40.f), Params))
	{
		const float Free = FMath::Max(0.f, Wall.Distance - 50.f);
		if (Free < 20.f)
		{
			StateSeconds = 0.f;          // blocked: wait and try again (the player will have moved)
			return;
		}
		Target = Here + Dir * Free;
	}
	FHitResult Floor;
	if (!World->LineTraceSingleByChannel(Floor, Target + FVector(0.f, 0.f, FloorProbeUp), Target - FVector(0.f, 0.f, FloorProbeDown), ECC_WorldStatic, Params))
	{
		StateSeconds = 0.f;              // no floor there (a pit): it does not jump off
		return;
	}
	Target.Z = Floor.ImpactPoint.Z;
	HopFrom = Here;
	HopTo = Target;
	bHopping = true;
	StateSeconds = 0.f;
}
