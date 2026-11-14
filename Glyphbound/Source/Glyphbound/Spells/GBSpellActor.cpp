#include "Spells/GBSpellActor.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"
#include "TimerManager.h"
#include "UI/GBPixelSpriteLibrary.h"

namespace
{
	// Pure geometry, not gameplay: how far the aimed point is pulled back from a wall, and how high above a point
	// the ground search starts (so a slope or a step under the aimed point is still found).
	constexpr float SpellActorAimPullBackCm = 20.f;
	constexpr float SpellActorGroundLiftCm = 100.f;
	// A ground search that meets a ceiling on its way up starts this far below it.
	constexpr float SpellActorCeilingClearanceCm = 2.f;
	constexpr float SpellActorMinTickInterval = 0.05f;
}

AGBSpellActor::AGBSpellActor()
{
	PrimaryActorTick.bCanEverTick = false;
	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;
}

void AGBSpellActor::InitSpell(const FGBSpellCast& InCast)
{
	SpellCast = InCast;
	bSpellInitialized = true;
	if (HasActorBegunPlay())
	{
		TryStartSpell();
	}
}

void AGBSpellActor::BeginPlay()
{
	Super::BeginPlay();
	if (!bSpellInitialized)
	{
		// Spawned without the spellbook (placed in a level, or InitSpell will come right after FinishSpawning).
		return;
	}
	TryStartSpell();
}

void AGBSpellActor::TryStartSpell()
{
	if (bSpellStarted || bEnded)
	{
		return;
	}
	bSpellStarted = true;
	StartSpell();
}

void AGBSpellActor::OnCastReleased()
{
	EndSpell();
}

void AGBSpellActor::EndSpell()
{
	if (bEnded)
	{
		return;
	}
	bEnded = true;
	GetWorldTimerManager().ClearAllTimersForObject(this);
	OnSpellEnded.Broadcast();
	Destroy();
}

void AGBSpellActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Destroyed by something else (level unload, era swap): the spellbook must still hear that the spell is over.
	if (!bEnded)
	{
		bEnded = true;
		OnSpellEnded.Broadcast();
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

bool AGBSpellActor::FindGround(const FVector& From, FVector& OutGround, float MaxDistance) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellFindGround), false, this);
	if (AActor* Caster = GetCaster())
	{
		Params.AddIgnoredActor(Caster);
	}
	FHitResult Hit;
	const FVector End = From - FVector::UpVector * MaxDistance;
	if (World->LineTraceSingleByObjectType(Hit, From, End, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
	{
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool AGBSpellActor::FindGroundUnder(const FVector& Point, float Lift, FVector& OutGround, float MaxDistance) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FVector From = Point + FVector::UpVector * FMath::Max(0.f, Lift);
	if (Lift > 0.f)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellFindCeiling), false, this);
		if (AActor* Caster = GetCaster())
		{
			Params.AddIgnoredActor(Caster);
		}
		FHitResult Ceiling;
		if (World->LineTraceSingleByObjectType(Ceiling, Point, From, FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			From.Z = FMath::Max(Point.Z, Ceiling.ImpactPoint.Z - SpellActorCeilingClearanceCm);
		}
	}
	return FindGround(From, OutGround, (From.Z - Point.Z) + MaxDistance);
}

float AGBSpellActor::ReadParam(FName Param, float DefaultValue) const
{
	if (const float* Value = SpellCast.Values.Find(Param))
	{
		return *Value;
	}
	UE_LOG(LogGB, Warning, TEXT("Spell '%s' (%s): param '%s' is missing in Spells.json, using %.2f."),
		*SpellCast.SpellId.ToString(), *GetClass()->GetName(), *Param.ToString(), DefaultValue);
	return DefaultValue;
}

FLinearColor AGBSpellActor::GetSpellColor() const
{
	const FLinearColor ElementColor = GBPlaceholder::ElementColor(static_cast<uint8>(SpellCast.Element));
	return UGBDataSubsystem::ParseColor(SpellCast.GetString(GBString::Color), ElementColor);
}

FName AGBSpellActor::GetSpriteId() const
{
	const FString Sprite = SpellCast.GetString(GBString::Sprite);
	return Sprite.IsEmpty() ? NAME_None : FName(*Sprite);
}

