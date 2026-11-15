#include "AI/GBSpriteAnimComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Rendering/PositionVertexBuffer.h"
#include "Rendering/StaticMeshVertexBuffer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "StaticMeshResources.h"

#include "Core/GBTypes.h"
#include "UI/GBPixelSpriteLibrary.h"

namespace GBSpriteAnimLocal
{
	const TCHAR* const PlanePath = TEXT("/Engine/BasicShapes/Plane.Plane");
	const TCHAR* const LitMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxSpriteLit.M_PixelVfxSpriteLit");
	const TCHAR* const UnlitMaterialPath = TEXT("/Game/Glyphbound/Materials/M_PixelVfxSprite.M_PixelVfxSprite");
	const TCHAR* const FallbackMaterialPath = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");
	const FName Idle(TEXT("Idle"));
	const FName Walk(TEXT("Walk"));
	const FName Run(TEXT("Run"));
	const FName Attack(TEXT("Attack"));
	const FName Attack2(TEXT("Attack2"));
	const FName CastClip(TEXT("Cast"));
	const FName Shoot(TEXT("Shoot"));
	const FName Leap(TEXT("Leap"));
	const FName Hurt(TEXT("Hurt"));
	const FName Death(TEXT("Death"));
	// Visual constants (not gameplay): the direction hysteresis, the timed-clip rate limits and the hit flash.
	constexpr float SectorHysteresis = 6.f;      // degrees past a border before the column changes
	constexpr float MinActionRate = 0.5f;
	constexpr float MaxActionRate = 2.5f;
	constexpr float FlashSeconds = 0.12f;
	constexpr float FlashGlow = 1.6f;
	// The engine plane (100 x 100 cm, normal +Z): its U grows along local +X and its V along local +Y (checked by the
	// Castle smoke step on the mesh's own vertex data, see PlaneUGrowsAlongX).
	constexpr bool bPlaneUAlongX = true;
}

// ---------------------------------------------------------------------------------------------------------------------
// Sheet and pure helpers
// ---------------------------------------------------------------------------------------------------------------------

bool FGBSpriteSheet::LoadFromJsonString(const FString& Text)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}
	FString IdText;
	if (Root->TryGetStringField(TEXT("Id"), IdText))
	{
		Id = FName(*IdText);
	}
	double Number = 0.0;
	CellWidth = Root->TryGetNumberField(TEXT("CellWidth"), Number) ? static_cast<int32>(Number) : 0;
	CellHeight = Root->TryGetNumberField(TEXT("CellHeight"), Number) ? static_cast<int32>(Number) : 0;
	PivotX = Root->TryGetNumberField(TEXT("PivotX"), Number) ? static_cast<float>(Number) : CellWidth * 0.5f;
	PivotY = Root->TryGetNumberField(TEXT("PivotY"), Number) ? static_cast<float>(Number) : static_cast<float>(CellHeight);
	PixelsPerMeter = Root->TryGetNumberField(TEXT("PixelsPerMeter"), Number) ? FMath::Max(static_cast<float>(Number), 1.f) : 44.f;
	Columns = Root->TryGetNumberField(TEXT("Columns"), Number) ? FMath::Max(static_cast<int32>(Number), 1) : GBSpriteAnim::NumDirections;
	Clips.Reset();
	const TSharedPtr<FJsonObject>* ClipsObject = nullptr;
	if (Root->TryGetObjectField(TEXT("Clips"), ClipsObject) && ClipsObject && ClipsObject->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ClipsObject)->Values)
		{
			const TSharedPtr<FJsonObject>* ClipObject = nullptr;
			if (!Pair.Value.IsValid() || !Pair.Value->TryGetObject(ClipObject) || !ClipObject || !ClipObject->IsValid())
			{
				continue;
			}
			const FJsonObject& C = **ClipObject;
			FGBSpriteClip Clip;
			Clip.Row = C.TryGetNumberField(TEXT("Row"), Number) ? static_cast<int32>(Number) : -1;
			Clip.Frames = C.TryGetNumberField(TEXT("Frames"), Number) ? static_cast<int32>(Number) : 0;
			if (Clip.Row < 0 || Clip.Frames < 1)
			{
				UE_LOG(LogGB, Warning, TEXT("Sprite sheet %s: clip %s refused (Row %d, Frames %d: Row must be 0 or more and Frames 1 or more)"), *IdText, *Pair.Key, Clip.Row, Clip.Frames);
				continue;
			}
			Clip.Fps = C.TryGetNumberField(TEXT("Fps"), Number) ? FMath::Max(static_cast<float>(Number), 0.1f) : 8.f;
			bool bLoop = false;
			Clip.bLoop = C.TryGetBoolField(TEXT("Loop"), bLoop) && bLoop;
			Clip.HitFrame = C.TryGetNumberField(TEXT("HitFrame"), Number) ? FMath::Clamp(static_cast<int32>(Number), 0, Clip.Frames - 1) : -1;
			const TArray<TSharedPtr<FJsonValue>>* Origin = nullptr;
			if (C.TryGetArrayField(TEXT("Origin"), Origin) && Origin && Origin->Num() >= 2)
			{
				Clip.bHasOrigin = true;
				Clip.Origin = FVector2D((*Origin)[0]->AsNumber(), (*Origin)[1]->AsNumber());
			}
			Clips.Add(FName(*Pair.Key), Clip);
		}
	}
	return CellWidth > 0 && CellHeight > 0 && Clips.Num() > 0;
}

