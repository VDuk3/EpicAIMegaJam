#include "Combat/GBProjectile.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBCombatLibrary.h"
#include "Combat/GBExplosionEffect.h"
#include "Core/GBPlaceholder.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "UI/GBPixelSpriteLibrary.h"
#include "CollisionQueryParams.h"
#include "Components/BillboardComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"

namespace GBProjectileLocal
{
	/** Same lift as the radial damage obstruction trace (GBCombatLibrary.cpp), so a blast on the floor is not blocked by it. */
	constexpr float KnockbackTraceLiftCm = 5.f;
}

// Instant lightning (owner, 2026-11-15): the Storm staff's shot and the Lightning Bolt spell do not fly. A projectile whose
// FlightVfx names "LightningArc" resolves its whole path on its first tick (AGBProjectile::Tick), through the same
// HitTarget / Explode / Finish code as a flying one, so damage, build-up, weak points and OnImpact (the staff's splash,
// the spell's area hit) are unchanged. Then a purple arc of the LAPTOP's lightning sprites (Tools/Jam/import_laptop_lightning.js)
// flickers from the staff's head to where it hit for a moment. The arc is half-ribbon tiles one after another, each
// turned about the bolt's axis to face the camera and sized to its distance from it, so the bolt keeps the same
// thickness on screen from the staff to far away. The same rule renders the previews in that script.
// Data: FlightVfx "LightningArc", FlightVfxHeight = the ribbon's height in cm per metre from the camera (12 or more =
// the big spell look: three layers, longer), ImpactVfx (the hit clip), ImpactVfxSize = the hit clip's smallest width, cm.
// Used by Spells/GBSpell_Projectile.cpp too (declared there), for the small arcs of the spell's area hit.
namespace GBLightningFx
{
	const FString ArcPrefix(TEXT("LightningArc"));
	const FName SparkClip(TEXT("LightningSpark"));
	/** Height of the ribbon clips' frame in their own cm (48 px at 1 cm per px). Half tiles are 128 wide, quarters 64. */
	constexpr float ClipHeightCm = 48.f;
	constexpr float HalfAspect = 128.f / 48.f;
	constexpr int32 MaxTilesPerLayer = 40;
	constexpr float BigHeightPer100 = 12.f;
	constexpr float FlickerStep = 0.035f;

	bool IsArcClip(FName ClipId)
	{
		return !ClipId.IsNone() && ClipId.ToString().StartsWith(ArcPrefix);
	}

