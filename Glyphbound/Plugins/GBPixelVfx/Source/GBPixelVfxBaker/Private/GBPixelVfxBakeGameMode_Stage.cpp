// AGBPixelVfxBakeGameMode: showcase and compare modes (lit stage, screenshots, quick benchmark).
#include "GBPixelVfxBakeGameMode.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "DynamicRHI.h"
#include "Engine/DirectionalLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/TextureRenderTarget2D.h"
#include "GameFramework/PlayerController.h"
#include "GBPixelVfxActor.h"
#include "GBPixelVfxComponent.h"
#include "GBPixelVfxSubsystem.h"
#include "GBPixelVfxTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "TextureResource.h"
#include "UnrealClient.h"

namespace GBPixelVfxStageLocal
{
	const TCHAR* StageMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxStage.M_PixelVfxStage");
	const TCHAR* CubePath = TEXT("/Engine/BasicShapes/Cube.Cube");
	const TCHAR* CylinderPath = TEXT("/Engine/BasicShapes/Cylinder.Cylinder");

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
	// Floor 3000 x 2000 cm, dark, glossy enough to show reflections.
	AddBox(World, StageActors, MaterialPath, FVector(0.f, 0.f, -50.f), FVector(30.f, 20.f, 1.f),
		FLinearColor(0.02f, 0.02f, 0.025f), 0.15f, 0.f);
	// Back wall, matte grey.
	AddBox(World, StageActors, MaterialPath, FVector(0.f, 1100.f, 600.f), FVector(30.f, 1.f, 14.f),
		FLinearColor(0.18f, 0.18f, 0.19f), 0.7f, 0.f);
	// Occluders between the clips and the wall so the clip lights throw visible shadows.
	AddBox(World, StageActors, MaterialPath, FVector(-500.f, 700.f, 150.f), FVector(4.f, 4.f, 4.f),
		FLinearColor(0.1f, 0.1f, 0.11f), 0.6f, 0.f);
	AddBox(World, StageActors, MaterialPath, FVector(100.f, 750.f, 225.f), FVector(3.f, 3.f, 5.5f),
		FLinearColor(0.1f, 0.1f, 0.11f), 0.6f, 0.f);
	AddCylinder(World, StageActors, MaterialPath, FVector(600.f, 700.f, 175.f), 150.f, 350.f,
		FLinearColor(0.1f, 0.1f, 0.11f), 0.6f);
	// One very dim directional light so the stage is readable but the clip lights dominate.
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
		}
		Dim->SetActorRotation(FRotator(-50.f, -30.f, 0.f));
		StageActors.Add(Dim);
	}
	// Stage camera, repositioned per shot.
	StageCam = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FTransform::Identity, Params);
	StagePC = EnsurePlayerController();
	if (StagePC && StageCam)
	{
		StagePC->SetViewTarget(StageCam);
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
		const float StepX = 600.f;
		const float X0 = -(StageClipIds.Num() - 1) * StepX * 0.5f;
		int32 SpawnedCount = 0;
		int32 PlayingCount = 0;
		for (int32 i = 0; i < StageClipIds.Num(); ++i)
		{
			AGBPixelVfxActor* Spawned = AGBPixelVfxActor::SpawnPixelVfx(
				this, StageClipIds[i], FTransform(FVector(X0 + i * StepX, 0.f, 0.f)), 1.f);
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
		UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag: pc=%d viewtarget=%s clips=%d spawned=%d playing=%d stageactors=%d cam=%s"),
			StagePC ? 1 : 0, StagePC && StagePC->GetViewTarget() ? *StagePC->GetViewTarget()->GetName() : TEXT("none"),
			StageClipIds.Num(), SpawnedCount, PlayingCount, StageActors.Num(),
			StageCam ? *StageCam->GetActorLocation().ToCompactString() : TEXT("none"));
		// Wide shot: the whole row in view.
		const float RowWidth = FMath::Max(600.f, (StageClipIds.Num() - 1) * StepX);
		const float Dist = RowWidth * 0.5f / FMath::Tan(FMath::DegreesToRadians(30.f)) + 700.f;
		if (StageCam)
		{
			StageCam->SetActorLocation(FVector(0.f, -Dist, 350.f));
			StageCam->SetActorRotation((FVector(0.f, 0.f, 250.f) - StageCam->GetActorLocation()).Rotation());
			StageCam->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Perspective;
			StageCam->GetCameraComponent()->FieldOfView = 60.f;
		}
		// TEMP PROBE (step 0 so it is in frame for the screenshots): upright green
		// stage-material quad plus a sprite-material cube at the first clip.
		if (World && StageClipIds.Num() > 0)
		{
			// TEMP PROBE: dump the decoded atlas bytes to Saved to verify the runtime decode.
			{
				const FString PngPath = FPaths::ProjectContentDir() / TEXT("Data/Vfx/FluidFire.png");
				TArray<uint8> PngBytes;
				uint64 ZeroBytes = 0;
				uint64 NonZeroAlpha = 0;
				uint8 MinByte = 255;
				uint8 MaxByte = 0;
				if (FFileHelper::LoadFileToArray(PngBytes, *PngPath))
				{
					FImage ProbeImage;
					if (FImageUtils::DecompressImage(PngBytes.GetData(), PngBytes.Num(), ProbeImage))
					{
						for (uint8 Byte : ProbeImage.RawData)
						{
							MinByte = FMath::Min(MinByte, Byte);
							MaxByte = FMath::Max(MaxByte, Byte);
							if (Byte == 0)
							{
								++ZeroBytes;
							}
						}
						const int64 PxCount = ProbeImage.RawData.Num() / 4;
						for (int64 p = 0; p < PxCount; ++p)
						{
							if (ProbeImage.RawData[p * 4 + 3] != 0)
							{
								++NonZeroAlpha;
							}
						}
						TArray<FColor> Dump;
						Dump.SetNum(PxCount);
						FMemory::Memcpy(Dump.GetData(), ProbeImage.RawData.GetData(), ProbeImage.RawData.Num());
						TArray64<uint8> Compressed;
						FImageUtils::PNGCompressImageArray(ProbeImage.SizeX, ProbeImage.SizeY,
							TArrayView64<const FColor>(Dump.GetData(), Dump.Num()), Compressed);
						TArray<uint8> Bytes;
						Bytes.Append(Compressed.GetData(), Compressed.Num());
						FFileHelper::SaveArrayToFile(Bytes, *(FPaths::ProjectSavedDir() / TEXT("PixelVfx/atlas_dump.png")));
						UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag atlasdump bytes=%d min=%d max=%d zero=%llu opaque=%llu fmt=%d %dx%d"),
							PngBytes.Num(), MinByte, MaxByte, ZeroBytes, NonZeroAlpha,
							(int32)ProbeImage.Format, ProbeImage.SizeX, ProbeImage.SizeY);
					}
					else
					{
						UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX showcase diag atlasdump decode failed"));
					}
				}
			}
			UStaticMesh* PlaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
				TEXT("/Engine/BasicShapes/Plane.Plane")));
			UStaticMesh* CubeMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
				TEXT("/Engine/BasicShapes/Cube.Cube")));
			auto LogMeshStats = [](const TCHAR* Label, const UStaticMesh* Mesh)
			{
				if (!Mesh || !Mesh->GetRenderData() || Mesh->GetRenderData()->LODResources.Num() == 0)
				{
					UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag mesh %s: no render data"), Label);
					return;
				}
				const FStaticMeshLODResources& LOD = Mesh->GetRenderData()->LODResources[0];
				UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag mesh %s: verts=%d tris=%d uvs=%d sections=%d"),
					Label, LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumVertices(),
					LOD.GetNumTriangles(), LOD.VertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords(),
					LOD.Sections.Num());
			};
			LogMeshStats(TEXT("Plane"), PlaneMesh);
			LogMeshStats(TEXT("Cube"), CubeMesh);
			UMaterialInterface* StageMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
				nullptr, TEXT("/Game/Glyphbound/Materials/M_PixelVfxStage.M_PixelVfxStage")));
			UMaterialInterface* SpriteMat = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
				nullptr, TEXT("/Game/Glyphbound/Materials/M_PixelVfxSprite.M_PixelVfxSprite")));
			UGBPixelVfxSubsystem* ProbeSubsystem = GetGameInstance()
				? GetGameInstance()->GetSubsystem<UGBPixelVfxSubsystem>()
				: nullptr;
			UTexture2D* Atlas = ProbeSubsystem ? ProbeSubsystem->GetAtlas(TEXT("FluidFire")) : nullptr;
			UTexture* GridTex = Cast<UTexture>(StaticLoadObject(UTexture::StaticClass(), nullptr,
				TEXT("/Engine/EngineMaterials/DefaultDiffuse.DefaultDiffuse")));
			UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag probe grid=%d atlas=%d"),
				GridTex ? 1 : 0, Atlas ? 1 : 0);
			const float ProbeStepX = 600.f;
			const float ProbeX0 = -(StageClipIds.Num() - 1) * ProbeStepX * 0.5f;
			FActorSpawnParameters ProbeParams;
			ProbeParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			auto SpawnProbeMesh = [&](const FVector& At, const FRotator& Rot, UStaticMesh* MeshAsset,
				const FVector& Scale, UMaterialInterface* Base, bool bSprite, UTexture* TexOverride)
			{
				AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Rot, At), ProbeParams);
				if (!Actor || !MeshAsset || !Base)
				{
					return;
				}
				UTexture* AtlasTex = TexOverride ? TexOverride : Atlas;
				UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
				Mesh->SetMobility(EComponentMobility::Movable);
				Actor->SetRootComponent(Mesh);
				Mesh->SetStaticMesh(MeshAsset);
				Mesh->SetRelativeScale3D(Scale);
				Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mesh->RegisterComponent();
				UMaterialInstanceDynamic* Dyn = UMaterialInstanceDynamic::Create(Base, Actor);
				if (bSprite && AtlasTex)
				{
					Dyn->SetTextureParameterValue(TEXT("Atlas"), AtlasTex);
					Dyn->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
					Dyn->SetScalarParameterValue(TEXT("Glow"), 1.5f);
					Dyn->SetScalarParameterValue(TEXT("FrameU"), 0.f);
					Dyn->SetScalarParameterValue(TEXT("FrameV"), 0.2f);
					Dyn->SetScalarParameterValue(TEXT("FrameW"), 0.2f);
					Dyn->SetScalarParameterValue(TEXT("FrameH"), -0.2f);
				}
				else
				{
					Dyn->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.f, 1.f, 0.f));
					Dyn->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
					Dyn->SetScalarParameterValue(TEXT("Metallic"), 0.f);
				}
				Mesh->SetMaterial(0, Dyn);
			};
			SpawnProbeMesh(FVector(ProbeX0 - 250.f, 0.f, 150.f), FRotator(0.f, 0.f, 90.f),
				PlaneMesh, FVector(2.f, 3.f, 1.f), StageMat, false, nullptr);
			SpawnProbeMesh(FVector(ProbeX0 + 250.f, 0.f, 150.f), FRotator::ZeroRotator,
				CubeMesh, FVector(1.5f, 1.5f, 1.5f), SpriteMat, true, nullptr);
			if (CubeMesh && SpriteMat && GridTex)
			{
				SpawnProbeMesh(FVector(ProbeX0 + 500.f, 0.f, 150.f), FRotator::ZeroRotator,
					CubeMesh, FVector(1.5f, 1.5f, 1.5f), SpriteMat, true, GridTex);
			}
			// TEMP PROBE: bare grey cube (default material) at the second clip.
			if (CubeMesh)
			{
				FActorSpawnParameters GreyParams;
				GreyParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(),
					FTransform(FVector(ProbeX0 + 600.f, 0.f, 150.f)), GreyParams))
				{
					UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Actor, NAME_None, RF_Transient);
					Mesh->SetMobility(EComponentMobility::Movable);
					Actor->SetRootComponent(Mesh);
					Mesh->SetStaticMesh(CubeMesh);
					Mesh->SetRelativeScale3D(FVector(1.5f, 1.5f, 1.5f));
					Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Mesh->RegisterComponent();
				}
			}
		}
		StageTimeMark = FPlatformTime::Seconds();
		StageStep = 1;
		break;
	}
	case 1:
	{
		// One-shots are re-spawned when they finish.
		for (int32 i = 0; i < StageClips.Num(); ++i)
		{
			AGBPixelVfxActor* Actor = StageClips[i].Get();
			const float StepX = 600.f;
			const float X0 = -(StageClipIds.Num() - 1) * StepX * 0.5f;
			if (!Actor || !Actor->PixelVfx || !Actor->PixelVfx->IsPlaying())
			{
				if (Actor)
				{
					Actor->Destroy();
				}
				AGBPixelVfxActor* Spawned = AGBPixelVfxActor::SpawnPixelVfx(
					this, StageClipIds[i], FTransform(FVector(X0 + i * StepX, 0.f, 0.f)), 1.f);
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
			// Ground-truth verdict via readback (screenshots have proven unreliable here):
			// capture from the stage camera and log the brightness distribution.
			if (StageCam)
			{
				FActorSpawnParameters VerdictParams;
				VerdictParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				if (AActor* VerdictActor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, VerdictParams))
				{
					USceneCaptureComponent2D* Verdict = NewObject<USceneCaptureComponent2D>(VerdictActor, NAME_None, RF_Transient);
					Verdict->SetMobility(EComponentMobility::Movable);
					VerdictActor->SetRootComponent(Verdict);
					Verdict->RegisterComponent();
					Verdict->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_RenderScenePrimitives;
					Verdict->CaptureSource = ESceneCaptureSource::SCS_SceneColorHDR;
					Verdict->bCaptureEveryFrame = false;
					Verdict->bCaptureOnMovement = false;
					Verdict->FOVAngle = 60.f;
					Verdict->ShowFlags.SetBloom(false);
					Verdict->ShowFlags.SetEyeAdaptation(false);
					Verdict->ShowFlags.SetMotionBlur(false);
					Verdict->ShowFlags.SetTonemapper(false);
					Verdict->ShowFlags.SetTemporalAA(false);
					VerdictActor->SetActorLocationAndRotation(StageCam->GetActorLocation(), StageCam->GetActorRotation());
					UTextureRenderTarget2D* VerdictRT = NewObject<UTextureRenderTarget2D>(GetTransientPackage(), NAME_None, RF_Transient);
					VerdictRT->InitCustomFormat(640, 360, PF_FloatRGBA, true);
					VerdictRT->UpdateResourceImmediate(true);
					Verdict->TextureTarget = VerdictRT;
					Verdict->CaptureScene();
					if (FTextureRenderTargetResource* Res = VerdictRT->GameThread_GetRenderTargetResource())
					{
						TArray<FLinearColor> Raw;
						if (Res->ReadLinearColorPixels(Raw) && Raw.Num() == 640 * 360)
						{
							float MaxLum = 0.f;
							double SumLum = 0.0;
							int32 Bright = 0;
							for (const FLinearColor& Px : Raw)
							{
								const float Lum = Px.R * 0.2126f + Px.G * 0.7152f + Px.B * 0.0722f;
								MaxLum = FMath::Max(MaxLum, Lum);
								SumLum += Lum;
								if (Lum > 1.f)
								{
									++Bright;
								}
							}
							UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase verdict: maxlum=%.2f meanlum=%.4f brightpx=%d of %d"),
								MaxLum, SumLum / Raw.Num(), Bright, Raw.Num());
						}
						else
						{
							UE_LOG(LogGBPixelVfx, Warning, TEXT("PVFX showcase verdict: readback failed"));
						}
					}
					VerdictActor->Destroy();
				}
			}
			for (int32 i = 0; i < StageClips.Num(); ++i)
			{
				if (AGBPixelVfxActor* Actor = StageClips[i].Get())
				{
					FVector Origin = Actor->GetActorLocation();
					FVector Extent(0.f);
					if (Actor->PixelVfx)
					{
						FVector QOrigin;
						FVector QExtent;
						Actor->PixelVfx->GetQuadWorldBounds(QOrigin, QExtent);
						UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag clip %s at %s quad %s extent %s playing=%d"),
							*StageClipIds[i].ToString(), *Origin.ToCompactString(),
							*QOrigin.ToCompactString(), *QExtent.ToCompactString(),
							Actor->PixelVfx->IsPlaying() ? 1 : 0);
					}
				}
				else
				{
					UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag clip %s: actor gone"),
						*StageClipIds[i].ToString());
				}
			}
			if (StageCam)
			{
				UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX showcase diag cam at %s rot %s fov %.0f"),
					*StageCam->GetActorLocation().ToCompactString(),
					*StageCam->GetActorRotation().ToCompactString(),
					StageCam->GetCameraComponent()->FieldOfView);
			}
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
				const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
				UE_LOG(LogGBPixelVfx, Display, TEXT("PVFX SHOWCASE: %d clips, screenshots in %s."),
					StageClipIds.Num(), *Dir);
				FinishRun();
				return;
			}
			if (AGBPixelVfxActor* Actor = StageClips[StageShotIndex].Get())
			{
				const FVector At = Actor->GetActorLocation();
				if (StageCam)
				{
					StageCam->SetActorLocation(At + FVector(0.f, -500.f, 220.f));
					StageCam->SetActorRotation((At + FVector(0.f, 0.f, 200.f) - StageCam->GetActorLocation()).Rotation());
				}
			}
			StageTimeMark = FPlatformTime::Seconds();
			StageStep = 4;
		}
		break;
	case 4:
		if (FPlatformTime::Seconds() - StageTimeMark >= 0.5)
		{
			const FString Dir = FPaths::ProjectSavedDir() / TEXT("PixelVfx/Showcase");
			TakeStageScreenshot(Dir / (TEXT("showcase_") + StageClipIds[StageShotIndex].ToString() + TEXT(".png")));
			++StageShotIndex;
			StageWait = 15;
			StageStep = 3;
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
			const FVector At(-600.f + ix * 400.f, -200.f + iy * 400.f, 0.f);
			AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(At), Params);
			if (!Actor)
			{
				continue;
			}
			UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor, NAME_None, RF_Transient);
			Actor->SetRootComponent(Comp);
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
			const FVector At(-600.f + ix * 400.f, -200.f + iy * 400.f, 0.f);
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
		// Camera on the whole stage.
		if (StageCam)
		{
			StageCam->SetActorLocation(FVector(0.f, -1900.f, 500.f));
			StageCam->SetActorRotation((FVector(0.f, 200.f, 250.f) - StageCam->GetActorLocation()).Rotation());
			StageCam->GetCameraComponent()->ProjectionMode = ECameraProjectionMode::Perspective;
			StageCam->GetCameraComponent()->FieldOfView = 60.f;
		}
		// Phase 0: the source system on the left, the baked clip on the right.
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *CmpJob.SystemPath);
		if (!System)
		{
			UE_LOG(LogGBPixelVfx, Error, TEXT("PVFX compare: cannot load system '%s'."), *CmpJob.SystemPath);
			FinishRun();
			return;
		}
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (AActor* Actor = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform(FVector(-250.f, 0.f, 0.f)), Params))
		{
			UNiagaraComponent* Comp = NewObject<UNiagaraComponent>(Actor, NAME_None, RF_Transient);
			Actor->SetRootComponent(Comp);
			Comp->RegisterComponent();
			Comp->SetAsset(System);
			Comp->SetRandomSeedOffset(0);
			Comp->SetAllowScalability(false);
			Comp->Activate(true);
			CmpActors.Add(Actor);
		}
		if (AGBPixelVfxActor* Baked = AGBPixelVfxActor::SpawnPixelVfx(this, CmpJob.Id, FTransform(FVector(250.f, 0.f, 0.f)), 1.f))
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
		const uint32 Gpu = RHIGetGPUFrameCycles();
		CmpCpuSum += Dt;
		CmpGpuSum += FPlatformTime::ToMilliseconds(Gpu);
		++CmpCpuCount;
		CmpPrevGpu = Gpu;
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
		// Baked population settles for 1 s (one-shots restart when they finish).
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
		const uint32 Gpu = RHIGetGPUFrameCycles();
		CmpCpuSum += Dt;
		CmpGpuSum += FPlatformTime::ToMilliseconds(Gpu);
		++CmpCpuCount;
		CmpPrevGpu = Gpu;
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
		const uint32 Gpu = RHIGetGPUFrameCycles();
		CmpCpuSum += Dt;
		CmpGpuSum += FPlatformTime::ToMilliseconds(Gpu);
		++CmpCpuCount;
		CmpPrevGpu = Gpu;
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
