#include "Core/GBPlaceholder.h"

#include "Components/StaticMeshComponent.h"
#include "Core/GBTypes.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UI/GBPixelSpriteLibrary.h"

namespace GBPlaceholderPrivate
{
	const FName ColorParam(TEXT("Color"));

	// Parameters of M_PixelSurface (Tools/Jam/make_materials.py builds it; keep the names in sync).
	const FName TextureParam(TEXT("Texture"));
	const FName TintParam(TEXT("Tint"));
	const FName TileSizeParam(TEXT("TileSize"));
	const FName GlowParam(TEXT("Glow"));
	const TCHAR* const PixelSurfacePath = TEXT("/Game/Glyphbound/Materials/M_PixelSurface.M_PixelSurface");
	const TCHAR* const PixelDecalPath = TEXT("/Game/Glyphbound/Materials/M_PixelDecal.M_PixelDecal");

	/** Smallest tile accepted from data, so a 0 in the JSON cannot divide by zero in the shader. */
	constexpr float MinTileSizeCm = 1.f;

	/** M_PixelSurface, or null when the asset was not generated (then the caller falls back to SetupShape). */
	UMaterialInterface* GetPixelSurfaceMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		static bool bKnownMissing = false;
		if (!Cached.IsValid() && !bKnownMissing)
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, PixelSurfacePath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
			if (!Cached.IsValid())
			{
				// Logged once: run Tools/Jam/Make-Materials.ps1 to create it.
				bKnownMissing = true;
				UE_LOG(LogGB, Warning, TEXT("Placeholder: material %s is missing (run Tools/Jam/Make-Materials.ps1); textured shapes use plain tinted shapes."), PixelSurfacePath);
			}
		}
		return Cached.Get();
	}

	/** M_PixelDecal, or null when the asset was not generated (then the caller falls back to SetupShape). */
	UMaterialInterface* GetPixelDecalMaterial()
	{
		static TWeakObjectPtr<UMaterialInterface> Cached;
		static bool bKnownMissing = false;
		if (!Cached.IsValid() && !bKnownMissing)
		{
			Cached = LoadObject<UMaterialInterface>(nullptr, PixelDecalPath, FStringView(), LOAD_NoWarn | LOAD_Quiet);
			if (!Cached.IsValid())
			{
				// Logged once: run Tools/Jam/Make-Materials.ps1 to create it.
				bKnownMissing = true;
				UE_LOG(LogGB, Warning, TEXT("Placeholder: material %s is missing (run Tools/Jam/Make-Materials.ps1); ground spells use plain tinted shapes."), PixelDecalPath);
			}
		}
		return Cached.Get();
	}

	// The engine shapes are 100 cm across.
	constexpr float ShapeSizeCm = 100.f;

	UMaterialInterface* GetMaterial(bool bGlow)
	{
		static TWeakObjectPtr<UMaterialInterface> LitMaterial;
		static TWeakObjectPtr<UMaterialInterface> GlowMaterial;

		TWeakObjectPtr<UMaterialInterface>& Cached = bGlow ? GlowMaterial : LitMaterial;
		if (!Cached.IsValid())
		{
			const TCHAR* Path = bGlow
				? TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")
				: TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
			Cached = LoadObject<UMaterialInterface>(nullptr, Path);
			if (!Cached.IsValid())
			{
				UE_LOG(LogGB, Warning, TEXT("Placeholder: cannot load material %s"), Path);
			}
		}
		return Cached.Get();
	}
}

UStaticMesh* GBPlaceholder::GetShapeMesh(EGBShape Shape)
{
	static TWeakObjectPtr<UStaticMesh> Cache[5];

	const int32 Index = FMath::Clamp(static_cast<int32>(Shape), 0, 4);
	if (!Cache[Index].IsValid())
	{
		static const TCHAR* const Paths[5] =
		{
			TEXT("/Engine/BasicShapes/Cube.Cube"),
			TEXT("/Engine/BasicShapes/Sphere.Sphere"),
			TEXT("/Engine/BasicShapes/Cylinder.Cylinder"),
			TEXT("/Engine/BasicShapes/Cone.Cone"),
			TEXT("/Engine/BasicShapes/Plane.Plane"),
		};
		Cache[Index] = LoadObject<UStaticMesh>(nullptr, Paths[Index]);
		if (!Cache[Index].IsValid())
		{
			UE_LOG(LogGB, Warning, TEXT("Placeholder: cannot load mesh %s"), Paths[Index]);
		}
	}
	return Cache[Index].Get();
}

UMaterialInstanceDynamic* GBPlaceholder::SetupShape(UStaticMeshComponent* Component, EGBShape Shape, const FVector& SizeCm, const FLinearColor& Color, bool bGlow)
{
	if (!Component)
	{
		return nullptr;
	}

	Component->SetStaticMesh(GetShapeMesh(Shape));
	Component->SetRelativeScale3D(SizeCm / GBPlaceholderPrivate::ShapeSizeCm);

	UMaterialInterface* BaseMaterial = GBPlaceholderPrivate::GetMaterial(bGlow);
	if (!BaseMaterial)
	{
		return nullptr;
	}

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	Material->SetVectorParameterValue(GBPlaceholderPrivate::ColorParam, Color);
	Component->SetMaterial(0, Material);
	return Material;
}

