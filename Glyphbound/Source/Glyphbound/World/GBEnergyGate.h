// An energy door that goes out when its progress flags are all set, and the sockets that set them.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GBEnergyGate.generated.h"

class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * A sheet of energy across a doorway (one mesh with a see-through material, set per placed instance). It blocks
 * bodies and shots while any of RequiredFlags is missing; what is behind it stays in sight. When every flag exists
 * it fades out over FadeSeconds and stops blocking. No flags listed = it never goes out.
 * The material may have a scalar "Fade" (1 = on, 0 = out); without it the sheet just disappears at the end.
 */
UCLASS()
class GLYPHBOUND_API AGBEnergyGate : public AActor
{
	GENERATED_BODY()

public:
	AGBEnergyGate();

	virtual void Tick(float DeltaSeconds) override;

	UStaticMeshComponent* GetMesh() const { return Mesh; }
	bool IsOut() const { return bOut; }

	/** Every one of these progress flags must exist for the gate to go out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|EnergyGate")
	TArray<FName> RequiredFlags;

	/** Seconds the sheet takes to fade once the last flag is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|EnergyGate")
	float FadeSeconds = 1.2f;

	/** Pure rule, for tests: true when Required is not empty and every flag of it is in Present. */
	static bool AllFlagsPresent(const TArray<FName>& Required, const TSet<FName>& Present);

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Glyphbound|EnergyGate", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FadeInstance;

	float Fade = 1.f;
	bool bOut = false;
};

/**
 * A device that takes one item from the player and sets a progress flag (one of an energy gate's). The player only
 * has to come within Reach of it carrying ItemId; the item is used up. A socket whose flag already exists is full.
 * It has no mesh of its own: it is placed on the prop that plays the device.
 */
UCLASS()
class GLYPHBOUND_API AGBKeySocket : public AActor
{
	GENERATED_BODY()

public:
	AGBKeySocket();

	virtual void Tick(float DeltaSeconds) override;

	bool IsFilled() const { return bFilled; }

	/** The item (Items.json) this socket takes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|KeySocket")
	FName ItemId = NAME_None;

	/** The progress flag it sets when filled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|KeySocket")
	FName Flag = NAME_None;

	/** How near the player must come (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|KeySocket")
	float Reach = 180.f;

	/** Shown on the HUD when the socket is filled, and when the player comes near without the item. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|KeySocket")
	FString FilledMessage = TEXT("Power cell set");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glyphbound|KeySocket")
	FString MissingMessage = TEXT("This socket needs a power cell");

protected:
	bool bFilled = false;
	bool bPlayerNear = false;
};