int32 FGBSpriteSheet::RowCount() const
{
	int32 Rows = 0;
	for (const TPair<FName, FGBSpriteClip>& Pair : Clips)
	{
		Rows = FMath::Max(Rows, Pair.Value.Row + Pair.Value.Frames);
	}
	return Rows;
}

int32 GBSpriteAnim::ViewSector(float ActorYawDegrees, float ViewerBearingDegrees, int32 PrevSector, float HysteresisDegrees)
{
	float Angle = FRotator::NormalizeAxis(ActorYawDegrees - ViewerBearingDegrees);
	if (Angle < 0.f)
	{
		Angle += 360.f;
	}
	const int32 Raw = FMath::RoundToInt(Angle / 45.f) % NumDirections;
	if (PrevSector >= 0 && PrevSector < NumDirections && PrevSector != Raw)
	{
		const float FromPrev = FMath::Abs(FRotator::NormalizeAxis(Angle - PrevSector * 45.f));
		if (FromPrev <= 22.5f + FMath::Max(HysteresisDegrees, 0.f))
		{
			return PrevSector;
		}
	}
	return Raw;
}

int32 GBSpriteAnim::FrameAtTime(int32 Frames, float Fps, bool bLoop, float Seconds, bool& bOutFinished)
{
	bOutFinished = false;
	if (Frames <= 0 || Fps <= 0.f)
	{
		bOutFinished = true;
		return 0;
	}
	const int32 Stepped = FMath::FloorToInt(FMath::Max(Seconds, 0.f) * Fps);
	if (bLoop)
	{
		return Stepped % Frames;
	}
	if (Stepped >= Frames)
	{
		bOutFinished = true;
		return Frames - 1;
	}
	return Stepped;
}

float GBSpriteAnim::ClipLength(int32 Frames, float Fps)
{
	return (Frames > 0 && Fps > 0.f) ? static_cast<float>(Frames) / Fps : 0.f;
}

