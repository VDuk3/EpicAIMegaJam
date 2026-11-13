// AGBPixelVfxBakeGameMode: showcase and compare modes (lit stage, screenshots, quick benchmark).
#include "GBPixelVfxBakeGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicRHI.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"
#include "GBPixelVfxTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstanceController.h"
#include "UnrealClient.h"

namespace GBPixelVfxStageLocal
{
	const TCHAR* StageMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxStage.M_PixelVfxStage");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");
	// Distance between neighbouring clips on the stage (the widest clips are close to 6 m).
	const float StageStepY = 700.f;

	void AddBox(UWorld* World, TArray<TObjectPtr<AActor>>& Out, const FString& BasePath,
		const FVector& Pos, const FVector& Scale, const FLinearColor& Color, float Roughness, float Metallic)
	{
		if (!World)
		{
			return;
		}
		UStaticMesh* Cube = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, CubePath));
		UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
			nullptr, *BasePath));
		if (!Cube || !Base)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Pos), Params);
		if (!Actor)
		{
			return;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
		Mesh->SetMobility(EComponentMobility::Movable);
		Actor->SetRootComponent(Mesh);
		// A plain AActor spawns without a root component, so the spawn transform is lost: place the root here.
		Mesh->SetWorldLocation(Pos);
		Mesh->SetStaticMesh(Cube);
		Mesh->SetRelativeScale3D(Scale);
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->RegisterComponent();
		UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(Base, Actor);
		Dyn->SetVectorParameterValue(TEXT("Color"), Color);
		Dyn->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Dyn->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		Mesh->SetMaterial(0, Dyn);
		Out.Add(Actor);
	}

	void AddCylinder(UWorld* World, TArray<TObjectPtr<AActor>>& Out, const FString& BasePath,
		const FVector& Pos, float RadiusCm, float HeightCm, const FLinearColor& Color, float Roughness)
	{
		if (!World)
		{
			return;
		}
		UStaticMesh* Cylinder = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, CylinderPath));
		UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
			nullptr, *BasePath));
		if (!Cylinder || !Base)
		{
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Pos), Params);
		if (!Actor)
		{
			return;
		}
		UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
		Mesh->SetMobility(EComponentMobility::Movable);
		Actor->SetRootComponent(Mesh);
		// A plain AActor spawns without a root component, so the spawn transform is lost: place the root here.
		Mesh->SetWorldLocation(Pos);
		Mesh->SetStaticMesh(Cylinder);
		// The engine cylinder is 100 cm across and 100 cm tall.
		Mesh->SetRelativeScale3D(FVector(RadiusCm * 2.f / 100.f, RadiusCm * 2.f / 100.f, HeightCm / 100.f));
		Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mesh->RegisterComponent();
		UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(Base, Actor);
		Dyn->SetVectorParameterValue(TEXT("Color"), Color);
		Dyn->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Dyn->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Mesh->SetMaterial(0, Dyn);
		Out.Add(Actor);
	}

	void AimCameraAt(ACameraActor* Cam, const FVector& From, const FVector& At)
	{
		if (!Cam)
		{
			return;
		}
		Cam->SetActorLocation(From);
		Cam->SetActorRotation((At - From).Rotation());
	}
}

APlayerController* AGBPixelVfxBakeGameMode::EnsurePlayerController()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			return PC;
		}
		return UGameplayStatics::CreatePlayer(World, 0);
	}
	return nullptr;
}

