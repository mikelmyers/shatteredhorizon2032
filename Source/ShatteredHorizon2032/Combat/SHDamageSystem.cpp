// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDamageSystem.h"
#include "SHFatigueSystem.h"
#include "SHMoraleSystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// =====================================================================
//  Constructor
// =====================================================================

USHDamageSystem::USHDamageSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.TickInterval = 0.1f; // Bleed tick doesn't need every frame.
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHDamageSystem::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	VitalStatus = ESHVitalStatus::Healthy;
	InitDefaultZoneMultipliers();

	// Cache sibling components.
	if (AActor* Owner = GetOwner())
	{
		CachedFatigueSystem = Owner->FindComponentByClass<USHFatigueSystem>();
		CachedMoraleSystem = Owner->FindComponentByClass<USHMoraleSystem>();
	}
}

void USHDamageSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (VitalStatus == ESHVitalStatus::Dead)
	{
		return;
	}

	TickBleeding(DeltaTime);

	if (VitalStatus == ESHVitalStatus::Incapacitated)
	{
		TickIncapacitated(DeltaTime);
	}
}

// =====================================================================
//  Default configuration
// =====================================================================

void USHDamageSystem::InitDefaultZoneMultipliers()
{
	if (ZoneDamageMultipliers.Num() > 0)
	{
		return;
	}

	ZoneDamageMultipliers.Add(ESHHitZone::Head, 10.f);		// Instant kill on rifle rounds.
	ZoneDamageMultipliers.Add(ESHHitZone::Torso, 1.5f);
	ZoneDamageMultipliers.Add(ESHHitZone::LeftArm, 0.6f);
	ZoneDamageMultipliers.Add(ESHHitZone::RightArm, 0.6f);
	ZoneDamageMultipliers.Add(ESHHitZone::LeftLeg, 0.7f);
	ZoneDamageMultipliers.Add(ESHHitZone::RightLeg, 0.7f);
}

// =====================================================================
//  Damage application
// =====================================================================

FSHDamageResult USHDamageSystem::ApplyDamage(const FSHDamageInfo& DamageInfo)
{
	FSHDamageResult Result;

	if (VitalStatus == ESHVitalStatus::Dead)
	{
		return Result;
	}

	Result = ProcessDamage(DamageInfo);

	// Notify delegates.
	OnDamageReceived.Broadcast(DamageInfo, Result);

	if (Result.WoundCreated != ESHWoundSeverity::None)
	{
		FSHWound Wound = CreateWound(DamageInfo, Result.WoundCreated);
		ActiveWounds.Add(Wound);
		OnWoundAdded.Broadcast(Wound);
	}

	// Update health.
	CurrentHealth = FMath::Max(CurrentHealth - Result.DamageDealt, 0.f);

	// Determine vital status change.
	UpdateVitalStatus();

	if (Result.IsLethal())
	{
		Result.bIsLethal = true;
	}
	if (VitalStatus == ESHVitalStatus::Incapacitated)
	{
		Result.bIsIncapacitating = true;
	}

	// Notify linked systems (fatigue, morale).
	NotifyLinkedSystems(DamageInfo, Result);

	return Result;
}

