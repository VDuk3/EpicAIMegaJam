// UGBPixelVfxComponent: quad + baked light track playback.
#include "GBPixelVfxComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "GBPixelVfxSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace GBPixelVfxComponentLocal
{
	// Quad mesh: the engine plane (100 x 100 cm in XY, normal +Z, two triangles). The sprite material is
	// two-sided, so the back of the quad shows the sprite mirrored.
	const TCHAR* MeshPath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* SpriteMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxSprite.M_PixelVfxSprite");
	const TCHAR* LitMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxSpriteLit.M_PixelVfxSpriteLit");
	const TCHAR* FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
}

UGBPixelVfxComponent::UGBPixelVfxComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

bool UGBPixelVfxComponent::Play(FName ClipId, bool bRestart)
{
	if (!bRestart && bPlaying && CurrentClipId == ClipId && CurrentClip.IsValid())
	{
		return true;
	}
	UGameInstance* GameInstance = nullptr;
	if (UWorld* World = GetWorld())
	{
		GameInstance = World->GetGameInstance();
	}
	UGBPixelVfxSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UGBPixelVfxSubsystem>() : nullptr;
	if (!Subsystem)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx Play '%s': no subsystem."), *ClipId.ToString());
		return false;
	}
	const FGBPixelVfxClip* Found = Subsystem->FindClip(ClipId);
	if (!Found)
	{
		return false;
	}
	UTexture2D* Atlas = Subsystem->GetAtlas(ClipId);
	if (!Atlas)
	{
		return false;
	}
	CurrentClip = MakeShared<FGBPixelVfxClip>(*Found);
	CurrentClipId = ClipId;
	if (!bLightCastsShadowsSet)
	{
		bLightCastsShadows = CurrentClip->Light.bCastShadows;
	}
	PlayTime = StartTimeOffset;
	CurrentFrame = -1;
	bPlaying = true;
	EnsureBuilt();
	UpdateFrame(true);
	return true;
}

void UGBPixelVfxComponent::Stop()
{
	bPlaying = false;
	if (ClipLight)
	{
		ClipLight->DestroyComponent();
		ClipLight = nullptr;
	}
	if (Quad)
	{
		Quad->DestroyComponent();
		Quad = nullptr;
	}
	QuadMaterial = nullptr;
}

void UGBPixelVfxComponent::EnsureBuilt()
{
	if (!CurrentClip.IsValid())
	{
		return;
	}
	if (!Quad)
	{
		UStaticMesh* PlaneMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr,
			GBPixelVfxComponentLocal::MeshPath));
		if (!PlaneMesh)
		{
			UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx: cannot load the quad mesh %s."),
				GBPixelVfxComponentLocal::MeshPath);
			return;
		}
		Quad = NewObject<UStaticMeshComponent>(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this), NAME_None, RF_Transient);
		Quad->SetMobility(EComponentMobility::Movable);
		Quad->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		Quad->SetStaticMesh(PlaneMesh);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCastShadow(false);
		Quad->SetVisibleInRayTracing(true);
		Quad->SetAffectDynamicIndirectLighting(true);
		Quad->bVisibleInReflectionCaptures = true;
		// Final size is known now (clip + SizeScale): set it before registering so the
		// initial render bounds are correct.
		Quad->SetRelativeScale3D(FVector(
			FMath::Max(0.01f, CurrentClip->WorldWidthCm * SizeScale / 100.f),
			FMath::Max(0.01f, CurrentClip->WorldHeightCm * SizeScale / 100.f), 1.f));
		Quad->RegisterComponent();
		Quad->MarkRenderStateDirty();
	}
	RefreshClipMaterial();
	RebuildLight();
}

void UGBPixelVfxComponent::RefreshClipMaterial()
{
	// Clip-dependent material state is refreshed on every successful Play so that
	// switching clips without Stop() picks up the new atlas, variant and glow.
	if (!Quad || !CurrentClip.IsValid())
	{
		return;
	}
	const TCHAR* MaterialPath = CurrentClip->bLit
		? GBPixelVfxComponentLocal::LitMaterialPath
		: GBPixelVfxComponentLocal::SpriteMaterialPath;
	UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
		nullptr, MaterialPath));
	if (!Base && !bQuadMissingMaterialWarned)
	{
		bQuadMissingMaterialWarned = true;
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx: material %s is missing, falling back to %s."),
			MaterialPath, GBPixelVfxComponentLocal::FallbackMaterialPath);
		Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(),
			nullptr, GBPixelVfxComponentLocal::FallbackMaterialPath));
	}
	if (!Base)
	{
		return;
	}
	QuadMaterial = UMaterialInstanceDynamic::Create(Base, this);
	UGBPixelVfxSubsystem* Subsystem = nullptr;
	if (UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			Subsystem = GameInstance->GetSubsystem<UGBPixelVfxSubsystem>();
		}
	}
	UTexture2D* Atlas = Subsystem ? Subsystem->GetAtlas(CurrentClipId) : nullptr;
	if (Atlas)
	{
		QuadMaterial->SetTextureParameterValue(TEXT("Atlas"), Atlas);
	}
	QuadMaterial->SetVectorParameterValue(TEXT("Tint"), FLinearColor::White);
	QuadMaterial->SetScalarParameterValue(TEXT("Glow"), CurrentClip->Glow);
	if (Quad)
	{
		Quad->SetMaterial(0, QuadMaterial);
	}
}