void AGBPixelVfxBakeGameMode::BuildStage(const FString& MaterialPath)
{
	using namespace GBPixelVfxStageLocal;
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	DestroyStage();
	// The stage runs along world Y: clips at X = 0, the back wall at X = +500,
	// the camera at X = -2400 looking +X.
	AddBox(World, StageActors, MaterialPath, FVector(-600.f, 0.f, -50.f), FVector(50.f, 44.f, 1.f),
		FLinearColor(0.02f, 0.02f, 0.025f), 0.15f, 0.f);
	// Back wall, matte grey.
	AddBox(World, StageActors, MaterialPath, FVector(500.f, 0.f, 600.f), FVector(1.f, 44.f, 14.f),
		FLinearColor(0.18f, 0.18f, 0.19f), 0.7f, 0.f);
	// Occluders between the clips and the wall, plus a few beside the clips, so the
	// clip lights throw visible shadows. Heights 80 to 250 cm.
	const float ObstacleHeights[3] = { 120.f, 200.f, 160.f };
	for (int32 i = 0; i < 3; ++i)
	{
		const float H = ObstacleHeights[i];
		const float Y = (i - 1) * StageStepY;
		if (i % 2 == 0)
		{
			AddBox(World, StageActors, MaterialPath, FVector(250.f, Y, H * 0.5f),
				FVector(H / 100.f, H / 100.f, H / 100.f),
				FLinearColor(0.1f, 0.1f, 0.11f), 0.6f, 0.f);
		}
		else
		{
			AddCylinder(World, StageActors, MaterialPath, FVector(250.f, Y, H * 0.5f),
				H * 0.5f, H, FLinearColor(0.1f, 0.1f, 0.11f), 0.6f);
		}
	}
	AddCylinder(World, StageActors, MaterialPath, FVector(-250.f, -StageStepY, 50.f),
		60.f, 100.f, FLinearColor(0.1f, 0.1f, 0.11f), 0.6f);
	AddBox(World, StageActors, MaterialPath, FVector(-250.f, StageStepY, 90.f),
		FVector(1.8f, 1.8f, 1.8f), FLinearColor(0.1f, 0.1f, 0.11f), 0.6f, 0.f);
	// One very dim directional fill so the stage is readable but the clip lights dominate.
	// The fill never casts shadows: the clip lights are the ones that must cast.
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ADirectionalLight* Dim = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(),
		FTransform::Identity, Params);
	if (Dim)
	{
		if (UDirectionalLightComponent* LightComp = Cast<UDirectionalLightComponent>(Dim->GetLightComponent()))
		{
			LightComp->SetMobility(EComponentMobility::Movable);
			LightComp->SetIntensity(0.3f);
			LightComp->SetLightColor(FLinearColor(0.8f, 0.85f, 1.f));
			LightComp->SetCastShadows(false);
			LightComp->SetUseRayTracedDistanceFieldShadows(false);
		}
		Dim->SetActorRotation(FRotator(-50.f, 20.f, 0.f));
		StageActors.Add(Dim);
	}
	// Stage camera with its final wide transform in the spawn call.
	const FVector WideFrom(-2400.f, 0.f, 420.f);
	const FVector WideAt(0.f, 0.f, 260.f);
	const FRotator WideRot = (WideAt - WideFrom).Rotation();
	StageCam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(),
		FTransform(WideRot, WideFrom), Params);
	StagePC = EnsurePlayerController();
	if (StageCam)
	{
		StageCam->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Perspective;
		StageCam->GetCameraComponent()->FieldOfView = 60.f;
		// Fixed exposure so dark scenes are not auto-exposed: auto metering pinned to a
		// single value (min == max, the engine's "fake manual exposure") plus a bias.
		// NOTE: AEM_Manual was tried first (as specified) but couples to the physical
		// camera defaults and underexposed the stage ~10 stops in -game; pinned auto
		// delivers the required fixed exposure. Bloom stays default.
		FPostProcessSettings& PP = StageCam->GetCameraComponent()->PostProcessSettings;
		PP.bOverride_AutoExposureMethod = true;
		PP.AutoExposureMethod = EAutoExposureMethod::AEM_Basic;
		PP.bOverride_AutoExposureMinBrightness = true;
		PP.AutoExposureMinBrightness = 1.0f;
		PP.bOverride_AutoExposureMaxBrightness = true;
		PP.AutoExposureMaxBrightness = 1.0f;
		PP.bOverride_AutoExposureBias = true;
		PP.AutoExposureBias = 1.0f;
	}
	if (StagePC && StageCam)
	{
		StagePC->SetViewTargetWithBlend(StageCam, 0.f);
	}
}