void GBSpriteAnim::TimeToHit(int32 HitFrame, float Fps, float HitInSeconds, float MinRate, float MaxRate, float& OutRate, float& OutStartSeconds)
{
	OutRate = 1.f;
	OutStartSeconds = 0.f;
	if (HitFrame < 0 || Fps <= 0.f || HitInSeconds <= 0.f)
	{
		return;
	}
	const float Natural = static_cast<float>(HitFrame) / Fps;
	OutRate = Natural > 0.f ? FMath::Clamp(Natural / HitInSeconds, MinRate, MaxRate) : 1.f;
	// Clip time of the hit is Natural; reached HitInSeconds from now at OutRate when it starts at Natural - HitInSeconds * rate.
	OutStartSeconds = Natural - HitInSeconds * OutRate;
}

FName GBSpriteAnim::ResolveClip(const FGBSpriteSheet& Sheet, FName Wanted)
{
	using namespace GBSpriteAnimLocal;
	if (Wanted.IsNone())
	{
		return NAME_None;
	}
	if (Sheet.Clips.Contains(Wanted))
	{
		return Wanted;
	}
	FName Next = NAME_None;
	if (Wanted == Attack2 || Wanted == CastClip || Wanted == Shoot || Wanted == Leap)
	{
		Next = Attack;
	}
	else if (Wanted == Run)
	{
		Next = Walk;
	}
	else if (Wanted == Walk)
	{
		Next = Idle;
	}
	return Next.IsNone() ? NAME_None : ResolveClip(Sheet, Next);
}

// ---------------------------------------------------------------------------------------------------------------------
// Subsystem
// ---------------------------------------------------------------------------------------------------------------------

UGBSpriteSheetSubsystem* UGBSpriteSheetSubsystem::Get(const UObject* WorldContext)
{
	if (!GEngine || !WorldContext)
	{
		return nullptr;
	}
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull);
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGBSpriteSheetSubsystem>() : nullptr;
}

FString UGBSpriteSheetSubsystem::SheetDir()
{
	return FPaths::ProjectContentDir() / TEXT("Data/Sprites");
}

bool UGBSpriteSheetSubsystem::Load(FName Id)
{
	if (Id.IsNone())
	{
		return false;
	}
	if (Sheets.Contains(Id) && Atlases.Contains(Id))
	{
		return true;
	}
	if (Failures.Contains(Id))
	{
		return false;
	}
	auto Fail = [this, Id](const FString& Why)
	{
		Failures.Add(Id, Why);
		UE_LOG(LogGB, Warning, TEXT("Sprite sheet '%s': %s (the enemy keeps its static sprite)"), *Id.ToString(), *Why);
		return false;
	};
	const FString JsonPath = SheetDir() / (Id.ToString() + TEXT(".json"));
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *JsonPath))
	{
		return Fail(FString::Printf(TEXT("%s is missing"), *JsonPath));
	}
	FGBSpriteSheet Sheet;
	if (!Sheet.LoadFromJsonString(Text))
	{
		return Fail(FString::Printf(TEXT("%s is broken (cells or clips missing)"), *JsonPath));
	}
	const FString PngPath = SheetDir() / (Id.ToString() + TEXT(".png"));
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *PngPath))
	{
		return Fail(FString::Printf(TEXT("%s is missing"), *PngPath));
	}
	FImage Image;
	if (!FImageUtils::DecompressImage(Bytes.GetData(), Bytes.Num(), Image))
	{
		return Fail(FString::Printf(TEXT("%s could not be decoded"), *PngPath));
	}
	// An 8-bit RGBA PNG decodes to BGRA8 (the format contract); anything else is refused (ImageCore is not linked by this module).
	if (Image.Format != ERawImageFormat::BGRA8)
	{
		return Fail(FString::Printf(TEXT("%s is not an 8-bit RGBA PNG"), *PngPath));
	}
	const int32 Width = Image.SizeX;
	const int32 Height = Image.SizeY;
	if (Width < Sheet.Columns * Sheet.CellWidth || Image.RawData.Num() != static_cast<int64>(Width) * Height * 4)
	{
		return Fail(FString::Printf(TEXT("%s is %dx%d, the %d columns need %d px of width"), *PngPath, Width, Height, Sheet.Columns, Sheet.Columns * Sheet.CellWidth));
	}
	// Every clip must fit inside the atlas rows (Row + Frames <= rows): one that does not is refused, the others stay.
	const int32 AtlasRows = Height / Sheet.CellHeight;
	for (auto It = Sheet.Clips.CreateIterator(); It; ++It)
	{
		if (It->Value.Row + It->Value.Frames > AtlasRows)
		{
			UE_LOG(LogGB, Warning, TEXT("Sprite sheet %s: clip %s refused (rows %d to %d, the atlas has %d rows)"), *Id.ToString(), *It->Key.ToString(), It->Value.Row, It->Value.Row + It->Value.Frames - 1, AtlasRows);
			It.RemoveCurrent();
		}
	}
	if (Sheet.Clips.Num() == 0)
	{
		return Fail(FString::Printf(TEXT("%s: no clip fits the %d atlas rows"), *PngPath, AtlasRows));
	}
	TArray<FColor> Pixels;
	Pixels.SetNumUninitialized(Width * Height);
	FMemory::Memcpy(Pixels.GetData(), Image.RawData.GetData(), Image.RawData.Num());
	UTexture2D* Texture = UGBPixelSpriteLibrary::CreatePixelTexture(Pixels, Width, Height, Id);
	if (!Texture)
	{
		return Fail(TEXT("the texture could not be created"));
	}
	const int32 ClipCount = Sheet.Clips.Num();
	Sheets.Add(Id, MakeShared<FGBSpriteSheet>(MoveTemp(Sheet)));
	Atlases.Add(Id, Texture);
	UE_LOG(LogGB, Display, TEXT("Sprite sheet '%s' loaded: %dx%d px, %d clips"), *Id.ToString(), Width, Height, ClipCount);
	return true;
}

