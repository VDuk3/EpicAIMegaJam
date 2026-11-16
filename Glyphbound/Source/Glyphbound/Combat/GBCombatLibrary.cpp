#include "Combat/GBCombatLibrary.h"

#include "AI/GBEnemyCharacter.h"
#include "Combat/GBElementalStatusComponent.h"
#include "Combat/GBStatusEffectComponent.h"
#include "Core/GBStatNames.h"
#include "Player/GBPlayerCharacter.h"
#include "Weapons/GBEquipmentComponent.h"
#include "Weapons/GBHeldItem.h"
#include "Stats/GBAttributeComponent.h"
#include "Stats/GBStatsComponent.h"
#include "Engine/Engine.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "EngineUtils.h"
#include "AI/GBValkyrieEnemy.h"

// ---- GBAudio: data-driven sound playback (Content/Data/Audio.json, written by Tools/Jam/audio_defs.js) ----
// Callers declare what they use by hand (no header, so Live Coding can apply it):
//   namespace GBAudio { bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul); ... }
namespace GBAudioImpl
{
	struct FEntry
	{
		TArray<FSoftObjectPath> Files;
		float Volume = 1.f, PitchMin = 1.f, PitchMax = 1.f, Range = 0.f, Cooldown = 0.f;
		double LastPlay = -1e9;
		int32 LastIndex = -1;
	};
	TMap<FName, FEntry> Table;
	TMap<FString, FName> MapMusic, MapAmbience; // map name part -> music key / ambience key
	bool bLoaded = false;
	struct FZone { FString Map; FBox Box; FName Music; };
	TArray<FZone> Zones; // a box of a map with its own music (Audio.json "Zones")
	TMap<int32, TStrongObjectPtr<USoundAttenuation>> Attenuations;
	TWeakObjectPtr<UAudioComponent> MusicComp, AmbComp;
	FName CurMusic, CurAmb;
	TWeakObjectPtr<UWorld> MusicWorld;
	double LastMenuPing = -1e9;