	const FGBPixelVfxClip* FindClip(const UObject* WorldContext, FName ClipId)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		return Vfx ? Vfx->FindClip(ClipId) : nullptr;
	}

	FName TileClip(int32 Index, bool bQuarter)
	{
		// Halves alternate H0, H1 (seamless: they are the two halves of one tileable ribbon); a quarter is the left
		// quarter of the half that would come next.
		return bQuarter ? FName(*FString::Printf(TEXT("LightningArcQ%d"), (Index % 2) * 2)) : FName(*FString::Printf(TEXT("LightningArcH%d"), Index % 2));
	}

	bool HasClips(const UObject* WorldContext)
	{
		for (int32 Index = 0; Index < 2; ++Index)
		{
			if (!FindClip(WorldContext, TileClip(Index, false)) || !FindClip(WorldContext, TileClip(Index, true)))
			{
				return false;
			}
		}
		return true;
	}

	FVector CameraLocation(const UWorld* World, const FVector& Fallback)
	{
		const APlayerController* Viewer = World ? World->GetFirstPlayerController() : nullptr;
		return (Viewer && Viewer->PlayerCameraManager) ? Viewer->PlayerCameraManager->GetCameraLocation() : Fallback;
	}

	FName LayerTag(int32 Layer)
	{
		return FName(*FString::Printf(TEXT("LightningLayer%d"), Layer));
	}

	/** One layer of the arc from A to B, built on Owner. Same tiling as arcTiles() in import_laptop_lightning.js. */
	void BuildLayer(AActor* Owner, const FVector& A, const FVector& B, const FVector& Cam, float HeightPer100, float Phase, int32 Layer)
	{
		const FVector Dir = (B - A).GetSafeNormal();
		const float Length = FVector::Dist(A, B);
		if (Dir.IsNearlyZero())
		{
			return;
		}
		float Pos = 0.f;
		for (int32 Index = 0; Index < MaxTilesPerLayer && Pos < Length; ++Index)
		{
			const FVector P = A + Dir * Pos;
			const float Height = FMath::Max(3.f, HeightPer100 * FVector::Dist(P, Cam) / 100.f);
			const float Width = Height * HalfAspect;
			const float Remaining = Length - Pos;
			bool bQuarter = false;
			if (Remaining < Width * 0.25f)
			{
				break;
			}
			if (Remaining < Width * 0.75f)
			{
				bQuarter = true;
			}
			// X along the bolt, Z toward the camera with the bolt's part removed (it turns about its own axis), Y = Z x X.
			const FVector ToCam = Cam - P;
			FVector Z = (ToCam - Dir * FVector::DotProduct(ToCam, Dir)).GetSafeNormal();
			if (Z.IsNearlyZero())
			{
				Z = FVector::CrossProduct(Dir, FMath::Abs(Dir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
			}
			const FVector Y = FVector::CrossProduct(Z, Dir);
			UGBPixelVfxComponent* Tile = NewObject<UGBPixelVfxComponent>(Owner, NAME_None, RF_Transient);
			Tile->FacingOverride = TEXT("Fixed");
			Tile->bEnableLight = false;
			Tile->SizeScale = Height / ClipHeightCm;
			Tile->PlayRate = 1.6f;
			Tile->StartTimeOffset = Phase;
			Tile->ComponentTags.Add(LayerTag(Layer));
			Tile->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
			Tile->SetWorldLocationAndRotation(P, FMatrix(Dir, Y, Z, FVector::ZeroVector).Rotator());
			Tile->RegisterComponent();
			if (!Tile->Play(TileClip(Index, bQuarter)))
			{
				Tile->DestroyComponent();
				break;
			}
			if (bQuarter)
			{
				break;
			}
			Pos += Width;
		}
	}

	/** The flickering arc from Start to End. Lives a moment, then removes itself. */
	void SpawnArc(UWorld* World, const FVector& Start, const FVector& End, float HeightPer100)
	{
		if (!World || FVector::DistSquared(Start, End) < 1.f)
		{
			return;
		}
		const bool bBig = HeightPer100 >= BigHeightPer100;
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGBPixelVfxActor* Arc = World->SpawnActor<AGBPixelVfxActor>(AGBPixelVfxActor::StaticClass(), FTransform(Start), Params);
		if (!Arc || !Arc->GetRootComponent())
		{
			return;
		}
		const FVector Cam = CameraLocation(World, Start);
		// Main bolt; a thinner one drawn from the far end (its zigzag runs the other way); the spell adds a third, thinner still.
		BuildLayer(Arc, Start, End, Cam, HeightPer100, 0.f, 0);
		BuildLayer(Arc, End, Start, Cam, HeightPer100 * 0.6f, 0.21f, 1);
		if (bBig)
		{
			BuildLayer(Arc, Start, End, Cam, HeightPer100 * 0.4f, 0.37f, 2);
		}
		// One unshadowed purple light a little way along the bolt, flickering with it.
		UPointLightComponent* Light = NewObject<UPointLightComponent>(Arc, NAME_None, RF_Transient);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetIntensityUnits(ELightUnits::Candelas);
		Light->SetLightColor(FLinearColor(FColor(0xB8, 0x3E, 0xCC)));
		Light->SetAttenuationRadius(bBig ? 1200.f : 700.f);
		Light->SetCastShadows(false);
		const float LightCandelas = bBig ? 90.f : 35.f;
		Light->SetIntensity(LightCandelas);
		Light->AttachToComponent(Arc->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		Light->SetWorldLocation(FMath::Lerp(Start, End, FMath::Min(0.3f, 400.f / FMath::Max(1.f, FVector::Dist(Start, End)))));
		Light->RegisterComponent();

		const float Life = bBig ? 0.3f : 0.16f;
		Arc->SetLifeSpan(Life);
		// Flicker: the thin layers blink at random, the whole arc drops out for one step in the middle, the light jumps.
		TSharedRef<int32> Step = MakeShared<int32>(0);
		const TWeakObjectPtr<UPointLightComponent> WeakLight(Light);
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(Arc, [Arc, Step, WeakLight, LightCandelas, Life]()
		{
			const int32 Now = ++(*Step);
			const bool bGap = Now == FMath::RoundToInt(Life * 0.5f / FlickerStep);
			TInlineComponentArray<UGBPixelVfxComponent*> Tiles(Arc);
			bool bLayerOn[3] = { !bGap, !bGap && FMath::FRand() < 0.7f, !bGap && FMath::FRand() < 0.6f };
			for (UGBPixelVfxComponent* Tile : Tiles)
			{
				for (int32 Layer = 0; Layer < 3; ++Layer)
				{
					if (Tile->ComponentHasTag(LayerTag(Layer)))
					{
						Tile->SetVisibility(bLayerOn[Layer], true);
					}
				}
			}
			if (UPointLightComponent* L = WeakLight.Get())
			{
				L->SetIntensity(bGap ? 0.f : LightCandelas * FMath::FRandRange(0.5f, 1.2f));
			}
		}), FlickerStep, true);
	}

	/** The hit clip at At, facing the camera; at least MinWidthCm wide, wider far away so it still reads. */
	void SpawnImpact(UWorld* World, FName ClipId, const FVector& At, float MinWidthCm, float HeightPer100)
	{
		const FGBPixelVfxClip* Clip = FindClip(World, ClipId);
		if (!World || !Clip || Clip->WorldWidthCm <= 0.f)
		{
			return;
		}
		const float Width = FMath::Max(MinWidthCm, HeightPer100 * 1.1f * FVector::Dist(CameraLocation(World, At), At) / 100.f);
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGBPixelVfxActor* Actor = World->SpawnActor<AGBPixelVfxActor>(AGBPixelVfxActor::StaticClass(), FTransform(At), Params);
		if (!Actor || !Actor->PixelVfx)
		{
			return;
		}
		UGBPixelVfxComponent* Vfx = Actor->PixelVfx;
		Vfx->SizeScale = Width / Clip->WorldWidthCm;
		Vfx->PlayRate = 1.3f;
		Vfx->FacingOverride = TEXT("Billboard");
		Vfx->bLightCastsShadows = false;
		Vfx->bLightCastsShadowsSet = true;
		Vfx->bAutoDestroyOwnerWhenFinished = true;
		if (!Vfx->Play(ClipId))
		{
			Actor->Destroy();
		}
	}

	/** The spark at the staff's head as it fires; rides the caster's camera so it stays on the drawn staff. */
	void SpawnSpark(UWorld* World, AActor* Caster, const FVector& At, float SizeCm, float Life)
	{
		const FGBPixelVfxClip* Clip = FindClip(World, SparkClip);
		if (!World || !Clip || Clip->WorldWidthCm <= 0.f)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGBPixelVfxActor* Actor = World->SpawnActor<AGBPixelVfxActor>(AGBPixelVfxActor::StaticClass(), FTransform(At), Params);
		if (!Actor || !Actor->PixelVfx)
		{
			return;
		}
		UGBPixelVfxComponent* Vfx = Actor->PixelVfx;
		Vfx->SizeScale = SizeCm / Clip->WorldWidthCm;
		Vfx->FacingOverride = TEXT("Billboard");
		Vfx->bLightCastsShadows = false;
		Vfx->bLightCastsShadowsSet = true;
		if (!Vfx->Play(SparkClip))
		{
			Actor->Destroy();
			return;
		}
		if (USceneComponent* Camera = Caster ? Caster->FindComponentByClass<UCameraComponent>() : nullptr)
		{
			Actor->AttachToComponent(Camera, FAttachmentTransformRules::KeepWorldTransform);
		}
		Actor->SetLifeSpan(Life);
	}
}

AGBProjectile::AGBProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Collision->SetCollisionObjectType(ECC_WorldDynamic);
	Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Collision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Collision->SetGenerateOverlapEvents(true);
	Collision->SetCanEverAffectNavigation(false);
	Collision->OnComponentHit.AddDynamic(this, &AGBProjectile::OnCollisionHit);
	Collision->OnComponentBeginOverlap.AddDynamic(this, &AGBProjectile::OnCollisionOverlap);
	RootComponent = Collision;

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->bRotationFollowsVelocity = true;
	// The launch velocity is given in world space by InitProjectile.
	Movement->bInitialVelocityInLocalSpace = false;
	Movement->bShouldBounce = false;
	Movement->ProjectileGravityScale = 0.f;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	Sprite->SetupAttachment(Collision);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->SetCastShadow(false);
	Mesh->SetCanEverAffectNavigation(false);

	// Energy bolt parts: empty and dark unless InitProjectile turns them on.
	BoltTrail = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BoltTrail"));
	BoltTrail->SetupAttachment(Collision);
	BoltTrail->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoltTrail->SetGenerateOverlapEvents(false);
	BoltTrail->SetCastShadow(false);
	BoltTrail->SetCanEverAffectNavigation(false);

	BoltLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("BoltLight"));
	BoltLight->SetupAttachment(Collision);
	BoltLight->SetMobility(EComponentMobility::Movable);
	BoltLight->CastShadows = false;
	BoltLight->IntensityUnits = ELightUnits::Candelas;
	BoltLight->Intensity = 0.f;
	BoltLight->SetVisibility(false);
}