const FGBSpriteSheet* UGBSpriteSheetSubsystem::FindSheet(FName Id)
{
	if (!Load(Id))
	{
		return nullptr;
	}
	const TSharedPtr<FGBSpriteSheet>* Found = Sheets.Find(Id);
	return Found ? Found->Get() : nullptr;
}

UTexture2D* UGBSpriteSheetSubsystem::GetAtlas(FName Id)
{
	return Load(Id) ? Atlases.FindRef(Id) : nullptr;
}

// ---------------------------------------------------------------------------------------------------------------------
// Component
// ---------------------------------------------------------------------------------------------------------------------

UGBSpriteAnimComponent::UGBSpriteAnimComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	// After the owner moved and turned this frame (the column follows the final yaw).
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

bool UGBSpriteAnimComponent::PlaneUGrowsAlongX()
{
	return GBSpriteAnimLocal::bPlaneUAlongX;
}

bool UGBSpriteAnimComponent::Setup(FName InSheetId, float InScale, const FLinearColor& InTint, float InGlow)
{
	using namespace GBSpriteAnimLocal;
	SheetId = InSheetId;
	Scale = FMath::Max(InScale, 0.05f);
	Tint = InTint;
	Glow = FMath::Max(InGlow, 0.f);
	Sheet = nullptr;
	Failure.Reset();
	UGBSpriteSheetSubsystem* Sheets = UGBSpriteSheetSubsystem::Get(this);
	if (!Sheets)
	{
		Failure = TEXT("no sprite sheet subsystem");
		return false;
	}
	const FGBSpriteSheet* Found = Sheets->FindSheet(SheetId);
	UTexture2D* Atlas = Found ? Sheets->GetAtlas(SheetId) : nullptr;
	if (!Found || !Atlas)
	{
		Failure = Sheets->GetFailure(SheetId);
		return false;
	}
	UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, PlanePath);
	UMaterialInterface* Base = LoadObject<UMaterialInterface>(nullptr, LitMaterialPath);
	if (!Base)
	{
		UE_LOG(LogGB, Warning, TEXT("Sprite sheet '%s': %s is missing, using the unlit sprite material"), *SheetId.ToString(), LitMaterialPath);
		Base = LoadObject<UMaterialInterface>(nullptr, UnlitMaterialPath);
	}
	if (!Base)
	{
		Base = LoadObject<UMaterialInterface>(nullptr, FallbackMaterialPath);
	}
	if (!Plane || !Base)
	{
		Failure = TEXT("the engine plane or the sprite material is missing");
		return false;
	}
	Sheet = Found;
	if (!Quad)
	{
		Quad = NewObject<UStaticMeshComponent>(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this), NAME_None, RF_Transient);
		Quad->SetMobility(EComponentMobility::Movable);
		Quad->SetStaticMesh(Plane);
		Quad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Quad->SetCanEverAffectNavigation(false);
		Quad->SetGenerateOverlapEvents(false);
		Quad->SetCastShadow(false);
		Quad->SetUsingAbsoluteRotation(true);
		Quad->SetUsingAbsoluteScale(true);
		Quad->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
		Quad->RegisterComponent();
	}
	Material = UMaterialInstanceDynamic::Create(Base, this);
	Material->SetTextureParameterValue(TEXT("Atlas"), Atlas);
	Material->SetVectorParameterValue(TEXT("Tint"), Tint);
	Material->SetScalarParameterValue(TEXT("Glow"), Glow);
	Quad->SetMaterial(0, Material);
	AppliedRow = -1;
	AppliedColumn = -1;
	AppliedGlow = Glow;
	LocoClip = NAME_None;
	UpdateFrame(0.f);
	UpdateQuad();
	return true;
}