	void Load()
	{
		if (bLoaded)
		{
			return;
		}
		bLoaded = true;
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectContentDir() / TEXT("Data/Audio.json"))))
		{
			return;
		}
		TSharedPtr<FJsonObject> Root;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid())
		{
			return;
		}
		const TSharedPtr<FJsonObject>* Sounds = nullptr;
		if (Root->TryGetObjectField(TEXT("Sounds"), Sounds))
		{
			for (const auto& Pair : (*Sounds)->Values)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (!Pair.Value->TryGetObject(Obj))
				{
					continue;
				}
				FEntry& Entry = Table.Add(FName(*Pair.Key));
				const TArray<TSharedPtr<FJsonValue>>* Files = nullptr;
				if ((*Obj)->TryGetArrayField(TEXT("Files"), Files))
				{
					for (const TSharedPtr<FJsonValue>& File : *Files)
					{
						Entry.Files.Add(FSoftObjectPath(File->AsString()));
					}
				}
				double Value = 0;
				if ((*Obj)->TryGetNumberField(TEXT("Volume"), Value)) { Entry.Volume = Value; }
				if ((*Obj)->TryGetNumberField(TEXT("PitchMin"), Value)) { Entry.PitchMin = Value; }
				if ((*Obj)->TryGetNumberField(TEXT("PitchMax"), Value)) { Entry.PitchMax = Value; }
				if ((*Obj)->TryGetNumberField(TEXT("Range"), Value)) { Entry.Range = Value; }
				if ((*Obj)->TryGetNumberField(TEXT("Cooldown"), Value)) { Entry.Cooldown = Value; }
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* ZoneList = nullptr;
		if (Root->TryGetArrayField(TEXT("Zones"), ZoneList))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ZoneList)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Min = nullptr;
				const TArray<TSharedPtr<FJsonValue>>* Max = nullptr;
				if (!Value->TryGetObject(Obj) || !(*Obj)->TryGetArrayField(TEXT("Min"), Min) || !(*Obj)->TryGetArrayField(TEXT("Max"), Max) || Min->Num() < 3 || Max->Num() < 3)
				{
					continue;
				}
				FZone Zone;
				(*Obj)->TryGetStringField(TEXT("Map"), Zone.Map);
				FString MusicKey;
				(*Obj)->TryGetStringField(TEXT("Music"), MusicKey);
				Zone.Music = FName(*MusicKey);
				Zone.Box = FBox(FVector((*Min)[0]->AsNumber(), (*Min)[1]->AsNumber(), (*Min)[2]->AsNumber()), FVector((*Max)[0]->AsNumber(), (*Max)[1]->AsNumber(), (*Max)[2]->AsNumber()));
				Zones.Add(Zone);
			}
		}
		const TSharedPtr<FJsonObject>* Maps = nullptr;
		if (Root->TryGetObjectField(TEXT("Maps"), Maps))
		{
			for (const auto& Pair : (*Maps)->Values)
			{
				const TSharedPtr<FJsonObject>* Obj = nullptr;
				if (Pair.Value->TryGetObject(Obj))
				{
					FString Music, Amb;
					(*Obj)->TryGetStringField(TEXT("Music"), Music);
					(*Obj)->TryGetStringField(TEXT("Ambience"), Amb);
					MapMusic.Add(FString(Pair.Key), FName(*Music));
					MapAmbience.Add(FString(Pair.Key), FName(*Amb));
				}
			}
		}
	}

	USoundBase* Pick(FEntry& Entry)
	{
		if (Entry.Files.Num() == 0)
		{
			return nullptr;
		}
		int32 Index = FMath::RandRange(0, Entry.Files.Num() - 1);
		if (Entry.Files.Num() > 1 && Index == Entry.LastIndex)
		{
			Index = (Index + 1) % Entry.Files.Num();
		}
		Entry.LastIndex = Index;
		return Cast<USoundBase>(Entry.Files[Index].TryLoad());
	}

	USoundAttenuation* GetAttenuation(float Range)
	{
		const int32 Key = FMath::RoundToInt(Range);
		TStrongObjectPtr<USoundAttenuation>& Slot = Attenuations.FindOrAdd(Key);
		if (!Slot.IsValid())
		{
			USoundAttenuation* A = NewObject<USoundAttenuation>(GetTransientPackage());
			A->Attenuation.bAttenuate = true;
			A->Attenuation.bSpatialize = true;
			A->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			A->Attenuation.AttenuationShapeExtents = FVector(Range * 0.2f, 0.f, 0.f);
			A->Attenuation.FalloffDistance = Range * 0.8f;
			A->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Linear;
			Slot.Reset(A);
		}
		return Slot.Get();
	}

	void Crossfade(UWorld* World, TWeakObjectPtr<UAudioComponent>& Comp, FName& Current, FName Want, float FadeIn)
	{
		if (Want == Current)
		{
			return;
		}
		if (UAudioComponent* Old = Comp.Get())
		{
			Old->FadeOut(1.5f, 0.f);
		}
		Comp.Reset();
		Current = Want;
		FEntry* Entry = Table.Find(Want);
		if (Want.IsNone() || !Entry)
		{
			return;
		}
		if (USoundBase* Sound = Pick(*Entry))
		{
			if (UAudioComponent* New = UGameplayStatics::CreateSound2D(World, Sound, Entry->Volume, 1.f, 0.f, nullptr, false, true))
			{
				New->bIsUISound = true; // keeps playing while the game is paused (menus)
				New->FadeIn(FadeIn, 1.f);
				Comp = New;
			}
		}
	}
}

