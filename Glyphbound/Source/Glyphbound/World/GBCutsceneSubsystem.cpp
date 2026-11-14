#include "World/GBCutsceneSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/GBTypes.h"
#include "Data/GBDataSubsystem.h"
#include "Player/GBPlayerCharacter.h"
#include "Player/GBPlayerController.h"
#include "QTE/GBQTEComponent.h"
#include "Misc/App.h"
#include "World/GBEraSubsystem.h"
#include "Stats/GBAttributeComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "World/GBProgressSubsystem.h"

namespace GBCutsceneLocal
{
	// A hitch (loading) must not eat several lines in one frame.
	constexpr float MaxStep = 0.1f;
	// Used when a line has no (or a non-positive) duration in the data.
	constexpr float MinLineSeconds = 0.1f;
	const FString EmptyText;
}

UGBCutsceneSubsystem* UGBCutsceneSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject || !GEngine)
	{
		return nullptr;
	}
	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
	return World ? World->GetSubsystem<UGBCutsceneSubsystem>() : nullptr;
}

bool UGBCutsceneSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

TStatId UGBCutsceneSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGBCutsceneSubsystem, STATGROUP_Tickables);
}

void UGBCutsceneSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		FlagChangedHandle = Progress->OnFlagChanged.AddUObject(this, &UGBCutsceneSubsystem::HandleFlagChanged);
	}
}

void UGBCutsceneSubsystem::Deinitialize()
{
	if (UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this))
	{
		Progress->OnFlagChanged.Remove(FlagChangedHandle);
	}
	FlagChangedHandle.Reset();
	// The world is going away: nothing to give back, and callbacks must not run on dead actors.
	bPlaying = false;
	PlayingOnFinished = nullptr;
	PlayingOnCancelled = nullptr;
	Queue.Reset();
	Super::Deinitialize();
}

const FString& UGBCutsceneSubsystem::GetLineText() const
{
	return (bPlaying && Lines.IsValidIndex(LineIndex)) ? Lines[LineIndex].Text : GBCutsceneLocal::EmptyText;
}

float UGBCutsceneSubsystem::GetLineSeconds() const
{
	return (bPlaying && Lines.IsValidIndex(LineIndex)) ? FMath::Max(GBCutsceneLocal::MinLineSeconds, Lines[LineIndex].Seconds) : 0.f;
}

bool UGBCutsceneSubsystem::Play(FName Id, TFunction<void()> OnFinished, TFunction<void()> OnCancelled)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBCutsceneDefinition* Def = Data ? Data->FindCutscene(Id) : nullptr;
	if (!Def || Def->Lines.Num() == 0)
	{
		UE_LOG(LogGB, Warning, TEXT("Cutscene: '%s' is unknown or has no lines; skipped."), *Id.ToString());
		if (OnFinished)
		{
			OnFinished();
		}
		return false;
	}

	FQueued Entry;
	Entry.Id = Id;
	Entry.OnFinished = MoveTemp(OnFinished);
	Entry.OnCancelled = MoveTemp(OnCancelled);
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	if (bPlaying || (Era && Era->IsTravelling()) || Queue.Num() > 0)
	{
		// Waits for its turn (Tick starts it once nothing plays and no era travel runs).
		Queue.Add(MoveTemp(Entry));
		return true;
	}
	Begin(Entry);
	return true;
}

void UGBCutsceneSubsystem::Begin(const FQueued& Entry)
{
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBCutsceneDefinition* Def = Data ? Data->FindCutscene(Entry.Id) : nullptr;
	if (!Def || Def->Lines.Num() == 0)
	{
		if (Entry.OnFinished)
		{
			Entry.OnFinished();
		}
		return;
	}

	AGBPlayerCharacter* Player = AGBPlayerCharacter::Get(this);
	if (Player && Player->IsDead())
	{
		// Nobody to show it to: cancelled, not finished.
		if (Entry.OnCancelled)
		{
			Entry.OnCancelled();
		}
		return;
	}

	bPlaying = true;
	PlayingId = Entry.Id;
	PlayingOnFinished = Entry.OnFinished;
	PlayingOnCancelled = Entry.OnCancelled;
	Lines = Def->Lines;
	LineIndex = 0;
	LineElapsed = 0.f;
	TotalElapsed = 0.f;
	SkipDelay = Data->GetGameValue(GBCutsceneKey::SkipDelay, 1.f);

	LockedPlayer = Player;
	if (Player)
	{
		// Whatever was running leaves first: a spell QTE (it would otherwise keep a time dilation and a weapon lock) and any menu.
		if (UGBQTEComponent* QTE = Player->GetQTE())
		{
			if (QTE->IsActive())
			{
				QTE->CancelQTE();
			}
		}
		if (AGBPlayerController* Controller = Cast<AGBPlayerController>(Player->GetController()))
		{
			const EGBMenu Open = Controller->GetOpenMenu();
			if (Open != EGBMenu::None && Open != EGBMenu::Death)
			{
				Controller->SetMenu(EGBMenu::None);
			}
		}
		Player->SetCutsceneLocked(true);
		if (UGBAttributeComponent* Attributes = Player->GetAttributes())
		{
			Attributes->SetCutsceneInvulnerable(true);
		}
		if (UGBEquipmentComponent* Equipment = Player->GetEquipment())
		{
			Equipment->SetCutsceneLocked(true);
		}
	}
	UE_LOG(LogGB, Display, TEXT("Cutscene: '%s' started (%d lines)."), *PlayingId.ToString(), Lines.Num());
}

void UGBCutsceneSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bPlaying)
	{
		const AGBPlayerCharacter* Player = LockedPlayer.Get();
		if (!Player || Player->IsDead())
		{
			Cancel();
			return;
		}
		// Real time: the world clock may be dilated (a QTE), a caption is not.
		const float Step = FMath::Min(static_cast<float>(FApp::GetDeltaTime()), GBCutsceneLocal::MaxStep);
		TotalElapsed += Step;
		LineElapsed += Step;
		while (bPlaying && LineElapsed >= GetLineSeconds())
		{
			LineElapsed -= GetLineSeconds();
			++LineIndex;
			if (!Lines.IsValidIndex(LineIndex))
			{
				Finish();
				return;
			}
		}
		return;
	}

	// A queued caption starts only when no era travel runs (the travel's end would hand control back mid-caption).
	if (Queue.Num() > 0)
	{
		const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
		if (!Era || !Era->IsTravelling())
		{
			const FQueued Next = Queue[0];
			Queue.RemoveAt(0);
			Begin(Next);
		}
	}
}

bool UGBCutsceneSubsystem::RequestSkip()
{
	if (!bPlaying)
	{
		return false;
	}
	if (CanSkip())
	{
		Finish();
	}
	return true;
}

void UGBCutsceneSubsystem::Stop()
{
	if (bPlaying)
	{
		Finish();
	}
}

void UGBCutsceneSubsystem::ReleasePlayer()
{
	if (AGBPlayerCharacter* Player = LockedPlayer.Get())
	{
		if (UGBAttributeComponent* Attributes = Player->GetAttributes())
		{
			Attributes->SetCutsceneInvulnerable(false);
		}
		if (UGBEquipmentComponent* Equipment = Player->GetEquipment())
		{
			Equipment->SetCutsceneLocked(false);
		}
		Player->SetCutsceneLocked(false);
	}
	LockedPlayer.Reset();
}

void UGBCutsceneSubsystem::Cancel()
{
	TFunction<void()> Cancelled;
	if (bPlaying)
	{
		bPlaying = false;
		Cancelled = MoveTemp(PlayingOnCancelled);
		PlayingOnCancelled = nullptr;
		PlayingOnFinished = nullptr;
		Lines.Reset();
		LineIndex = 0;
		ReleasePlayer();
		UE_LOG(LogGB, Display, TEXT("Cutscene: '%s' cancelled."), *PlayingId.ToString());
	}
	TArray<FQueued> Dropped = MoveTemp(Queue);
	Queue.Reset();
	if (Cancelled)
	{
		Cancelled();
	}
	for (const FQueued& Entry : Dropped)
	{
		if (Entry.OnCancelled)
		{
			Entry.OnCancelled();
		}
	}
}

void UGBCutsceneSubsystem::Finish()
{
	if (!bPlaying)
	{
		return;
	}
	bPlaying = false;
	const FName FinishedId = PlayingId;
	TFunction<void()> Done = MoveTemp(PlayingOnFinished);
	PlayingOnFinished = nullptr;
	PlayingOnCancelled = nullptr;
	Lines.Reset();
	LineIndex = 0;

	ReleasePlayer();
	UE_LOG(LogGB, Display, TEXT("Cutscene: '%s' finished."), *FinishedId.ToString());

	if (Done)
	{
		Done();
	}
	// The next queued caption starts from Tick (it waits for an era travel the callback may have started).
}

void UGBCutsceneSubsystem::HandleFlagChanged(FName Flag)
{
	UGBProgressSubsystem* Progress = UGBProgressSubsystem::Get(this);
	if (!Progress || !Progress->HasFlag(Flag))
	{
		return; // a flag cleared, not set
	}
	const UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		return;
	}
	// Ids first: no data pointer is held while Play() runs code.
	TArray<FName> ToPlay;
	for (const FGBCutsceneDefinition& Def : Data->GetCutscenes())
	{
		// Once per run: marked as consumed when accepted, so clearing and setting the flag again does not replay it.
		if (!Def.TriggerFlag.IsNone() && Def.TriggerFlag == Flag && Progress->MarkCutsceneConsumed(Def.Id))
		{
			ToPlay.Add(Def.Id);
		}
	}
	for (const FName Id : ToPlay)
	{
		Play(Id);
	}
}
