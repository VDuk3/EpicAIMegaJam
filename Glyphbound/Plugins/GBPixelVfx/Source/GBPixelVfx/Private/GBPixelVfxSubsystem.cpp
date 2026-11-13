// UGBPixelVfxSubsystem: clip JSON + PNG atlas loading with caching.
#include "GBPixelVfxSubsystem.h"

#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

FString UGBPixelVfxSubsystem::VfxDataDir()
{
	return FPaths::ProjectContentDir() / TEXT("Data/Vfx");
}

bool UGBPixelVfxSubsystem::LoadClip(FName Id) const
{
	if (Id.IsNone())
	{
		return false;
	}
	if (ClipCache.Contains(Id) || MissingIds.Contains(Id))
	{
		return ClipCache.Contains(Id);
	}
	const FString JsonPath = VfxDataDir() / (Id.ToString() + TEXT(".json"));
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx clip '%s' is missing (%s)."), *Id.ToString(), *JsonPath);
		MissingIds.Add(Id);
		return false;
	}
	FGBPixelVfxClip Clip;
	if (!Clip.LoadFromJsonString(JsonText))
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx clip '%s' is broken (%s)."), *Id.ToString(), *JsonPath);
		MissingIds.Add(Id);
		return false;
	}
	ClipCache.Add(Id, MoveTemp(Clip));
	return true;
}

const FGBPixelVfxClip* UGBPixelVfxSubsystem::FindClip(FName Id) const
{
	if (!LoadClip(Id))
	{
		return nullptr;
	}
	return ClipCache.Find(Id);
}

UTexture2D* UGBPixelVfxSubsystem::GetAtlas(FName Id) const
{
	const FGBPixelVfxClip* Clip = FindClip(Id);
	if (!Clip)
	{
		return nullptr;
	}
	if (TObjectPtr<UTexture2D>* Cached = AtlasCache.Find(Id))
	{
		if (*Cached)
		{
			return *Cached;
		}
	}
	const FString PngPath = VfxDataDir() / Clip->AtlasFile;
	TArray<uint8> PngBytes;
	if (!FFileHelper::LoadFileToArray(PngBytes, *PngPath))
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx atlas for '%s' is missing (%s)."), *Id.ToString(), *PngPath);
		MissingIds.Add(Id);
		ClipCache.Remove(Id);
		return nullptr;
	}
	FImage Image;
	if (!FImageUtils::DecompressImage(PngBytes.GetData(), PngBytes.Num(), Image)
		|| Image.Format != ERawImageFormat::BGRA8)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx atlas for '%s' could not be decoded."), *Id.ToString());
		return nullptr;
	}
	const int32 AtlasW = Image.SizeX;
	const int32 AtlasH = Image.SizeY;
	const int32 ExpectW = Clip->Columns * Clip->FrameWidth;
	const int32 ExpectH = Clip->Rows() * Clip->FrameHeight;
	if (AtlasW != ExpectW || AtlasH != ExpectH
		|| Image.RawData.Num() != static_cast<int64>(AtlasW) * AtlasH * 4)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx atlas for '%s' is %dx%d but the clip needs %dx%d."),
			*Id.ToString(), AtlasW, AtlasH, ExpectW, ExpectH);
		return nullptr;
	}
	UTexture2D* Texture = UTexture2D::CreateTransient(AtlasW, AtlasH, PF_B8G8R8A8);
	if (!Texture || !Texture->GetPlatformData() || Texture->GetPlatformData()->Mips.Num() == 0)
	{
		UE_LOG(LogGBPixelVfx, Warning, TEXT("PixelVfx atlas for '%s': could not create a transient texture."), *Id.ToString());
		return nullptr;
	}
	Texture->Filter = TF_Nearest;
	Texture->SRGB = true;
	Texture->NeverStream = true;
	Texture->CompressionSettings = TC_EditorIcon;
	Texture->LODGroup = TEXTUREGROUP_Pixels2D;
	Texture->AddressX = TA_Clamp;
	Texture->AddressY = TA_Clamp;
#if WITH_EDITORONLY_DATA
	Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
	// FColor is B, G, R, A in memory: exactly BGRA8; rows go top to bottom.
	FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, Image.RawData.GetData(), Image.RawData.Num());
	Mip.BulkData.Unlock();
	Texture->UpdateResource();
	AtlasCache.Add(Id, Texture);
	return Texture;
}

TArray<FName> UGBPixelVfxSubsystem::ListClipIds() const
{
	TArray<FName> Ids;
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *(VfxDataDir() / TEXT("*.json")), true, false);
	for (const FString& File : Files)
	{
		Ids.Add(FName(*FPaths::GetBaseFilename(File)));
	}
	Ids.Sort([](const FName& A, const FName& B) { return A.LexicalLess(B); });
	return Ids;
}

void UGBPixelVfxSubsystem::ClearCache()
{
	AtlasCache.Reset();
	ClipCache.Reset();
	MissingIds.Reset();
}