namespace GBAudio
{
	bool Play(const UObject* Ctx, FName Key, const FVector* Loc, float PitchMul, float VolMul)
	{
		using namespace GBAudioImpl;
		Load();
		FEntry* Entry = Table.Find(Key);
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!Entry || !World || Entry->Files.Num() == 0)
		{
			return false;
		}
		const double Now = World->GetRealTimeSeconds();
		if (Now - Entry->LastPlay < Entry->Cooldown)
		{
			return true;
		}
		Entry->LastPlay = Now;
		USoundBase* Sound = Pick(*Entry);
		if (!Sound)
		{
			return true;
		}
		const float Pitch = FMath::FRandRange(Entry->PitchMin, FMath::Max(Entry->PitchMin, Entry->PitchMax)) * PitchMul;
		const bool bVaries = Entry->PitchMax > Entry->PitchMin;
		const float Volume = Entry->Volume * VolMul * (bVaries ? FMath::FRandRange(0.88f, 1.f) : 1.f);
		if (Entry->Range <= 0.f || !Loc)
		{
			UGameplayStatics::PlaySound2D(World, Sound, Volume, Pitch);
		}
		else
		{
			UGameplayStatics::PlaySoundAtLocation(World, Sound, *Loc, FRotator::ZeroRotator, Volume, Pitch, 0.f, GetAttenuation(Entry->Range));
		}
		return true;
	}

	/** A looping 3D sound at a place (torches). The component dies with its world. */
	void SpawnLoop(const UObject* Ctx, FName Key, const FVector& Loc)
	{
		using namespace GBAudioImpl;
		Load();
		FEntry* Entry = Table.Find(Key);
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (!Entry || !World)
		{
			return;
		}
		if (USoundBase* Sound = Pick(*Entry))
		{
			UGameplayStatics::SpawnSoundAtLocation(World, Sound, Loc, FRotator::ZeroRotator, Entry->Volume, FMath::FRandRange(0.92f, 1.08f),
				FMath::FRandRange(0.f, 20.f), GetAttenuation(Entry->Range > 0.f ? Entry->Range : 1200.f));
		}
	}

	bool IsFutureMap(const UObject* Ctx)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		const FString Name = World ? World->GetMapName() : FString();
		return Name.Contains(TEXT("Future")) || Name.Contains(TEXT("Entity"));
	}

	void MenuPing(const UObject* Ctx)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Ctx, EGetWorldErrorMode::ReturnNull) : nullptr;
		if (World)
		{
			GBAudioImpl::LastMenuPing = World->GetRealTimeSeconds();
		}
	}

	/** Called about twice a second by the game mode: picks the music (menu / boss / map) and the ambience, crossfading on change. */
	void UpdateMusic(UWorld* World)
	{
		using namespace GBAudioImpl;
		Load();
		if (!World)
		{
			return;
		}
		if (MusicWorld.Get() != World)
		{
			MusicWorld = World;
			MusicComp.Reset();
			AmbComp.Reset();
			CurMusic = NAME_None;
			CurAmb = NAME_None;
		}
		FName WantMusic, WantAmb;
		const FString MapName = World->GetMapName();
		for (const auto& Pair : MapMusic)
		{
			if (MapName.Contains(Pair.Key))
			{
				WantMusic = Pair.Value;
				if (const FName* Amb = MapAmbience.Find(Pair.Key)) { WantAmb = *Amb; }
				break;
			}
		}
		const bool bMenu = World->GetRealTimeSeconds() - LastMenuPing < 0.6;
		bool bInZone = false;
		if (!bMenu)
		{
			const APlayerController* ZonePC = World->GetFirstPlayerController();
			const APawn* ZonePawn = ZonePC ? ZonePC->GetPawn() : nullptr;
			for (const FZone& Zone : Zones)
			{
				if (ZonePawn && MapName.Contains(Zone.Map) && Zone.Box.IsInside(ZonePawn->GetActorLocation()))
				{
					WantMusic = Zone.Music;
					bInZone = true;
					break;
				}
			}
		}
		if (bMenu)
		{
			WantMusic = FName(TEXT("music_menu"));
		}
		else if (!bInZone && !MapName.Contains(TEXT("Entity")))
		{
			// A Valkyrie alive and near the player: her fight's music.
			const APlayerController* PC = World->GetFirstPlayerController();
			const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
			if (Pawn)
			{
				for (TActorIterator<AGBValkyrieEnemy> It(World); It; ++It)
				{
					if (UGBCombatLibrary::IsAlive(*It) && FVector::DistSquared(It->GetActorLocation(), Pawn->GetActorLocation()) < FMath::Square(5500.f))
					{
						WantMusic = FName(IsFutureMap(World) ? TEXT("music_boss_future") : TEXT("music_boss_past"));
						break;
					}
				}
			}
		}
		// Combat tracks start about two seconds after the player enters the zone (no combat-start system exists), not at once.
		static FName PendingMusic;
		static double PendingSince = 0.0;
		if (WantMusic != CurMusic)
		{
			const double NowReal = World->GetRealTimeSeconds();
			if (WantMusic != PendingMusic)
			{
				PendingMusic = WantMusic;
				PendingSince = NowReal;
			}
			const bool bCombatTrack = WantMusic == FName(TEXT("music_boss_past")) || WantMusic == FName(TEXT("music_boss_future")) || WantMusic == FName(TEXT("music_entity")) || bInZone;
			if (bCombatTrack && NowReal - PendingSince < 2.0)
			{
				WantMusic = CurMusic;
			}
		}
		else
		{
			PendingMusic = NAME_None;
		}
		if (WantMusic != CurMusic && (WantMusic == FName(TEXT("music_boss_past")) || WantMusic == FName(TEXT("music_boss_future"))))
		{
			Play(World, FName(TEXT("boss_roar")), nullptr, 1.f, 1.f);
		}
		Crossfade(World, MusicComp, CurMusic, WantMusic, 2.0f);
		Crossfade(World, AmbComp, CurAmb, WantAmb, 3.0f);
	}
}

