#include "World/GBPortal.h"

#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/UObjectGlobals.h"
#include "Core/GBPlaceholder.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "World/GBCutsceneSubsystem.h"
#include "World/GBEraSubsystem.h"
#include "World/GBProgressSubsystem.h"
namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc = nullptr, float PitchMul = 1.f, float VolMul = 1.f); void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc); bool IsFutureMap(const UObject* Ctx); void MenuPing(const UObject* Ctx); void UpdateMusic(UWorld* World); }

// Map portals (2026-11-15): the castle and the base are two maps, so most portals open the other map instead of
// swapping the era in place. A placed AGBPortal is configured by its actor tags (no new properties, so no Editor
// restart was needed):
//   GBMap=<long package name>   open this map (map-portal mode)
//   GBArrive=<id>               arrive at the actor tagged GBArrival=<id> in that map (empty: the map's player start)
//   GBRequires=<flag>           usable only once this progress flag is set
//   GBOnceFlag=<flag>           sets this flag when used and is not usable while it is set
//   GBSetFlag=<flag>            also sets this flag when used (e.g. SealUsed, so the old seal goes dark)
//   GBCutscene=<id>             plays this caption cutscene first (Cutscenes.json)
//   GBOnEnter                   fires by walking into it (no prompt); GBRadius=<cm> sets the reach (default 250)
//   GBNoFrame                   no placeholder frame: the look is placed apart (pixel clip, cyber frame)
// The player's whole state crosses (weapons, spells, items, skills) through the progress checkpoint, the era is the
// new map's own, and arriving restores health, mana and ammo and saves the checkpoint (portals are the safe points).

namespace GBPortalLocal
{
	// Frame layout (cm), visuals only. The opening faces +X / -X; pillars stand on +Y / -Y.
	const FVector PillarSize(40.f, 40.f, 360.f);
	constexpr float PillarOffsetY = 140.f;
	const FVector LintelSize(40.f, 320.f, 40.f);
	const FVector SwirlSize(40.f, 240.f, 320.f);
	constexpr float PortalInteractRadius = 150.f;
	constexpr float DefaultEnterRadius = 250.f;
	constexpr float EnterPoll = 0.1f;
	const FName PortalSpriteId(TEXT("portal"));
	// Cold cyan leads to the future, warm orange leads to the past.
	const FLinearColor ToFutureColor(0.15f, 0.85f, 1.f);
	const FLinearColor ToPastColor(1.f, 0.55f, 0.15f);

	UStaticMeshComponent* MakeFramePiece(AActor* Owner, USceneComponent* Parent, const TCHAR* Name)
	{
		UStaticMeshComponent* Piece = Owner->CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Piece->SetupAttachment(Parent);
		Piece->SetMobility(EComponentMobility::Movable);
		Piece->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Piece->SetGenerateOverlapEvents(false);
		Piece->SetCastShadow(false);
		return Piece;
	}

	FString TagValue(const AActor* Actor, const TCHAR* Key)
	{
		if (!Actor)
		{
			return FString();
		}
		for (const FName& Tag : Actor->Tags)
		{
			const FString S = Tag.ToString();
			if (S.StartsWith(Key, ESearchCase::IgnoreCase))
			{
				return S.Mid(FCString::Strlen(Key)).TrimStartAndEnd();
			}
		}
		return FString();
	}

	bool HasFlag(const UObject* Context, const FString& Flag)
	{
		const UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(Context);
		return Progress && !Flag.IsEmpty() && Progress->HasFlag(FName(*Flag));
	}

	// ---- the hand-over to the next map ----
	bool bHandoverPending = false;
	FString PendingArrival;
	FDelegateHandle MapLoadedHandle;
	constexpr float HandoverPoll = 0.1f;
	constexpr int32 HandoverTries = 50;