AGBProjectile* AGBProjectile::SpawnProjectile(UWorld* World, const FGBProjectileConfig& InConfig, const FVector& Origin, const FVector& Direction, AActor* InInstigator)
{
	if (!World)
	{
		return nullptr;
	}

	const FVector LaunchDirection = Direction.IsNearlyZero() ? FVector::ForwardVector : Direction.GetSafeNormal();
	const FTransform SpawnTransform(LaunchDirection.Rotation(), Origin);

	AGBProjectile* Projectile = World->SpawnActorDeferred<AGBProjectile>(AGBProjectile::StaticClass(), SpawnTransform, InInstigator,
		Cast<APawn>(InInstigator), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Projectile)
	{
		UE_LOG(LogGB, Warning, TEXT("Projectile: spawn failed"));
		return nullptr;
	}

	Projectile->InitProjectile(InConfig, LaunchDirection, InInstigator);
	Projectile->FinishSpawning(SpawnTransform);

	if (IsValid(InInstigator) && IsValid(Projectile))
	{
		Projectile->Collision->IgnoreActorWhenMoving(InInstigator, true);
		if (UPrimitiveComponent* InstigatorRoot = Cast<UPrimitiveComponent>(InInstigator->GetRootComponent()))
		{
			InstigatorRoot->IgnoreActorWhenMoving(Projectile, true);
		}
	}

	return IsValid(Projectile) ? Projectile : nullptr;
}