bool UGBSpriteAnimComponent::HasClip(FName Clip) const
{
	return Sheet && Sheet->Clips.Contains(Clip);
}

float UGBSpriteAnimComponent::GetWorldHeight() const
{
	return Sheet ? Sheet->CellHeight * 100.f / Sheet->PixelsPerMeter * Scale : 0.f;
}

bool UGBSpriteAnimComponent::PlayAction(FName Clip, float HitInSeconds)
{
	if (!Sheet || bDead)
	{
		return false;
	}
	const FName Resolved = GBSpriteAnim::ResolveClip(*Sheet, Clip);
	const FGBSpriteClip* Data = Resolved.IsNone() ? nullptr : Sheet->FindClip(Resolved);
	if (!Data)
	{
		return false;
	}
	ActionClip = Resolved;
	ActionRate = 1.f;
	ActionTime = 0.f;
	if (HitInSeconds > 0.f && Data->HitFrame >= 0)
	{
		GBSpriteAnim::TimeToHit(Data->HitFrame, Data->Fps, HitInSeconds, GBSpriteAnimLocal::MinActionRate, GBSpriteAnimLocal::MaxActionRate, ActionRate, ActionTime);
	}
	bHitPending = Data->HitFrame >= 0;
	bHitCrossed = false;
	HurtClip = NAME_None;
	++ActionsPlayed;
	return true;
}

int32 UGBSpriteAnimComponent::GetClipFrames(FName Clip) const
{
	const FGBSpriteClip* Data = Sheet ? Sheet->FindClip(Clip) : nullptr;
	return Data ? Data->Frames : 0;
}

float UGBSpriteAnimComponent::GetActionLength() const
{
	const FGBSpriteClip* Data = (Sheet && !ActionClip.IsNone()) ? Sheet->FindClip(ActionClip) : nullptr;
	return Data ? GBSpriteAnim::ClipLength(Data->Frames, Data->Fps) / FMath::Max(ActionRate, 0.01f) : 0.f;
}

void UGBSpriteAnimComponent::CancelAction()
{
	ActionClip = NAME_None;
	bHitPending = false;
}

bool UGBSpriteAnimComponent::TakeHitFrame()
{
	const bool bWas = bHitCrossed;
	bHitCrossed = false;
	return bWas;
}

