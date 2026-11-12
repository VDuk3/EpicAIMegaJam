#include "QTE/GBQTEComponent.h"
#include "Core/GBTypes.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Stats/GBStatsComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/IConsoleManager.h"
#include "Misc/App.h"

static TAutoConsoleVariable<int32> CVarGBQTE(
	TEXT("gb.QTE"),
	1,
	TEXT("Spell quick-time event. 1 = on (if QTE.json bEnabled), 0 = off: spells cast at their default strength."),
	ECVF_Default);

namespace GBQTEComponentPrivate
{
	// Fallbacks used only when QTE.json lacks the key (a warning is logged). The real values live in QTE.json.
	constexpr float DefaultTimeDilation = 0.1f;
	constexpr float DefaultGestureIdleSubmitTime = 0.25f;
	constexpr float DefaultLineSubmitLengthMult = 2.5f;
	constexpr float DefaultStickGestureScale = 600.f;
	constexpr float FullCircleDegrees = 360.f;
}

UGBQTEComponent::UGBQTEComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Ticks only while a QTE runs (enabled in StartQTE, disabled in Finish). Not while the game is paused (default).
	PrimaryComponentTick.bStartWithTickEnabled = false;
	Random.GenerateNewSeed();
}

bool UGBQTEComponent::IsEnabled() const
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const bool bDataEnabled = Data ? Data->GetQTE().bEnabled : true;
	return bDataEnabled && CVarGBQTE.GetValueOnGameThread() != 0;
}

float UGBQTEComponent::ReadStat(FName Key, float Default) const
{
	float Base = Default;
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const float* Found = Data ? Data->GetQTE().Stats.Find(Key) : nullptr;
	if (Found)
	{
		Base = *Found;
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("QTE: QTE.json has no Stats entry '%s'; using %f."), *Key.ToString(), Default);
	}
	const UGBStatsComponent* Stats = UGBStatsComponent::Find(GetOwner());
	return Stats ? Stats->Evaluate(Key, Base) : Base;
}

float UGBQTEComponent::GetStickGestureScale() const
{
	return ReadStat(GBQTEStat::StickGestureScale, GBQTEComponentPrivate::DefaultStickGestureScale);
}

float UGBQTEComponent::GetFailManaRefund() const
{
	return ReadStat(GBQTEStat::FailManaRefund, 0.f);
}

float UGBQTEComponent::GetFailCooldownMult() const
{
	return ReadStat(GBQTEStat::FailCooldownMult, 1.f);
}

FGBQTEConfig UGBQTEComponent::BuildConfig() const
{
	const FGBQTEConfig Defaults;
	FGBQTEConfig Config;
	Config.SequenceLength = FMath::Max(0, FMath::RoundToInt(ReadStat(GBStat::QTESequenceLength, static_cast<float>(Defaults.SequenceLength))));
	Config.GestureCount = FMath::Max(0, FMath::RoundToInt(ReadStat(GBStat::QTEGestureCount, static_cast<float>(Defaults.GestureCount))));
	Config.TimeLimit = ReadStat(GBStat::QTETimeLimit, Defaults.TimeLimit);
	Config.GestureMinScore = ReadStat(GBQTEStat::GestureMinScore, Defaults.GestureMinScore);
	Config.GestureMinLength = FMath::Max(0.f, ReadStat(GBQTEStat::GestureMinLength, Defaults.GestureMinLength));

	if (const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		const UEnum* GestureEnum = StaticEnum<EGBGesture>();
		const int64 MaxGesture = static_cast<int64>(EGBGesture::CircleCounterClockwise);
		for (const FString& Name : Data->GetQTE().Gestures)
		{
			const int64 Value = GestureEnum->GetValueByNameString(Name);
			if (Value == INDEX_NONE || Value < 0 || Value > MaxGesture)
			{
				UE_LOG(LogGB, Warning, TEXT("QTE: unknown gesture '%s' in QTE.json; ignored."), *Name);
				continue;
			}
			Config.AllowedGestures.AddUnique(static_cast<EGBGesture>(Value));
		}
	}
	return Config;
}