void UGBPixelVfxComponent::RebuildLight()
{
	const bool bWantsLight = CurrentClip.IsValid() && CurrentClip->Light.bEnabled && bEnableLight;
	if (!bWantsLight)
	{
		if (ClipLight)
		{
			ClipLight->DestroyComponent();
			ClipLight = nullptr;
		}
		return;
	}
	if (!ClipLight)
	{
		ClipLight = NewObject<UPointLightComponent>(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this), NAME_None, RF_Transient);
		ClipLight->SetMobility(EComponentMobility::Movable);
		ClipLight->SetIntensityUnits(ELightUnits::Candelas);
		ClipLight->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		ClipLight->RegisterComponent();
	}
	ClipLight->SetLightColor(CurrentClip->Light.Color);
	ClipLight->SetAttenuationRadius(CurrentClip->Light.RadiusCm * FMath::Max(0.01f, SizeScale));
	ClipLight->SetCastShadows(bLightCastsShadows);
}

void UGBPixelVfxComponent::UpdateFrame(bool bForce)
{
	if (!bPlaying || !CurrentClip.IsValid())
	{
		return;
	}
	bool bOutFinished = false;
	const int32 Frame = FGBPixelVfxClip::FrameAtTime(*CurrentClip, PlayTime, bOutFinished);
	if (bOutFinished)
	{
		Stop();
		if (bAutoDestroyOwnerWhenFinished && GetOwner())
		{
			GetOwner()->Destroy();
		}
		return;
	}
	if (Frame == CurrentFrame && !bForce)
	{
		return;
	}
	CurrentFrame = Frame;
	const FVector4f Rect = FGBPixelVfxClip::FrameRect(*CurrentClip, Frame);
	if (QuadMaterial)
	{
		// The frame rect rides as four scalars (see make_pixelvfx_material.py): masks on a
		// uniform that feeds the texture UVs fail to compile, so there is no vector swizzle.
		// The Plane mesh is assumed to run U along +X and V along +Y with V = 0 at the bottom;
		// UE samples (0, 0) at the image top-left, so V is flipped here.
		QuadMaterial->SetScalarParameterValue(TEXT("FrameU"), Rect.X);
		QuadMaterial->SetScalarParameterValue(TEXT("FrameV"), Rect.Y + Rect.W);
		QuadMaterial->SetScalarParameterValue(TEXT("FrameW"), Rect.Z);
		QuadMaterial->SetScalarParameterValue(TEXT("FrameH"), -Rect.W);
	}
	if (ClipLight && CurrentClip->Light.bEnabled)
	{
		float TrackValue = 1.f;
		if (CurrentClip->Light.TrackMode.Equals(TEXT("Constant"), ESearchCase::IgnoreCase)
			|| CurrentClip->Light.Track.Num() != CurrentClip->FrameCount)
		{
			TrackValue = 1.f;
		}
		else if (CurrentClip->Light.Track.IsValidIndex(Frame))
		{
			TrackValue = CurrentClip->Light.Track[Frame];
		}
		ClipLight->SetIntensity(CurrentClip->Light.Intensity * TrackValue * LightIntensityScale);
	}
}

FVector UGBPixelVfxComponent::PivotWorldOffset() const
{
	// Quad-local position of the pivot point; the quad is shifted so it lands on the component.
	if (!CurrentClip.IsValid())
	{
		return FVector::ZeroVector;
	}
	const float W = CurrentClip->WorldWidthCm * SizeScale;
	const float H = CurrentClip->WorldHeightCm * SizeScale;
	return FVector((CurrentClip->PivotU - 0.5f) * W, (0.5f - CurrentClip->PivotV) * H, 0.f);
}