void UGBSpriteAnimComponent::NotifyHit()
{
	if (!Sheet || bDead)
	{
		return;
	}
	FlashTime = GBSpriteAnimLocal::FlashSeconds;
	if (ActionClip.IsNone() && Sheet->Clips.Contains(GBSpriteAnimLocal::Hurt))
	{
		HurtClip = GBSpriteAnimLocal::Hurt;
		HurtTime = 0.f;
	}
}

float UGBSpriteAnimComponent::PlayDeath()
{
	if (!Sheet)
	{
		return 0.f;
	}
	const FGBSpriteClip* Data = Sheet->FindClip(GBSpriteAnimLocal::Death);
	if (!Data)
	{
		return 0.f;
	}
	bDead = true;
	DeathClip = GBSpriteAnimLocal::Death;
	DeathTime = 0.f;
	ActionClip = NAME_None;
	HurtClip = NAME_None;
	bHitPending = false;
	FlashTime = 0.f;
	UpdateFrame(0.f);
	return GBSpriteAnim::ClipLength(Data->Frames, Data->Fps);
}

float UGBSpriteAnimComponent::GetDeathHeldSeconds() const
{
	const FGBSpriteClip* Data = (Sheet && bDead) ? Sheet->FindClip(DeathClip) : nullptr;
	return Data ? FMath::Max(0.f, DeathTime - GBSpriteAnim::ClipLength(Data->Frames, Data->Fps)) : 0.f;
}

bool UGBSpriteAnimComponent::GetClipOriginWorld(FName Clip, FVector& OutWorld) const
{
	const FName Resolved = Sheet ? GBSpriteAnim::ResolveClip(*Sheet, Clip) : NAME_None;
	const FGBSpriteClip* Data = Resolved.IsNone() ? nullptr : Sheet->FindClip(Resolved);
	if (!Data || !Data->bHasOrigin || !GetOwner())
	{
		return false;
	}
	// The d=0 column shows the face: its screen right is the character's own left.
	const float CmPerPixel = 100.f / Sheet->PixelsPerMeter * Scale;
	const FVector Left = -GetOwner()->GetActorRightVector();
	const FVector Forward = GetOwner()->GetActorForwardVector().GetSafeNormal2D();
	OutWorld = GetComponentLocation() + Left * ((Data->Origin.X - Sheet->PivotX) * CmPerPixel) + FVector::UpVector * ((Sheet->PivotY - Data->Origin.Y) * CmPerPixel) + Forward * 20.f;
	return true;
}

FName UGBSpriteAnimComponent::PickLocomotion() const
{
	using namespace GBSpriteAnimLocal;
	const float Speed = GetOwner() ? GetOwner()->GetVelocity().Size2D() : 0.f;
	FName Wanted = Idle;
	if (Speed >= RunSpeed)
	{
		Wanted = Run;
	}
	else if (Speed >= WalkSpeed)
	{
		Wanted = Walk;
	}
	else if (!IdleOverride.IsNone() && Sheet->Clips.Contains(IdleOverride))
	{
		return IdleOverride;
	}
	const FName Resolved = GBSpriteAnim::ResolveClip(*Sheet, Wanted);
	if (!Resolved.IsNone())
	{
		return Resolved;
	}
	// A sheet without Idle: whatever loops first.
	for (const TPair<FName, FGBSpriteClip>& Pair : Sheet->Clips)
	{
		if (Pair.Value.bLoop)
		{
			return Pair.Key;
		}
	}
	return NAME_None;
}