void AGBSpellActor::SetupSpellSprite(UBillboardComponent* Billboard, float DefaultHeight) const
{
	if (!Billboard)
	{
		return;
	}
	const FName SpriteId = GetSpriteId();
	UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(this);
	if (SpriteId.IsNone() || !Library)
	{
		Billboard->SetHiddenInGame(true);
		Billboard->SetVisibility(false);
		return;
	}
	Library->SetupBillboard(Billboard, SpriteId, SpellCast.Get(GBSpellParam::SpriteSize, DefaultHeight));
}

void AGBSpellActor::GetCasterAim(FVector& OutOrigin, FVector& OutDirection) const
{
	OutOrigin = SpellCast.Origin;
	OutDirection = SpellCast.Direction.GetSafeNormal();
	AActor* Caster = GetCaster();
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Caster))
	{
		OutOrigin = Player->GetAimOrigin();
		OutDirection = Player->GetAimDirection();
	}
	else if (Caster)
	{
		FRotator EyeRotation;
		Caster->GetActorEyesViewPoint(OutOrigin, EyeRotation);
		OutDirection = EyeRotation.Vector();
	}
	if (OutDirection.IsNearlyZero())
	{
		OutDirection = FVector::ForwardVector;
	}
}

FVector AGBSpellActor::FindAimedGroundPoint(float MaxRange) const
{
	AActor* Caster = GetCaster();
	FVector Fallback = Caster ? Caster->GetActorLocation() : SpellCast.Origin;
	if (Caster)
	{
		Fallback.Z -= Caster->GetSimpleCollisionHalfHeight();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return Fallback;
	}

	const FVector Dir = SpellCast.Direction.GetSafeNormal();
	FVector Point = SpellCast.Origin;
	if (MaxRange > 0.f && !Dir.IsNearlyZero())
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellAim), false, this);
		if (Caster)
		{
			Params.AddIgnoredActor(Caster);
		}
		FHitResult Hit;
		const FVector End = SpellCast.Origin + Dir * MaxRange;
		if (World->LineTraceSingleByChannel(Hit, SpellCast.Origin, End, ECC_Visibility, Params))
		{
			Point = Hit.ImpactPoint - Dir * SpellActorAimPullBackCm;
		}
		else
		{
			Point = End;
		}
	}
	else
	{
		return FindGroundUnder(Fallback, SpellActorGroundLiftCm, Point) ? Point : Fallback;
	}

	FVector Ground;
	if (FindGroundUnder(Point, SpellActorGroundLiftCm, Ground))
	{
		return Ground;
	}
	return Fallback;
}

bool AGBSpellActor::IsHostileTarget(const AActor* Actor) const
{
	if (!Actor || Actor == this || Actor == GetCaster())
	{
		return false;
	}
	if (!UGBAttributeComponent::Find(Actor) || !UGBCombatLibrary::IsAlive(Actor))
	{
		return false;
	}
	return UGBCombatLibrary::AreHostile(GetCaster(), Actor);
}

void AGBSpellActor::QueryHostiles(const FVector& Center, const FQuat& Rotation, const FCollisionShape& Shape, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GBSpellArea), false, this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Center, Rotation, ObjectParams, Shape, Params);
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (IsHostileTarget(Actor))
		{
			OutActors.AddUnique(Actor);
		}
	}
}

void AGBSpellActor::GatherHostilesInSphere(const FVector& Center, float Radius, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (Radius <= 0.f)
	{
		return;
	}
	QueryHostiles(Center, FQuat::Identity, FCollisionShape::MakeSphere(Radius), OutActors);
}

void AGBSpellActor::GatherHostilesOnDisc(const FVector& GroundCenter, float Radius, float Height, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (Radius <= 0.f || Height <= 0.f)
	{
		return;
	}
	const FVector BoxCenter = GroundCenter + FVector(0.f, 0.f, Height * 0.5f);
	QueryHostiles(BoxCenter, FQuat::Identity, FCollisionShape::MakeBox(FVector(Radius, Radius, Height * 0.5f)), OutActors);
	// The box is only the broad phase: keep actors whose collision touches the disc horizontally.
	OutActors.RemoveAll([&GroundCenter, Radius](const AActor* Actor)
	{
		return FVector::Dist2D(Actor->GetActorLocation(), GroundCenter) > Radius + Actor->GetSimpleCollisionRadius();
	});
}