void UGBPixelVfxComponent::UpdateQuadTransform()
{
	if (!Quad || !CurrentClip.IsValid())
	{
		return;
	}
	// Quad size follows the clip and SizeScale (set before RegisterComponent too).
	const float W = CurrentClip->WorldWidthCm * SizeScale;
	const float H = CurrentClip->WorldHeightCm * SizeScale;
	Quad->SetRelativeScale3D(FVector(FMath::Max(0.01f, W / 100.f), FMath::Max(0.01f, H / 100.f), 1.f));

	EGBPixelVfxFacing Facing = CurrentClip->Facing;
	if (!FacingOverride.IsNone())
	{
		const FString Text = FacingOverride.ToString();
		if (Text.Equals(TEXT("Billboard"), ESearchCase::IgnoreCase)) Facing = EGBPixelVfxFacing::Billboard;
		else if (Text.Equals(TEXT("Upright"), ESearchCase::IgnoreCase)) Facing = EGBPixelVfxFacing::Upright;
		else if (Text.Equals(TEXT("Ground"), ESearchCase::IgnoreCase)) Facing = EGBPixelVfxFacing::Ground;
		else if (Text.Equals(TEXT("Fixed"), ESearchCase::IgnoreCase)) Facing = EGBPixelVfxFacing::Fixed;
	}

	const FVector CompLoc = GetComponentLocation();
	FQuat R = GetComponentQuat();
	if (Facing == EGBPixelVfxFacing::Fixed)
	{
		R = GetComponentQuat();
	}
	else if (Facing == EGBPixelVfxFacing::Ground)
	{
		// The quad stays flat (normal = world +Z) while following the component's yaw.
		const FRotator CompRot = GetComponentQuat().Rotator();
		R = FRotator(0.f, CompRot.Yaw, 0.f).Quaternion();
	}
	else
	{
		FVector CamPos = FVector::ZeroVector;
		FVector CamUp = FVector::UpVector;
		bool bHasCamera = false;
		if (UWorld* World = GetWorld())
		{
			if (APlayerCameraManager* Manager = UGameplayStatics::GetPlayerCameraManager(World, 0))
			{
				CamPos = Manager->GetCameraLocation();
				CamUp = Manager->GetCameraRotation().RotateVector(FVector::UpVector);
				bHasCamera = true;
			}
		}
		if (bHasCamera)
		{
			// The direction uses the COMPONENT location (not the quad's): the pivot
			// must land on the component for every facing mode.
			FVector ToCam = CamPos - CompLoc;
			if (Facing == EGBPixelVfxFacing::Upright)
			{
				ToCam.Z = 0.f;
				CamUp = FVector::UpVector;
			}
			if (ToCam.SizeSquared() > 1.f)
			{
				const FVector ZAxis = ToCam.GetSafeNormal();
				FVector XAxis = FVector::CrossProduct(CamUp, ZAxis);
				if (XAxis.SizeSquared() < 1.e-6f)
				{
					XAxis = FVector(1.f, 0.f, 0.f);
				}
				else
				{
					XAxis.Normalize();
				}
				const FVector YAxis = FVector::CrossProduct(ZAxis, XAxis);
				R = FMatrix(XAxis, YAxis, ZAxis, FVector::ZeroVector).Rotator().Quaternion();
			}
			else
			{
				R = Quad->GetComponentQuat();
			}
		}
		else
		{
			// No player camera: keep the last rotation.
			R = Quad->GetComponentQuat();
		}
	}

	FVector QuadLoc = CompLoc;
	FVector LightLoc = CompLoc;
	FGBPixelVfxClip::ComputeQuadAndLightPlacement(*CurrentClip, SizeScale, CompLoc, R, QuadLoc, LightLoc);
	Quad->SetWorldLocationAndRotation(QuadLoc, R);

	if (ClipLight)
	{
		ClipLight->SetWorldLocation(LightLoc);
	}
}

void UGBPixelVfxComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bPlaying || !CurrentClip.IsValid())
	{
		return;
	}
	PlayTime += DeltaTime * PlayRate;
	UpdateFrame(false);
	UpdateQuadTransform();
}

void UGBPixelVfxComponent::GetQuadWorldBounds(FVector& OutOrigin, FVector& OutExtent) const
{
	OutOrigin = FVector::ZeroVector;
	OutExtent = FVector::ZeroVector;
	if (Quad && Quad->GetStaticMesh())
	{
		const FBoxSphereBounds MeshBounds = Quad->GetStaticMesh()->GetBounds();
		OutOrigin = Quad->GetComponentTransform().TransformPosition(MeshBounds.Origin);
		OutExtent = Quad->GetComponentTransform().TransformVector(MeshBounds.BoxExtent);
	}
}

void UGBPixelVfxComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Stop();
	Super::EndPlay(EndPlayReason);
}

void UGBPixelVfxComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	Stop();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
