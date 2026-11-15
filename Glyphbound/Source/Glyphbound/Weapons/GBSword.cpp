#include "Weapons/GBSword.h"

#include "Combat/GBCombatLibrary.h"
#include "Components/PointLightComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Core/GBPlaceholder.h"
#include "Core/GBStatNames.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Player/GBPlayerCharacter.h"
#include "Stats/GBAttributeComponent.h"

namespace
{
	UStaticMeshComponent* MakeSwordHiltPart(AActor* Owner, USceneComponent* Parent, FName Name, EGBShape Shape, const FVector& Location,
		const FVector& Size, const FLinearColor& Color, bool bGlow)
	{
		UStaticMeshComponent* Part = NewObject<UStaticMeshComponent>(Owner, Name);
		Part->SetupAttachment(Parent);
		Part->SetRelativeLocation(Location);
		Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Part->SetGenerateOverlapEvents(false);
		Part->SetCastShadow(false);
		Part->RegisterComponent();
		GBPlaceholder::SetupShape(Part, Shape, Size, Color, bGlow);
		return Part;
	}

	/**
	 * A sword with Strings["ViewMesh"] (the Fab katana, owner 2026-11-15) is shown as a 3D view model like the guns:
	 * no hands, only the weapon and its own swing. Stats (all optional, cm / degrees, in the anchor's camera space):
	 * ViewScale, ViewOffsetX/Y/Z (where the grip sits), ViewPitch/ViewYaw/ViewRoll (lean of the blade),
	 * ViewGripX (grip point along the mesh's X, source units), SwingTime. The mesh's blade runs along -X, edge on -Z.
	 */
	namespace GBSwordView
	{
		const FName ViewMesh(TEXT("ViewMesh"));
		const FName Scale(TEXT("ViewScale"));
		const FName OffsetX(TEXT("ViewOffsetX"));
		const FName OffsetY(TEXT("ViewOffsetY"));
		const FName OffsetZ(TEXT("ViewOffsetZ"));
		const FName Pitch(TEXT("ViewPitch"));
		const FName Yaw(TEXT("ViewYaw"));
		const FName Roll(TEXT("ViewRoll"));
		const FName GripX(TEXT("ViewGripX"));
		const FName SwingTime(TEXT("SwingTime"));
		const TCHAR* EnchantLightName = TEXT("SwordEnchantLight");

		float DefStat(const FGBWeaponDefinition& Def, FName Key, float Fallback)
		{
			const float* Found = Def.Stats.Find(Key);
			return Found ? *Found : Fallback;
		}

		/** 0 -> 1 -> 0 over the swing: a fast cut down to the left and a slower return. */
		float SwingAmount(float Since, float Duration)
		{
			if (Duration <= 0.f || Since < 0.f || Since >= Duration)
			{
				return 0.f;
			}
			const float T = Since / Duration;
			const float CutEnd = 0.35f;
			return T < CutEnd ? FMath::InterpEaseOut(0.f, 1.f, T / CutEnd, 2.f) : FMath::InterpEaseInOut(1.f, 0.f, (T - CutEnd) / (1.f - CutEnd), 2.f);
		}
	}
}

