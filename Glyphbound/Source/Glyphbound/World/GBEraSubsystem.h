// Which era the world is in, and the travel between eras (fade out, swap, fade in).
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Core/GBTypes.h"
#include "GBEraSubsystem.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnEraChanged, EGBEra /*NewEra*/);

/**
 * One map, two times (GDD 4). The swap is done two ways at once, so level design can use either:
 *  - actors with a UGBEraComponent appear / disappear with their era;
 *  - streaming sub-levels whose package name ends in "_Past" or "_Future" are shown / hidden with their era.
 * Travel is a fade (Game.json: EraFadeOutTime, EraHoldTime, EraFadeInTime) that hides the swap: the era changes
 * while the screen is fully covered. On arrival everything of the player is restored (health, mana, ammo) and a
 * checkpoint is saved: portals are the safe points of the map.
 */
UCLASS()
class GLYPHBOUND_API UGBEraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UGBEraSubsystem* Get(const UObject* WorldContextObject);

	EGBEra GetCurrentEra() const { return CurrentEra; }

	/** Changes era at once, no fade (level start, checkpoint restore). Broadcasts OnEraChanged. */
	void SetEraImmediate(EGBEra NewEra);

	/**
	 * Starts the travel fade to TargetEra. If bTeleport, the traveller is moved to Destination while the screen
	 * is covered. Returns false if a travel is already running.
	 */
	bool RequestTravel(EGBEra TargetEra, AActor* Traveller, bool bTeleport = false, const FTransform& Destination = FTransform::Identity);

	bool IsTravelling() const { return bTravelling; }

	/** 0 = clear, 1 = screen fully covered. The HUD draws it. */
	float GetFadeAlpha() const { return FadeAlpha; }

	/** True if something with this presence exists in Era. */
	static bool IsPresentIn(EGBEraPresence Presence, EGBEra Era);
	static EGBEra OtherEra(EGBEra Era) { return Era == EGBEra::Past ? EGBEra::Future : EGBEra::Past; }

	/** Fired after the swap (actors and sub-levels already updated), with the screen still covered when travelling. */
	FGBOnEraChanged OnEraChanged;
	/** Fired when a travel has fully finished (fade in done). */
	FGBOnEraChanged OnTravelFinished;

	// UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

protected:
	void ApplyEra();

	EGBEra CurrentEra = EGBEra::Past;
	bool bTravelling = false;
	float FadeAlpha = 0.f;
	/** 0 fading out, 1 holding, 2 fading in. */
	int32 TravelStage = 0;
	float StageTime = 0.f;
	EGBEra PendingEra = EGBEra::Past;
	TWeakObjectPtr<AActor> PendingTraveller;
	bool bPendingTeleport = false;
	FTransform PendingDestination;
};