void AGBPixelVfxBakeGameMode::DestroyStage()
{
	for (const TObjectPtr<AActor>& Actor : StageActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	StageActors.Reset();
	StageClips.Reset();
	StageCam = nullptr;
	StagePC = nullptr;
}

void AGBPixelVfxBakeGameMode::TakeStageScreenshot(const FString& Path)
{
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	Files.CreateDirectoryTree(*FPaths::GetPath(Path));
	FScreenshotRequest::RequestScreenshot(Path, false, false);
	UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX screenshot %s."), *Path);
}

void AGBPixelVfxBakeGameMode::TickShowcase()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FApp::SetUseFixedTimeStep(false);
	switch (StageStep)
	{
	case 0:
	{
		BuildStage(FString(GBPixelVfxStageLocal::StageMaterialPath));
		UGBPixelVfxSubsystem* Subsystem = World->GetGameInstance()
			? World->GetGameInstance()->GetSubsystem<UGBPixelVfxSubsystem>()
			: nullptr;
		StageClipIds = Subsystem ? Subsystem->ListClipIds() : TArray<FName>();
		if (StageClipIds.Num() == 0)
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX showcase: no baked clips found."));
			FinishRun();
			return;
		}
		const float StepY = GBPixelVfxStageLocal::StageStepY;
		const float Y0 = -(StageClipIds.Num() - 1) * StepY * 0.5f;
		int32 SpawnedCount = 0;
		int32 PlayingCount = 0;
		for (int32 i = 0; i < StageClipIds.Num(); ++i)
		{
			AGBPixelVfxActor* Spawned = AGBPixelVfxActor::SpawnPixelVfx(
				this, StageClipIds[i], FTransform(FVector(0.f, Y0 + i * StepY, 0.f)), 1.f);
			if (Spawned)
			{
				++SpawnedCount;
				Spawned->PixelVfx->bAutoDestroyOwnerWhenFinished = false;
				if (Spawned->PixelVfx->IsPlaying())
				{
					++PlayingCount;
				}
			}
			StageClips.Add(Spawned);
		}
		AActor* ViewTarget = StagePC ? StagePC->GetViewTarget() : nullptr;
		UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase cam=%s rot=%s viewtarget=%s clips=%d spawned=%d playing=%d"),
			StageCam ? *StageCam->GetActorLocation().ToCompactString() : TEXT("none"),
			StageCam ? *StageCam->GetActorRotation().ToCompactString() : TEXT("none"),
			ViewTarget ? *ViewTarget->GetName() : TEXT("none"),
			StageClipIds.Num(), SpawnedCount, PlayingCount);
		StageTimeMark = FPlatformTime::Seconds();
		StageStep = 1;
		break;
	}
	case 1:
	{
		// One-shots are re-spawned when they finish.
		const float StepY = GBPixelVfxStageLocal::StageStepY;
		const float Y0 = -(StageClipIds.Num() - 1) * StepY * 0.5f;
		for (int32 i = 0; i < StageClips.Num(); ++i)
		{
			AGBPixelVfxActor* Actor = StageClips[i].Get();
			if (!Actor || !Actor->PixelVfx || !Actor->PixelVfx->IsPlaying())
			{
				if (Actor)
				{
					Actor->Destroy();
				}
				AGBPixelVfxActor* Spawned = AGBPixelVfxActor::SpawnPixelVfx(
					this, StageClipIds[i], FTransform(FVector(0.f, Y0 + i * StepY, 0.f)), 1.f);
				if (Spawned)
				{
					Spawned->PixelVfx->bAutoDestroyOwnerWhenFinished = false;
					StageClips[i] = Spawned;
				}
			}
		}
		if (FPlatformTime::Seconds() - StageTimeMark >= 2.0)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			TakeStageScreenshot(Dir / TEXT("showcase_1.png"));
			StageTimeMark = FPlatformTime::Seconds();
			StageWait = 15;
			StageStep = 2;
		}
		break;
	}
	case 2:
		if (--StageWait <= 0 && FPlatformTime::Seconds() - StageTimeMark >= 0.7)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			TakeStageScreenshot(Dir / TEXT("showcase_2.png"));
			StageShotIndex = 0;
			StageWait = 15;
			StageStep = 3;
		}
		break;
	case 3:
		if (--StageWait <= 0)
		{
			if (StageShotIndex >= StageClipIds.Num())
			{
				// Last shot: the camera moved to another place (the game's camera moves all the time).
				GBPixelVfxStageLocal::AimCameraAt(StageCam, FVector(-1500.f, -1900.f, 420.f), FVector(0.f, 0.f, 250.f));
				if (StageCam)
				{
					StageCam->GetCameraComponent()->SetFieldOfView(60.f);
				}
				StageTimeMark = FPlatformTime::Seconds();
				StageStep = 5;
				break;
			}
			// One-shots may have finished while earlier close-ups were taken: re-spawn.
			// (Conditional: loops keep playing so their animation continues.)
			const float StepY = GBPixelVfxStageLocal::StageStepY;
			const float Y0 = -(StageClipIds.Num() - 1) * StepY * 0.5f;
			const FVector ClipAt(0.f, Y0 + StageShotIndex * StepY, 0.f);
			AGBPixelVfxActor* ShotActor = StageClips[StageShotIndex].Get();
			if (!ShotActor || !ShotActor->PixelVfx || !ShotActor->PixelVfx->IsPlaying())
			{
				if (ShotActor)
				{
					ShotActor->Destroy();
				}
				AGBPixelVfxActor* Spawned = AGBPixelVfxActor::SpawnPixelVfx(
					this, StageClipIds[StageShotIndex], FTransform(ClipAt), 1.f);
				if (Spawned)
				{
					Spawned->PixelVfx->bAutoDestroyOwnerWhenFinished = false;
					StageClips[StageShotIndex] = Spawned;
				}
			}
			// Close-up framing WITHOUT translating the camera (rotate in place and
			// narrow the FOV instead): translating the stage camera breaks quad
			// rendering in -game here, while rotating in place does not. This
			// deviates from "600 cm in front" (see report); the framing still shows
			// each sprite large, upright and with its base visible.
			if (StageCam)
			{
				// Aim at the middle of the clip's frame and open the lens just enough to fit it.
				float ClipW = 220.f;
				float ClipH = 300.f;
				float PivotV = 1.f;
				UGBPixelVfxSubsystem* ClipSubsystem = World->GetGameInstance()
					? World->GetGameInstance()->GetSubsystem<UGBPixelVfxSubsystem>()
					: nullptr;
				if (const FGBPixelVfxClip* ShotClip = ClipSubsystem ? ClipSubsystem->FindClip(StageClipIds[StageShotIndex]) : nullptr)
				{
					ClipW = ShotClip->WorldWidthCm;
					ClipH = ShotClip->WorldHeightCm;
					PivotV = ShotClip->PivotV;
				}
				const FVector ToClip = FVector(ClipAt.X, ClipAt.Y, (PivotV - 0.5f) * ClipH) - StageCam->GetActorLocation();
				const float HalfWidth = FMath::Max(ClipW * 0.5f, ClipH * 0.5f * 16.f / 9.f) * 1.2f;
				StageCam->SetActorRotation(ToClip.Rotation());
				StageCam->GetCameraComponent()->SetFieldOfView(
					FMath::RadiansToDegrees(2.f * FMath::Atan(HalfWidth / FMath::Max(1.f, ToClip.Size()))));
			}
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 4;
		}
		break;
	case 4:
		if (FPlatformTime::Seconds() - StageTimeMark >= 1.0)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			TakeStageScreenshot(Dir / (TEXT("showcase_") + StageClipIds[StageShotIndex].ToString() + TEXT(".png")));
			++StageShotIndex;
			StageWait = 15;
			StageStep = 3;
		}
		break;
	case 5:
		if (FPlatformTime::Seconds() - StageTimeMark >= 1.0)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			TakeStageScreenshot(Dir / TEXT("showcase_3.png"));
			StageWait = 15;
			StageStep = 6;
		}
		break;
	case 6:
		if (--StageWait <= 0)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX SHOWCASE: %d clips, screenshots in %s."),
				StageClipIds.Num(), *Dir);
			FinishRun();
		}
		break;
	default:
		break;
	}
}