namespace GBCombatLocal
{
	/** The obstruction trace starts this far above the explosion origin, so the floor under it does not block it. */
	constexpr float RadialTraceLiftCm = 5.f;

	/** Weapons.json staff stats (fractions): bonus to every hit of the staff's own element / to every hit of any element. */
	const FName StaffElementDamageBonus(TEXT("ElementDamageBonus"));
	const FName StaffAllDamageBonus(TEXT("AllDamageBonus"));

	/**
	 * Damage multiplier from the staff the player is holding right now (active item in the Staff slot):
	 * (1 + AllDamageBonus) * (1 + ElementDamageBonus if the hit's element is the staff's element).
	 * Covers everything that goes through DealDamage while it is held: bolts, splashes, spells, effects over time.
	 */
	float HeldStaffDamageMult(const AActor* Instigator, EGBElement HitElement)
	{
		const AGBPlayerCharacter* Player = Cast<AGBPlayerCharacter>(Instigator);
		const UGBEquipmentComponent* Equipment = Player ? Player->GetEquipment() : nullptr;
		const AGBHeldItem* Item = Equipment ? Equipment->GetActiveItem() : nullptr;
		if (!Item || Item->GetSlot() != EGBHeldSlot::Staff)
		{
			return 1.f;
		}
		float Mult = 1.f + FMath::Max(0.f, Item->GetStat(StaffAllDamageBonus, 0.f));
		if (HitElement == Item->GetElement())
		{
			Mult *= 1.f + FMath::Max(0.f, Item->GetStat(StaffElementDamageBonus, 0.f));
		}
		return Mult;
	}
}

FGBOnDamageDealt& UGBCombatLibrary::OnDamageDealt()
{
	static FGBOnDamageDealt Event;
	return Event;
}

FGBOnShotTraced& UGBCombatLibrary::OnShotTraced()
{
	static FGBOnShotTraced Event;
	return Event;
}