void UGBSpriteAnimComponent::UpdateFrame(float DeltaTime)
{
	if (!Sheet)
	{
		return;
	}
	FName Clip = NAME_None;
	int32 Frame = 0;
	bool bFinished = false;
	FlashTime = FMath::Max(0.f, FlashTime - DeltaTime);

	if (bDead)
	{
		DeathTime += DeltaTime;
		if (const FGBSpriteClip* Data = Sheet->FindClip(DeathClip))
		{
			Clip = DeathClip;
			Frame = GBSpriteAnim::FrameAtTime(Data->Frames, Data->Fps, false, DeathTime, bFinished);
		}
	}
	if (Clip.IsNone() && !ActionClip.IsNone())
	{
		const FGBSpriteClip* Data = Sheet->FindClip(ActionClip);
		if (Data)
		{
			ActionTime += DeltaTime * ActionRate;
			if (bHitPending && ActionTime >= static_cast<float>(Data->HitFrame) / Data->Fps)
			{
				bHitPending = false;
				bHitCrossed = true;
				++HitFramesCrossed;
			}
			Frame = GBSpriteAnim::FrameAtTime(Data->Frames, Data->Fps, false, ActionTime, bFinished);
			Clip = ActionClip;
		}
		if (!Data || bFinished)
		{
			ActionClip = NAME_None;
			Clip = NAME_None;
		}
	}
	if (Clip.IsNone() && !HurtClip.IsNone())
	{
		const FGBSpriteClip* Data = Sheet->FindClip(HurtClip);
		if (Data)
		{
			HurtTime += DeltaTime;
			Frame = GBSpriteAnim::FrameAtTime(Data->Frames, Data->Fps, false, HurtTime, bFinished);
			Clip = HurtClip;
		}
		if (!Data || bFinished)
		{
			HurtClip = NAME_None;
			Clip = NAME_None;
		}
	}
	if (Clip.IsNone())
	{
		const FName Wanted = PickLocomotion();
		if (Wanted != LocoClip)
		{
			LocoClip = Wanted;
			LocoTime = 0.f;
		}
		LocoTime += DeltaTime;
		if (const FGBSpriteClip* Data = LocoClip.IsNone() ? nullptr : Sheet->FindClip(LocoClip))
		{
			Clip = LocoClip;
			Frame = GBSpriteAnim::FrameAtTime(Data->Frames, Data->Fps, Data->bLoop, LocoTime, bFinished);
		}
	}
	CurrentClip = Clip;
	CurrentFrame = Clip.IsNone() ? -1 : Frame;
}

void UGBSpriteAnimComponent::ApplyCell(int32 Row, int32 Column)
{
	if (!Material || !Sheet)
	{
		return;
	}
	const float GlowNow = Glow + (FlashTime > 0.f ? GBSpriteAnimLocal::FlashGlow : 0.f);
	if (GlowNow != AppliedGlow)
	{
		AppliedGlow = GlowNow;
		Material->SetScalarParameterValue(TEXT("Glow"), GlowNow);
	}
	if (Row == AppliedRow && Column == AppliedColumn)
	{
		return;
	}
	AppliedRow = Row;
	AppliedColumn = Column;
	UTexture2D* Atlas = nullptr;
	if (UGBSpriteSheetSubsystem* Sheets = UGBSpriteSheetSubsystem::Get(this))
	{
		Atlas = Sheets->GetAtlas(SheetId);
	}
	const float AtlasW = Atlas ? static_cast<float>(Atlas->GetSizeX()) : static_cast<float>(Sheet->Columns * Sheet->CellWidth);
	const float AtlasH = Atlas ? static_cast<float>(Atlas->GetSizeY()) : static_cast<float>(Sheet->RowCount() * Sheet->CellHeight);
	const float U = Column * Sheet->CellWidth / AtlasW;
	const float W = Sheet->CellWidth / AtlasW;
	const float V = Row * Sheet->CellHeight / AtlasH;
	const float H = Sheet->CellHeight / AtlasH;
	// The quad's local +X points to the camera's left and +Y up (see UpdateQuad); V = 0 at the bottom of the plane, so V is
	// flipped, and U is flipped too when it grows along +X, so the image reads unmirrored (a right hand stays a right hand).
	const bool bFlipU = GBSpriteAnimLocal::bPlaneUAlongX;
	Material->SetScalarParameterValue(TEXT("FrameU"), bFlipU ? U + W : U);
	Material->SetScalarParameterValue(TEXT("FrameW"), bFlipU ? -W : W);
	Material->SetScalarParameterValue(TEXT("FrameV"), V + H);
	Material->SetScalarParameterValue(TEXT("FrameH"), -H);
}