FSHDamageResult USHDamageSystem::ProcessDamage(const FSHDamageInfo& DamageInfo)
{
	FSHDamageResult Result;
	Result.HitZone = DamageInfo.HitZone;

	float Damage = DamageInfo.BaseDamage;

	// Apply zone multiplier.
	const float* ZoneMult = ZoneDamageMultipliers.Find(DamageInfo.HitZone);
	if (ZoneMult)
	{
		Damage *= *ZoneMult;
	}

	// Headshots with ballistic damage — graze vs direct hit.
	// Grazing impacts (>60° from surface normal) cause stagger, not instant death.
	if (DamageInfo.HitZone == ESHHitZone::Head && DamageInfo.DamageType == ESHDamageType::Ballistic)
	{
		// Compute impact angle from damage direction vs vertical (head approximation).
		// A direct hit has a small angle to the surface normal; a graze is >60°.
		const FVector HeadUp = FVector::UpVector;
		const FVector ImpactDir = DamageInfo.DamageDirection.GetSafeNormal();
		const float CosAngle = FMath::Abs(FVector::DotProduct(HeadUp, ImpactDir));
		const float AngleFromNormal = FMath::RadiansToDegrees(FMath::Acos(CosAngle));

		if (AngleFromNormal > 60.f)
		{
			// Skull graze — stagger + 3-5 sec recovery, NOT lethal.
			Result.DamageDealt = Damage * 0.3f; // Reduced damage
			Result.bIsLethal = false;
			Result.WoundCreated = ESHWoundSeverity::Serious;
			Result.bIsHeadGraze = true;
			return Result;
		}
		else
		{
			// Direct headshot — lethal.
			Result.DamageDealt = CurrentHealth + 1.f;
			Result.bIsLethal = true;
			Result.WoundCreated = ESHWoundSeverity::Fatal;
			return Result;
		}
	}

	// Armor reduction.
	bool bArmorHit = false;
	bool bPenetrated = false;
	Damage = ApplyArmorReduction(Damage, DamageInfo.HitZone, DamageInfo.PenetrationRating, bArmorHit, bPenetrated);
	Result.bArmorHit = bArmorHit;
	Result.bArmorPenetrated = bPenetrated;
	Result.DamageAbsorbed = DamageInfo.BaseDamage - Damage;
	if (Result.DamageAbsorbed < 0.f)
	{
		Result.DamageAbsorbed = 0.f;
	}

	// Concussion damage ignores armor partially.
	if (DamageInfo.DamageType == ESHDamageType::Concussion && bArmorHit && !bPenetrated)
	{
		// Blunt trauma through armor.
		Damage = DamageInfo.BaseDamage * 0.3f;
	}

	Result.DamageDealt = FMath::Max(Damage, 0.f);
	Result.WoundCreated = DetermineWoundSeverity(Result.DamageDealt, DamageInfo.HitZone);

	return Result;
}

float USHDamageSystem::ApplyArmorReduction(float IncomingDamage, ESHHitZone Zone, int32 PenetrationRating, bool& bOutArmorHit, bool& bOutPenetrated)
{
	bOutArmorHit = false;
	bOutPenetrated = false;

	FSHArmorPlate* Plate = nullptr;
	for (auto& AP : EquippedArmor)
	{
		if (AP.CoveredZone == Zone && AP.Integrity > 0.f)
		{
			Plate = &AP;
			break;
		}
	}

	if (!Plate)
	{
		return IncomingDamage;
	}

	bOutArmorHit = true;

	// Check penetration: if projectile pen rating exceeds armor class, it goes through.
	if (PenetrationRating > Plate->ArmorClass)
	{
		bOutPenetrated = true;

		// Armor still absorbs some damage and degrades.
		const float AbsorbedFraction = 0.3f * Plate->Integrity;
		const float Absorbed = IncomingDamage * AbsorbedFraction;

		Plate->Integrity = FMath::Max(Plate->Integrity - 0.2f, 0.f);
		return IncomingDamage - Absorbed;
	}

	// Projectile stopped by armor.
	const float MaxAbsorb = Plate->MaxAbsorption * Plate->Integrity;
	const float Absorbed = FMath::Min(IncomingDamage, MaxAbsorb);

	// Degrade armor integrity.
	const float IntegrityLoss = (IncomingDamage / Plate->MaxAbsorption) * 0.15f;
	Plate->Integrity = FMath::Max(Plate->Integrity - IntegrityLoss, 0.f);

	// Spalling damage if no anti-spall.
	float Remaining = IncomingDamage - Absorbed;
	if (!Plate->bHasAntiSpall && Remaining <= 0.f)
	{
		// Minor spall damage even when stopped.
		Remaining = IncomingDamage * 0.05f;
	}

	return FMath::Max(Remaining, 0.f);
}