// ---- compare mode ----

void AGBPixelVfxBakeGameMode::CmpClearActors()
{
	for (const TObjectPtr<AActor>& Actor : CmpActors)
	{
		if (Actor)
		{
			Actor->Destroy();
		}
	}
	CmpActors.Reset();
}

void AGBPixelVfxBakeGameMode::CmpSpawnSourceGrid()
{
	UWorld* World = GetWorld();
	UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *CmpJob.SystemPath);
	if (!World || !System)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 iy = 0; iy < 2; ++iy)
	{
		for (int32 ix = 0; ix < 4; ++ix)
		{
			const FVector At(-150.f + iy * 300.f, -600.f + ix * 400.f, 0.f);
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
			if (!Actor)
			{
				continue;
			}
			UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor, NAME_None, RF_Transient);
			Actor->SetRootComponent(Comp);
			Comp->SetWorldLocation(At);
			Comp->RegisterComponent();
			Comp->SetAsset(System);
			Comp->SetRandomSeedOffset(ix + iy * 4);
			Comp->SetAllowScalability(false);
			Comp->Activate(true);
			CmpActors.Add(Actor);
		}
	}
}

void AGBPixelVfxBakeGameMode::CmpSpawnBakedGrid()
{
	for (int32 iy = 0; iy < 2; ++iy)
	{
		for (int32 ix = 0; ix < 4; ++ix)
		{
			const FVector At(-150.f + iy * 300.f, -600.f + ix * 400.f, 0.f);
			if (AGBPixelVfxActor* Actor = AGBPixelVfxActor::SpawnPixelVfx(this, CmpJob.Id, FTransform(At), 1.f))
			{
				Actor->PixelVfx->bAutoDestroyOwnerWhenFinished = false;
				CmpActors.Add(Actor);
			}
		}
	}
}