void AGBProjectile::InitProjectile(const FGBProjectileConfig& InConfig, const FVector& Direction, AActor* InInstigator)
{
	Config = InConfig;
	if (InInstigator)
	{
		Config.Damage.Instigator = InInstigator;
	}
	Config.Damage.Causer = this;
	StartLocation = GetActorLocation();

	const float Radius = FMath::Max(Config.Radius, 1.f);
	Collision->SetSphereRadius(Radius);

	Movement->InitialSpeed = Config.Speed;
	Movement->MaxSpeed = Config.Speed;
	Movement->ProjectileGravityScale = Config.GravityScale;
	Movement->bShouldBounce = Config.FuseTime > 0.f;
	Movement->Bounciness = Config.Bounciness;
	Movement->Friction = Config.Friction;
	Movement->Velocity = Direction.GetSafeNormal() * Config.Speed;

	const float VisualSize = Radius * 2.f * FMath::Max(Config.VisualScale, 0.01f);
	if (Config.bPixelArt)
	{
		Mesh->SetVisibility(false);
		const bool bLightningArc = GBLightningFx::IsArcClip(Config.FlightVfxId);
		if (bLightningArc && GBLightningFx::HasClips(this))
		{
			// Instant lightning: nothing flies and nothing touches it; the first Tick resolves the hit (collision off
			// is how Tick knows). See GBLightningFx above.
			Sprite->SetVisibility(false);
			Sprite->SetHiddenInGame(true);
			SetActorEnableCollision(false);
		}
		else if (bLightningArc)
		{
			UE_LOG(LogGB, Warning, TEXT("Projectile: lightning arc clips (LightningArcH0/H1/Q0/Q2) not found; the bolt flies as the sprite '%s' instead"), *Config.SpriteId.ToString());
			if (UGBPixelSpriteLibrary* SpriteLibrary = UGBPixelSpriteLibrary::Get(this))
			{
				SpriteLibrary->SetupBillboard(Sprite, Config.SpriteId, VisualSize);
			}
		}
		else if (!Config.FlightVfxId.IsNone() && SetupFlightVfx())
		{
			Sprite->SetVisibility(false);
			Sprite->SetHiddenInGame(true);
		}
		else if (UGBPixelSpriteLibrary* SpriteLibrary = UGBPixelSpriteLibrary::Get(this))
		{
			SpriteLibrary->SetupBillboard(Sprite, Config.SpriteId, VisualSize);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Projectile: no pixel sprite library; sprite '%s' not shown"), *Config.SpriteId.ToString());
		}
	}
	else if (Config.bEnergyBolt)
	{
		Sprite->SetVisibility(false);
		Sprite->SetHiddenInGame(true);
		// Ellipsoids stretched along the local X axis, which the movement keeps pointing along the velocity.
		const float Length = FMath::Max(Config.BoltLength, VisualSize);
		const float Thick = FMath::Max(Config.BoltThickness, 1.f);
		const FLinearColor HotCore = FMath::Lerp(Config.Color, FLinearColor::White, 0.2f);
		GBPlaceholder::SetupShape(Mesh, EGBShape::Sphere, FVector(Length, Thick, Thick), HotCore, /*bGlow*/ true);
		const float TrailLength = FMath::Max(Config.BoltTrailLength, 0.f);
		if (TrailLength > 1.f)
		{
			GBPlaceholder::SetupShape(BoltTrail, EGBShape::Sphere, FVector(TrailLength, Thick * 0.6f, Thick * 0.6f), Config.Color * 0.35f, /*bGlow*/ true);
			BoltTrail->SetRelativeLocation(FVector(-(Length * 0.5f + TrailLength * 0.35f), 0.f, 0.f));
		}
		if (Config.BoltLightIntensity > 0.f)
		{
			BoltLight->SetLightColor(Config.Color);
			BoltLight->SetIntensity(Config.BoltLightIntensity);
			BoltLight->SetAttenuationRadius(FMath::Max(Config.BoltLightRadius, 1.f));
			BoltLight->SetVisibility(true);
		}
	}
	else
	{
		Sprite->SetVisibility(false);
		Sprite->SetHiddenInGame(true);
		GBPlaceholder::SetupShape(Mesh, EGBShape::Sphere, FVector(VisualSize), Config.Color, /*bGlow*/ true);
	}
}