UMaterialInstanceDynamic* GBPlaceholder::SetupTexturedShape(UStaticMeshComponent* Component, EGBShape Shape, const FVector& SizeCm, FName TextureId,
	float TileSizeCm, const FLinearColor& Tint, bool bGlow)
{
	if (!Component)
	{
		return nullptr;
	}

	UMaterialInterface* BaseMaterial = GBPlaceholderPrivate::GetPixelSurfaceMaterial();
	if (!BaseMaterial)
	{
		return SetupShape(Component, Shape, SizeCm, Tint);
	}

	Component->SetStaticMesh(GetShapeMesh(Shape));
	Component->SetRelativeScale3D(SizeCm / GBPlaceholderPrivate::ShapeSizeCm);

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	if (UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(Component))
	{
		Material->SetTextureParameterValue(GBPlaceholderPrivate::TextureParam, Library->GetTileTexture(TextureId));
	}
	else
	{
		// Not in a game world yet (the library lives on the game instance): the material's default texture shows.
		UE_LOG(LogGB, Warning, TEXT("Placeholder: no pixel sprite library for '%s' on %s; using the default texture."),
			*TextureId.ToString(), *GetNameSafe(Component->GetOwner()));
	}
	Material->SetVectorParameterValue(GBPlaceholderPrivate::TintParam, Tint);
	Material->SetScalarParameterValue(GBPlaceholderPrivate::TileSizeParam, FMath::Max(TileSizeCm, GBPlaceholderPrivate::MinTileSizeCm));
	Material->SetScalarParameterValue(GBPlaceholderPrivate::GlowParam, bGlow ? 1.f : 0.f);
	Component->SetMaterial(0, Material);
	return Material;
}

void GBPlaceholder::SetShapeColor(UStaticMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
	{
		// "Color" on SetupShape materials, "Tint" on SetupTexturedShape ones; the other name is ignored.
		Material->SetVectorParameterValue(GBPlaceholderPrivate::ColorParam, Color);
		Material->SetVectorParameterValue(GBPlaceholderPrivate::TintParam, Color);
	}
}

UMaterialInstanceDynamic* GBPlaceholder::SetupFlatSprite(UStaticMeshComponent* Component, FName SpriteId, float SizeCm, const FLinearColor& Tint, float Glow)
{
	if (!Component)
	{
		return nullptr;
	}

	const float SafeSize = FMath::Max(1.f, SizeCm);
	UMaterialInterface* BaseMaterial = GBPlaceholderPrivate::GetPixelDecalMaterial();
	if (!BaseMaterial)
	{
		return SetupShape(Component, EGBShape::Plane, FVector(SafeSize, SafeSize, 100.f), Tint, true);
	}

	Component->SetStaticMesh(GetShapeMesh(EGBShape::Plane));
	const float Scale = SafeSize / GBPlaceholderPrivate::ShapeSizeCm;
	Component->SetRelativeScale3D(FVector(Scale, Scale, 1.f));

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(BaseMaterial, Component);
	if (UGBPixelSpriteLibrary* Library = UGBPixelSpriteLibrary::Get(Component))
	{
		Material->SetTextureParameterValue(GBPlaceholderPrivate::TextureParam, Library->GetSprite(SpriteId));
	}
	else
	{
		// Not in a game world yet (the library lives on the game instance): the material's default texture shows.
		UE_LOG(LogGB, Warning, TEXT("Placeholder: no pixel sprite library for '%s' on %s; using the default texture."),
			*SpriteId.ToString(), *GetNameSafe(Component->GetOwner()));
	}
	Material->SetVectorParameterValue(GBPlaceholderPrivate::TintParam, Tint);
	Material->SetScalarParameterValue(GBPlaceholderPrivate::GlowParam, Glow);
	Component->SetMaterial(0, Material);
	return Material;
}

void GBPlaceholder::SetFlatSpriteLook(UStaticMeshComponent* Component, const FLinearColor& Tint, float Glow)
{
	if (!Component)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
	{
		// "Tint"/"Glow" on decal materials; "Color" on the SetupShape fallback (the other names are ignored).
		Material->SetVectorParameterValue(GBPlaceholderPrivate::TintParam, Tint);
		Material->SetScalarParameterValue(GBPlaceholderPrivate::GlowParam, Glow);
		Material->SetVectorParameterValue(GBPlaceholderPrivate::ColorParam, Tint);
	}
}

FLinearColor GBPlaceholder::ElementColor(uint8 ElementAsByte)
{
	switch (static_cast<EGBElement>(ElementAsByte))
	{
	case EGBElement::Physical:  return FLinearColor(0.55f, 0.55f, 0.55f);
	case EGBElement::Arcane:    return FLinearColor(0.55f, 0.20f, 1.00f);
	case EGBElement::Fire:      return FLinearColor(1.00f, 0.40f, 0.05f);
	case EGBElement::Lightning: return FLinearColor(1.00f, 0.90f, 0.15f);
	case EGBElement::Ice:       return FLinearColor(0.20f, 0.90f, 1.00f);
	case EGBElement::Air:       return FLinearColor(0.65f, 1.00f, 0.70f);
	default:                    return FLinearColor::White;
	}
}
