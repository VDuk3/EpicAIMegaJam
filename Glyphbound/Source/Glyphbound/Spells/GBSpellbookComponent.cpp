#include "Spells/GBSpellbookComponent.h"
#include "Core/GBStatNames.h"
#include "Data/GBDataSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/GBPlayerCharacter.h"
#include "QTE/GBQTEComponent.h"
#include "Spells/GBSpellActor.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Weapons/GBEquipmentComponent.h"
#include "World/GBProgressSubsystem.h"

UGBSpellbookComponent::UGBSpellbookComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	Loadout.Init(NAME_None, NumSlots);
}

void UGBSpellbookComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		DataReloadedHandle = Data->OnDataReloaded.AddUObject(this, &UGBSpellbookComponent::ReapplyPassives);
	}
	if (UGBEquipmentComponent* Equipment = GetOwnerEquipment())
	{
		ActiveItemChangedHandle = Equipment->OnActiveItemChanged.AddUObject(this, &UGBSpellbookComponent::HandleActiveItemChanged);
	}
	if (UGBAttributeComponent* Attributes = GetOwnerAttributes())
	{
		DeathHandle = Attributes->OnDeath.AddUObject(this, &UGBSpellbookComponent::HandleOwnerDeath);
	}
}

void UGBSpellbookComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UGBDataSubsystem* Data = UGBDataSubsystem::Get(this))
	{
		Data->OnDataReloaded.Remove(DataReloadedHandle);
	}
	if (UGBEquipmentComponent* Equipment = GetOwnerEquipment())
	{
		Equipment->OnActiveItemChanged.Remove(ActiveItemChangedHandle);
	}
	if (UGBAttributeComponent* Attributes = GetOwnerAttributes())
	{
		Attributes->OnDeath.Remove(DeathHandle);
	}
	CancelSustained();
	Super::EndPlay(EndPlayReason);
}

// ---- Owner components ----

UGBStatsComponent* UGBSpellbookComponent::GetOwnerStats() const
{
	return UGBStatsComponent::Find(GetOwner());
}

UGBAttributeComponent* UGBSpellbookComponent::GetOwnerAttributes() const
{
	return UGBAttributeComponent::Find(GetOwner());
}

UGBEquipmentComponent* UGBSpellbookComponent::GetOwnerEquipment() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UGBEquipmentComponent>() : nullptr;
}

UGBQTEComponent* UGBSpellbookComponent::GetOwnerQTE() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UGBQTEComponent>() : nullptr;
}

double UGBSpellbookComponent::GetNow() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0;
}

// ---- Setup ----

void UGBSpellbookComponent::InitializeFromData()
{
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data)
	{
		UE_LOG(LogGB, Warning, TEXT("Spellbook: no data subsystem, starting with no spells."));
		return;
	}

	CancelSustained();
	RemoveAllPassives();
	KnownSpells.Reset();
	Loadout.Init(NAME_None, NumSlots);
	CooldownEnd.Reset();
	CooldownLength.Reset();

	for (const FGBSpellDefinition& Definition : Data->GetSpells())
	{
		if (Definition.bStartsKnown)
		{
			LearnSpellInternal(Definition.Id, false);
		}
	}

	const FGBPlayerDefinition& Player = Data->GetPlayer();
	for (const FName SpellId : Player.StartingSpells)
	{
		LearnSpellInternal(SpellId, false);
	}

	EnchantSpellId = Player.EnchantSpell;
	EnchantElement = Player.StartingEnchantElement;
	if (!EnchantSpellId.IsNone())
	{
		LearnSpellInternal(EnchantSpellId, false);
	}
	else
	{
		UE_LOG(LogGB, Warning, TEXT("Spellbook: Player.json has no EnchantSpell; the enchant slot is empty."));
	}

	int32 Slot = 0;
	for (const FName SpellId : Player.StartingLoadout)
	{
		if (Slot >= NumSlots)
		{
			break;
		}
		if (IsEquippable(SpellId))
		{
			Loadout[Slot++] = SpellId;
		}
		else
		{
			UE_LOG(LogGB, Warning, TEXT("Spellbook: starting loadout spell '%s' is unknown, not known or not equippable; skipped."), *SpellId.ToString());
		}
	}

	OnLoadoutChanged.Broadcast();
}