namespace GBProjectileLocal
{
	const FGBPixelVfxClip* FindVfxClip(const UObject* WorldContext, FName ClipId)
	{
		const UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
		const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
		const UGBPixelVfxSubsystem* Vfx = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
		return Vfx ? Vfx->FindClip(ClipId) : nullptr;
	}
}

bool AGBProjectile::SetupFlightVfx()
{
	const FGBPixelVfxClip* Clip = GBProjectileLocal::FindVfxClip(this, Config.FlightVfxId);
	if (!Clip || Clip->WorldHeightCm <= 0.f)
	{
		UE_LOG(LogGB, Warning, TEXT("Projectile: baked clip '%s' not found; the sprite is used instead"), *Config.FlightVfxId.ToString());
		return false;
	}
	FlightVfx = NewObject<UGBPixelVfxComponent>(this, NAME_None, RF_Transient);
	FlightVfx->SizeScale = FMath::Max(Config.FlightVfxHeight, 1.f) / Clip->WorldHeightCm;
	// Faces the camera fully (a shot can go up or down) and is centred on the bright core of the flame, not on its base.
	FlightVfx->FacingOverride = TEXT("Billboard");
	FlightVfx->bPivotAtLightAnchor = true;
	// A shadow-casting light on every fast projectile is not worth its cost.
	FlightVfx->bLightCastsShadows = false;
	FlightVfx->bLightCastsShadowsSet = true;
	FlightVfx->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);
	FlightVfx->RegisterComponent();
	if (!FlightVfx->Play(Config.FlightVfxId))
	{
		FlightVfx->DestroyComponent();
		FlightVfx = nullptr;
		return false;
	}
	return true;
}