	void TryHandover(TWeakObjectPtr<UWorld> WeakWorld, int32 TriesLeft)
	{
		UWorld* World = WeakWorld.Get();
		if (!World || !bHandoverPending)
		{
			return;
		}
		AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(World);
		UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(World);
		if (!Player || !Progress || !Player->GetController())
		{
			if (TriesLeft > 0)
			{
				FTimerHandle Retry;
				World->GetTimerManager().SetTimer(Retry, FTimerDelegate::CreateLambda([WeakWorld, TriesLeft]() { TryHandover(WeakWorld, TriesLeft - 1); }),
					HandoverPoll, false);
			}
			else
			{
				bHandoverPending = false;
				UE_LOG(LogGB, Warning, TEXT("Portal: no player in the new map to hand the state over to."));
			}
			return;
		}
		bHandoverPending = false;
		if (!Progress->HasCheckpoint())
		{
			return;
		}

		FTransform Arrival = Player->GetActorTransform();
		if (!PendingArrival.IsEmpty())
		{
			const FName ArrivalTag(*(FString(TEXT("GBArrival=")) + PendingArrival));
			bool bFound = false;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				if (It->ActorHasTag(ArrivalTag))
				{
					// Arrive standing: the actor's spot is the floor, the capsule's centre goes above it.
					const FVector Floor = It->GetActorLocation();
					Arrival = FTransform(FRotator(0.f, It->GetActorRotation().Yaw, 0.f), Floor + FVector(0.f, 0.f, 100.f));
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				UE_LOG(LogGB, Warning, TEXT("Portal: no actor tagged %s in %s; the player stays at the player start."), *ArrivalTag.ToString(), *World->GetMapName());
			}
		}

		FGBPlayerSnapshot Snapshot = Progress->GetCheckpoint();
		Snapshot.Transform = Arrival;
		Snapshot.ControlRotation = Arrival.Rotator();
		// The new map's own era (its player start says so), not the era the player left.
		if (const UGBEraSubsystem* Era = UGBEraSubsystem::Get(World))
		{
			Snapshot.Era = Era->GetCurrentEra();
		}
		Player->LoadFromSnapshot(Snapshot);
		Progress->SaveCheckpoint(Player);
		// Portals are the safe points: full health, mana and ammo on arrival.
		Progress->RestoreCheckpoint(Player);
		UE_LOG(LogGB, Log, TEXT("Portal: player state handed over to %s (arrival '%s')."), *World->GetMapName(), *PendingArrival);
	}

	/** A map portal is in use (its cutscene is running or its map is opening); cleared when a map loads. */
	bool bPortalBusy = false;

	void OnMapLoaded(UWorld* World)
	{
		bPortalBusy = false;
		if (bHandoverPending && World)
		{
			TryHandover(World, HandoverTries);
		}
	}
}

/** Opens MapPath with the player's whole state handed over; ArrivalId names the GBArrival=<id> spot (may be empty). Also used by AGBSeal. */
void GBBeginMapTravel(UObject* Context, AGBPlayerCharacter* Player, const FString& MapPath, const FString& ArrivalId)
{
	if (!Context)
	{
		return;
	}
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(Context))
	{
		if (Player)
		{
			Progress->SaveCheckpoint(Player);
		}
		GBPortalLocal::bHandoverPending = true;
		GBPortalLocal::PendingArrival = ArrivalId;
		if (!GBPortalLocal::MapLoadedHandle.IsValid())
		{
			GBPortalLocal::MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddStatic(&GBPortalLocal::OnMapLoaded);
		}
	}
	UE_LOG(LogGB, Log, TEXT("Portal: opening %s (arrival '%s')."), *MapPath, *ArrivalId);
	UGameplayStatics::OpenLevel(Context, FName(*MapPath));
}

namespace GBPortalLocal
{
	bool IsMapPortalReady(const AGBPortal* Portal)
	{
		if (!Portal || bPortalBusy)
		{
			return false;
		}
		const UGBEraSubsystem* Era = UGBEraSubsystem::Get(Portal);
		if (Era && Era->IsTravelling())
		{
			return false;
		}
		const FString Requires = TagValue(Portal, TEXT("GBRequires="));
		if (!Requires.IsEmpty() && !HasFlag(Portal, Requires))
		{
			return false;
		}
		const FString Once = TagValue(Portal, TEXT("GBOnceFlag="));
		return Once.IsEmpty() || !HasFlag(Portal, Once);
	}

