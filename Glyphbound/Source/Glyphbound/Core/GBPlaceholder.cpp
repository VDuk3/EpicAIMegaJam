#include "Core/GBPlaceholder.h"

#include "Components/StaticMeshComponent.h"
#include "Core/GBTypes.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

namespace GBPlaceholderPrivate
{
	const FName ColorParam(TEXT("Color"));

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

void GBPlaceholder::SetShapeColor(UStaticMeshComponent* Component, const FLinearColor& Color)
{
	if (!Component)
	{
		return;
	}
	if (UMaterialInstanceDynamic* Material = Cast<UMaterialInstanceDynamic>(Component->GetMaterial(0)))
	{
		Material->SetVectorParameterValue(GBPlaceholderPrivate::ColorParam, Color);
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