float UGBCombatLibrary::DealDamage(AActor* Target, const FGBDamageInfo& Info, bool bAllowFriendlyFire)
{
	if (!IsValid(Target))
	{
		return 0.f;
	}

	UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Target);
	if (!Attributes)
	{
		return 0.f;
	}

	AActor* DamageInstigator = Info.Instigator.Get();
	if (!IsValid(DamageInstigator))
	{
		DamageInstigator = nullptr;
	}

	if (!CanDamage(DamageInstigator, Target))
	{
		// Friendly fire explicitly allows same-team hits; anything without attributes or dead stays immune.
		if (!bAllowFriendlyFire || !UGBAttributeComponent::Find(Target) || !IsAlive(Target))
		{
			return 0.f;
		}
	}

	FGBDamageInfo FinalInfo = Info;
	FinalInfo.Instigator = DamageInstigator;
	if (DamageInstigator)
	{
		if (const UGBStatsComponent* InstigatorStats = UGBStatsComponent::Find(DamageInstigator))
		{
			FinalInfo.Amount *= InstigatorStats->GetStat(GBStat::DamageDealt, 1.f);
		}
		if (Target != DamageInstigator)
		{
			FinalInfo.Amount *= GBCombatLocal::HeldStaffDamageMult(DamageInstigator, FinalInfo.Element);
		}
	}

	if (FinalInfo.Amount <= 0.f)
	{
		return 0.f;
	}

	const float Dealt = Attributes->ApplyDamage(FinalInfo);
	if (Dealt > 0.f)
	{
		OnDamageDealt().Broadcast(Target, FinalInfo, Dealt);
		ApplyBuildUp(Target, FinalInfo);
		{
			// Hit sound at the point of impact: the player's pain, a sword's steel, an element's crackle, or flesh / machinery.
			const FVector At = FinalInfo.HitLocation.IsNearlyZero() ? Target->GetActorLocation() : FinalInfo.HitLocation;
			if (Cast<AGBPlayerCharacter>(Target))
			{
				GBAudio::Play(Target, FName(TEXT("player_hurt")), nullptr, 1.f, 1.f);
			}
			else
			{
				const FString Source = FinalInfo.SourceId.ToString();
				FName Key = FName(TEXT("hit_flesh"));
				if (Source.Contains(TEXT("Sword")) || Source.Contains(TEXT("Katana")))
				{
					Key = FName(TEXT("hit_sword"));
				}
				else if (FinalInfo.Element == EGBElement::Lightning)
				{
					Key = FName(TEXT("hit_Lightning"));
				}
				else if (FinalInfo.Element == EGBElement::Fire)
				{
					Key = FName(TEXT("hit_Fire"));
				}
				else if (FinalInfo.Element == EGBElement::Ice)
				{
					Key = FName(TEXT("hit_Ice"));
				}
				else if (GBAudio::IsFutureMap(Target))
				{
					Key = FName(TEXT("hit_robot"));
				}
				GBAudio::Play(Target, Key, &At, 1.f, 1.f);
			}
		}
	}

	if (Dealt > 0.f && !FinalInfo.OnHitEffect.IsNone())
	{
		if (UGBStatusEffectComponent* Effects = UGBStatusEffectComponent::Find(Target))
		{
			Effects->ApplyEffect(FinalInfo.OnHitEffect, DamageInstigator, FinalInfo.EffectDurationScale, FinalInfo.EffectStrengthScale);
		}
	}

	return Dealt;
}

FVector UGBCombatLibrary::GetDamageTargetLocation(const AActor* Target, const FVector& FromPoint)
{
	if (const AGBEnemyCharacter* Enemy = Cast<AGBEnemyCharacter>(Target))
	{
		return Enemy->GetDamageTargetLocation(FromPoint);
	}
	return Target ? Target->GetActorLocation() : FromPoint;
}