// ---- Knowledge ----

bool UGBSpellbookComponent::KnowsSpell(FName SpellId) const
{
	return !SpellId.IsNone() && KnownSpells.Contains(SpellId);
}

bool UGBSpellbookComponent::LearnSpell(FName SpellId)
{
	const bool bNew = LearnSpellInternal(SpellId, true);
	if (bNew)
	{
		OnLoadoutChanged.Broadcast();
	}
	return bNew;
}

bool UGBSpellbookComponent::LearnSpellInternal(FName SpellId, bool bAutoEquip)
{
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	if (!Definition)
	{
		UE_LOG(LogGB, Warning, TEXT("Spellbook: cannot learn unknown spell '%s'."), *SpellId.ToString());
		return false;
	}
	if (KnownSpells.Contains(SpellId))
	{
		return false;
	}
	KnownSpells.Add(SpellId);

	if (Definition->bPassive)
	{
		ApplyPassive(SpellId);
	}
	else if (bAutoEquip && IsEquippable(SpellId) && !Loadout.Contains(SpellId))
	{
		// A newly learned spell goes into the first free slot, so it can be used without opening the loadout.
		const int32 FreeSlot = Loadout.IndexOfByKey(NAME_None);
		if (FreeSlot != INDEX_NONE)
		{
			Loadout[FreeSlot] = SpellId;
		}
	}
	return true;
}

int32 UGBSpellbookComponent::LearnGroup(FName UnlockGroup)
{
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	if (!Data || UnlockGroup.IsNone())
	{
		return 0;
	}
	TArray<FName> GroupSpells;
	for (const FGBSpellDefinition& Definition : Data->GetSpells())
	{
		if (Definition.UnlockGroup == UnlockGroup)
		{
			GroupSpells.Add(Definition.Id);
		}
	}
	int32 Learned = 0;
	for (const FName SpellId : GroupSpells)
	{
		if (LearnSpellInternal(SpellId, true))
		{
			++Learned;
		}
	}
	if (Learned > 0)
	{
		OnLoadoutChanged.Broadcast();
	}
	return Learned;
}

// ---- Passives ----

FName UGBSpellbookComponent::PassiveSource(FName SpellId)
{
	return FName(*FString::Printf(TEXT("Passive.%s"), *SpellId.ToString()));
}

void UGBSpellbookComponent::ApplyPassive(FName SpellId)
{
	UGBStatsComponent* Stats = GetOwnerStats();
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	if (!Stats)
	{
		return;
	}
	if (!Definition || !Definition->bPassive)
	{
		Stats->RemoveModifiers(PassiveSource(SpellId));
		return;
	}
	Stats->AddModifiers(PassiveSource(SpellId), Definition->PassiveModifiers);
}

void UGBSpellbookComponent::RemoveAllPassives()
{
	if (UGBStatsComponent* Stats = GetOwnerStats())
	{
		for (const FName SpellId : KnownSpells)
		{
			Stats->RemoveModifiers(PassiveSource(SpellId));
		}
	}
}

void UGBSpellbookComponent::ReapplyPassives()
{
	for (const FName SpellId : KnownSpells)
	{
		ApplyPassive(SpellId);
	}
}

// ---- Loadout ----

bool UGBSpellbookComponent::IsEquippable(FName SpellId) const
{
	if (SpellId.IsNone() || SpellId == EnchantSpellId || !KnowsSpell(SpellId))
	{
		return false;
	}
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	return Definition && !Definition->bPassive && !Definition->bRequiresGun && !Definition->ActorClass.IsEmpty();
}

FName UGBSpellbookComponent::GetSlotSpell(int32 Slot) const
{
	return Loadout.IsValidIndex(Slot) ? Loadout[Slot] : NAME_None;
}