void AGBProjectile::SpawnImpactVfx(const FVector& Origin)
{
	UWorld* World = GetWorld();
	const FGBPixelVfxClip* Clip = GBProjectileLocal::FindVfxClip(this, Config.ImpactVfxId);
	if (!World || !Clip || Clip->WorldWidthCm <= 0.f)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const float Scale = FMath::Max(Config.ExplosionRadius * 2.f * Config.ImpactVfxSize, 1.f) / Clip->WorldWidthCm;
	// The clip is centred on its bright centre: on a floor hit half of the burst would be under the floor, so it is
	// lifted until its centre is a fifth of the frame above the ground (static world only: never an enemy it hit).
	FVector At = Origin;
	const float MinHeight = Clip->WorldHeightCm * Scale * 0.2f;
	FHitResult Floor;
	const FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBImpactVfxFloor), false, this);
	if (World->LineTraceSingleByObjectType(Floor, Origin + FVector(0.f, 0.f, 5.f), Origin - FVector(0.f, 0.f, MinHeight),
		FCollisionObjectQueryParams(ECC_WorldStatic), TraceParams))
	{
		At.Z = Floor.ImpactPoint.Z + MinHeight;
	}
	AGBPixelVfxActor* Actor = World->SpawnActor<AGBPixelVfxActor>(AGBPixelVfxActor::StaticClass(), FTransform(At), Params);
	if (!Actor || !Actor->PixelVfx)
	{
		return;
	}
	UGBPixelVfxComponent* Vfx = Actor->PixelVfx;
	Vfx->SizeScale = Scale;
	Vfx->PlayRate = FMath::Max(Config.ImpactVfxRate, 0.1f);
	Vfx->FacingOverride = TEXT("Billboard");
	Vfx->bPivotAtLightAnchor = true;
	Vfx->bLightCastsShadows = false;
	Vfx->bLightCastsShadowsSet = true;
	Vfx->bAutoDestroyOwnerWhenFinished = true;
	if (!Vfx->Play(Config.ImpactVfxId))
	{
		Actor->Destroy();
	}
}

bool AGBProjectile::HasFlightVfx() const
{
	return FlightVfx && FlightVfx->IsPlaying();
}

float AGBProjectile::GetBoltLength() const
{
	return (Config.bEnergyBolt && !Config.bPixelArt && Mesh) ? Mesh->GetRelativeScale3D().X * 100.f : 0.f;
}

bool AGBProjectile::HasBoltLight() const
{
	return Config.bEnergyBolt && !Config.bPixelArt && BoltLight && BoltLight->IsVisible() && BoltLight->Intensity > 0.f;
}

void AGBProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void AGBProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Undo the instigator's ignore entry so its list does not grow with every shot.
	AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (IsValid(ProjectileInstigator))
	{
		if (UPrimitiveComponent* InstigatorRoot = Cast<UPrimitiveComponent>(ProjectileInstigator->GetRootComponent()))
		{
			InstigatorRoot->IgnoreActorWhenMoving(this, false);
		}
	}

	Super::EndPlay(EndPlayReason);
}