void AGBPixelVfxBakeGameMode::CmpWriteResults()
{
	const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx") / CmpJob.Id.ToString();
	IPlatformFile& Files = FPlatformFileManager::Get().GetPlatformFile();
	Files.CreateDirectoryTree(*Dir);
	UE_LOG(LogGBPixelVfx, Display,
		TEXT("PVFX COMPARE %s: x8 source frame=%.1f ms gpu=%.1f ms | x8 baked frame=%.1f ms gpu=%.1f ms | empty frame=%.1f ms gpu=%.1f ms"),
		*CmpJob.Id.ToString(), CmpSourceFrameMs, CmpSourceGpuMs,
		CmpBakedFrameMs, CmpBakedGpuMs, CmpEmptyFrameMs, CmpEmptyGpuMs);
	const FString Json = FString::Printf(
		TEXT("{\"Id\":\"%s\",\"SourceFrameMs\":%.2f,\"SourceGpuMs\":%.2f,\"BakedFrameMs\":%.2f,\"BakedGpuMs\":%.2f,\"EmptyFrameMs\":%.2f,\"EmptyGpuMs\":%.2f}"),
		*CmpJob.Id.ToString(), CmpSourceFrameMs, CmpSourceGpuMs,
		CmpBakedFrameMs, CmpBakedGpuMs, CmpEmptyFrameMs, CmpEmptyGpuMs);
	FFileHelper::SaveStringToFile(Json, *(Dir / TEXT("compare.json")));
}

