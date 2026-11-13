// Floating damage numbers of AGBHUD: one per hit on anything but the local player, rising and fading out.
#include "UI/GBHUD.h"

#include "UI/GBHUDShared.h"
#include "Stats/GBAttributeComponent.h"
#include "Data/GBDataSubsystem.h"
#include "Core/GBPlaceholder.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace GBDamageNumberLocal
{
	/** Pure presentation: length and size of the "pop" of a big hit. */
	constexpr float PopTime = 0.15f;
	constexpr float PopExtra = 0.35f;

	struct FTuning
	{
		float Lifetime = 0.9f;
		float RiseSpeed = 90.f;
		float DriftSpeed = 40.f;
		float TextSize = 22.f;
		float BigScale = 1.7f;
		float BigHealthFraction = 0.25f;
		float FadeStart = 0.6f;
		int32 MaxAlive = 40;
		float MergeWindow = 0.08f;
	};

	FTuning ReadTuning(const UObject* Context)
	{
		FTuning T;
		if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(Context))
		{
			T.Lifetime = Data->GetGameValue(GBHudKey::DamageNumberLifetime, T.Lifetime);
			T.RiseSpeed = Data->GetGameValue(GBHudKey::DamageNumberRiseSpeed, T.RiseSpeed);
			T.DriftSpeed = Data->GetGameValue(GBHudKey::DamageNumberDriftSpeed, T.DriftSpeed);
			T.TextSize = Data->GetGameValue(GBHudKey::DamageNumberTextSize, T.TextSize);
			T.BigScale = Data->GetGameValue(GBHudKey::DamageNumberBigScale, T.BigScale);
			T.BigHealthFraction = Data->GetGameValue(GBHudKey::DamageNumberBigHealthFraction, T.BigHealthFraction);
			T.FadeStart = Data->GetGameValue(GBHudKey::DamageNumberFadeStart, T.FadeStart);
			T.MaxAlive = FMath::RoundToInt(Data->GetGameValue(GBHudKey::DamageNumberMaxAlive, (float)T.MaxAlive));
			T.MergeWindow = Data->GetGameValue(GBHudKey::DamageNumberMergeWindow, T.MergeWindow);
		}
		T.Lifetime = FMath::Max(0.05f, T.Lifetime);
		T.TextSize = FMath::Max(4.f, T.TextSize);
		T.BigScale = FMath::Max(1.f, T.BigScale);
		T.FadeStart = FMath::Clamp(T.FadeStart, 0.f, 0.99f);
		T.MaxAlive = FMath::Max(1, T.MaxAlive);
		T.MergeWindow = FMath::Max(0.f, T.MergeWindow);
		return T;
	}

	bool IsBigHit(float Amount, float MaxHealth, float Fraction)
	{
		return MaxHealth > 0.f && Fraction > 0.f && Amount >= Fraction * MaxHealth;
	}
}

double AGBHUD::GetWorldTime() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

void AGBHUD::HandleDamageDealt(AActor* Target, const FGBDamageInfo& Info, float DamageDealt)
{
	using namespace GBDamageNumberLocal;
	// The delegate is global: ignore other worlds and the local player (the red flash covers the player).
	if (DamageDealt <= 0.f || (Target && Target->GetWorld() != GetWorld()) || (Target && Target == GetOwningPawn()))
	{
		return;
	}
	if (!Target && Info.HitLocation.IsZero())
	{
		return;
	}
	const FTuning Tuning = ReadTuning(this);
	const double Now = GetWorldTime();
	const UGBAttributeComponent* Attributes = Target ? UGBAttributeComponent::Find(Target) : nullptr;
	const float MaxHealth = Attributes ? Attributes->GetMaxHealth() : 0.f;

	// Hits on the same target in a very short window (pellets, ticks) become one number.
	if (Target && Tuning.MergeWindow > 0.f)
	{
		for (FDamageNumber& Number : DamageNumbers)
		{
			if (Number.Target.Get() == Target && Now - Number.StartTime <= Tuning.MergeWindow)
			{
				Number.Amount += DamageDealt;
				Number.bBig = Number.bBig || Info.bHeadshot || IsBigHit(Number.Amount, Number.TargetMaxHealth, Tuning.BigHealthFraction);
				return;
			}
		}
	}

	FDamageNumber Number;
	Number.Target = Target;
	if (!Info.HitLocation.IsZero())
	{
		Number.WorldPosition = Info.HitLocation;
	}
	else
	{
		FVector Origin;
		FVector Extent;
		Target->GetActorBounds(true, Origin, Extent);
		Number.WorldPosition = Origin + FVector(0.f, 0.f, Extent.Z);
	}
	Number.Drift = FMath::FRandRange(-1.f, 1.f);
	Number.Amount = DamageDealt;
	Number.TargetMaxHealth = MaxHealth;
	Number.Element = Info.Element;
	Number.bBig = Info.bHeadshot || IsBigHit(DamageDealt, MaxHealth, Tuning.BigHealthFraction);
	Number.StartTime = Now;
	DamageNumbers.Add(Number);
	while (DamageNumbers.Num() > Tuning.MaxAlive)
	{
		DamageNumbers.RemoveAt(0);
	}
}