void AGBProjectile::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bFinished)
	{
		return;
	}

	// Instant lightning (GBLightningFx): the whole path is resolved now, then the projectile is finished.
	if (!GetActorEnableCollision() && GBLightningFx::IsArcClip(Config.FlightVfxId))
	{
		UWorld* World = GetWorld();
		const FVector Start = StartLocation;
		FVector Dir = Movement ? Movement->Velocity.GetSafeNormal() : FVector::ZeroVector;
		if (Dir.IsNearlyZero())
		{
			Dir = GetActorForwardVector();
		}
		const float Range = Config.MaxRange > 0.f ? Config.MaxRange : Config.Speed * Config.MaxLifetime;
		FVector Stop = Start + Dir * Range;
		const FCollisionShape Shape = FCollisionShape::MakeSphere(FMath::Max(Config.Radius, 1.f));
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GBLightningTrace), false, this);
		if (AActor* Caster = Config.Damage.Instigator.Get())
		{
			Params.AddIgnoredActor(Caster);
		}
		// One sweep with the flying projectile's own collision responses (overlaps characters and dynamic things, blocks
		// on the static world), nearest first, through the same HitTarget a flying one uses; the blocking hit is handled
		// like OnCollisionHit (a damageable blocker is hit, otherwise the bolt ends there).
		FVector ArcEnd = Stop;
		TArray<FVector, TInlineAllocator<4>> HitPoints;
		TArray<FHitResult> Hits;
		if (World)
		{
			World->SweepMultiByChannel(Hits, Start, Stop, FQuat::Identity, ECC_WorldDynamic, Shape, Params,
				FCollisionResponseParams(Collision->GetCollisionResponseToChannels()));
		}
		for (const FHitResult& Hit : Hits)
		{
			if (bFinished)
			{
				break;
			}
			AActor* Other = Hit.GetActor();
			const FVector Point = (Hit.bStartPenetrating && Other) ? FMath::ClosestPointOnSegment(Other->GetActorLocation(), Start, Stop) : FVector(Hit.ImpactPoint);
			if (IsValidTarget(Other))
			{
				HitPoints.Add(Point);
				ArcEnd = Point;
				HitTarget(Other, Point);
			}
			if (Hit.bBlockingHit && !bFinished)
			{
				ArcEnd = Hit.bStartPenetrating ? Start : FVector(Hit.ImpactPoint);
				HitPoints.AddUnique(ArcEnd);
				if (Config.ExplosionRadius > 0.f)
				{
					Explode(ArcEnd);
				}
				Finish(Other, ArcEnd);
			}
		}
		if (!bFinished)
		{
			// A miss ends at its range, in the air.
			ArcEnd = Stop;
			if (Config.ExplosionRadius > 0.f)
			{
				Explode(ArcEnd);
			}
			Finish(nullptr, ArcEnd);
		}
		const float HeightPer100 = FMath::Max(Config.FlightVfxHeight, 1.f);
		const bool bBig = HeightPer100 >= GBLightningFx::BigHeightPer100;
		GBLightningFx::SpawnArc(World, Start, ArcEnd, HeightPer100);
		for (const FVector& Point : HitPoints)
		{
			GBLightningFx::SpawnImpact(World, Config.ImpactVfxId, Point, Config.ImpactVfxSize, HeightPer100);
		}
		GBLightningFx::SpawnSpark(World, Config.Damage.Instigator.Get(), Start, bBig ? 9.f : 6.f, bBig ? 0.22f : 0.12f);
		return;
	}

	const FVector Location = GetActorLocation();
	const float Age = GetGameTimeSinceCreation();

	// A baked flight clip right in front of the camera fills the screen with big pixel blocks (an enemy shaman's fire
	// reaching the player's face, castle visit smoke): hide it within 1.5 m of the viewer; the impact still plays.
	if (FlightVfx)
	{
		const APlayerController* Viewer = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
		if (Viewer && Viewer->PlayerCameraManager)
		{
			const bool bTooClose = FVector::DistSquared(Viewer->PlayerCameraManager->GetCameraLocation(), Location) < FMath::Square(150.f);
			FlightVfx->SetVisibility(!bTooClose, true);
		}
	}
	const bool bOutOfRange = Config.MaxRange > 0.f && FVector::Dist(StartLocation, Location) >= Config.MaxRange;
	const bool bTooOld = Config.MaxLifetime > 0.f && Age > Config.MaxLifetime;
	const bool bFuseDone = Config.FuseTime > 0.f && Age >= Config.FuseTime;

	if (bOutOfRange || bTooOld || bFuseDone)
	{
		if (Config.ExplosionRadius > 0.f)
		{
			Explode(Location);
		}
		Finish(nullptr, Location);
	}
}

bool AGBProjectile::IsValidTarget(const AActor* Other) const
{
	if (!IsValid(Other) || Other == this || Other->IsA<AGBProjectile>())
	{
		return false;
	}

	const AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (Other == ProjectileInstigator)
	{
		return false;
	}

	if (!UGBCombatLibrary::CanDamage(ProjectileInstigator, Other))
	{
		return false;
	}

	for (const TWeakObjectPtr<AActor>& Damaged : DamagedActors)
	{
		if (Damaged.Get() == Other)
		{
			return false;
		}
	}
	return true;
}

void AGBProjectile::HitTarget(AActor* Other, const FVector& ImpactPoint)
{
	DamagedActors.Add(Other);

	if (Config.ExplosionRadius > 0.f)
	{
		Explode(ImpactPoint);
	}
	else
	{
		FGBDamageInfo Info = Config.Damage;
		Info.Causer = this;
		Info.HitLocation = ImpactPoint;
		Info.HitDirection = Movement->Velocity.GetSafeNormal();
		// Only shots with a head bonus (damage or build-up) look for the weak point: a spell bolt on the
		// head is a normal hit, so its damage number is not shown as a headshot.
		if (Config.HeadshotMult > 1.f || Info.WeakPointBuildUpMult > 1.f)
		{
			UGBCombatLibrary::ApplyWeakPointBonus(Other, Info, Config.HeadshotMult);
		}
		UGBCombatLibrary::DealDamage(Other, Info);
	}

	// Neutral things with attributes (ice walls, destructibles) stop the shot even when it pierces.
	const bool bHitNeutral = UGBCombatLibrary::GetTeam(Other) == EGBTeam::Neutral;
	if (!Config.bPierce || bHitNeutral)
	{
		Finish(Other, ImpactPoint);
	}
}

void AGBProjectile::OnCollisionOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	if (bFinished || !IsValidTarget(OtherActor))
	{
		return;
	}

	const FVector ImpactPoint = (bFromSweep && !SweepResult.bStartPenetrating) ? FVector(SweepResult.ImpactPoint) : GetActorLocation();
	HitTarget(OtherActor, ImpactPoint);
}