void AGBSword::InitFromDefinition(const FGBWeaponDefinition& Definition, AGBPlayerCharacter* InOwner)
{
	Super::InitFromDefinition(Definition, InOwner);

	const FName Required[] = { GBStat::Damage, GBStat::FireInterval, GBStat::MeleeRange, GBStat::MeleeArc };
	for (const FName& Stat : Required)
	{
		if (!Definition.Stats.Contains(Stat))
		{
			UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': missing stat '%s', using a default"), *Definition.Id.ToString(), *Stat.ToString());
		}
	}

	if (const FString* MeshPath = Definition.Strings.Find(GBSwordView::ViewMesh))
	{
		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, **MeshPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			// Grip holds the whole view model; Guard and Gem stay null (the HUD draws no pixel blade for it either).
			Grip = NewObject<UStaticMeshComponent>(this, TEXT("ViewModel"));
			Grip->SetupAttachment(Root);
			Grip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Grip->SetGenerateOverlapEvents(false);
			Grip->SetCastShadow(true);
			Grip->bSelfShadowOnly = true;
			Grip->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
			Grip->SetStaticMesh(Mesh);
			Grip->RegisterComponent();

			const float Scale = FMath::Max(0.01f, GBSwordView::DefStat(Definition, GBSwordView::Scale, 0.12f));
			// Blade up and a little forward and towards the view centre; the edge faces forward.
			const FQuat Lean = FRotator(GBSwordView::DefStat(Definition, GBSwordView::Pitch, 0.f), GBSwordView::DefStat(Definition, GBSwordView::Yaw, 0.f),
				GBSwordView::DefStat(Definition, GBSwordView::Roll, 0.f)).Quaternion();
			const FVector BladeDir = Lean.RotateVector(FVector(0.35f, -0.25f, 0.9f).GetSafeNormal());
			const FVector EdgeDir = Lean.RotateVector(FVector(1.f, 0.f, -0.3f).GetSafeNormal());
			const FQuat MeshRot = FRotationMatrix::MakeFromXZ(-BladeDir, -EdgeDir).ToQuat();
			const FVector GripLocal(GBSwordView::DefStat(Definition, GBSwordView::GripX, 210.f), 0.f, 0.f);
			const FVector GripAt(GBSwordView::DefStat(Definition, GBSwordView::OffsetX, 0.f), GBSwordView::DefStat(Definition, GBSwordView::OffsetY, 0.f),
				GBSwordView::DefStat(Definition, GBSwordView::OffsetZ, 0.f));
			Grip->SetRelativeScale3D(FVector(Scale));
			Grip->SetRelativeRotation(MeshRot);
			Grip->SetRelativeLocation(GripAt - MeshRot.RotateVector(GripLocal * Scale));
			ShownElement = Element;
			return;
		}
		UE_LOG(LogGB, Warning, TEXT("Weapons.json '%s': ViewMesh '%s' not found; using the placeholder hilt and the pixel blade."), *Definition.Id.ToString(), **MeshPath);
	}

	// Placeholder 3D hilt (art rule: the hilt comes from the future). Sizes in cm are purely visual; the blade
	// rises along +Z from the guard and is drawn by the HUD as pixel art.
	Grip = MakeSwordHiltPart(this, Root, TEXT("Grip"), EGBShape::Cylinder, FVector(0.f, 0.f, -5.f), FVector(3.f, 3.f, 10.f), FLinearColor(0.10f, 0.10f, 0.12f), false);
	Guard = MakeSwordHiltPart(this, Root, TEXT("Guard"), EGBShape::Cube, FVector(0.f, 0.f, 1.f), FVector(4.f, 14.f, 2.f), FLinearColor(0.35f, 0.35f, 0.40f), false);
	ShownElement = Element;
	Gem = MakeSwordHiltPart(this, Root, TEXT("Gem"), EGBShape::Sphere, FVector(2.5f, 0.f, 1.f), FVector(2.5f, 2.5f, 2.5f),
		GBPlaceholder::ElementColor(static_cast<uint8>(ShownElement)), true);
}

void AGBSword::Tick(float DeltaSeconds)
{
	if (Grip && Root && !Gem)
	{
		// 3D view model: its own swing (a cut down to the left, pivoting on the grip), and a light in the enchant's color.
		const FGBWeaponDefinition* Def = GetDefinition();
		const float Duration = Def ? GBSwordView::DefStat(*Def, GBSwordView::SwingTime, 0.4f) : 0.4f;
		const float A = GBSwordView::SwingAmount(GetTimeSinceLastAttack(), FMath::Min(Duration, FMath::Max(0.05f, GetStat(GBStat::FireInterval, 0.5f))));
		Root->SetRelativeRotation(FRotator(-55.f * A, -30.f * A, -35.f * A));
		Root->SetRelativeLocation(FVector(6.f * A, -14.f * A, 2.f * A));

		EGBElement EnchantElement = EGBElement::Physical;
		FName EnchantEffect = NAME_None;
		const bool bEnchanted = IsEquipped() && GetActiveEnchant(EnchantElement, EnchantEffect);
		UPointLightComponent* Light = nullptr;
		for (UActorComponent* Component : GetComponents())
		{
			if (Component && Component->GetFName() == GBSwordView::EnchantLightName)
			{
				Light = Cast<UPointLightComponent>(Component);
			}
		}
		if (bEnchanted && !Light)
		{
			Light = NewObject<UPointLightComponent>(this, GBSwordView::EnchantLightName);
			Light->SetMobility(EComponentMobility::Movable);
			Light->SetupAttachment(Grip);
			Light->SetIntensityUnits(ELightUnits::Candelas);
			Light->SetAttenuationRadius(500.f);
			Light->SetSourceRadius(4.f);
			Light->SetCastShadows(false);
			Light->RegisterComponent();
			AddInstanceComponent(Light);
		}
		if (Light)
		{
			Light->SetVisibility(bEnchanted);
			if (bEnchanted)
			{
				// Halfway up the blade (mesh space: the blade runs along -X from the guard).
				Light->SetRelativeLocation(FVector(-40.f, 0.f, 0.f));
				Light->SetLightColor(GBPlaceholder::ElementColor(static_cast<uint8>(EnchantElement)));
				const float Time = GetWorld() ? (float)GetWorld()->GetTimeSeconds() : 0.f;
				Light->SetIntensity(8.f * (0.85f + 0.15f * FMath::Sin(Time * 9.f)));
			}
		}
		Super::Tick(DeltaSeconds);
		return;
	}
	if (Gem && ShownElement != Element)
	{
		ShownElement = Element;
		GBPlaceholder::SetShapeColor(Gem, GBPlaceholder::ElementColor(static_cast<uint8>(ShownElement)));
	}
	Super::Tick(DeltaSeconds);
}