	void ClearOnce(const UObject* Context, const FString& Once)
	{
		bPortalBusy = false;
		UGBProgressSubsystem* Progress = Context ? UGBProgressSubsystem::Get(Context) : nullptr;
		if (Progress && !Once.IsEmpty())
		{
			Progress->SetFlag(FName(*Once), false);
		}
	}

	void UseMapPortal(AGBPortal* Portal, AGBPlayerCharacter* Player)
	{
		if (!IsMapPortalReady(Portal) || !Player || Player->IsDead())
		{
			return;
		}
		bPortalBusy = true;
		const FString Map = TagValue(Portal, TEXT("GBMap="));
		const FString Arrival = TagValue(Portal, TEXT("GBArrive="));
		const FString Once = TagValue(Portal, TEXT("GBOnceFlag="));
		const FString SetFlag = TagValue(Portal, TEXT("GBSetFlag="));
		if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(Portal))
		{
			if (!Once.IsEmpty()) { Progress->SetFlag(FName(*Once)); }
			if (!SetFlag.IsEmpty()) { Progress->SetFlag(FName(*SetFlag)); }
		}

		TWeakObjectPtr<AGBPortal> WeakPortal(Portal);
		TWeakObjectPtr<AGBPlayerCharacter> WeakPlayer(Player);
		auto Travel = [WeakPortal, WeakPlayer, Map, Arrival, Once]()
		{
			AGBPortal* P = WeakPortal.Get();
			AGBPlayerCharacter* Pl = WeakPlayer.Get();
			if (!P || !Pl || Pl->IsDead())
			{
				ClearOnce(P ? static_cast<const UObject*>(P) : static_cast<const UObject*>(Pl), Once);
				return;
			}
			GBBeginMapTravel(P, Pl, Map, Arrival);
		};
		auto Cancelled = [WeakPortal, Once]()
		{
			ClearOnce(WeakPortal.Get(), Once);
		};
		const FString Cutscene = TagValue(Portal, TEXT("GBCutscene="));
		UGBCutsceneSubsystem* Cutscenes = UGBCutsceneSubsystem::Get(Portal);
		if (Cutscenes && !Cutscene.IsEmpty())
		{
			Cutscenes->Play(FName(*Cutscene), Travel, Cancelled);
		}
		else
		{
			Travel();
		}
	}
}

AGBPortal::AGBPortal()
{
	PillarLeft = GBPortalLocal::MakeFramePiece(this, Root, TEXT("PillarLeft"));
	PillarLeft->SetRelativeLocation(FVector(0.f, -GBPortalLocal::PillarOffsetY, GBPortalLocal::PillarSize.Z * 0.5f));
	PillarRight = GBPortalLocal::MakeFramePiece(this, Root, TEXT("PillarRight"));
	PillarRight->SetRelativeLocation(FVector(0.f, GBPortalLocal::PillarOffsetY, GBPortalLocal::PillarSize.Z * 0.5f));
	Lintel = GBPortalLocal::MakeFramePiece(this, Root, TEXT("Lintel"));
	Lintel->SetRelativeLocation(FVector(0.f, 0.f, GBPortalLocal::PillarSize.Z + GBPortalLocal::LintelSize.Z * 0.5f));

	InteractVolume->InitSphereRadius(GBPortalLocal::PortalInteractRadius);
}

