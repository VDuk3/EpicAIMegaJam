// Put it on any actor that exists in only one era, or that depends on a progress flag.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GBTypes.h"
#include "GBEraComponent.generated.h"

/**
 * Decides whether its owner is "present": visible, colliding and ticking. Present when ALL of these hold:
 *  - the current era matches Presence;
 *  - RequiredFlag is empty or set   (cross-era unlock: "open in the future because of what was done in the past");
 *  - RemovedByFlag is empty or not set (a barrier that disappears once something is done).
 * Era-gated zones (GDD 4) are built with this: a wall that is PastOnly, a bridge that is FutureOnly and
 * requires the flag set by a past encounter, etc. Flags live in UGBProgressSubsystem.
 */
UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBEraComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBEraComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Era")
	EGBEraPresence Presence = EGBEraPresence::Both;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Era")
	FName RequiredFlag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|Era")
	FName RemovedByFlag = NAME_None;

	/** For actors spawned at runtime: set the fields, then call this. */
	void Configure(EGBEraPresence InPresence, FName InRequiredFlag = NAME_None, FName InRemovedByFlag = NAME_None);

	bool IsPresent() const { return bPresent; }

	/** Re-evaluates and applies (called on era change and flag change). */
	void Refresh();

	/** Pure rule, also used by the tests. */
	static bool ComputePresent(EGBEraPresence Presence, EGBEra CurrentEra, bool bRequiredFlagOk, bool bRemovedFlagSet);

	/** Fired when presence flips, after it is applied. Enemies use it to pause their AI. */
	FSimpleMulticastDelegate OnPresenceChanged;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void ApplyPresence(bool bNewPresent);
	void HandleEraChanged(EGBEra NewEra);
	void HandleFlagChanged(FName Flag);

	FDelegateHandle EraChangedHandle;
	FDelegateHandle FlagChangedHandle;
	bool bSavedHidden = false;

	bool bPresent = true;
	/** The owner's own collision / tick settings, remembered so presence can be restored exactly. */
	bool bSavedCollision = true;
	bool bSavedTick = true;
	bool bHasSaved = false;
	/** Owner components whose tick was enabled when presence was lost (attributes, status effects...): re-enabled exactly on return, so nothing simulates in the other era. */
	TArray<TWeakObjectPtr<UActorComponent>> SavedTickComponents;
};
