// Bake / showcase / compare entry point. Started by PixelVfx-Jam.ps1 with one of
// -PvfxBake=<Id|All>, -PvfxCompare=<Id>, -PvfxShowcase. Runs a small Tick state machine,
// logs PVFX lines, then logs PVFX DONE and quits. Does nothing when no switch is present.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GBPixelVfxBakeJob.h"
#include "GBPixelVfxBakeGameMode.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class ACameraActor;
class ADirectionalLight;
class AGBPixelVfxActor;

UCLASS()
class GBPIXELVFXBAKER_API AGBPixelVfxBakeGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGBPixelVfxBakeGameMode();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

private:
	enum class ERunMode : uint8 { None, Bake, Showcase, Compare };
	ERunMode RunMode = ERunMode::None;
	FString RunArg;

	// Bake mode
	TArray<FString> BakeJobFiles;
	int32 BakeJobIndex = 0;
	FGBPixelVfxBakeJob Job;

	enum class EBakeStep : uint8
	{
		StartJob,
		WaitReady,
		RunSim,
		ConvertWrite,
		NextJob
	};
	EBakeStep BakeStep = EBakeStep::StartJob;
	int32 StepTick = 0;
	bool bRespawned = false;

	// One capture rig per job (destroyed before the next job).
	TObjectPtr<AActor> NiagaraActor;
	TObjectPtr<UNiagaraComponent> NiagaraComp;
	TObjectPtr<UNiagaraSystem> NiagaraSystem;
	TObjectPtr<USceneCaptureComponent2D> CaptureComp;
	TObjectPtr<UTextureRenderTarget2D> CaptureRT;
	TObjectPtr<ACameraActor> ViewCam;
	TObjectPtr<ADirectionalLight> KeyLight;

	// Simulation runner: one Tick is one simulation frame while the fixed step is on.
	int32 SimTick = 0;
	TArray<int32> CaptureAtTicks;
	int32 CaptureCursor = 0;
	int32 CaptureStride = 5;
	int32 CaptureTotalFrames = 0;
	struct FSimFrame
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FLinearColor> Color;
		TArray<float> Opacity;
	};
	TArray<FSimFrame> SimFrames;
	bool bRunningPrepass = false;
	int32 PrepassBudget = 0;
	bool bLegacyCapture = false;
	bool bRetriedWide = false;
	float FramingWidthCm = 0.f;
	FVector FramingCenterCm = FVector::ZeroVector;

	// Showcase / compare shared stage
	TObjectPtr<ACameraActor> StageCam;
	TObjectPtr<APlayerController> StagePC;
	TArray<TObjectPtr<AActor>> StageActors;
	int32 StageStep = 0;
	int32 StageWait = 0;
	double StageTimeMark = 0.0;
	TArray<FName> StageClipIds;
	int32 StageShotIndex = 0;
	TArray<TWeakObjectPtr<AGBPixelVfxActor>> StageClips;

	// Compare mode
	FGBPixelVfxBakeJob CmpJob;
	TArray<TObjectPtr<AActor>> CmpActors;
	int32 CmpMeasureLeft = 0;
	double CmpPrevNow = 0.0;
	uint32 CmpPrevGpu = 0;
	double CmpCpuSum = 0.0;
	double CmpGpuSum = 0.0;
	int32 CmpCpuCount = 0;
	double CmpSourceFrameMs = 0.0;
	double CmpSourceGpuMs = 0.0;
	double CmpBakedFrameMs = 0.0;
	double CmpBakedGpuMs = 0.0;
	double CmpEmptyFrameMs = 0.0;
	double CmpEmptyGpuMs = 0.0;

	void ParseCommandLine();
	void FinishRun();
	void QuitSoon();

	// Bake
	void TickBake();
	void BakeDestroyRig();
	bool BakeSpawnRig(int32 RTWidth);
	void BakeFillCaptureTicks(bool bPrepassNow);
	bool BakeCaptureOne();
	void BakeAnalysePrepass();
	void BakeConvertAndWrite();
	void BakeLogAndCleanup(bool bOk);
	FVector BakeCameraDirection() const;

	// Shared
	APlayerController* EnsurePlayerController();
	void BuildStage(const FString& StageMaterialPath);
	void DestroyStage();
	void TakeStageScreenshot(const FString& Path);
	void TickShowcase();
	void TickCompare();
	void CmpClearActors();
	void CmpSpawnSourceGrid();
	void CmpSpawnBakedGrid();
	void CmpWriteResults();
};