bool UGBSpellbookComponent::EquipSpell(int32 Slot, FName SpellId)
{
	if (Slot < 0 || Slot >= NumSlots)
	{
		return false;
	}
	if (Loadout.Num() != NumSlots)
	{
		Loadout.SetNum(NumSlots);
	}
	if (SpellId.IsNone())
	{
		Loadout[Slot] = NAME_None;
		OnLoadoutChanged.Broadcast();
		return true;
	}
	if (!IsEquippable(SpellId))
	{
		return false;
	}
	for (int32 Other = 0; Other < NumSlots; ++Other)
	{
		if (Other != Slot && Loadout[Other] == SpellId)
		{
			Loadout[Other] = NAME_None;
		}
	}
	Loadout[Slot] = SpellId;
	OnLoadoutChanged.Broadcast();
	return true;
}

void UGBSpellbookComponent::SetEnchantElement(EGBElement InElement)
{
	if (EnchantElement != InElement)
	{
		EnchantElement = InElement;
		OnLoadoutChanged.Broadcast();
	}
}

// ---- Casting ----

bool UGBSpellbookComponent::IsBusy() const
{
	if (!PendingSpellId.IsNone())
	{
		return true;
	}
	if (const UGBQTEComponent* QTE = GetOwnerQTE())
	{
		if (QTE->IsActive())
		{
			return true;
		}
	}
	return IsValid(ActiveSustained) && ActiveSustained->IsSustaining();
}

EGBCastFailReason UGBSpellbookComponent::CanCast(FName SpellId) const
{
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	if (!Definition || Definition->bPassive)
	{
		return EGBCastFailReason::UnknownSpell;
	}
	if (!KnowsSpell(SpellId))
	{
		return EGBCastFailReason::NotKnown;
	}
	const UGBEquipmentComponent* Equipment = GetOwnerEquipment();
	const bool bRightItem = Equipment && (Definition->bRequiresGun ? Equipment->IsHoldingGun() : Equipment->IsHoldingStaff());
	if (!bRightItem)
	{
		return EGBCastFailReason::WrongHeldItem;
	}
	if (IsBusy())
	{
		return EGBCastFailReason::Busy;
	}
	if (GetCooldownRemaining(SpellId) > 0.f)
	{
		return EGBCastFailReason::OnCooldown;
	}
	const UGBAttributeComponent* Attributes = GetOwnerAttributes();
	const float Cost = GetManaCost(SpellId);
	if (Cost > 0.f && (!Attributes || !Attributes->HasMana(Cost)))
	{
		return EGBCastFailReason::NotEnoughMana;
	}
	return EGBCastFailReason::None;
}

bool UGBSpellbookComponent::TryCastSlot(int32 Slot)
{
	const FName SpellId = GetSlotSpell(Slot);
	if (!SpellId.IsNone() && IsValid(ActiveSustained) && ActiveSustained->GetSpellCast().SpellId == SpellId)
	{
		ActiveSustained->OnCastReleased();
		return true;
	}
	if (SpellId.IsNone())
	{
		OnCastFailed.Broadcast(NAME_None, EGBCastFailReason::EmptySlot);
		return false;
	}
	return TryCastSpell(SpellId);
}

bool UGBSpellbookComponent::TryCastEnchant()
{
	if (EnchantSpellId.IsNone())
	{
		OnCastFailed.Broadcast(NAME_None, EGBCastFailReason::UnknownSpell);
		return false;
	}
	return TryCastSpell(EnchantSpellId);
}

bool UGBSpellbookComponent::TryCastSpell(FName SpellId)
{
	const EGBCastFailReason Reason = CanCast(SpellId);
	if (Reason != EGBCastFailReason::None)
	{
		OnCastFailed.Broadcast(SpellId, Reason);
		return false;
	}
	return BeginCast(SpellId);
}