void AGBSword::GetBodySegment(AActor* Target, FVector& OutBottom, FVector& OutTop)
{
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
		{
			const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
			const FVector Center = Character->GetActorLocation();
			OutBottom = FVector(Center.X, Center.Y, Center.Z - HalfHeight);
			OutTop = FVector(Center.X, Center.Y, Center.Z + HalfHeight);
			return;
		}
	}
	if (const UCapsuleComponent* Capsule = Target ? Target->FindComponentByClass<UCapsuleComponent>() : nullptr)
	{
		const float HalfHeight = Capsule->GetScaledCapsuleHalfHeight();
		const FVector Center = Capsule->GetComponentLocation();
		OutBottom = FVector(Center.X, Center.Y, Center.Z - HalfHeight);
		OutTop = FVector(Center.X, Center.Y, Center.Z + HalfHeight);
		return;
	}
	FVector Origin;
	FVector Extent;
	Target->GetActorBounds(false, Origin, Extent, false);
	OutBottom = FVector(Origin.X, Origin.Y, Origin.Z - Extent.Z);
	OutTop = FVector(Origin.X, Origin.Y, Origin.Z + Extent.Z);
}

bool AGBSword::PerformAttack()
{
	AGBPlayerCharacter* Player = GetPlayerOwner();
	UWorld* World = GetWorld();
	if (!Player || !World)
	{
		return false;
	}

	const FVector Origin = Player->GetAimOrigin();
	const FVector Forward = Player->GetAimDirection().GetSafeNormal();
	const float Range = GetStat(GBStat::MeleeRange, 200.f);
	const float HalfArcCos = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(GetStat(GBStat::MeleeArc, 90.f), 0.f, 360.f) * 0.5f));
	if (Range <= 0.f)
	{
		return true;
	}

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(GBSwordOverlap), false, Player);
	OverlapParams.AddIgnoredActor(this);

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity, ObjectParams, FCollisionShape::MakeSphere(Range), OverlapParams);

	const FGBDamageInfo BaseInfo = MakeDamageInfo();
	const float HeadshotMult = GetStat(GBStat::HeadshotMult, 1.f);
	TSet<AActor*> Done;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Target = Overlap.GetActor();
		if (!Target || Done.Contains(Target))
		{
			continue;
		}
		Done.Add(Target);

		if (!UGBAttributeComponent::Find(Target) || !UGBCombatLibrary::IsAlive(Target) || !UGBCombatLibrary::AreHostile(Player, Target))
		{
			continue;
		}

		// Arc and range selection stays on the target's center, so nothing previously hit is missed.
		const FVector TargetLocation = Target->GetActorLocation();
		const FVector ToTarget = TargetLocation - Origin;
		const FVector ToTargetDir = ToTarget.GetSafeNormal();
		if (!ToTargetDir.IsNearlyZero() && FVector::DotProduct(Forward, ToTargetDir) < HalfArcCos)
		{
			continue;
		}

		FVector BodyBottom;
		FVector BodyTop;
		GetBodySegment(Target, BodyBottom, BodyTop);
		const FVector HitLocation = UGBCombatLibrary::ClosestBodyPointToAim(Origin, Forward, Range, BodyBottom, BodyTop);

		// Not through walls: trace to the aimed hit location, not the target's center.
		FCollisionQueryParams WallParams(SCENE_QUERY_STAT(GBSwordWall), false, Player);
		WallParams.AddIgnoredActor(this);
		WallParams.AddIgnoredActor(Target);
		if (World->LineTraceTestByChannel(Origin, HitLocation, ECC_Visibility, WallParams))
		{
			continue;
		}

		FGBDamageInfo Info = BaseInfo;
		Info.HitLocation = HitLocation;
		Info.HitDirection = ToTargetDir;
		UGBCombatLibrary::ApplyWeakPointBonus(Target, Info, HeadshotMult);
		UGBCombatLibrary::DealDamage(Target, Info);
	}
	return true;
}