void AGBHUD::DrawDamageNumbers(EGBHudSkin Skin)
{
	using namespace GBDamageNumberLocal;
	if (DamageNumbers.Num() == 0 || !Canvas)
	{
		return;
	}
	const FTuning Tuning = ReadTuning(this);
	const double Now = GetWorldTime();
	DamageNumbers.RemoveAll([Now, &Tuning](const FDamageNumber& Number) { return Now - Number.StartTime >= Tuning.Lifetime || Now < Number.StartTime; });

	const float S = UIScale();
	const APlayerCameraManager* Camera = PlayerOwner ? PlayerOwner->PlayerCameraManager.Get() : nullptr;
	const FVector CameraLocation = Camera ? Camera->GetCameraLocation() : FVector::ZeroVector;
	const FVector CameraForward = Camera ? Camera->GetCameraRotation().Vector() : FVector::ForwardVector;

	for (const FDamageNumber& Number : DamageNumbers)
	{
		if (Camera && FVector::DotProduct(Number.WorldPosition - CameraLocation, CameraForward) <= 0.f)
		{
			continue;
		}
		const FVector Screen = Canvas->Project(Number.WorldPosition, true);
		if (Screen.Z <= 0.f)
		{
			continue;
		}
		const float Age = (float)(Now - Number.StartTime);
		const float Life01 = FMath::Clamp(Age / Tuning.Lifetime, 0.f, 1.f);
		const float Alpha = Life01 <= Tuning.FadeStart ? 1.f : 1.f - (Life01 - Tuning.FadeStart) / (1.f - Tuning.FadeStart);

		float SizePx = Tuning.TextSize;
		const FLinearColor Base = GBPlaceholder::ElementColor((uint8)Number.Element);
		FLinearColor Color = FMath::Lerp(Base, FLinearColor::White, 0.25f);
		if (Number.bBig)
		{
			// Scale up then settle, and a brighter, warmer color.
			const float Pop01 = FMath::Clamp(Age / PopTime, 0.f, 1.f);
			SizePx *= Tuning.BigScale * (1.f + PopExtra * FMath::Sin(Pop01 * UE_PI));
			Color = FMath::Lerp(Base, FLinearColor(1.f, 1.f, 0.6f, 1.f), 0.6f);
		}
		Color.A = Alpha;

		const float X = Screen.X + Number.Drift * Tuning.DriftSpeed * S * Age;
		const FString Text = FString::Printf(TEXT("%d"), FMath::Max(1, FMath::RoundToInt(Number.Amount)));
		const float Rise = Tuning.RiseSpeed * S * Age;
		if (Skin == EGBHudSkin::Cyberpunk)
		{
			// Condensed numerals with a dark keyline; big hits in yellow with a chromatic split.
			FCyTextStyle Style;
			Style.Face = ECyFace::Condensed;
			Style.Px = SizePx * 1.25f;
			Style.Color = Number.bBig ? GBAlpha(GBCy::Yellow, Alpha) : Color;
			Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.85f);
			Style.Split = Number.bBig ? 2.f * S : 0.f;
			const FVector2D Size = MeasureCyText(Text, Style);
			DrawCyText(Text, X, Screen.Y - Rise - Size.Y * 0.5f, Style, 0.5f);
		}
		else
		{
			// Whole pixels of the game's pixel font: the size steps between integer scales.
			FPixelTextStyle Style;
			Style.Scale = FMath::Max(2, FMath::RoundToInt(SizePx * S / 7.f));
			Style.Color = Color;
			Style.Ink = Number.bBig ? &GBRpg::InkEmber : nullptr;
			if (Number.bBig)
			{
				Style.Color = FLinearColor(1.f, 1.f, 1.f, Alpha);
			}
			Style.Outline = GBRpg::Outline;
			Style.Shadow = FLinearColor(0.f, 0.f, 0.f, 0.6f);
			const float Y = Screen.Y - Rise - 3.5f * Style.Scale;
			DrawPixelText(Number.bBig ? Text + TEXT("!") : Text, X, Y, Style, 0.5f);
		}
	}
}