ESHWoundSeverity USHDamageSystem::DetermineWoundSeverity(float DamageDealt, ESHHitZone Zone) const
{
	if (DamageDealt <= 0.f)
	{
		return ESHWoundSeverity::None;
	}

	// Zone-aware severity thresholds.
	const bool bIsLimb = (Zone == ESHHitZone::LeftArm || Zone == ESHHitZone::RightArm ||
						  Zone == ESHHitZone::LeftLeg || Zone == ESHHitZone::RightLeg);

	if (DamageDealt >= 80.f)
		return ESHWoundSeverity::Fatal;
	if (DamageDealt >= 50.f)
		return bIsLimb ? ESHWoundSeverity::Serious : ESHWoundSeverity::Critical;
	if (DamageDealt >= 25.f)
		return bIsLimb ? ESHWoundSeverity::Light : ESHWoundSeverity::Serious;
	if (DamageDealt >= 10.f)
		return ESHWoundSeverity::Light;

	return ESHWoundSeverity::Scratch;
}

FSHWound USHDamageSystem::CreateWound(const FSHDamageInfo& DamageInfo, ESHWoundSeverity Severity)
{
	FSHWound Wound;
	Wound.HitZone = DamageInfo.HitZone;
	Wound.DamageType = DamageInfo.DamageType;
	Wound.Severity = Severity;

	// Bleeding based on severity and damage type.
	switch (Severity)
	{
	case ESHWoundSeverity::Scratch:
		Wound.bIsBleeding = false;
		Wound.BleedRate = 0.f;
		break;
	case ESHWoundSeverity::Light:
		Wound.bIsBleeding = true;
		Wound.BleedRate = 0.5f;
		break;
	case ESHWoundSeverity::Serious:
		Wound.bIsBleeding = true;
		Wound.BleedRate = 2.f;
		break;
	case ESHWoundSeverity::Critical:
		Wound.bIsBleeding = true;
		Wound.BleedRate = 5.f;
		break;
	case ESHWoundSeverity::Fatal:
		Wound.bIsBleeding = true;
		Wound.BleedRate = 10.f;
		break;
	default:
		break;
	}

	// Burns don't bleed as much but cause ongoing damage differently.
	if (DamageInfo.DamageType == ESHDamageType::Burn)
	{
		Wound.bIsBleeding = false;
		Wound.BleedRate *= 0.3f; // Reduced bleed but still causes health drain (tissue damage).
		if (Wound.BleedRate > 0.f)
		{
			Wound.bIsBleeding = true;
		}
	}

	if (const UWorld* World = GetWorld())
	{
		Wound.TimeReceived = World->GetTimeSeconds();
	}

	return Wound;
}

// =====================================================================
//  Medical treatment
// =====================================================================

bool USHDamageSystem::ApplySelfBandage()
{
	if (VitalStatus == ESHVitalStatus::Dead || VitalStatus == ESHVitalStatus::Incapacitated)
	{
		return false;
	}

	// Find the worst untreated bleeding wound that a self-bandage can handle (Light only).
	FSHWound* TargetWound = nullptr;
	for (auto& Wound : ActiveWounds)
	{
		if (Wound.bIsBleeding && !Wound.bIsTreated && Wound.Severity <= ESHWoundSeverity::Light)
		{
			if (!TargetWound || Wound.BleedRate > TargetWound->BleedRate)
			{
				TargetWound = &Wound;
			}
		}
	}

	if (!TargetWound)
	{
		return false;
	}

	TargetWound->bIsBleeding = false;
	TargetWound->BleedRate = 0.f;
	TargetWound->bIsTreated = true;
	return true;
}

bool USHDamageSystem::ApplyMedicTreatment()
{
	if (VitalStatus == ESHVitalStatus::Dead)
	{
		return false;
	}

	// Medic can treat the worst untreated wound (Serious and Critical).
	FSHWound* TargetWound = nullptr;
	for (auto& Wound : ActiveWounds)
	{
		if (!Wound.bIsTreated)
		{
			if (!TargetWound ||
				static_cast<uint8>(Wound.Severity) > static_cast<uint8>(TargetWound->Severity))
			{
				TargetWound = &Wound;
			}
		}
	}

	if (!TargetWound)
	{
		return false;
	}

	TargetWound->bIsBleeding = false;
	TargetWound->BleedRate = 0.f;
	TargetWound->bIsTreated = true;

	// Notify linked morale system.
	if (CachedMoraleSystem)
	{
		CachedMoraleSystem->NotifyMedicTreatment();
	}

	return true;
}