int32 UGBCombatLibrary::DealRadialDamage(const UObject* WorldContextObject, const FVector& Origin, float Radius, const FGBDamageInfo& Info,
	TConstArrayView<AActor*> IgnoreActors, bool bLinearFalloff)
{
	if (Radius <= 0.f || !GEngine)
	{
		return 0;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (World && Radius >= 120.f)
	{
		GBAudio::Play(WorldContextObject, FName(TEXT("explosion")), &Origin, 1.f, 1.f);
	}
	if (!World)
	{
		return 0;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(GBRadialDamage), false);
	for (AActor* Ignored : IgnoreActors)
	{
		if (Ignored)
		{
			QueryParams.AddIgnoredActor(Ignored);
		}
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Radius), QueryParams);

	AActor* DamageInstigator = Info.Instigator.Get();
	if (!IsValid(DamageInstigator))
	{
		DamageInstigator = nullptr;
	}

	TSet<AActor*> Visited;
	int32 DamagedCount = 0;

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!IsValid(Target) || Visited.Contains(Target))
		{
			continue;
		}
		Visited.Add(Target);

		if (IgnoreActors.Contains(Target) || !UGBAttributeComponent::Find(Target))
		{
			continue;
		}

		if (!CanDamage(DamageInstigator, Target))
		{
			continue;
		}

		const FVector TargetLocation = GetDamageTargetLocation(Target, Origin);
		const float Distance = FVector::Dist(TargetLocation, Origin);
		if (Distance > Radius)
		{
			continue;
		}

		// Solid geometry between the blast and the target blocks the damage.
		FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(GBRadialObstruction), true);
		TraceParams.AddIgnoredActor(Target);
		for (AActor* Ignored : IgnoreActors)
		{
			if (Ignored)
			{
				TraceParams.AddIgnoredActor(Ignored);
			}
		}
		FHitResult Obstruction;
		const FVector TraceStart = Origin + FVector(0.f, 0.f, GBCombatLocal::RadialTraceLiftCm);
		if (World->LineTraceSingleByChannel(Obstruction, TraceStart, TargetLocation, ECC_Visibility, TraceParams)
			&& Obstruction.bBlockingHit && Obstruction.GetActor() && Obstruction.GetActor() != Target)
		{
			continue;
		}

		FGBDamageInfo TargetInfo = Info;
		TargetInfo.HitLocation = TargetLocation;
		TargetInfo.HitDirection = (TargetLocation - Origin).GetSafeNormal();
		if (bLinearFalloff)
		{
			TargetInfo.Amount *= FMath::Clamp(1.f - Distance / Radius, 0.f, 1.f);
		}

		if (DealDamage(Target, TargetInfo) > 0.f)
		{
			++DamagedCount;
		}
	}

	return DamagedCount;
}

EGBTeam UGBCombatLibrary::GetTeam(const AActor* Actor)
{
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Actor);
	return Attributes ? Attributes->Team : EGBTeam::Neutral;
}

bool UGBCombatLibrary::AreHostile(const AActor* A, const AActor* B)
{
	if (!A || !B)
	{
		return false;
	}
	return AreTeamsHostile(GetTeam(A), GetTeam(B));
}

bool UGBCombatLibrary::AreTeamsHostile(EGBTeam A, EGBTeam B)
{
	return (A == EGBTeam::Player && B == EGBTeam::Enemy) || (A == EGBTeam::Enemy && B == EGBTeam::Player);
}

bool UGBCombatLibrary::IsAlive(const AActor* Actor)
{
	if (!IsValid(Actor))
	{
		return false;
	}
	const UGBAttributeComponent* Attributes = UGBAttributeComponent::Find(Actor);
	return !Attributes || !Attributes->IsDead();
}

bool UGBCombatLibrary::CanDamage(const AActor* Instigator, const AActor* Target)
{
	if (!IsValid(Target))
	{
		return false;
	}
	const UGBAttributeComponent* TargetAttributes = UGBAttributeComponent::Find(Target);
	if (!TargetAttributes || TargetAttributes->IsDead())
	{
		return false;
	}
	if (!IsValid(Instigator))
	{
		return true;
	}
	const EGBTeam InstigatorTeam = GetTeam(Instigator);
	const EGBTeam TargetTeam = TargetAttributes->Team;
	if (InstigatorTeam == EGBTeam::Neutral || TargetTeam == EGBTeam::Neutral)
	{
		return true;
	}
	return AreTeamsHostile(InstigatorTeam, TargetTeam);
}

float UGBCombatLibrary::DistanceFalloffMultiplier(float Distance, float FalloffStart, float FalloffEnd, float NearMult, float FarMult)
{
	if (Distance <= FalloffStart)
	{
		return NearMult;
	}
	if (FalloffEnd <= FalloffStart || Distance >= FalloffEnd)
	{
		return FarMult;
	}
	const float Alpha = (Distance - FalloffStart) / (FalloffEnd - FalloffStart);
	return FMath::Lerp(NearMult, FarMult, Alpha);
}

bool UGBCombatLibrary::IsHeadshot(float HitZ, float FeetZ, float BodyHeight, float HeadFraction)
{
	if (HeadFraction <= 0.f || BodyHeight <= 0.f)
	{
		return false;
	}
	const float HeadLineZ = FeetZ + BodyHeight * (1.f - FMath::Min(HeadFraction, 1.f));
	return HitZ >= HeadLineZ;
}