bool UGBSpellbookComponent::BeginCast(FName SpellId)
{
	const float Cost = GetManaCost(SpellId);
	UGBAttributeComponent* Attributes = GetOwnerAttributes();
	if (Cost > 0.f && (!Attributes || !Attributes->SpendMana(Cost)))
	{
		OnCastFailed.Broadcast(SpellId, EGBCastFailReason::NotEnoughMana);
		return false;
	}

	UGBEquipmentComponent* Equipment = GetOwnerEquipment();
	if (Equipment)
	{
		Equipment->SetSwitchLocked(true);
	}
	PendingSpellId = SpellId;

	UGBQTEComponent* QTE = GetOwnerQTE();
	if (!QTE)
	{
		// No QTE on the owner: cast at default strength, as when the QTE is switched off.
		FGBQTEResult Result;
		Result.bSuccess = true;
		Result.bUsedQTE = false;
		FinishCast(SpellId, Result);
		return true;
	}

	// The delegate may fire before StartQTE returns (QTE off).
	if (!QTE->StartQTE(SpellId, FGBOnQTEFinished::CreateUObject(this, &UGBSpellbookComponent::HandleQTEFinished, SpellId)))
	{
		PendingSpellId = NAME_None;
		if (Equipment)
		{
			Equipment->SetSwitchLocked(false);
		}
		if (Attributes && Cost > 0.f)
		{
			Attributes->RestoreMana(Cost);
		}
		OnCastFailed.Broadcast(SpellId, EGBCastFailReason::Busy);
		return false;
	}
	return true;
}

void UGBSpellbookComponent::HandleQTEFinished(const FGBQTEResult& Result, FName SpellId)
{
	FinishCast(SpellId, Result);
}

void UGBSpellbookComponent::FinishCast(FName SpellId, const FGBQTEResult& Result)
{
	PendingSpellId = NAME_None;
	if (UGBEquipmentComponent* Equipment = GetOwnerEquipment())
	{
		Equipment->SetSwitchLocked(false);
	}

	// The owner died while the QTE was running (or the cast outlived it): never spawn from a dead caster.
	bool bOwnerDead = false;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(GetOwner()))
	{
		bOwnerDead = Player->IsDead();
	}
	else if (const UGBAttributeComponent* Attributes = GetOwnerAttributes())
	{
		bOwnerDead = Attributes->IsDead();
	}
	if (bOwnerDead)
	{
		return;
	}

	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	if (!Definition)
	{
		OnCastFailed.Broadcast(SpellId, EGBCastFailReason::UnknownSpell);
		return;
	}

	UGBStatsComponent* Stats = GetOwnerStats();
	StartCooldown(SpellId, GBSpell::ResolveParam(*Definition, GBStat::Cooldown, Result.Power, Result.Reach, Result.bUsedQTE, Stats, 0.f));

	if (!Result.bSuccess)
	{
		OnCastFailed.Broadcast(SpellId, EGBCastFailReason::QTEFailed);
		return;
	}

	FGBSpellCast SpellCast = GBSpell::ResolveCast(*Definition, Result.Power, Result.Reach, Result.bUsedQTE, Stats);
	AActor* Owner = GetOwner();
	SpellCast.Caster = Owner;
	if (const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Owner))
	{
		SpellCast.Origin = Player->GetAimOrigin();
		SpellCast.Direction = Player->GetAimDirection();
	}
	else if (Owner)
	{
		FRotator EyeRotation;
		Owner->GetActorEyesViewPoint(SpellCast.Origin, EyeRotation);
		SpellCast.Direction = EyeRotation.Vector();
	}
	if (SpellId == EnchantSpellId)
	{
		SpellCast.Element = EnchantElement;
	}

	if (Definition->ActorClass.IsEmpty())
	{
		UE_LOG(LogGB, Error, TEXT("Spellbook: spell '%s' has no ActorClass in Spells.json."), *SpellId.ToString());
		return;
	}
	UClass* BehaviorClass = LoadClass<AGBSpellActor>(nullptr, *Definition->ActorClass);
	UWorld* World = GetWorld();
	if (!BehaviorClass || !World)
	{
		UE_LOG(LogGB, Error, TEXT("Spellbook: spell '%s' ActorClass '%s' is not a GBSpellActor class."), *SpellId.ToString(), *Definition->ActorClass);
		return;
	}

	const FTransform SpawnTransform(SpellCast.Direction.Rotation(), SpellCast.Origin);
	AGBSpellActor* SpellActor = World->SpawnActorDeferred<AGBSpellActor>(BehaviorClass, SpawnTransform, Owner,
		Cast<APawn>(Owner), ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpellActor)
	{
		UE_LOG(LogGB, Error, TEXT("Spellbook: could not spawn the behavior of spell '%s'."), *SpellId.ToString());
		return;
	}
	SpellActor->InitSpell(SpellCast);
	SpellActor->FinishSpawning(SpawnTransform);

	if (IsValid(SpellActor) && SpellActor->IsSustaining())
	{
		ActiveSustained = SpellActor;
		SpellActor->OnSpellEnded.AddUObject(this, &UGBSpellbookComponent::HandleSustainedEnded);
	}
	OnSpellCast.Broadcast(SpellCast);
}