bool USHDamageSystem::Stabilize()
{
	if (VitalStatus != ESHVitalStatus::Incapacitated)
	{
		return false;
	}

	bIsStabilized = true;

	// Stop all bleeding.
	for (auto& Wound : ActiveWounds)
	{
		Wound.bIsBleeding = false;
		Wound.BleedRate = 0.f;
	}

	return true;
}

bool USHDamageSystem::Revive()
{
	if (VitalStatus != ESHVitalStatus::Incapacitated)
	{
		return false;
	}

	CurrentHealth = ReviveHealthAmount;
	bIsStabilized = false;
	VitalStatus = ESHVitalStatus::Wounded;
	OnVitalStatusChanged.Broadcast(VitalStatus);

	// Treat all bleeding wounds as part of the revive process.
	for (auto& Wound : ActiveWounds)
	{
		if (Wound.bIsBleeding)
		{
			Wound.bIsBleeding = false;
			Wound.BleedRate = 0.f;
			Wound.bIsTreated = true;
		}
	}

	return true;
}

// =====================================================================
//  Armor management
// =====================================================================

void USHDamageSystem::EquipArmorPlate(const FSHArmorPlate& Plate)
{
	// Replace existing plate for the same zone.
	for (auto& Existing : EquippedArmor)
	{
		if (Existing.CoveredZone == Plate.CoveredZone)
		{
			Existing = Plate;
			return;
		}
	}
	EquippedArmor.Add(Plate);
}

void USHDamageSystem::RemoveArmorPlate(ESHHitZone Zone)
{
	EquippedArmor.RemoveAll([Zone](const FSHArmorPlate& P) { return P.CoveredZone == Zone; });
}

bool USHDamageSystem::HasArmorForZone(ESHHitZone Zone) const
{
	for (const auto& Plate : EquippedArmor)
	{
		if (Plate.CoveredZone == Zone && Plate.Integrity > 0.f)
		{
			return true;
		}
	}
	return false;
}

float USHDamageSystem::GetArmorIntegrity(ESHHitZone Zone) const
{
	for (const auto& Plate : EquippedArmor)
	{
		if (Plate.CoveredZone == Zone)
		{
			return Plate.Integrity;
		}
	}
	return 0.f;
}

// =====================================================================
//  Queries
// =====================================================================

bool USHDamageSystem::IsBleeding() const
{
	for (const auto& Wound : ActiveWounds)
	{
		if (Wound.bIsBleeding)
		{
			return true;
		}
	}
	return false;
}

float USHDamageSystem::GetTotalBleedRate() const
{
	float Total = 0.f;
	for (const auto& Wound : ActiveWounds)
	{
		if (Wound.bIsBleeding)
		{
			Total += Wound.BleedRate;
		}
	}
	return Total;
}

ESHWoundSeverity USHDamageSystem::GetWorstWoundOnZone(ESHHitZone Zone) const
{
	ESHWoundSeverity Worst = ESHWoundSeverity::None;
	for (const auto& Wound : ActiveWounds)
	{
		if (Wound.HitZone == Zone && static_cast<uint8>(Wound.Severity) > static_cast<uint8>(Worst))
		{
			Worst = Wound.Severity;
		}
	}
	return Worst;
}

float USHDamageSystem::GetAimPenaltyMultiplier() const
{
	float Penalty = 1.f;

	const ESHWoundSeverity LeftArm = GetWorstWoundOnZone(ESHHitZone::LeftArm);
	const ESHWoundSeverity RightArm = GetWorstWoundOnZone(ESHHitZone::RightArm);

	// Use the worse of the two arms.
	const ESHWoundSeverity WorstArm = static_cast<uint8>(LeftArm) > static_cast<uint8>(RightArm) ? LeftArm : RightArm;

	switch (WorstArm)
	{
	case ESHWoundSeverity::Scratch: Penalty = 0.95f; break;
	case ESHWoundSeverity::Light:   Penalty = 0.85f; break;
	case ESHWoundSeverity::Serious: Penalty = 0.6f;  break;
	case ESHWoundSeverity::Critical:Penalty = 0.3f;  break;
	case ESHWoundSeverity::Fatal:   Penalty = 0.1f;  break;
	default: break;
	}

	return Penalty;
}

