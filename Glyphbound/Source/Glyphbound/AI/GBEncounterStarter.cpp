#include "AI/GBEncounterStarter.h"

#include "AI/GBEncounter.h"
#include "Player/GBPlayerCharacter.h"
#include "UI/GBHUD.h"
#include "World/GBEraSubsystem.h"

namespace GBEncounterStarterLocal
{
	// Placeholder console size (cm) and color: visuals only.
	const FVector ConsoleSize(70.f, 50.f, 110.f);
	const FLinearColor ConsoleColor(0.55f, 0.2f, 0.9f);
}

AGBEncounterStarter::AGBEncounterStarter()
{
}

void AGBEncounterStarter::BeginPlay()
{
	Super::BeginPlay();
	if (bShowPlaceholder)
	{
		SetPlaceholderLook(false, NAME_None, GBEncounterStarterLocal::ConsoleColor, GBEncounterStarterLocal::ConsoleSize);
	}
	if (!Encounter)
	{
		UE_LOG(LogGB, Warning, TEXT("%s: no Encounter set; it will do nothing."), *GetName());
	}
}

bool AGBEncounterStarter::IsEncounterAvailable() const
{
	if (!Encounter || Encounter->GetState() != EGBEncounterState::Waiting)
	{
		return false;
	}
	const UGBEraSubsystem* Era = UGBEraSubsystem::Get(this);
	return !Era || UGBEraSubsystem::IsPresentIn(Encounter->Presence, Era->GetCurrentEra());
}

bool AGBEncounterStarter::IsReady() const
{
	return IsEncounterAvailable() && Encounter->AreRequiredFlagsMet();
}

bool AGBEncounterStarter::CanInteract(const AGBPlayerCharacter* Player) const
{
	return Super::CanInteract(Player) && IsEncounterAvailable() && Player && !Player->IsDead();
}

void AGBEncounterStarter::Interact(AGBPlayerCharacter* Player)
{
	if (!CanInteract(Player))
	{
		return;
	}
	if (!Encounter->AreRequiredFlagsMet())
	{
		if (AGBHUD* Hud = AGBHUD::Get(this))
		{
			Hud->ShowMessage(LockedText);
		}
		return;
	}
	Encounter->StartEncounter();
}

FString AGBEncounterStarter::GetInteractPrompt(const AGBPlayerCharacter* Player) const
{
	return IsReady() ? Prompt : LockedText;
}
