// AGBPixelVfxBakeGameMode: bake / showcase / compare state machines. See the header for the contract.
#include "GBPixelVfxBakeGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "DynamicRHI.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxConvert.h"
#include "GBPixelVfxSubsystem.h"
#include "GBPixelVfxTypes.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/DateTime.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TextureResource.h"
#include "UObject/ConstructorHelpers.h"

namespace GBPixelVfxBakeLocal
{
	float Saturate(float X) { return FMath::Clamp(X, 0.f, 1.f); }

	bool EnsureDir(const FString& Dir)
	{
		IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
		return Files.CreateDirectoryTree(*Dir);
	}

	// Bilinear resample of a linear HDR frame to the review cell size, tone-mapped x/(1+x).
	void BlitSourceCell(const FLinearColor* Src, int32 SrcW, int32 SrcH, FColor* Dst, int32 DstW, int32 DstH)
	{
		for (int32 y = 0; y < DstH; ++y)
		{
			const float SrcY = (static_cast<float>(y) + 0.5f) * SrcH / DstH - 0.5f;
			const int32 Y0 = FMath::Clamp(FMath::FloorToInt(SrcY), 0, SrcH - 1);
			const int32 Y1 = FMath::Min(Y0 + 1, SrcH - 1);
			const float TY = FMath::Clamp(SrcY - Y0, 0.f, 1.f);
			for (int32 x = 0; x < DstW; ++x)
			{
				const float SrcX = (static_cast<float>(x) + 0.5f) * SrcW / DstW - 0.5f;
				const int32 X0 = FMath::Clamp(FMath::FloorToInt(SrcX), 0, SrcW - 1);
				const int32 X1 = FMath::Min(X0 + 1, SrcW - 1);
				const float TX = FMath::Clamp(SrcX - X0, 0.f, 1.f);
				const FLinearColor A = Src[Y0 * SrcW + X0] * (1.f - TX) + Src[Y0 * SrcW + X1] * TX;
				const FLinearColor B = Src[Y1 * SrcW + X0] * (1.f - TX) + Src[Y1 * SrcW + X1] * TX;
				const FLinearColor C = A * (1.f - TY) + B * TY;
				Dst[y * DstW + x] = FColor(
					static_cast<uint8>(FMath::Clamp(C.R / (1.f + C.R), 0.f, 1.f) * 255.f + 0.5f),
					static_cast<uint8>(FMath::Clamp(C.G / (1.f + C.G), 0.f, 1.f) * 255.f + 0.5f),
					static_cast<uint8>(FMath::Clamp(C.B / (1.f + C.B), 0.f, 1.f) * 255.f + 0.5f),
					255);
			}
		}
	}

	// Nearest upscale of one baked frame.
	void BlitPixelCell(const FColor* Src, int32 SrcW, int32 SrcH, FColor* Dst, int32 DstW, int32 DstH)
	{
		for (int32 y = 0; y < DstH; ++y)
		{
			const int32 SY = FMath::Min(y * SrcH / DstH, SrcH - 1);
			for (int32 x = 0; x < DstW; ++x)
			{
				const int32 SX = FMath::Min(x * SrcW / DstW, SrcW - 1);
				Dst[y * DstW + x] = Src[SY * SrcW + SX];
			}
		}
	}

	bool SavePng(const FString& Path, int32 W, int32 H, const TArray<FColor>& Pixels)
	{
		if (Pixels.Num() != W * H)
		{
			return false;
		}
		TArray64<uint8> Compressed;
		FImageUtils::PNGCompressImageArray(W, H, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), Compressed);
		if (Compressed.Num() == 0)
		{
			return false;
		}
		TArray<uint8> Bytes;
		Bytes.Append(Compressed.GetData(), static_cast<int32>(FMath::Min<int64>(Compressed.Num(), MAX_int32)));
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}
}

AGBPixelVfxBakeGameMode::AGBPixelVfxBakeGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	DefaultPawnClass = nullptr;
	HUDClass = nullptr;
}

void AGBPixelVfxBakeGameMode::ParseCommandLine()
{
	FString BakeArg;
	FString CompareArg;
	if (FParse::Value(FCommandLine::Get(), TEXT("-PvfxBake="), BakeArg))
	{
		RunMode = ERunMode::Bake;
		RunArg = BakeArg;
	}
	else if (FParse::Value(FCommandLine::Get(), TEXT("-PvfxCompare="), CompareArg))
	{
		RunMode = ERunMode::Compare;
		RunArg = CompareArg;
	}
	else if (FString(FCommandLine::Get()).Contains(TEXT("-PvfxShowcase")))
	{
		RunMode = ERunMode::Showcase;
	}
}

void AGBPixelVfxBakeGameMode::BeginPlay()
{
	Super::BeginPlay();
	ParseCommandLine();
	if (RunMode == ERunMode::None)
	{
		return;
	}
	if (RunMode == ERunMode::Bake)
	{
		BakeJobFiles.Reset();
		for (const FString& File : FGBPixelVfxBakeJob::ListJobFiles())
		{
			if (RunArg.Equals(TEXT("All"), ESearchCase::IgnoreCase)
				|| FPaths::GetBaseFilename(File).Equals(RunArg, ESearchCase::IgnoreCase))
			{
				BakeJobFiles.Add(File);
			}
		}
		if (BakeJobFiles.Num() == 0)
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX no bake job matches '%s'."), *RunArg);
			FinishRun();
		}
	}
}

void AGBPixelVfxBakeGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	switch (RunMode)
	{
	case ERunMode::Bake: TickBake(); break;
	case ERunMode::Showcase: TickShowcase(); break;
	case ERunMode::Compare: TickCompare(); break;
	default: break;
	}
}

void AGBPixelVfxBakeGameMode::FinishRun()
{
	UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX DONE"));
	RunMode = ERunMode::None;
	FPlatformMisc::RequestExit(false);
}

// ---------------------------------------------------------------- bake mode ----

FVector AGBPixelVfxBakeGameMode::BakeCameraDirection() const
{
	const float Yaw = FMath::DegreesToRadians(Job.ViewYawDeg);
	const float Pitch = FMath::DegreesToRadians(Job.ViewPitchDeg);
	return FVector(FMath::Cos(Pitch) * FMath::Cos(Yaw), FMath::Cos(Pitch) * FMath::Sin(Yaw),
		FMath::Sin(Pitch));
}

void AGBPixelVfxBakeGameMode::BakeDestroyRig()
{
	if (NiagaraActor)
	{
		NiagaraActor->Destroy();
	}
	NiagaraActor = nullptr;
	NiagaraComp = nullptr;
	NiagaraSystem = nullptr;
	if (CaptureComp)
	{
		if (AActor* CaptureActor = CaptureComp->GetOwner())
		{
			CaptureActor->Destroy();
		}
	}
	CaptureComp = nullptr;
	CaptureRT = nullptr;
	if (ViewCam)
	{
		ViewCam->Destroy();
	}
	ViewCam = nullptr;
	if (KeyLight)
	{
		KeyLight->Destroy();
	}
	KeyLight = nullptr;
}

bool AGBPixelVfxBakeGameMode::BakeSpawnRig(int32 RTWidth)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *Job.SystemPath);
	if (!NiagaraSystem)
	{
		UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: cannot load Niagara system '%s' (is NiagaraFluids on the command line?)."),
			*Job.Id.ToString(), *Job.SystemPath);
		return false;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	NiagaraActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	if (!NiagaraActor)
	{
		return false;
	}
	NiagaraComp = NewObject<UNiagaraComponent>(NiagaraActor, TEXT("Niagara"));
	NiagaraActor->SetRootComponent(NiagaraComp);
	NiagaraComp->RegisterComponent();
	NiagaraComp->SetAsset(NiagaraSystem);
	NiagaraComp->SetRandomSeedOffset(0);
	NiagaraComp->SetAllowScalability(false);
	NiagaraComp->bAutoActivate = false;
	for (const auto& Pair : Job.UserFloats)
	{
		NiagaraComp->SetVariableFloat(Pair.Key, Pair.Value);
	}
	for (const auto& Pair : Job.UserVectors)
	{
		NiagaraComp->SetVariableVec3(Pair.Key, Pair.Value);
	}
	for (const auto& Pair : Job.UserColors)
	{
		NiagaraComp->SetVariableLinearColor(Pair.Key, Pair.Value);
	}
	for (const auto& Pair : Job.UserStaticMeshes)
	{
		UStaticMesh* Mesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, *Pair.Value));
		if (Mesh)
		{
			NiagaraComp->SetVariableStaticMesh(Pair.Key, Mesh);
		}
		else
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX bake: user static mesh %s not found"), *Pair.Value);
		}
	}
	NiagaraComp->Activate(true);

	const FVector Dir = BakeCameraDirection();
	const FRotator CamRot = FRotationMatrix::MakeFromX(Dir).Rotator();
	const FVector RightV = CamRot.RotateVector(FVector::RightVector);
	const FVector UpV = CamRot.RotateVector(FVector::UpVector);
	const bool bOrtho = Job.Projection.Equals(TEXT("Ortho"), ESearchCase::IgnoreCase);
	const int32 RTW = FMath::Max(16, RTWidth);
	const int32 RTH = FMath::Max(16, FMath::RoundToInt(RTW * Job.FrameHeight / static_cast<float>(FMath::Max(1, Job.FrameWidth))));

	AActor* CaptureActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
	CaptureComp = NewObject<USceneCaptureComponent2D>(CaptureActor, TEXT("PvfxCapture"));
	CaptureActor->SetRootComponent(CaptureComp);
	CaptureComp->RegisterComponent();
	CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	if (bLegacyCapture)
	{
		CaptureComp->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_LegacySceneCapture;
	}
	else
	{
		CaptureComp->ShowOnlyComponent(NiagaraComp);
	}
	CaptureComp->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
	CaptureComp->bCaptureEveryFrame = false;
	CaptureComp->bCaptureOnMovement = false;
	CaptureComp->bAlwaysPersistRenderingState = true;
	CaptureComp->bInheritMainViewCameraPostProcessSettings = false;
	CaptureComp->ShowFlags.SetBloom(false);
	CaptureComp->ShowFlags.SetEyeAdaptation(false);
	CaptureComp->ShowFlags.SetMotionBlur(false);
	CaptureComp->ShowFlags.SetTonemapper(false);
	CaptureComp->ShowFlags.SetToneCurve(false);
	CaptureComp->ShowFlags.SetTemporalAA(false);
	CaptureComp->ShowFlags.SetGrain(false);
	CaptureComp->ShowFlags.SetVignette(false);
	CaptureComp->ShowFlags.SetCameraImperfections(false);
	CaptureComp->ShowFlags.SetLensFlares(false);
	CaptureComp->ShowFlags.SetDepthOfField(false);
	if (bOrtho)
	{
		CaptureComp->ProjectionType = ECameraProjectionMode::Orthographic;
		CaptureComp->OrthoWidth = FramingWidthCm;
		CaptureActor->SetActorLocation(FramingCenterCm - Dir * 2000.f);
	}
	else
	{
		CaptureComp->ProjectionType = ECameraProjectionMode::Perspective;
		CaptureComp->FOVAngle = Job.FovDeg;
		const float Dist = (FramingWidthCm * 0.5f) / FMath::Tan(FMath::DegreesToRadians(Job.FovDeg * 0.5f));
		CaptureActor->SetActorLocation(FramingCenterCm - Dir * Dist);
	}
	CaptureActor->SetActorRotation(CamRot);
	CaptureRT = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
	CaptureRT->InitCustomFormat(RTW, RTH, PF_FloatRGBA, true);
	CaptureRT->UpdateResourceImmediate(true);
	CaptureComp->TextureTarget = CaptureRT;

	if (Job.bKeyLight)
	{
		KeyLight = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FTransform::Identity, Params);
		// Style guide: light from the top left as seen by the capture camera.
		const FVector Travel = (Dir - UpV + RightV * 0.8f).GetSafeNormal();
		if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(KeyLight->GetLightComponent()))
		{
			LightComp->SetMobility(EComponentMobility::Movable);
			LightComp->SetIntensity(Job.KeyLightIntensity);
			LightComp->SetLightColor(FLinearColor::White);
		}
		KeyLight->SetActorRotation(Travel.Rotation());
	}

	// The main view must render every frame and must look at the effect (GPU sims do not
	// advance in scene captures): park a camera actor on the capture and make it the view target.
	ViewCam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	ViewCam->SetActorLocation(CaptureActor->GetActorLocation());
	ViewCam->SetActorRotation(CamRot);
	if (bOrtho)
	{
		ViewCam->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Orthographic;
		ViewCam->GetCameraComponent()->OrthoWidth = FramingWidthCm;
	}
	else
	{
		ViewCam->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Perspective;
		ViewCam->GetCameraComponent()->FieldOfView = Job.FovDeg;
	}
	if (APlayerController* PC = EnsurePlayerController())
	{
		PC->SetViewTarget(ViewCam);
	}
	else
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: no player controller, the main view may not advance the sim."),
			*Job.Id.ToString());
	}
	return true;
}