void AGBSpellActor::GatherHostilesInBox(const FVector& Center, const FQuat& Rotation, const FVector& Extent, TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (Extent.GetMin() <= 0.f)
	{
		return;
	}
	QueryHostiles(Center, Rotation, FCollisionShape::MakeBox(Extent), OutActors);
}

void AGBSpellActor::EndSpellAfter(float Seconds)
{
	if (bEnded)
	{
		return;
	}
	if (Seconds <= 0.f)
	{
		GetWorldTimerManager().SetTimerForNextTick(this, &AGBSpellActor::EndSpell);
	}
	else
	{
		GetWorldTimerManager().SetTimer(EndTimer, this, &AGBSpellActor::EndSpell, Seconds, false);
	}
}

void AGBSpellActor::DamageTarget(AActor* Target, float Amount, const FVector& SourceLocation)
{
	if (!Target)
	{
		return;
	}
	FGBDamageInfo Info = SpellCast.MakeDamageInfo(NAME_None, this);
	Info.Amount = Amount;
	Info.HitLocation = Target->GetActorLocation();
	Info.HitDirection = (Target->GetActorLocation() - SourceLocation).GetSafeNormal();
	UGBCombatLibrary::DealDamage(Target, Info);
}

void AGBSpellActor::ApplyOnHitEffect(AActor* Target) const
{
	const FString EffectId = SpellCast.GetString(GBString::OnHitEffect);
	if (!Target || EffectId.IsEmpty())
	{
		return;
	}
	if (UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(Target))
	{
		Effects->ApplyEffect(FName(*EffectId), GetCaster(),
			SpellCast.Get(GBStat::EffectDuration, 1.f), SpellCast.Get(GBStat::EffectStrength, 1.f));
	}
}

float AGBSpellActor::SafeInterval(float Interval)
{
	return FMath::Max(Interval, SpellActorMinTickInterval);
}

float AGBSpellActor::ComputeGroundFigureZ(const FVector& GroundPoint, float Radius, const TArray<FVector4>& Existing, float Lift, float LayerStep, float MaxRise)
{
	float Top = GroundPoint.Z + Lift;
	for (const FVector4& Entry : Existing)
	{
		const float Dx = Entry.X - GroundPoint.X;
		const float Dy = Entry.Y - GroundPoint.Y;
		if (FMath::Sqrt(Dx * Dx + Dy * Dy) >= Radius + Entry.W)
		{
			continue;
		}
		if (FMath::Abs(Entry.Z - GroundPoint.Z) > MaxRise)
		{
			continue;
		}
		Top = FMath::Max(Top, Entry.Z + LayerStep);
	}
	return Top;
}

void AGBSpellActor::PlaceGroundFigure(UStaticMeshComponent* Figure, float Radius)
{
	if (!Figure)
	{
		return;
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float Lift = Data ? Data->GetGameValue(GBGroundFigure::Lift, 3.f) : 3.f;
	const float LayerStep = Data ? Data->GetGameValue(GBGroundFigure::LayerStep, 1.5f) : 1.5f;
	const float MaxRise = Data ? Data->GetGameValue(GBGroundFigure::LayerMaxRise, 60.f) : 60.f;

	const FVector GroundPoint = GetActorLocation();
	TArray<FVector4> Existing;
	for (TActorIterator<AGBSpellActor> It(World); It; ++It)
	{
		AGBSpellActor* Other = *It;
		if (!Other || Other == this || !Other->bHasGroundFigure || Other->bEnded)
		{
			continue;
		}
		if (Other->IsPendingKillPending())
		{
			continue;
		}
		Existing.Add(FVector4(Other->GetActorLocation().X, Other->GetActorLocation().Y, Other->GroundFigureZ, Other->GroundFigureRadius));
	}

	const float FigureZ = ComputeGroundFigureZ(GroundPoint, Radius, Existing, Lift, LayerStep, MaxRise);
	Figure->SetWorldLocation(FVector(GroundPoint.X, GroundPoint.Y, FigureZ));

	GroundFigureRadius = FMath::Max(1.f, Radius);
	GroundFigureZ = FigureZ;
	bHasGroundFigure = true;
}