FVector UGBCombatLibrary::ApplySpread(const FVector& Direction, float SpreadHalfAngleDegrees, FRandomStream& Random)
{
	const FVector Dir = Direction.GetSafeNormal();
	if (Dir.IsNearlyZero() || SpreadHalfAngleDegrees <= 0.f)
	{
		return Dir;
	}
	return Random.VRandCone(Dir, FMath::DegreesToRadians(SpreadHalfAngleDegrees));
}

float UGBCombatLibrary::ComputeMovementSpread(float BaseSpread, float MoveSpread, float Speed, float FullSpeed)
{
	const float Base = FMath::Max(0.f, BaseSpread);
	if (FullSpeed <= 0.f || MoveSpread <= 0.f)
	{
		return Base;
	}
	return Base + MoveSpread * FMath::Clamp(Speed / FullSpeed, 0.f, 1.f);
}

bool UGBCombatLibrary::ApplyWeakPointBonus(const AActor* Target, FGBDamageInfo& Info, float WeakPointMult)
{
	Info.bHeadshot = IsWeakPointHit(Target, Info.HitLocation);
	if (!Info.bHeadshot || WeakPointMult <= 1.f)
	{
		return Info.bHeadshot;
	}
	Info.Amount *= WeakPointMult;
	return true;
}

bool UGBCombatLibrary::IsWeakPointHit(const AActor* Target, const FVector& HitLocation)
{
	const AGBEnemyCharacter* Enemy = Cast<AGBEnemyCharacter>(Target);
	return Enemy && Enemy->IsHeadshotLocation(HitLocation);
}

int32 UGBCombatLibrary::NewBuildUpEventId()
{
	static int32 NextId = 0;
	++NextId;
	if (NextId == 0)
	{
		++NextId;
	}
	return NextId;
}

float UGBCombatLibrary::ApplyBuildUp(AActor* Target, const FGBDamageInfo& Info)
{
	if (Info.BuildUp <= 0.f)
	{
		return 0.f;
	}
	const int32 Index = GBElemental::ElementIndex(Info.Element);
	if (Index < 0)
	{
		return 0.f;
	}
	AActor* Instigator = Info.Instigator.Get();
	if (IsValid(Instigator))
	{
		// Keep the pointer; null instigators still build up (CanDamage allows them).
	}
	else
	{
		Instigator = nullptr;
	}
	if (!CanDamage(Instigator, Target))
	{
		return 0.f;
	}
	UGBElementalStatusComponent* Elemental = UGBElementalStatusComponent::Find(Target);
	if (!Elemental)
	{
		return 0.f;
	}
	const float Mult = Info.bHeadshot ? FMath::Max(1.f, Info.WeakPointBuildUpMult) : 1.f;
	const float Amount = Info.BuildUp * Mult;
	Elemental->AddBuildUp(Info.Element, Amount, Info.BuildUpEventId, Instigator);
	return Amount;
}

FVector UGBCombatLibrary::ClosestBodyPointToAim(const FVector& AimOrigin, const FVector& AimDirection, float AimLength, const FVector& BodyBottom, const FVector& BodyTop)
{
	const FVector AimEnd = AimOrigin + AimDirection * AimLength;
	FVector OutAimPoint = AimOrigin;
	FVector OutBodyPoint = BodyBottom;
	FMath::SegmentDistToSegmentSafe(AimOrigin, AimEnd, BodyBottom, BodyTop, OutAimPoint, OutBodyPoint);
	return OutBodyPoint;
}

float UGBCombatLibrary::ComputePrecisionRecoveryCooldown(int32 Rank, float Base, float PerRank, float MinCooldown)
{
	if (Rank <= 0)
	{
		return 0.f;
	}
	return FMath::Max(MinCooldown, Base - PerRank * static_cast<float>(Rank));
}

bool UGBCombatLibrary::ShouldPrecisionRecover(bool bKillingBlowWeakPoint, bool bInstigatorIsPlayer, int32 Rank, float TimeSinceLastHeal, float Cooldown)
{
	return bKillingBlowWeakPoint && bInstigatorIsPlayer && Rank > 0 && TimeSinceLastHeal >= Cooldown;
}