float USHDamageSystem::GetMovementPenaltyMultiplier() const
{
	float Penalty = 1.f;

	const ESHWoundSeverity LeftLeg = GetWorstWoundOnZone(ESHHitZone::LeftLeg);
	const ESHWoundSeverity RightLeg = GetWorstWoundOnZone(ESHHitZone::RightLeg);

	// Both legs compound.
	auto LegPenalty = [](ESHWoundSeverity S) -> float
	{
		switch (S)
		{
		case ESHWoundSeverity::Scratch: return 0.95f;
		case ESHWoundSeverity::Light:   return 0.8f;
		case ESHWoundSeverity::Serious: return 0.5f;
		case ESHWoundSeverity::Critical:return 0.25f;
		case ESHWoundSeverity::Fatal:   return 0.1f;
		default: return 1.f;
		}
	};

	Penalty = FMath::Min(LegPenalty(LeftLeg), LegPenalty(RightLeg));

	// If both legs are wounded, compound the penalty.
	if (LeftLeg != ESHWoundSeverity::None && RightLeg != ESHWoundSeverity::None)
	{
		Penalty *= 0.8f;
	}

	return FMath::Max(Penalty, 0.05f);
}

// =====================================================================
//  Tick helpers
// =====================================================================

void USHDamageSystem::TickBleeding(float DeltaTime)
{
	const float TotalBleed = GetTotalBleedRate();
	if (TotalBleed <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Max(CurrentHealth - TotalBleed * DeltaTime, 0.f);
	UpdateVitalStatus();
}

void USHDamageSystem::TickIncapacitated(float DeltaTime)
{
	if (bIsStabilized)
	{
		return; // Stabilized characters don't bleed out.
	}

	IncapacitatedTimer -= DeltaTime;
	if (IncapacitatedTimer <= 0.f)
	{
		// Bleed-out death.
		VitalStatus = ESHVitalStatus::Dead;
		CurrentHealth = 0.f;
		OnVitalStatusChanged.Broadcast(VitalStatus);
		OnCharacterDied.Broadcast();
	}
}

void USHDamageSystem::UpdateVitalStatus()
{
	if (VitalStatus == ESHVitalStatus::Dead)
	{
		return;
	}

	const ESHVitalStatus OldStatus = VitalStatus;

	if (CurrentHealth <= 0.f)
	{
		if (VitalStatus != ESHVitalStatus::Incapacitated)
		{
			// Check if this should be incapacitation or death.
			// Headshots and massive damage bypass incapacitation.
			if (CurrentHealth <= -IncapacitationThreshold)
			{
				VitalStatus = ESHVitalStatus::Dead;
			}
			else
			{
				VitalStatus = ESHVitalStatus::Incapacitated;
				IncapacitatedTimer = IncapacitatedBleedOutTime;
				bIsStabilized = false;
			}
		}
	}
	else if (ActiveWounds.Num() > 0 && VitalStatus != ESHVitalStatus::Incapacitated)
	{
		VitalStatus = ESHVitalStatus::Wounded;
	}
	else if (ActiveWounds.Num() == 0)
	{
		VitalStatus = ESHVitalStatus::Healthy;
	}

	if (VitalStatus != OldStatus)
	{
		OnVitalStatusChanged.Broadcast(VitalStatus);

		if (VitalStatus == ESHVitalStatus::Dead)
		{
			OnCharacterDied.Broadcast();
		}
		else if (VitalStatus == ESHVitalStatus::Incapacitated)
		{
			OnCharacterIncapacitated.Broadcast();
		}
	}
}

void USHDamageSystem::NotifyLinkedSystems(const FSHDamageInfo& DamageInfo, const FSHDamageResult& Result)
{
	// Notify fatigue system of injury.
	if (CachedFatigueSystem && Result.WoundCreated != ESHWoundSeverity::None)
	{
		const float Severity = static_cast<float>(static_cast<uint8>(Result.WoundCreated)) / 5.f;
		CachedFatigueSystem->NotifyInjury(Severity);
	}

	// Notify morale system.
	if (CachedMoraleSystem && Result.DamageDealt > 0.f)
	{
		CachedMoraleSystem->ReportMoraleEvent(ESHMoraleEvent::Wounded);
	}
}
