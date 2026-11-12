// Combat logic of the elemental build-up (GDD 6): three independent bars (fire, lightning, ice).
// No visuals here: looks bind to the events. Ticks only while a bar is not empty.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/GBTypes.h"
#include "Combat/GBBuildUpMeter.h"
#include "GBElementalStatusComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_ThreeParams(FGBOnBuildUpChanged, EGBElement /*Element*/, float /*Value*/, float /*Threshold*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FGBOnElementalStatusStarted, EGBElement /*Element*/, FName /*Reaction*/, float /*Duration*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FGBOnElementalStatusEnded, EGBElement /*Element*/);

/** Keys of Game.json "Values" read by the elemental status component. */
namespace GBElementalStatusKey
{
	inline const FName TickInterval(TEXT("ElementalTickInterval")); // seconds between build-up ticks while a bar is not empty
	inline const FName FrostSlowMax(TEXT("FrostSlowMax"));         // FrostSlow is clamped to this, so the slow can never fully stop the target
}

UCLASS(ClassGroup = (Glyphbound), meta = (BlueprintSpawnableComponent))
class GLYPHBOUND_API UGBElementalStatusComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGBElementalStatusComponent();

	static UGBElementalStatusComponent* Find(const AActor* Actor);

	/**
	 * Raw build-up of Element (Fire / Lightning / Ice; others ignored), before the owner's BuildUpTaken.
	 * EventId != 0 is deduped through FGBBuildUpEventLog. Returns true if it started the status.
	 */
	bool AddBuildUp(EGBElement Element, float Amount, int32 EventId, AActor* Instigator);

	float GetBuildUp(EGBElement Element) const;
	float GetThreshold(EGBElement Element) const;
	EGBBuildUpPhase GetPhase(EGBElement Element) const;
	float GetPhaseTimeRemaining(EGBElement Element) const;
	bool IsStatusActive(EGBElement Element) const;
	FName GetReaction(EGBElement Element) const;
	void ClearAll();

	FGBOnBuildUpChanged OnBuildUpChanged;
	FGBOnElementalStatusStarted OnStatusStarted;
	FGBOnElementalStatusEnded OnStatusEnded;
	FGBOnElementalStatusEnded OnImmunityEnded;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool ReadConfig(int32 Index, FGBBuildUpConfig& OutConfig, FName& OutReaction) const;
	void StartStatus(int32 Index, const FGBBuildUpConfig& Config, FName Reaction, float Duration, AActor* Instigator);
	void EndStatus(int32 Index);
	void UpdateTickEnabled();
	void HandleOwnerDeath(const FGBDamageInfo& KillingBlow);
	void HandleDataReloaded();

	FGBBuildUpMeter Meters[3];
	FGBBuildUpEventLog EventLog;

	/** Enemy ids (with element) already warned about an unknown reaction: "EnemyId:Element". */
	mutable TSet<FName> WarnedReactions;
	/** Game.json keys already warned about a missing default. */
	mutable TSet<FName> WarnedDefaults;

	FDelegateHandle DeathHandle;
	FDelegateHandle DataReloadedHandle;

	/**
	 * Game time (dilated, so the QTE slow-down applies) the meters were last advanced to; -1 when idle.
	 * The component keeps its own clock instead of trusting the tick's DeltaTime, so a hit between two
	 * ticks starts its status (or its decay delay) at the hit, not at the previous tick.
	 */
	double LastAdvanceTime = -1.0;
	double GetGameTime() const;
};
