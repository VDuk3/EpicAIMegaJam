#include "UI/GBPixelSpriteLibrary.h"

#include "Data/GBDataSubsystem.h"
#include "Components/BillboardComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "TextureResource.h"

namespace GBSpriteLocal
{
	const FColor MissingColor(255, 0, 255, 255);
	const FColor TransparentColor(0, 0, 0, 0);

	int32 HexValue(TCHAR C)
	{
		if (C >= TEXT('0') && C <= TEXT('9')) return C - TEXT('0');
		if (C >= TEXT('a') && C <= TEXT('f')) return 10 + C - TEXT('a');
		if (C >= TEXT('A') && C <= TEXT('F')) return 10 + C - TEXT('A');
		return -1;
	}

	/** "#RRGGBB" or "#RRGGBBAA" (the '#' is optional) -> sRGB color. Parsed here, byte-exact, so the art keeps its exact colors. */
	bool ParseHexColor(const FString& InHex, FColor& OutColor)
	{
		FString Hex = InHex.TrimStartAndEnd();
		if (Hex.StartsWith(TEXT("#")))
		{
			Hex.RightChopInline(1);
		}
		if (Hex.Len() != 6 && Hex.Len() != 8)
		{
			return false;
		}
		uint8 Bytes[4] = { 0, 0, 0, 255 };
		for (int32 i = 0; i < Hex.Len() / 2; ++i)
		{
			const int32 Hi = HexValue(Hex[i * 2]);
			const int32 Lo = HexValue(Hex[i * 2 + 1]);
			if (Hi < 0 || Lo < 0)
			{
				return false;
			}
			Bytes[i] = (uint8)(Hi * 16 + Lo);
		}
		OutColor = FColor(Bytes[0], Bytes[1], Bytes[2], Bytes[3]);
		return true;
	}
}

UGBPixelSpriteLibrary* UGBPixelSpriteLibrary::Get(const UObject* WorldContextObject)
{
	const UWorld* World = (GEngine && WorldContextObject) ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGBPixelSpriteLibrary>() : nullptr;
}

void UGBPixelSpriteLibrary::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UGBDataSubsystem* Data = Collection.InitializeDependency<UGBDataSubsystem>())
	{
		Data->OnDataReloaded.AddUObject(this, &UGBPixelSpriteLibrary::ClearCache);
	}
}

void UGBPixelSpriteLibrary::ClearCache()
{
	Cache.Reset();
	TileCache.Reset();
}

bool UGBPixelSpriteLibrary::BuildPixels(const FGBSpriteDefinition& Definition, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
{
	OutPixels.Reset();
	OutWidth = 0;
	OutHeight = 0;

	if (Definition.Rows.Num() == 0 || Definition.Rows[0].Len() == 0)
	{
		return false;
	}
	const int32 Width = Definition.Rows[0].Len();
	for (const FString& Row : Definition.Rows)
	{
		if (Row.Len() != Width)
		{
			return false;
		}
	}

	TMap<TCHAR, FColor> Colors;
	for (const TPair<FString, FString>& Entry : Definition.Palette)
	{
		if (Entry.Key.Len() == 0)
		{
			continue;
		}
		FColor Color;
		Colors.Add(Entry.Key[0], GBSpriteLocal::ParseHexColor(Entry.Value, Color) ? Color : GBSpriteLocal::MissingColor);
	}

	const int32 Height = Definition.Rows.Num();
	OutPixels.Reserve(Width * Height);
	for (const FString& Row : Definition.Rows)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const TCHAR C = Row[X];
			if (C == TEXT('.') || C == TEXT(' '))
			{
				OutPixels.Add(GBSpriteLocal::TransparentColor);
			}
			else if (const FColor* Found = Colors.Find(C))
			{
				OutPixels.Add(*Found);
			}
			else
			{
				OutPixels.Add(GBSpriteLocal::MissingColor);
			}
		}
	}
	OutWidth = Width;
	OutHeight = Height;
	return true;
}

UTexture2D* UGBPixelSpriteLibrary::CreateTexture(const TArray<FColor>& Pixels, int32 Width, int32 Height, FName DebugName, bool bWrap)
{
	return CreatePixelTexture(Pixels, Width, Height, DebugName, bWrap);
}

UTexture2D* UGBPixelSpriteLibrary::CreatePixelTexture(const TArray<FColor>& Pixels, int32 Width, int32 Height, FName DebugName, bool bWrap)
{
	if (Width <= 0 || Height <= 0 || Pixels.Num() != Width * Height)
	{
		return nullptr;
	}

	UTexture2D* Texture = UTexture2D::CreateTransient(Width, Height, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		UE_LOG(LogGB, Warning, TEXT("Pixel sprite '%s': could not create a %dx%d texture."), *DebugName.ToString(), Width, Height);
		return nullptr;
	}

	Texture->Filter = TF_Nearest;
	Texture->SRGB = true;
	Texture->NeverStream = true;
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	// Sprites clamp (no bleeding from the opposite edge); tiles repeat across a structure's faces.
	Texture->AddressX = bWrap ? TA_Wrap : TA_Clamp;
	Texture->AddressY = bWrap ? TA_Wrap : TA_Clamp;
#if WITH_EDITORONLY_DATA
	Texture->MipGenSettings = TMGS_NoMipmaps;
#endif

	// FColor is laid out B, G, R, A in memory, which is exactly PF_B8G8R8A8; rows go top to bottom.
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Pixels.GetData(), Pixels.Num() * sizeof(FColor));
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	return Texture;
}