void AGBPixelVfxBakeGameMode::BakeFillCaptureTicks(bool bPrepassNow)
{
	CaptureAtTicks.Reset();
	CaptureCursor = 0;
	const int32 WarmupFrames = FMath::Max(0, FMath::RoundToInt(Job.WarmupSeconds * Job.SimFps));
	if (bPrepassNow)
	{
		// The framing union covers the warmed-up window only, like the real pass: the
		// transient rise from t = 0 would inflate the box of a looping clip.
		const int32 TotalFrames = WarmupFrames + FMath::Max(1, FMath::RoundToInt(Job.DurationSeconds * Job.SimFps));
		for (int32 Tick = WarmupFrames; Tick < TotalFrames; Tick += 4)
		{
			CaptureAtTicks.Add(Tick);
		}
		CaptureTotalFrames = CaptureAtTicks.Num();
	}
	else
	{
		const int32 Blend = (Job.bLoop && Job.LoopBlendFrames > 0) ? Job.LoopBlendFrames : 0;
		const int32 Total = Job.FrameCount() + Blend;
		CaptureStride = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Job.SimFps) / FMath::Max(1, Job.Fps)));
		for (int32 i = 0; i < Total; ++i)
		{
			// Each sample is scheduled independently so non-divisible rates keep the declared fps.
			CaptureAtTicks.Add(WarmupFrames + FMath::RoundToInt(static_cast<float>(i * Job.SimFps) / FMath::Max(1, Job.Fps)));
		}
		CaptureTotalFrames = Total;
	}
}

bool AGBPixelVfxBakeGameMode::BakeCaptureOne()
{
	if (!CaptureComp || !CaptureRT)
	{
		return false;
	}
	CaptureComp->CaptureScene();
	FTextureRenderTargetResource* Res = CaptureRT->GameThread_GetRenderTargetResource();
	if (!Res)
	{
		return false;
	}
	TArray<FLinearColor> Raw;
	if (!Res->ReadLinearColorPixels(Raw) || Raw.Num() != CaptureRT->SizeX * CaptureRT->SizeY)
	{
		return false;
	}
	FSimFrame Frame;
	Frame.Width = CaptureRT->SizeX;
	Frame.Height = CaptureRT->SizeY;
	Frame.Color.SetNum(Raw.Num());
	Frame.Opacity.SetNum(Raw.Num());
	for (int32 i = 0; i < Raw.Num(); ++i)
	{
		Frame.Color[i] = Raw[i];
		Frame.Opacity[i] = GBPixelVfxBakeLocal::Saturate(1.f - Raw[i].A);
	}
	SimFrames.Add(MoveTemp(Frame));
	return true;
}