void UGBSpellbookComponent::HandleSustainedEnded()
{
	ActiveSustained = nullptr;
}

void UGBSpellbookComponent::HandleActiveItemChanged(AGBHeldItem* NewItem)
{
	CancelSustained();
}

void UGBSpellbookComponent::HandleOwnerDeath(const FGBDamageInfo& KillingBlow)
{
	CancelSustained();
}

void UGBSpellbookComponent::CancelSustained()
{
	if (IsValid(ActiveSustained))
	{
		AGBSpellActor* Sustained = ActiveSustained;
		ActiveSustained = nullptr;
		Sustained->OnCastReleased();
	}
	ActiveSustained = nullptr;
}

// ---- Cooldowns and cost ----

void UGBSpellbookComponent::StartCooldown(FName SpellId, float Seconds)
{
	if (Seconds <= 0.f)
	{
		CooldownEnd.Remove(SpellId);
		CooldownLength.Remove(SpellId);
		return;
	}
	CooldownEnd.Add(SpellId, GetNow() + Seconds);
	CooldownLength.Add(SpellId, Seconds);
}

float UGBSpellbookComponent::GetCooldownRemaining(FName SpellId) const
{
	const double* End = CooldownEnd.Find(SpellId);
	return End ? static_cast<float>(FMath::Max(0.0, *End - GetNow())) : 0.f;
}

float UGBSpellbookComponent::GetCooldownDuration(FName SpellId) const
{
	return CooldownLength.FindRef(SpellId);
}

float UGBSpellbookComponent::GetManaCost(FName SpellId) const
{
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	const FGBSpellDefinition* Definition = Data ? Data->FindSpell(SpellId) : nullptr;
	if (!Definition)
	{
		return 0.f;
	}
	return FMath::Max(0.f, GBSpell::ResolveParam(*Definition, GBStat::ManaCost, 0.f, 0.f, false, GetOwnerStats(), 0.f));
}

// ---- Snapshot ----

void UGBSpellbookComponent::SaveToSnapshot(FGBPlayerSnapshot& Snapshot) const
{
	Snapshot.KnownSpells = KnownSpells;
	Snapshot.Loadout = Loadout;
	Snapshot.EnchantElement = EnchantElement;
}

void UGBSpellbookComponent::LoadFromSnapshot(const FGBPlayerSnapshot& Snapshot)
{
	CancelSustained();
	RemoveAllPassives();

	KnownSpells.Reset();
	UGBDataSubsystem* Data = UGBDataSubsystem::Get(this);
	for (const FName SpellId : Snapshot.KnownSpells)
	{
		if (Data && Data->FindSpell(SpellId))
		{
			KnownSpells.AddUnique(SpellId);
		}
	}
	if (!EnchantSpellId.IsNone() && Data && Data->FindSpell(EnchantSpellId))
	{
		KnownSpells.AddUnique(EnchantSpellId);
	}

	Loadout.Init(NAME_None, NumSlots);
	for (int32 Slot = 0; Slot < NumSlots && Slot < Snapshot.Loadout.Num(); ++Slot)
	{
		const FName SpellId = Snapshot.Loadout[Slot];
		if (IsEquippable(SpellId) && !Loadout.Contains(SpellId))
		{
			Loadout[Slot] = SpellId;
		}
	}
	EnchantElement = Snapshot.EnchantElement;

	CooldownEnd.Reset();
	CooldownLength.Reset();
	ReapplyPassives();
	OnLoadoutChanged.Broadcast();
}