UTexture2D* UGBPixelSpriteLibrary::GetSprite(FName SpriteId)
{
	return GetOrBuild(SpriteId, /*bWrap*/ false);
}

UTexture2D* UGBPixelSpriteLibrary::GetTileTexture(FName SpriteId)
{
	return GetOrBuild(SpriteId, /*bWrap*/ true);
}

UTexture2D* UGBPixelSpriteLibrary::GetOrBuild(FName SpriteId, bool bWrap)
{
	TMap<FName, TObjectPtr<UTexture2D>>& TargetCache = bWrap ? TileCache : Cache;
	if (TObjectPtr<UTexture2D>* Cached = TargetCache.Find(SpriteId))
	{
		if (*Cached)
		{
			return *Cached;
		}
	}

	TObjectPtr<UTexture2D>& Missing = bWrap ? MissingTileTexture : MissingTexture;
	if (!Missing)
	{
		constexpr int32 Size = 8;
		TArray<FColor> Checker;
		Checker.Reserve(Size * Size);
		for (int32 Y = 0; Y < Size; ++Y)
		{
			for (int32 X = 0; X < Size; ++X)
			{
				Checker.Add(((X / 2 + Y / 2) % 2) ? FColor::Black : GBSpriteLocal::MissingColor);
			}
		}
		Missing = CreateTexture(Checker, Size, Size, TEXT("Missing"), bWrap);
	}

	UTexture2D* Result = nullptr;
	const UGameInstance* GameInstance = GetGameInstance();
	const UGBDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UGBDataSubsystem>() : nullptr;
	const FGBSpriteDefinition* Definition = Data ? Data->FindSprite(SpriteId) : nullptr;
	if (!Definition)
	{
		UE_LOG(LogGB, Warning, TEXT("Pixel sprite '%s' is not in Sprites.json; using the magenta checker."), *SpriteId.ToString());
	}
	else
	{
		TArray<FColor> Pixels;
		int32 Width = 0;
		int32 Height = 0;
		if (BuildPixels(*Definition, Pixels, Width, Height))
		{
			Result = CreateTexture(Pixels, Width, Height, SpriteId, bWrap);
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Pixel sprite '%s' has no rows or rows of different lengths; using the magenta checker."), *SpriteId.ToString());
		}
	}

	if (!Result)
	{
		Result = Missing;
	}
	// Missing sprites are cached too, so the warning is logged once per id and use (until the next data reload).
	TargetCache.Add(SpriteId, Result);
	return Result;
}

FIntPoint UGBPixelSpriteLibrary::GetSpriteSize(FName SpriteId) const
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UGBDataSubsystem* Data = GameInstance ? GameInstance->GetSubsystem<UGBDataSubsystem>() : nullptr;
	const FGBSpriteDefinition* Definition = Data ? Data->FindSprite(SpriteId) : nullptr;
	if (!Definition || Definition->Rows.Num() == 0)
	{
		return FIntPoint::ZeroValue;
	}
	return FIntPoint(Definition->Rows[0].Len(), Definition->Rows.Num());
}

void UGBPixelSpriteLibrary::SetupBillboard(UBillboardComponent* Billboard, FName SpriteId, float WorldHeight)
{
	if (!Billboard)
	{
		return;
	}
	UTexture2D* Texture = GetSprite(SpriteId);
	if (!Texture)
	{
		return;
	}

	// FSpriteSceneProxy: half extents = ComponentMaxAxisScale * SpriteScale * 0.25 * TextureSize (FBatchedElements
	// builds the quad from Position +- SizeX / SizeY), so the full height is 0.5 * Scale * TextureHeight.
	// In editor builds the proxy also multiplies by the owner's SpriteScale and UBillboardComponent::EditorScale.
	const float TextureHeight = (float)FMath::Max(1, Texture->GetSizeY());
	float Scale = WorldHeight > 0.f ? 2.f * WorldHeight / TextureHeight : 1.f;
#if WITH_EDITOR
	if (const AActor* Owner = Billboard->GetOwner())
	{
		if (Owner->SpriteScale > KINDA_SMALL_NUMBER)
		{
			Scale /= Owner->SpriteScale;
		}
	}
#endif
#if WITH_EDITORONLY_DATA
	if (UBillboardComponent::EditorScale > KINDA_SMALL_NUMBER)
	{
		Scale /= UBillboardComponent::EditorScale;
	}
#endif

	// Billboards use absolute scale by default, so the relative scale is the world scale.
	Billboard->SetRelativeScale3D(FVector(Scale));
	Billboard->bIsScreenSizeScaled = false;
	Billboard->SetSprite(Texture);
	Billboard->SetHiddenInGame(false);
	Billboard->SetVisibility(true);
	// The proxy reads the scale only when it is created: rebuild it.
	Billboard->MarkRenderStateDirty();
}