void UGBSpriteAnimComponent::UpdateQuad()
{
	if (!Quad || !Sheet)
	{
		return;
	}
	const FVector Feet = GetComponentLocation();
	FVector ToCamera = FVector::ZeroVector;
	if (UWorld* World = GetWorld())
	{
		if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
		{
			ToCamera = Camera->GetCameraLocation() - Feet;
		}
	}
	ToCamera.Z = 0.f;
	if (ToCamera.SizeSquared() < 1.f)
	{
		ToCamera = GetOwner() ? GetOwner()->GetActorForwardVector() : FVector::ForwardVector;
		ToCamera.Z = 0.f;
	}
	const FVector T = ToCamera.GetSafeNormal();
	const float ActorYaw = GetOwner() ? GetOwner()->GetActorRotation().Yaw : 0.f;
	const float Bearing = FMath::RadiansToDegrees(FMath::Atan2(T.Y, T.X));
	Sector = GBSpriteAnim::ViewSector(ActorYaw, Bearing, Sector, GBSpriteAnimLocal::SectorHysteresis);

	// Normal (local Z) to the camera, local Y up, local X = Y x Z (the camera's left): a proper rotation of the plane.
	const FVector Up = FVector::UpVector;
	const FVector X = FVector::CrossProduct(Up, T);
	const FQuat Rotation = FMatrix(X, Up, T, FVector::ZeroVector).ToQuat();
	const float CmPerPixel = 100.f / Sheet->PixelsPerMeter * Scale;
	const FVector CameraRight = -X;
	const FVector Centre = Feet + CameraRight * ((Sheet->CellWidth * 0.5f - Sheet->PivotX) * CmPerPixel) + Up * ((Sheet->PivotY - Sheet->CellHeight * 0.5f) * CmPerPixel);
	Quad->SetWorldLocationAndRotation(Centre, Rotation);
	Quad->SetWorldScale3D(FVector(Sheet->CellWidth * CmPerPixel / 100.f, Sheet->CellHeight * CmPerPixel / 100.f, 1.f));

	if (const FGBSpriteClip* Data = CurrentClip.IsNone() ? nullptr : Sheet->FindClip(CurrentClip))
	{
		ApplyCell(Data->Row + FMath::Clamp(CurrentFrame, 0, Data->Frames - 1), FMath::Clamp(Sector, 0, Sheet->Columns - 1));
	}
}

void UGBSpriteAnimComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Sheet)
	{
		return;
	}
	UpdateFrame(DeltaTime);
	UpdateQuad();
}

void UGBSpriteAnimComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if (Quad)
	{
		Quad->DestroyComponent();
		Quad = nullptr;
	}
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

FString UGBSpriteAnimComponent::Describe() const
{
	if (!Sheet)
	{
		return FString::Printf(TEXT("sheet=%s missing (%s)"), *SheetId.ToString(), Failure.IsEmpty() ? TEXT("not set up") : *Failure);
	}
	return FString::Printf(TEXT("sheet=%s clip=%s frame=%d dir=%d action=%s actions=%d hitFrames=%d dead=%d height=%.0fcm"), *SheetId.ToString(), *CurrentClip.ToString(), CurrentFrame, Sector,
		ActionClip.IsNone() ? TEXT("-") : *ActionClip.ToString(), ActionsPlayed, HitFramesCrossed, bDead ? 1 : 0, GetWorldHeight());
}