void AGBPortal::BeginPlay()
{
	Super::BeginPlay();
	// A new map (or a new PIE run after one stopped mid-caption): no portal is in use.
	GBPortalLocal::bPortalBusy = false;

	const bool bNoFrame = ActorHasTag(FName(TEXT("GBNoFrame")));
	if (bNoFrame)
	{
		Mesh->SetVisibility(false);
		Mesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Sprite->SetHiddenInGame(true);
		Sprite->SetVisibility(false);
		PillarLeft->SetVisibility(false);
		PillarRight->SetVisibility(false);
		Lintel->SetVisibility(false);
		InteractVolume->SetRelativeLocation(FVector(0.f, 0.f, 100.f));
	}
	else
	{
		SetPlaceholderLook(true, GBPortalLocal::PortalSpriteId, FLinearColor::White, GBPortalLocal::SwirlSize);
		GBPlaceholder::SetupShape(PillarLeft, EGBShape::Cube, GBPortalLocal::PillarSize, GBPortalLocal::ToFutureColor, true);
		GBPlaceholder::SetupShape(PillarRight, EGBShape::Cube, GBPortalLocal::PillarSize, GBPortalLocal::ToFutureColor, true);
		GBPlaceholder::SetupShape(Lintel, EGBShape::Cube, GBPortalLocal::LintelSize, GBPortalLocal::ToFutureColor, true);
	}

	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		EraChangedHandle = Era->OnEraChanged.AddUObject(this, &AGBPortal::UpdateColor);
		UpdateColor(Era->GetCurrentEra());
	}

	// Walk-in portals (the castle door's event) watch the player's distance; no prompt is shown for them.
	if (ActorHasTag(FName(TEXT("GBOnEnter"))))
	{
		const FString RadiusText = GBPortalLocal::TagValue(this, TEXT("GBRadius="));
		const float Radius = RadiusText.IsEmpty() ? GBPortalLocal::DefaultEnterRadius : FCString::Atof(*RadiusText);
		if (UWorld* World = GetWorld())
		{
			FTimerHandle Poll;
			World->GetTimerManager().SetTimer(Poll, FTimerDelegate::CreateWeakLambda(this, [this, Radius]()
			{
				AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
				if (!Player || Player->IsDead() || !GBPortalLocal::IsMapPortalReady(this))
				{
					return;
				}
				if (FVector::Dist(Player->GetActorLocation(), GetActorLocation()) <= Radius)
				{
					GBPortalLocal::UseMapPortal(this, Player);
				}
			}), GBPortalLocal::EnterPoll, true);
		}
	}
}

void AGBPortal::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->OnEraChanged.Remove(EraChangedHandle);
	}
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}
	Super::EndPlay(EndPlayReason);
}

void AGBPortal::UpdateColor(EGBEra CurrentEra)
{
	const FLinearColor Color = UGBEraSubsystem::OtherEra(CurrentEra) == EGBEra::Future ? GBPortalLocal::ToFutureColor : GBPortalLocal::ToPastColor;
	GBPlaceholder::SetShapeColor(PillarLeft, Color);
	GBPlaceholder::SetShapeColor(PillarRight, Color);
	GBPlaceholder::SetShapeColor(Lintel, Color);
}

EGBEra AGBPortal::GetTargetEra() const
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	return UGBEraSubsystem::OtherEra(Era ? Era->GetCurrentEra() : EGBEra::Past);
}

bool AGBPortal::CanInteract(const AGBPlayerCharacter* Player) const
{
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	if (!(Super::CanInteract(Player) && Era && !Era->IsTravelling() && Player && !Player->IsDead()))
	{
		return false;
	}
	if (!GBPortalLocal::TagValue(this, TEXT("GBMap=")).IsEmpty())
	{
		return !ActorHasTag(FName(TEXT("GBOnEnter"))) && GBPortalLocal::IsMapPortalReady(this);
	}
	return true;
}

void AGBPortal::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	GBAudio::Play(this, FName(TEXT("portal")), nullptr, 1.f, 1.f);
	if (!GBPortalLocal::TagValue(this, TEXT("GBMap=")).IsEmpty())
	{
		GBPortalLocal::UseMapPortal(this, Player);
		return;
	}
	if (UGBEraSubsystem* Era = UGBEraSubsystem::Get(this))
	{
		Era->RequestTravel(GetTargetEra(), Player, bTeleport, Destination);
	}
}

FString AGBPortal::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	if (!GBPortalLocal::TagValue(this, TEXT("GBMap=")).IsEmpty())
	{
		return GBPortalLocal::TagValue(this, TEXT("GBMap=")).Contains(TEXT("FutureBase")) ? TEXT("Step into the future") : TEXT("Step into the past");
	}
	return GetTargetEra() == EGBEra::Future ? TEXT("Travel to the future") : TEXT("Travel to the past");
}
