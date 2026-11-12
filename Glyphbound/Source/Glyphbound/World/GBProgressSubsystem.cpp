#include "World/GBProgressSubsystem.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Player/GBPlayerCharacter.h"

UGBProgressSubsystem* UGBProgressSubsystem::Get(const UObject* WorldContextObject)
{
	if (!WorldContextObject)
	{
		return nullptr;
	}
	if (const UGameInstance* DirectInstance = Cast<UGameInstance>(WorldContextObject))
	{
		return DirectInstance->GetSubsystem<UGBProgressSubsystem>();
	}
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull) : nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGBProgressSubsystem>() : nullptr;
}

bool UGBProgressSubsystem::HasFlag(FName Flag) const
{
	return Flags.Contains(Flag);
}

void UGBProgressSubsystem::SetFlag(FName Flag, bool bValue)
{
	if (Flag.IsNone())
	{
		UE_LOG(LogGB, Warning, TEXT("Progress: SetFlag called with an empty flag name."));
		return;
	}

	const bool bChanged = bValue ? !Flags.Contains(Flag) : Flags.Contains(Flag);
	if (!bChanged)
	{
		return;
	}

	if (bValue)
	{
		Flags.Add(Flag);
	}
	else
	{
		Flags.Remove(Flag);
	}
	UE_LOG(LogGB, Log, TEXT("Progress: flag %s = %s"), *Flag.ToString(), bValue ? TEXT("true") : TEXT("false"));
	OnFlagChanged.Broadcast(Flag);
}

void UGBProgressSubsystem::SaveCheckpoint(AGBPlayerCharacter* Player)
{
	if (!Player)
	{
		UE_LOG(LogGB, Warning, TEXT("Progress: SaveCheckpoint without a player."));
		return;
	}

	FGBPlayerSnapshot Snapshot;
	Player->SaveToSnapshot(Snapshot);
	Snapshot.bValid = true;
	Checkpoint = MoveTemp(Snapshot);
	OnCheckpointSaved.Broadcast();
}

bool UGBProgressSubsystem::RestoreCheckpoint(AGBPlayerCharacter* Player)
{
	if (!Player || !Checkpoint.bValid)
	{
		return false;
	}
	Player->RespawnAt(Checkpoint);
	return true;
}

void UGBProgressSubsystem::ResetProgress()
{
	const TSet<FName> Cleared = MoveTemp(Flags);
	Flags.Reset();
	Checkpoint = FGBPlayerSnapshot();
	for (const FName& Flag : Cleared)
	{
		OnFlagChanged.Broadcast(Flag);
	}
}