void AGBPixelVfxBakeGameMode::BakeAnalysePrepass()
{
	using namespace GBPixelVfxBakeLocal;
	const float TestLevel = Job.AlphaThreshold * 0.5f;
	// The visibility test uses the job's alpha rule: additive effects (fire) stay near A = 1
	// in the capture, so opacity alone would call them invisible.
	const bool bLuminance = Job.AlphaMode.Equals(TEXT("Luminance"), ESearchCase::IgnoreCase);
	const bool bMax = !bLuminance && !Job.AlphaMode.Equals(TEXT("Opacity"), ESearchCase::IgnoreCase);
	const int32 RTW = SimFrames.Num() > 0 ? SimFrames[0].Width : 0;
	const int32 RTH = SimFrames.Num() > 0 ? SimFrames[0].Height : 0;
	int32 MinX = RTW;
	int32 MaxX = -1;
	int32 MinY = RTH;
	int32 MaxY = -1;
	float MaxOpacity = 0.f;
	float MaxLuminance = 0.f;
	for (const FSimFrame& Frame : SimFrames)
	{
		for (int32 y = 0; y < Frame.Height; ++y)
		{
			for (int32 x = 0; x < Frame.Width; ++x)
			{
				const int32 At = y * Frame.Width + x;
				const float Opacity = Frame.Opacity[At];
				const FLinearColor& RGB = Frame.Color[At];
				// Conversion evaluates alpha after Capture.Exposure: the prepass must too,
				// or a raised exposure can make outer pixels visible only in the final pass.
				const float Lum = (RGB.R * 0.2126f + RGB.G * 0.7152f + RGB.B * 0.0722f) * Job.Exposure;
				MaxOpacity = FMath::Max(MaxOpacity, Opacity);
				MaxLuminance = FMath::Max(MaxLuminance, Lum);
				float A = Opacity;
				if (bLuminance)
				{
					A = Saturate(Lum);
				}
				else if (bMax)
				{
					A = FMath::Max(Opacity, Saturate(Lum));
				}
				if (A > TestLevel)
				{
					MinX = FMath::Min(MinX, x);
					MaxX = FMath::Max(MaxX, x);
					MinY = FMath::Min(MinY, y);
					MaxY = FMath::Max(MaxY, y);
				}
			}
		}
	}
	if (MaxX < MinX)
	{
		if (!bLegacyCapture)
		{
			UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: nothing visible with the show-only list (max opacity %.2f, max luminance %.2f), retrying with the full scene."),
				*Job.Id.ToString(), MaxOpacity, MaxLuminance);
			bLegacyCapture = true;
			BakeDestroyRig();
			BakeSpawnRig(256);
			BakeStep = EBakeStep::WaitReady;
			StepTick = 0;
			return;
		}
		UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: nothing visible (max opacity %.2f, max luminance %.2f), failing the job."),
			*Job.Id.ToString(), MaxOpacity, MaxLuminance);
		BakeLogAndCleanup(false);
		return;
	}
	// The stored pivot must stay valid without clamping: include the projected source
	// origin (the job actor at world 0,0,0) in the framing box before fitting the aspect.
	{
		const FRotator CamRotKeys = CaptureComp ? CaptureComp->GetComponentRotation() : FRotator::ZeroRotator;
		const FVector RightKeys = CamRotKeys.RotateVector(FVector::RightVector);
		const FVector UpKeys = CamRotKeys.RotateVector(FVector::UpVector);
		const float FramingHeightKeys = FramingWidthCm * Job.FrameHeight / static_cast<float>(FMath::Max(1, Job.FrameWidth));
		const FVector RelO = FVector::ZeroVector - FramingCenterCm;
		const float AO = FVector::DotProduct(RelO, RightKeys) / FMath::Max(1.f, FramingWidthCm) + 0.5f;
		const float BO = 0.5f - FVector::DotProduct(RelO, UpKeys) / FMath::Max(1.f, FramingHeightKeys);
		const int32 OPX = FMath::FloorToInt(AO * RTW - 0.5f);
		const int32 OPY = FMath::FloorToInt(BO * RTH - 0.5f);
		MinX = FMath::Min(MinX, OPX);
		MaxX = FMath::Max(MaxX, OPX);
		MinY = FMath::Min(MinY, OPY);
		MaxY = FMath::Max(MaxY, OPY);
	}
	const bool bTouchesEdge = (MinX <= 1 || MinY <= 1 || MaxX >= RTW - 2 || MaxY >= RTH - 2);
	if (bTouchesEdge && !bRetriedWide)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: the first framing guess is too tight, doubling the width once and retrying."),
			*Job.Id.ToString());
		bRetriedWide = true;
		FramingWidthCm *= 2.f;
		BakeDestroyRig();
		BakeSpawnRig(256);
		BakeStep = EBakeStep::WaitReady;
		StepTick = 0;
		return;
	}
	if (bTouchesEdge)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: the effect still touches the capture edge after doubling."), *Job.Id.ToString());
	}
	// Pixels to world offsets on the capture plane (center-plane approximation for perspective).
	const FRotator CamRot = CaptureComp->GetComponentRotation();
	const FVector RightV = CamRot.RotateVector(FVector::RightVector);
	const FVector UpV = CamRot.RotateVector(FVector::UpVector);
	const float FramingHeightCm = FramingWidthCm * Job.FrameHeight / static_cast<float>(FMath::Max(1, Job.FrameWidth));
	const FVector OldCenter = FramingCenterCm;
	float MinA = FLT_MAX;
	float MaxA = -FLT_MAX;
	float MinB = FLT_MAX;
	float MaxB = -FLT_MAX;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		const int32 PX = (Corner % 2 == 0) ? MinX : MaxX;
		const int32 PY = (Corner < 2) ? MinY : MaxY;
		const float A = ((static_cast<float>(PX) + 0.5f) / RTW - 0.5f) * FramingWidthCm;
		const float B = (0.5f - (static_cast<float>(PY) + 0.5f) / RTH) * FramingHeightCm;
		MinA = FMath::Min(MinA, A);
		MaxA = FMath::Max(MaxA, A);
		MinB = FMath::Min(MinB, B);
		MaxB = FMath::Max(MaxB, B);
	}
	float BoxW = (MaxA - MinA) * (1.f + 2.f * Job.AutoFramePadding);
	float BoxH = (MaxB - MinB) * (1.f + 2.f * Job.AutoFramePadding);
	// Keep the frame aspect: grow the short side.
	const float Aspect = Job.FrameWidth / static_cast<float>(FMath::Max(1, Job.FrameHeight));
	if (BoxW / FMath::Max(1.f, BoxH) < Aspect)
	{
		BoxW = BoxH * Aspect;
	}
	else
	{
		BoxH = BoxW / Aspect;
	}
	FramingCenterCm = OldCenter + RightV * ((MinA + MaxA) * 0.5f) + UpV * ((MinB + MaxB) * 0.5f);
	FramingWidthCm = BoxW;
	UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX %s: auto-frame %.0f cm at (%.0f, %.0f, %.0f)."),
		*Job.Id.ToString(), FramingWidthCm, FramingCenterCm.X, FramingCenterCm.Y, FramingCenterCm.Z);
	// Real pass with the new framing: destroy and re-spawn the system.
	BakeDestroyRig();
	bRespawned = true;
	BakeSpawnRig(Job.Resolution);
	BakeStep = EBakeStep::WaitReady;
	StepTick = 0;
}