bool UGBQTEComponent::StartQTE(FName SpellId, FGBOnQTEFinished OnFinished)
{
	if (bActive)
	{
		return false;
	}

	if (!IsEnabled())
	{
		FGBQTEResult Result;
		Result.Power = 0.f;
		Result.Reach = 0.f;
		Result.bUsedQTE = false;
		Result.bSuccess = true;
		LastResult = Result;
		OnFinished.ExecuteIfBound(Result);
		return true;
	}

	const float TimeDilation = ReadStat(GBStat::QTETimeDilation, GBQTEComponentPrivate::DefaultTimeDilation);
	GestureIdleSubmitTime = ReadStat(GBQTEStat::GestureIdleSubmitTime, GBQTEComponentPrivate::DefaultGestureIdleSubmitTime);
	LineSubmitLengthMult = ReadStat(GBQTEStat::LineSubmitLengthMult, GBQTEComponentPrivate::DefaultLineSubmitLengthMult);

	Session = FGBQTESession();
	Session.Start(BuildConfig(), Random);
	ActiveSpellId = SpellId;
	FinishedDelegate = OnFinished;
	GestureIdleTime = 0.f;
	bActive = true;

	if (Session.IsFinished())
	{
		// No steps configured: nothing to play.
		Finish();
		return true;
	}

	UGameplayStatics::SetGlobalTimeDilation(this, TimeDilation);
	SetComponentTickEnabled(true);
	return true;
}

void UGBQTEComponent::CancelQTE()
{
	if (!bActive)
	{
		return;
	}
	Session.Phase = EGBQTEPhase::Finished;
	// A cancelled QTE never casts: the result is unsuccessful even if some steps were right.
	bForceFailResult = true;
	Finish();
}

void UGBQTEComponent::HandleKey(EGBQTEKey Key)
{
	if (!bActive)
	{
		return;
	}
	Session.PressKey(Key);
	if (Session.IsFinished())
	{
		Finish();
	}
}

void UGBQTEComponent::HandleGestureInput(const FVector2D& Delta)
{
	if (!bActive || Session.Phase != EGBQTEPhase::Gesture || Delta.IsNearlyZero())
	{
		return;
	}
	Session.AddGestureDelta(Delta);
	GestureIdleTime = 0.f;
}

void UGBQTEComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bActive)
	{
		return;
	}

	// DeltaTime is dilated by the QTE slow-down; the QTE runs on the undilated frame time.
	const float RealDelta = static_cast<float>(FApp::GetDeltaTime());
	Session.Advance(RealDelta);

	if (Session.Phase == EGBQTEPhase::Gesture)
	{
		GestureIdleTime += RealDelta;
		if (Session.IsGestureReadyToJudge() && Session.Gestures.IsValidIndex(Session.GestureScores.Num()))
		{
			const EGBGesture Wanted = Session.Gestures[Session.GestureScores.Num()];
			const bool bCircle = Wanted == EGBGesture::CircleClockwise || Wanted == EGBGesture::CircleCounterClockwise;

			bool bSubmit = GestureIdleTime >= GestureIdleSubmitTime;
			if (bCircle)
			{
				bSubmit |= FMath::Abs(FGBGestureRecognizer::TotalTurnDegrees(Session.CurrentStroke)) >= GBQTEComponentPrivate::FullCircleDegrees;
			}
			else
			{
				bSubmit |= FGBGestureRecognizer::PathLength(Session.CurrentStroke) >= LineSubmitLengthMult * Session.Config.GestureMinLength;
			}

			if (bSubmit)
			{
				Session.SubmitGesture();
				GestureIdleTime = 0.f;
			}
		}
	}

	if (Session.IsFinished())
	{
		Finish();
	}
}

void UGBQTEComponent::Finish()
{
	if (!bActive)
	{
		return;
	}
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	bActive = false;
	SetComponentTickEnabled(false);
	FGBQTEResult Result = Session.GetResult();
	if (bForceFailResult)
	{
		Result.bSuccess = false;
		Result.bUsedQTE = true;
	}
	bForceFailResult = false;
	LastResult = Result;
	ActiveSpellId = NAME_None;

	// Copy first: the callback may start a new QTE, which overwrites FinishedDelegate (and LastResult).
	FGBOnQTEFinished Delegate = FinishedDelegate;
	FinishedDelegate.Unbind();
	Delegate.ExecuteIfBound(Result);
}

void UGBQTEComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (bActive)
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		bActive = false;
		FinishedDelegate.Unbind();
	}
	Super::EndPlay(EndPlayReason);
}