void AGBProjectile::OnCollisionHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (bFinished)
	{
		return;
	}

	const FVector ImpactPoint = Hit.ImpactPoint;

	if (IsValidTarget(OtherActor))
	{
		HitTarget(OtherActor, ImpactPoint);
		return;
	}

	// The world: grenades keep bouncing until the fuse ends.
	if (Config.FuseTime > 0.f)
	{
		return;
	}

	if (Config.ExplosionRadius > 0.f)
	{
		Explode(ImpactPoint);
	}
	Finish(OtherActor, ImpactPoint);
}

void AGBProjectile::Explode(const FVector& Origin)
{
	FGBDamageInfo Info = Config.Damage;
	Info.Causer = this;
	UGBCombatLibrary::DealRadialDamage(this, Origin, Config.ExplosionRadius, Info, TConstArrayView<AActor*>(), Config.bExplosionFalloff);

	// Seen and felt even when the blast hits nothing.
	AGBExplosionEffect::Spawn(GetWorld(), Origin, Config.ExplosionRadius, Config.Color, Config.ExplosionEffectTime);
	if (!Config.ImpactVfxId.IsNone())
	{
		SpawnImpactVfx(Origin);
	}
	if (Config.ExplosionKnockback > 0.f)
	{
		ApplyExplosionKnockback(Origin);
	}
}

void AGBProjectile::ApplyExplosionKnockback(const FVector& Origin)
{
	UWorld* World = GetWorld();
	const float Radius = Config.ExplosionRadius;
	if (!World || Radius <= 0.f)
	{
		return;
	}

	// Smallest upward part of the push direction: a walking character pushed only sideways is stopped at once by
	// ground friction; a little lift makes it fly, so the push is felt even for a blast at the target's height.
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float KnockbackMinLift = Data ? Data->GetGameValue(GBProjectileKey::KnockbackMinLift, 0.35f) : 0.35f;

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBExplosionKnockback), false, this);
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	AActor* ProjectileInstigator = Config.Damage.Instigator.Get();
	if (!IsValid(ProjectileInstigator))
	{
		ProjectileInstigator = nullptr;
	}

	TSet<ACharacter*> Visited;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ACharacter* Character = Cast<ACharacter>(Overlap.GetActor());
		if (!IsValid(Character) || Character == ProjectileInstigator || Visited.Contains(Character))
		{
			continue;
		}
		Visited.Add(Character);

		if (!UGBCombatLibrary::CanDamage(ProjectileInstigator, Character))
		{
			continue;
		}

		const FVector TargetLocation = UGBCombatLibrary::GetDamageTargetLocation(Character, Origin);
		const float Distance = FVector::Dist(TargetLocation, Origin);
		if (Distance > Radius)
		{
			continue;
		}

		// Same rule as the blast damage: solid geometry between the blast and the target stops the push.
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBKnockbackObstruction), true, this);
		TraceParams.AddIgnoredActor(Character);
		FHitResult Obstruction;
		const FVector TraceStart = Origin + FVector(0.f, 0.f, GBProjectileLocal::KnockbackTraceLiftCm);
		if (World->LineTraceSingleByChannel(Obstruction, TraceStart, TargetLocation, ECC_Visibility, TraceParams)
			&& Obstruction.bBlockingHit && Obstruction.GetActor() && Obstruction.GetActor() != Character)
		{
			continue;
		}

		const float Strength = Config.ExplosionKnockback * FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
		if (Strength <= 0.f)
		{
			continue;
		}

		FVector Direction = (TargetLocation - Origin).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			// Blast right on the target: push it back along the projectile's flight.
			Direction = Movement ? Movement->Velocity.GetSafeNormal() : FVector::ZeroVector;
		}
		Direction.Z = FMath::Max(Direction.Z, KnockbackMinLift);
		Direction = Direction.GetSafeNormal();
		Character->LaunchCharacter(Direction * Strength, /*bXYOverride*/ true, /*bZOverride*/ true);
	}
}

void AGBProjectile::Finish(AActor* HitActor, const FVector& Location)
{
	if (bFinished)
	{
		return;
	}
	bFinished = true;

	if (Movement)
	{
		Movement->StopMovementImmediately();
	}
	OnImpact.Broadcast(this, HitActor, Location);
	Destroy();
}