void AGBPixelVfxBakeGameMode::BakeConvertAndWrite()
{
	using namespace GBPixelVfxBakeLocal;
	using namespace GBPixelVfxConvert;

	FSettings Settings;
	Settings.FrameWidth = Job.FrameWidth;
	Settings.FrameHeight = Job.FrameHeight;
	Settings.Fps = Job.Fps;
	Settings.bLoop = Job.bLoop;
	Settings.LoopBlendFrames = Job.LoopBlendFrames;
	Settings.AlphaMode = Job.AlphaMode;
	Settings.AlphaThreshold = Job.AlphaThreshold;
	Settings.ColorMode = Job.ColorMode;
	Settings.HotLuminance = Job.HotLuminance;
	Settings.HotSaturation = Job.HotSaturation;
	Settings.Ramps = Job.Ramps;
	Settings.LevelsPercentile = Job.LevelsPercentile;
	Settings.Gamma = Job.Gamma;
	Settings.Hysteresis = Job.Hysteresis;
	Settings.MinIslandPixels = Job.MinIslandPixels;
	Settings.bFillHoles = Job.bFillHoles;
	Settings.MinColorClusterPixels = Job.MinColorClusterPixels;
	Settings.FadeOutFrames = Job.FadeOutFrames;
	Settings.FadeOutMode = Job.FadeOutMode;
	Settings.Outline = Job.Outline;
	Settings.bLightEnabled = Job.bLightEnabled;

	if (Settings.Ramps.Num() == 0)
	{
		// A job without Ramps uses one default ramp (the FluidFire fire ramp).
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: the job has no palette ramps, using the default fire ramp."),
			*Job.Id.ToString());
		TArray<FColor> DefaultRamp;
		const TCHAR* Hex[] = { TEXT("#4B0F0F"), TEXT("#BC2A12"), TEXT("#F03416"),
			TEXT("#F3610B"), TEXT("#E2DE9C"), TEXT("#F5F3CF") };
		for (const TCHAR* H : Hex)
		{
			FColor Srgb;
			if (GBPixelVfxConvert::ParseHexColor(FString(H), Srgb))
			{
				DefaultRamp.Add(Srgb);
			}
		}
		Settings.Ramps.Add(MoveTemp(DefaultRamp));
	}
	TArray<FWorkFrame> Frames;
	Frames.Reserve(SimFrames.Num());
	for (const FSimFrame& Captured : SimFrames)
	{
		FWorkFrame Frame;
		Frame.Width = Captured.Width;
		Frame.Height = Captured.Height;
		Frame.Color = Captured.Color;
		Frame.Opacity = Captured.Opacity;
		Frames.Add(MoveTemp(Frame));
	}
	ApplyExposure(Frames, Job.Exposure);
	ApplyLoopBlend(Frames, Job.FrameCount(), Settings.LoopBlendFrames, Settings.bLoop);
	TArray<FWorkFrame> Small;
	Small.Reserve(Frames.Num());
	for (const FWorkFrame& Frame : Frames)
	{
		Small.Add(DownsampleFrame(Frame, Settings.FrameWidth, Settings.FrameHeight));
	}
	float Peak = 1.f;
	float HotPeak = 1.f;
	float ColdPeak = 1.f;
	ComputePeaks(Small, Settings, Peak, HotPeak, ColdPeak);
	TArray<FFrameMap> Maps;
	Maps.Reserve(Small.Num());
	for (const FWorkFrame& Frame : Small)
	{
		Maps.Add(MapFrame(Frame, Settings, Peak, HotPeak, ColdPeak));
		if (Maps.Num() > 1)
		{
			ApplyHysteresis(Maps.Last(), Maps[Maps.Num() - 2], Settings);
		}
	}
	if (Settings.bLoop && Maps.Num() > 1)
	{
		ApplyHysteresis(Maps[0], Maps.Last(), Settings);
	}
	for (FFrameMap& Map : Maps)
	{
		RemoveSmallIslands(Map, Settings.MinIslandPixels);
		if (Settings.bFillHoles)
		{
			FillOnePixelHoles(Map);
		}
	}
	// Color clusters, then the one-shot fade-out, both before the outline.
	// The light track is computed after the fade so a faded frame emits less.
	for (FFrameMap& Map : Maps)
	{
		RecolorSmallColorClusters(Map, Settings, Settings.MinColorClusterPixels);
	}
	ApplyFadeOut(Maps, Settings);
	TArray<float> Track;
	float AnchorU = 0.5f;
	float AnchorV = 0.6f;
	ComputeLightTrack(Maps, Settings, Track, AnchorU, AnchorV);
	TArray<TArray<FColor>> Pixels;
	ResolveFrames(Maps, Settings, Pixels);

	// Pivot: project the system origin on the capture plane.
	const FRotator CamRot = CaptureComp->GetComponentRotation();
	const FVector RightV = CamRot.RotateVector(FVector::RightVector);
	const FVector UpV = CamRot.RotateVector(FVector::UpVector);
	const float FramingHeightCm = FramingWidthCm * Job.FrameHeight / static_cast<float>(FMath::Max(1, Job.FrameWidth));
	const FVector Rel = FVector::ZeroVector - FramingCenterCm;
	const float PivotU = 0.5f + FVector::DotProduct(Rel, RightV) / FMath::Max(1.f, FramingWidthCm);
	const float PivotV = 0.5f - FVector::DotProduct(Rel, UpV) / FMath::Max(1.f, FramingHeightCm);

	FGBPixelVfxClip Clip;
	Clip.Id = Job.Id;
	Clip.AtlasFile = Job.Id.ToString() + TEXT(".png");
	Clip.FrameWidth = Job.FrameWidth;
	Clip.FrameHeight = Job.FrameHeight;
	int32 Columns = FMath::Max(1, FMath::CeilToInt(FMath::Sqrt(static_cast<float>(Maps.Num()))));
	while (Columns > 1 && Columns * Job.FrameWidth > 1024)
	{
		--Columns;
	}
	Clip.Columns = Columns;
	Clip.FrameCount = Maps.Num();
	Clip.Fps = Job.Fps;
	Clip.bLoop = Job.bLoop;
	Clip.WorldWidthCm = FramingWidthCm;
	Clip.WorldHeightCm = FramingHeightCm;
	Clip.PivotU = PivotU;
	Clip.PivotV = PivotV;
	if (Job.Facing.Equals(TEXT("Billboard"), ESearchCase::IgnoreCase)) Clip.Facing = EGBPixelVfxFacing::Billboard;
	else if (Job.Facing.Equals(TEXT("Ground"), ESearchCase::IgnoreCase)) Clip.Facing = EGBPixelVfxFacing::Ground;
	else if (Job.Facing.Equals(TEXT("Fixed"), ESearchCase::IgnoreCase)) Clip.Facing = EGBPixelVfxFacing::Fixed;
	else Clip.Facing = EGBPixelVfxFacing::Upright;
	Clip.Glow = Job.Glow;
	Clip.bLit = Job.bLit;
	Clip.Light.bEnabled = Job.bLightEnabled;
	Clip.Light.ColorHex = Job.LightColorHex;
	Clip.Light.Color = Job.LightColor;
	Clip.Light.Intensity = Job.LightIntensity;
	Clip.Light.RadiusCm = Job.LightRadiusCm;
	Clip.Light.bCastShadows = Job.bLightCastShadows;
	Clip.Light.TrackMode = Job.LightTrackMode;
	Clip.Light.AnchorU = AnchorU;
	Clip.Light.AnchorV = AnchorV;
	if (Job.LightTrackMode.Equals(TEXT("Constant"), ESearchCase::IgnoreCase))
	{
		Clip.Light.Track.SetNum(Maps.Num());
		for (float& Entry : Clip.Light.Track)
		{
			Entry = 1.f;
		}
	}
	else
	{
		Clip.Light.Track = Track;
	}
	Clip.SourceSystem = Job.SystemPath;
	Clip.SourceDurationSeconds = Job.DurationSeconds;
	Clip.SourceSimFps = Job.SimFps;
	Clip.EngineVersion = FEngineVersion::Current().ToString();
	Clip.BakedUtc = FDateTime::UtcNow().ToIso8601();

	const FString DataDir = FPaths::ProjectContentDir() / TEXT("Data/Vfx");
	EnsureDir(DataDir);
	const FString PngPath = DataDir / Clip.AtlasFile;
	const FString JsonPath = DataDir / (Job.Id.ToString() + TEXT(".json"));

	const int32 Rows = Clip.Rows();
	TArray<FColor> Atlas;
	Atlas.SetNumZeroed(Columns * Job.FrameWidth * Rows * Job.FrameHeight);
	for (int32 f = 0; f < Pixels.Num(); ++f)
	{
		const int32 Col = f % Columns;
		const int32 Row = f / Columns;
		for (int32 y = 0; y < Job.FrameHeight; ++y)
		{
			for (int32 x = 0; x < Job.FrameWidth; ++x)
			{
				Atlas[(Row * Job.FrameHeight + y) * Columns * Job.FrameWidth + Col * Job.FrameWidth + x] =
					Pixels[f][y * Job.FrameWidth + x];
			}
		}
	}
	if (!SavePng(PngPath, Columns * Job.FrameWidth, Rows * Job.FrameHeight, Atlas))
	{
		UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: could not write %s."), *Job.Id.ToString(), *PngPath);
		BakeLogAndCleanup(false);
		return;
	}
	FString JsonText;
	if (!Clip.SaveToJsonString(JsonText) || !FFileHelper::SaveStringToFile(JsonText, *JsonPath))
	{
		UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: could not write %s."), *Job.Id.ToString(), *JsonPath);
		BakeLogAndCleanup(false);
		return;
	}

	// Review files.
	const FString ReviewDir = FPaths::ProjectSavedDir() / TEXT("PixelVfx") / Job.Id.ToString();
	EnsureDir(ReviewDir);
	const int32 CellW = Job.FrameWidth * 6;
	const int32 CellH = Job.FrameHeight * 6;
	const int32 Gutter = 6;
	const int32 SheetCols = 8;
	const int32 SheetRows = (Pixels.Num() + SheetCols - 1) / SheetCols;
	const int32 SheetW = SheetCols * CellW + (SheetCols + 1) * Gutter;
	const int32 SheetH = SheetRows * CellH + (SheetRows + 1) * Gutter;
	const FColor Bg(0x3A, 0x3A, 0x3A, 255);
	const FColor CellBg(0x16, 0x16, 0x1C, 255);
	TArray<FColor> Sheet;
	Sheet.SetNum(SheetW * SheetH);
	for (FColor& Cell : Sheet)
	{
		Cell = Bg;
	}
	TArray<FColor> Source;
	Source.SetNum(SheetW * SheetH);
	for (FColor& Cell : Source)
	{
		Cell = FColor::Black;
	}
	TArray<FColor> CellBuf;
	CellBuf.SetNum(CellW * CellH);
	for (int32 f = 0; f < Pixels.Num(); ++f)
	{
		const int32 SX = Gutter + (f % SheetCols) * (CellW + Gutter);
		const int32 SY = Gutter + (f / SheetCols) * (CellH + Gutter);
		BlitPixelCell(Pixels[f].GetData(), Job.FrameWidth, Job.FrameHeight, CellBuf.GetData(), CellW, CellH);
		// Baked cells are opaque on a dark background so viewers never show them as white.
		for (int32 i = 0; i < CellBuf.Num(); ++i)
		{
			if (CellBuf[i].A == 0)
			{
				CellBuf[i] = CellBg;
			}
		}
		for (int32 y = 0; y < CellH; ++y)
		{
			for (int32 x = 0; x < CellW; ++x)
			{
				Sheet[(SY + y) * SheetW + SX + x] = CellBuf[y * CellW + x];
			}
		}
		// The matching source frame before conversion (post exposure, tone-mapped, smooth).
		if (SimFrames.IsValidIndex(f))
		{
			BlitSourceCell(SimFrames[f].Color.GetData(), SimFrames[f].Width, SimFrames[f].Height,
				CellBuf.GetData(), CellW, CellH);
			for (int32 y = 0; y < CellH; ++y)
			{
				for (int32 x = 0; x < CellW; ++x)
				{
					Source[(SY + y) * SheetW + SX + x] = CellBuf[y * CellW + x];
				}
			}
		}
	}
	SavePng(ReviewDir / TEXT("sheet.png"), SheetW, SheetH, Sheet);
	SavePng(ReviewDir / TEXT("source.png"), SheetW, SheetH, Source);
	int32 BigFrame = 0;
	for (int32 f = 0; f < Clip.Light.Track.Num(); ++f)
	{
		if (Clip.Light.Track[f] >= Clip.Light.Track[BigFrame])
		{
			BigFrame = f;
		}
	}
	if (Pixels.IsValidIndex(BigFrame))
	{
		const int32 BigW = Job.FrameWidth * 12;
		const int32 BigH = Job.FrameHeight * 12;
		TArray<FColor> Big;
		Big.SetNum(BigW * BigH);
		BlitPixelCell(Pixels[BigFrame].GetData(), Job.FrameWidth, Job.FrameHeight, Big.GetData(), BigW, BigH);
		for (FColor& Pixel : Big)
		{
			if (Pixel.A == 0)
			{
				Pixel = CellBg;
			}
		}
		SavePng(ReviewDir / TEXT("frame_big.png"), BigW, BigH, Big);
	}

	int32 SolidTotal = 0;
	for (const FFrameMap& Map : Maps)
	{
		SolidTotal += Map.SolidCount();
	}
	const float SolidAvg = Maps.Num() > 0 && Job.FrameWidth > 0 && Job.FrameHeight > 0
		? 100.f * SolidTotal / (Maps.Num() * Job.FrameWidth * Job.FrameHeight)
		: 0.f;
	if (SolidAvg < 1.f || SolidAvg > 95.f)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX %s: average solid coverage is %.1f%% (framing or threshold may be wrong)."),
			*Job.Id.ToString(), SolidAvg);
	}
	UE_LOG(LogGBPixelVfx, Display,
		TEXT("PVFX BAKE %s: frames=%d frame=%dx%d world=%.0fx%.0f cm peak=%.2f solid=%.1f%% colors=%d -> %s"),
		*Job.Id.ToString(), Maps.Num(), Job.FrameWidth, Job.FrameHeight,
		FramingWidthCm, FramingHeightCm, Peak, SolidAvg, CountDistinctColors(Pixels), *PngPath);
	BakeLogAndCleanup(true);
}