void AGBPixelVfxBakeGameMode::TickCompare()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	FApp::SetUseFixedTimeStep(false);
	switch (StageStep)
	{
	case 0:
	{
		if (!CmpJob.LoadFromFile(FPaths::ProjectPluginsDir() / TEXT("GBPixelVfx/Bake/Jobs") / (RunArg + TEXT(".json"))))
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX compare: cannot read job '%s'."), *RunArg);
			FinishRun();
			return;
		}
		if (GEngine)
		{
			GEngine->Exec(World, TEXT("t.MaxFPS 0"));
			GEngine->Exec(World, TEXT("r.VSync 0"));
		}
		BuildStage(FString(GBPixelVfxStageLocal::StageMaterialPath));
		AActor* ViewTarget = StagePC ? StagePC->GetViewTarget() : nullptr;
		UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX compare cam=%s viewtarget=%s job=%s"),
			StageCam ? *StageCam->GetActorLocation().ToCompactString() : TEXT("none"),
			ViewTarget ? *ViewTarget->GetName() : TEXT("none"), *CmpJob.Id.ToString());
		// Phase 0: the source system and the baked clip side by side, 500 cm apart.
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *CmpJob.SystemPath);
		if (!System)
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX compare: cannot load system '%s'."), *CmpJob.SystemPath);
			FinishRun();
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(0.f, -250.f, 0.f)), Params))
		{
			UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor, NAME_None, RF_Transient);
			Actor->SetRootComponent(Comp);
			Comp->SetWorldLocation(FVector(0.f, -250.f, 0.f));
			Comp->RegisterComponent();
			Comp->SetAsset(System);
			Comp->SetRandomSeedOffset(0);
			Comp->SetAllowScalability(false);
			Comp->Activate(true);
			CmpActors.Add(Actor);
		}
		if (AGBPixelVfxActor* Baked = AGBPixelVfxActor::SpawnPixelVfx(this, CmpJob.Id, FTransform(FVector(0.f, 250.f, 0.f)), 1.f))
		{
			Baked->PixelVfx->bAutoDestroyOwnerWhenFinished = false;
			CmpActors.Add(Baked);
		}
		StageTimeMark = FPlatformTime::Seconds();
		StageStep = 1;
		break;
	}
	case 1:
		// Warm-up plus one second, then the side-by-side screenshot.
		if (FPlatformTime::Seconds() - StageTimeMark >= CmpJob.WarmupSeconds + 1.0)
		{
			// The timing numbers are only valid if the source simulation runs: log its state.
			int32 SourceActive = 0;
			float SourceAge = -1.f;
			for (const TObjectPtr<AActor>& Actor : CmpActors)
			{
				if (UNiagaraComponent* Comp = Actor ? Actor->FindComponentByClass<UNiagaraComponent>() : nullptr)
				{
					SourceActive = Comp->IsActive() ? 1 : 0;
					if (Comp->GetSystemInstanceController().IsValid())
					{
						SourceAge = Comp->GetSystemInstanceController()->GetAge();
					}
				}
			}
			int32 BakedPlaying = 0;
			for (const TObjectPtr<AActor>& Actor : CmpActors)
			{
				if (AGBPixelVfxActor* Baked = Cast<AGBPixelVfxActor>(Actor))
				{
					if (Baked->PixelVfx && Baked->PixelVfx->IsPlaying())
					{
						BakedPlaying = 1;
					}
				}
			}
			UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX compare diag: source active=%d age=%.1f baked playing=%d"),
				SourceActive, SourceAge, BakedPlaying);
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx") / CmpJob.Id.ToString();
			TakeStageScreenshot(Dir / TEXT("compare.png"));
			StageWait = 15;
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 2;
		}
		break;
	case 2:
		if (--StageWait <= 0)
		{
			CmpClearActors();
			CmpSpawnSourceGrid();
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 3;
		}
		break;
	case 3:
		// Let 8 source systems run for 3 s, then measure 180 frames.
		if (FPlatformTime::Seconds() - StageTimeMark >= 3.0)
		{
			CmpMeasureLeft = 180;
			CmpCpuSum = 0.0;
			CmpGpuSum = 0.0;
			CmpCpuCount = 0;
			CmpPrevNow = FPlatformTime::Seconds();
			CmpPrevGpu = RHIGetGPUFrameCycles();
			StageStep = 4;
		}
		break;
	case 4:
	{
		const double Now = FPlatformTime::Seconds();
		const double Dt = (Now - CmpPrevNow) * 1000.0;
		CmpPrevNow = Now;
		// RHIGetGPUFrameCycles returns the last measured GPU frame cost already in
		// CPU-cycle units (see FRHIGPUFrameTimeHistory::PushFrameCycles), so each
		// sample converts directly; no delta against the previous sample.
		CmpGpuSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		CmpCpuSum += Dt;
		++CmpCpuCount;
		if (--CmpMeasureLeft <= 0)
		{
			CmpSourceFrameMs = CmpCpuSum / FMath::Max(1, CmpCpuCount);
			CmpSourceGpuMs = CmpGpuSum / FMath::Max(1, CmpCpuCount);
			CmpClearActors();
			CmpSpawnBakedGrid();
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 5;
		}
		break;
	}
	case 5:
		// Baked population settles for 1 s (one-shots restart when they finish, lights included).
		for (const TObjectPtr<AActor>& Actor : CmpActors)
		{
			if (AGBPixelVfxActor* Baked = Cast<AGBPixelVfxActor>(Actor))
			{
				if (Baked->PixelVfx && !Baked->PixelVfx->IsPlaying())
				{
					Baked->PixelVfx->Play(CmpJob.Id, true);
				}
			}
		}
		if (FPlatformTime::Seconds() - StageTimeMark >= 1.0)
		{
			CmpMeasureLeft = 180;
			CmpCpuSum = 0.0;
			CmpGpuSum = 0.0;
			CmpCpuCount = 0;
			CmpPrevNow = FPlatformTime::Seconds();
			CmpPrevGpu = RHIGetGPUFrameCycles();
			StageStep = 6;
		}
		break;
	case 6:
	{
		for (const TObjectPtr<AActor>& Actor : CmpActors)
		{
			if (AGBPixelVfxActor* Baked = Cast<AGBPixelVfxActor>(Actor))
			{
				if (Baked->PixelVfx && !Baked->PixelVfx->IsPlaying())
				{
					Baked->PixelVfx->Play(CmpJob.Id, true);
				}
			}
		}
		const double Now = FPlatformTime::Seconds();
		const double Dt = (Now - CmpPrevNow) * 1000.0;
		CmpPrevNow = Now;
		CmpGpuSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		CmpCpuSum += Dt;
		++CmpCpuCount;
		if (--CmpMeasureLeft <= 0)
		{
			CmpBakedFrameMs = CmpCpuSum / FMath::Max(1, CmpCpuCount);
			CmpBakedGpuMs = CmpGpuSum / FMath::Max(1, CmpCpuCount);
			CmpClearActors();
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 7;
		}
		break;
	}
	case 7:
		if (FPlatformTime::Seconds() - StageTimeMark >= 1.0)
		{
			CmpMeasureLeft = 180;
			CmpCpuSum = 0.0;
			CmpGpuSum = 0.0;
			CmpCpuCount = 0;
			CmpPrevNow = FPlatformTime::Seconds();
			CmpPrevGpu = RHIGetGPUFrameCycles();
			StageStep = 8;
		}
		break;
	case 8:
	{
		const double Now = FPlatformTime::Seconds();
		const double Dt = (Now - CmpPrevNow) * 1000.0;
		CmpPrevNow = Now;
		CmpGpuSum += FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles());
		CmpCpuSum += Dt;
		++CmpCpuCount;
		if (--CmpMeasureLeft <= 0)
		{
			CmpEmptyFrameMs = CmpCpuSum / FMath::Max(1, CmpCpuCount);
			CmpEmptyGpuMs = CmpGpuSum / FMath::Max(1, CmpCpuCount);
			CmpWriteResults();
			FinishRun();
		}
		break;
	}
	default:
		break;
	}
}