void AGBPixelVfxBakeGameMode::BakeLogAndCleanup(bool bOk)
{
	if (!bOk)
	{
		UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: job failed."), *Job.Id.ToString());
	}
	BakeDestroyRig();
	FApp::SetUseFixedTimeStep(false);
	++BakeJobIndex;
	BakeStep = EBakeStep::NextJob;
	StepTick = 0;
}

void AGBPixelVfxBakeGameMode::TickBake()
{
	switch (BakeStep)
	{
	case EBakeStep::StartJob:
	{
		if (BakeJobIndex >= BakeJobFiles.Num())
		{
			FinishRun();
			return;
		}
		if (!Job.LoadFromFile(BakeJobFiles[BakeJobIndex]))
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX cannot read job file '%s'."),
				*BakeJobFiles[BakeJobIndex]);
			++BakeJobIndex;
			return;
		}
		UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX baking %s (%s)."), *Job.Id.ToString(), *Job.SystemPath);
		FApp::SetUseFixedTimeStep(true);
		FApp::SetFixedDeltaTime(1.0 / FMath::Max(1, Job.SimFps));
		FramingWidthCm = Job.WidthCm;
		FramingCenterCm = Job.CenterCm;
		bLegacyCapture = false;
		bRetriedWide = false;
		bRespawned = false;
		SimFrames.Reset();
		if (!BakeSpawnRig(Job.bAutoFrame ? 256 : Job.Resolution))
		{
			BakeLogAndCleanup(false);
			return;
		}
		SimTick = 0;
		BakeStep = EBakeStep::WaitReady;
		StepTick = 0;
		break;
	}
	case EBakeStep::WaitReady:
	{
		++StepTick;
		if (NiagaraSystem && NiagaraSystem->IsReadyToRun())
		{
			UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX %s: system ready, running the sim."),
				*Job.Id.ToString());
			// Fresh pass: the prepass and the real pass each simulate from t = 0.
			SimTick = 0;
			CaptureCursor = 0;
			SimFrames.Reset();
			bRunningPrepass = Job.bAutoFrame && !bRespawned;
			BakeFillCaptureTicks(bRunningPrepass);
			BakeStep = EBakeStep::RunSim;
			StepTick = 0;
		}
		else if (StepTick == 1 || StepTick % 600 == 0)
		{
			UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX %s: waiting for the system to compile (%d)."),
				*Job.Id.ToString(), StepTick);
		}
		else if (StepTick > 3600)
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: the system never became ready to run."),
				*Job.Id.ToString());
			BakeLogAndCleanup(false);
		}
		break;
	}
	case EBakeStep::RunSim:
	{
		++SimTick;
		if (CaptureCursor < CaptureAtTicks.Num() && SimTick >= CaptureAtTicks[CaptureCursor])
		{
			if (!BakeCaptureOne())
			{
				UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX %s: capture readback failed at sim tick %d."),
					*Job.Id.ToString(), SimTick);
				BakeLogAndCleanup(false);
				break;
			}
			++CaptureCursor;
		}
		if (CaptureCursor >= CaptureTotalFrames && CaptureTotalFrames > 0)
		{
			if (bRunningPrepass)
			{
				bRunningPrepass = false;
				BakeAnalysePrepass();
			}
			else
			{
				BakeStep = EBakeStep::ConvertWrite;
			}
		}
		break;
	}
	case EBakeStep::ConvertWrite:
		BakeConvertAndWrite();
		BakeStep = EBakeStep::NextJob;
		StepTick = 0;
		break;
	case EBakeStep::NextJob:
		BakeStep = EBakeStep::StartJob;
		break;
	}
}
