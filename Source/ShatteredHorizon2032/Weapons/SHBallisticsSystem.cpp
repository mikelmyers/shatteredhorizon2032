// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "Weapons/SHBallisticsSystem.h"
#include "Weapons/SHProjectile.h"
#include "Weapons/SHWeaponData.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "CollisionQueryParams.h"

// ---------------------------------------------------------------------------
// Subsystem lifecycle
// ---------------------------------------------------------------------------

void USHBallisticsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	CurrentWind = FSHWindParams();
}

void USHBallisticsSystem::Deinitialize()
{
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Wind
// ---------------------------------------------------------------------------

void USHBallisticsSystem::SetWindParams(const FSHWindParams& InWind)
{
	CurrentWind = InWind;
}

// ---------------------------------------------------------------------------
// Drag model
// ---------------------------------------------------------------------------

FVector USHBallisticsSystem::ComputeDragDeceleration(
	const FVector& Velocity,
	float DragCoefficient,
	float CrossSectionCm2,
	float MassGrains) const
{
	// Convert units:
	//   CrossSection: cm^2 -> m^2
	//   Mass: grains -> kg (1 grain = 0.00006479891 kg)
	//   Velocity is in cm/s
	const float CrossSectionM2 = CrossSectionCm2 * 1e-4f;
	const float MassKg = MassGrains * 0.00006479891f;

	const float SpeedCmS = Velocity.Size();
	if (SpeedCmS < KINDA_SMALL_NUMBER || MassKg < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// Account for wind — effective velocity is bullet velocity minus wind velocity
	const FVector WindVelocityCmS = CurrentWind.WindDirection.GetSafeNormal() * CurrentWind.WindSpeedMPS * 100.0f;
	const FVector EffectiveVelocity = Velocity - WindVelocityCmS;
	const float EffectiveSpeed = EffectiveVelocity.Size();

	if (EffectiveSpeed < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// F_drag = 0.5 * rho * v^2 * Cd * A
	// But v is in cm/s, we want force in Newtons:
	//   Convert effective speed to m/s for the drag formula, then convert
	//   resulting deceleration back to cm/s^2.
	const float EffectiveSpeedMS = EffectiveSpeed / 100.0f;
	const float DragForceN = 0.5f * AirDensityKgM3 * FMath::Square(EffectiveSpeedMS) * DragCoefficient * CrossSectionM2;

	// a = F/m  (m/s^2), convert to cm/s^2
	const float DecelMS2 = DragForceN / MassKg;
	const float DecelCmS2 = DecelMS2 * 100.0f;

	// Deceleration opposes effective velocity
	const FVector DragDir = -EffectiveVelocity.GetSafeNormal();
	return DragDir * DecelCmS2;
}

// ---------------------------------------------------------------------------
// Core projectile step
// ---------------------------------------------------------------------------

void USHBallisticsSystem::StepProjectile(
	const FVector& InPosition,
	const FVector& InVelocity,
	float DeltaTime,
	const USHWeaponData* WeaponData,
	FVector& OutPosition,
	FVector& OutVelocity) const
{
	if (!WeaponData || DeltaTime <= 0.0f)
	{
		OutPosition = InPosition;
		OutVelocity = InVelocity;
		return;
	}

	const FSHBallisticCoefficients& BC = WeaponData->Ballistics;

	// Gravity (downward in UE Z-up)
	const FVector GravityAccel = FVector(0.0f, 0.0f, -GravityCmS2);

	// Drag
	const FVector DragAccel = ComputeDragDeceleration(
		InVelocity,
		BC.DragCoefficient,
		BC.CrossSectionArea,
		BC.BulletMassGrains);

	// Wind lateral push — simplified cross-wind force
	const FVector WindVelocityCmS = CurrentWind.WindDirection.GetSafeNormal() * CurrentWind.WindSpeedMPS * 100.0f;
	const FVector BulletDir = InVelocity.GetSafeNormal();
	// Only the component of wind perpendicular to bullet path affects drift
	const FVector WindCrossComponent = WindVelocityCmS - (FVector::DotProduct(WindVelocityCmS, BulletDir) * BulletDir);
	// Simplified wind drift acceleration scaled by inverse BC (lower BC = more wind effect)
	const float WindFactor = (BC.BallisticCoefficient > KINDA_SMALL_NUMBER) ? (0.1f / BC.BallisticCoefficient) : 0.0f;
	const FVector WindAccel = WindCrossComponent * WindFactor;

	// Total acceleration
	const FVector TotalAccel = GravityAccel + DragAccel + WindAccel;

	// Verlet integration for better accuracy
	OutVelocity = InVelocity + TotalAccel * DeltaTime;
	OutPosition = InPosition + (InVelocity + OutVelocity) * 0.5f * DeltaTime;
}

// ---------------------------------------------------------------------------
// Damage falloff
// ---------------------------------------------------------------------------

float USHBallisticsSystem::CalcDamageAtDistance(const USHWeaponData* WeaponData, float DistanceCm) const
{
	if (!WeaponData)
	{
		return 0.0f;
	}

	const float EffectiveRangeCm = WeaponData->EffectiveRangeM * 100.0f;
	const float MaxRangeCm = WeaponData->MaxRangeM * 100.0f;

	if (DistanceCm <= 0.0f)
	{
		return WeaponData->BaseDamage;
	}

	if (DistanceCm >= MaxRangeCm)
	{
		return 0.0f;
	}

	// Linear falloff from base damage to (base * falloff) at effective range,
	// then from that to 0 at max range.
	if (DistanceCm <= EffectiveRangeCm)
	{
		const float Alpha = DistanceCm / EffectiveRangeCm;
		const float DamageAtRange = WeaponData->BaseDamage * WeaponData->DamageFalloffAtRange;
		return FMath::Lerp(WeaponData->BaseDamage, DamageAtRange, Alpha);
	}
	else
	{
		const float DamageAtEffective = WeaponData->BaseDamage * WeaponData->DamageFalloffAtRange;
		const float Alpha = (DistanceCm - EffectiveRangeCm) / (MaxRangeCm - EffectiveRangeCm);
		return FMath::Lerp(DamageAtEffective, 0.0f, Alpha);
	}
}

// ---------------------------------------------------------------------------
// Surface thickness estimation
// ---------------------------------------------------------------------------

float USHBallisticsSystem::EstimateSurfaceThickness(
	const FHitResult& HitResult,
	const FVector& InDirection,
	float MaxDepthCm) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return MaxDepthCm;
	}

	// Trace backwards from a point beyond the entry, looking for the exit face
	const FVector TraceStart = HitResult.ImpactPoint + InDirection * MaxDepthCm;
	const FVector TraceEnd = HitResult.ImpactPoint;

	FHitResult ExitHit;
	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = true;
	Params.AddIgnoredActor(HitResult.GetActor()); // We want the same actor — remove from ignore
	// Actually, we need to hit the same actor's back face. UE doesn't trace back-faces
	// by default on complex collision. Use a simple sphere sweep as approximation.
	Params = FCollisionQueryParams(SCENE_QUERY_STAT(SHPenThickness), true);

	if (World->LineTraceSingleByChannel(ExitHit, TraceStart, TraceEnd, ECC_Visibility, Params))
	{
		// Distance between entry and exit
		return FVector::Dist(HitResult.ImpactPoint, ExitHit.ImpactPoint);
	}

	// Could not find exit — assume max depth
	return MaxDepthCm;
}

// ---------------------------------------------------------------------------
// Penetration
// ---------------------------------------------------------------------------

FSHPenetrationResult USHBallisticsSystem::TryPenetrate(
	const FHitResult& HitResult,
	const FVector& InDirection,
	float InSpeed,
	float InDamage,
	const USHWeaponData* WeaponData,
	ESHPenetrationMaterial Material) const
{
	FSHPenetrationResult Result;

	if (!WeaponData)
	{
		return Result;
	}

	const FSHPenetrationEntry* PenEntry = WeaponData->FindPenetration(Material);
	if (!PenEntry || PenEntry->MaxPenetrationCm <= 0.0f)
	{
		return Result; // Cannot penetrate this material
	}

	// Estimate actual surface thickness
	const float Thickness = EstimateSurfaceThickness(HitResult, InDirection, PenEntry->MaxPenetrationCm * 2.0f);

	if (Thickness > PenEntry->MaxPenetrationCm)
	{
		return Result; // Too thick
	}

	// Scale retention by how much of the max penetration was used
	const float PenetrationRatio = Thickness / PenEntry->MaxPenetrationCm;
	const float RetainedDamageFraction = FMath::Lerp(PenEntry->DamageRetention, PenEntry->DamageRetention * 0.5f, PenetrationRatio);
	const float RetainedVelocityFraction = FMath::Lerp(PenEntry->VelocityRetention, PenEntry->VelocityRetention * 0.5f, PenetrationRatio);

	Result.bPenetrated = true;
	Result.ExitPoint = HitResult.ImpactPoint + InDirection * Thickness;
	Result.ExitDirection = InDirection; // Slight random deflection could be added
	Result.DamageAfterPenetration = InDamage * RetainedDamageFraction;
	Result.VelocityAfterPenetration = InSpeed * RetainedVelocityFraction;

	// Add slight random deflection after penetration
	const float DeflectionAngle = FMath::RandRange(0.0f, 2.0f); // degrees
	const FVector RandomPerp = FMath::VRand();
	const FVector Perp = FVector::CrossProduct(InDirection, RandomPerp).GetSafeNormal();
	Result.ExitDirection = InDirection.RotateAngleAxis(DeflectionAngle, Perp).GetSafeNormal();

	return Result;
}

// ---------------------------------------------------------------------------
// Ricochet
// ---------------------------------------------------------------------------

FSHRicochetResult USHBallisticsSystem::TryRicochet(
	const FHitResult& HitResult,
	const FVector& InDirection,
	float InSpeed,
	float InDamage,
	ESHPenetrationMaterial Material) const
{
	FSHRicochetResult Result;

	// Calculate incidence angle (angle between bullet and surface)
	const float CosAngle = FMath::Abs(FVector::DotProduct(InDirection, HitResult.ImpactNormal));
	const float IncidenceAngleDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(CosAngle, 0.0f, 1.0f)));
	// IncidenceAngleDeg is angle from surface normal — we want angle from surface (grazing)
	const float GrazingAngleDeg = 90.0f - IncidenceAngleDeg;

	if (GrazingAngleDeg < MinRicochetAngleDeg || GrazingAngleDeg > MaxRicochetAngleDeg)
	{
		return Result; // Too steep or too shallow
	}

	// Ricochet probability: higher at shallow angles, higher off hard surfaces
	float BaseProbability = 0.0f;
	float SpeedRetention = 0.0f;

	switch (Material)
	{
	case ESHPenetrationMaterial::Steel:
		BaseProbability = 0.70f;
		SpeedRetention = 0.50f;
		break;
	case ESHPenetrationMaterial::Concrete:
		BaseProbability = 0.50f;
		SpeedRetention = 0.35f;
		break;
	case ESHPenetrationMaterial::Dirt:
	case ESHPenetrationMaterial::Sandbag:
		BaseProbability = 0.05f;
		SpeedRetention = 0.15f;
		break;
	case ESHPenetrationMaterial::Wood:
		BaseProbability = 0.15f;
		SpeedRetention = 0.20f;
		break;
	case ESHPenetrationMaterial::Glass:
		BaseProbability = 0.10f;
		SpeedRetention = 0.25f;
		break;
	default:
		BaseProbability = 0.02f;
		SpeedRetention = 0.10f;
		break;
	}

	// Scale probability by angle — more likely at shallower angles
	const float AngleFactor = 1.0f - ((GrazingAngleDeg - MinRicochetAngleDeg) / (MaxRicochetAngleDeg - MinRicochetAngleDeg));
	const float FinalProbability = BaseProbability * AngleFactor;

	if (FMath::FRand() > FinalProbability)
	{
		return Result; // No ricochet
	}

	// Reflect direction off surface normal
	const FVector Reflected = FMath::GetReflectionVector(InDirection, HitResult.ImpactNormal);

	// Add random spread to reflected direction
	const float RicochetSpreadDeg = FMath::RandRange(1.0f, 8.0f);
	const FVector SpreadAxis = FMath::VRand();
	const FVector SpreadReflected = Reflected.RotateAngleAxis(RicochetSpreadDeg, SpreadAxis).GetSafeNormal();

	Result.bRicocheted = true;
	Result.RicochetOrigin = HitResult.ImpactPoint + HitResult.ImpactNormal * 1.0f; // offset slightly off surface
	Result.RicochetDirection = SpreadReflected;
	Result.RicochetSpeed = InSpeed * SpeedRetention;
	Result.RicochetDamage = InDamage * SpeedRetention * 0.5f; // damage drops significantly

	return Result;
}

// ---------------------------------------------------------------------------
// Hitscan (close-range instant trace)
// ---------------------------------------------------------------------------

void USHBallisticsSystem::PerformHitscan(
	const FVector& Origin,
	const FVector& Direction,
	const USHWeaponData* WeaponData,
	AActor* Instigator,
	TArray<FSHBallisticHitResult>& OutHits) const
{
	UWorld* World = GetWorld();
	if (!World || !WeaponData)
	{
		return;
	}

	const float MaxTraceDist = WeaponData->HitscanThresholdM * 100.0f;
	const FVector TraceEnd = Origin + Direction * MaxTraceDist;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SHHitscan), true);
	Params.bReturnPhysicalMaterial = true;
	if (Instigator)
	{
		Params.AddIgnoredActor(Instigator);
	}

	TArray<FHitResult> Hits;
	// Multi-trace to support penetration through multiple surfaces
	World->LineTraceMultiByChannel(Hits, Origin, TraceEnd, ECC_GameTraceChannel1, Params);

	float CurrentDamage = WeaponData->BaseDamage;
	float CurrentSpeed = WeaponData->MuzzleVelocityMPS * 100.0f;

	for (const FHitResult& Hit : Hits)
	{
		if (CurrentDamage <= 0.0f || CurrentSpeed <= 0.0f)
		{
			break;
		}

		const float Distance = Hit.Distance;
		const float DamageAtDist = CalcDamageAtDistance(WeaponData, Distance);
		const float DamageRatio = (WeaponData->BaseDamage > 0.0f) ? (DamageAtDist / WeaponData->BaseDamage) : 1.0f;
		const float AdjustedDamage = CurrentDamage * DamageRatio;

		FSHBallisticHitResult BHit;
		BHit.bHit = true;
		BHit.HitLocation = Hit.ImpactPoint;
		BHit.HitNormal = Hit.ImpactNormal;
		BHit.DistanceTravelled = Distance;
		BHit.RemainingDamage = AdjustedDamage;
		BHit.RemainingVelocity = CurrentSpeed;
		BHit.HitActor = Hit.GetActor();
		BHit.HitBoneName = Hit.BoneName;
		BHit.PhysMaterial = Hit.PhysMaterial.Get();

		ESHPenetrationMaterial SHMat = PhysMaterialToSHMaterial(Hit.PhysMaterial.Get());
		BHit.SurfaceMaterial = SHMat;

		OutHits.Add(BHit);

		// Try penetration for next surface
		FSHPenetrationResult PenResult = TryPenetrate(Hit, Direction, CurrentSpeed, AdjustedDamage, WeaponData, SHMat);
		if (PenResult.bPenetrated)
		{
			CurrentDamage = PenResult.DamageAfterPenetration;
			CurrentSpeed = PenResult.VelocityAfterPenetration;
		}
		else
		{
			// Bullet stopped
			break;
		}
	}
}

// ---------------------------------------------------------------------------
// Supersonic crack
// ---------------------------------------------------------------------------

bool USHBallisticsSystem::ShouldTriggerSupersonicCrack(
	const FVector& BulletPosition,
	const FVector& BulletVelocity,
	const FVector& ListenerPosition,
	FVector& OutCrackLocation) const
{
	const float BulletSpeed = BulletVelocity.Size();
	if (BulletSpeed < SpeedOfSoundCmS)
	{
		return false; // Subsonic — no crack
	}

	// Find closest point on the bullet trajectory to the listener
	const FVector BulletDir = BulletVelocity.GetSafeNormal();
	const FVector ToListener = ListenerPosition - BulletPosition;
	const float Projection = FVector::DotProduct(ToListener, BulletDir);

	// Only trigger for bullets that have passed the listener (or are passing)
	if (Projection < 0.0f)
	{
		return false;
	}

	const FVector ClosestPoint = BulletPosition + BulletDir * Projection;
	const float DistToListener = FVector::Dist(ClosestPoint, ListenerPosition);

	if (DistToListener <= SupersonicCrackRadiusCm)
	{
		OutCrackLocation = ClosestPoint;
		return true;
	}

	return false;
}

// ---------------------------------------------------------------------------
// Explosive / fragmentation
// ---------------------------------------------------------------------------

void USHBallisticsSystem::ProcessExplosion(
	const FVector& DetonationPoint,
	const USHWeaponData* WeaponData,
	APawn* Instigator,
	AActor* DamageCauser) const
{
	UWorld* World = GetWorld();
	if (!World || !WeaponData || !WeaponData->bIsExplosive)
	{
		return;
	}

	const float InnerRadiusCm = WeaponData->ExplosiveInnerRadiusM * 100.0f;
	const float OuterRadiusCm = WeaponData->ExplosiveOuterRadiusM * 100.0f;

	// Apply radial damage
	AController* InstigatorController = Instigator ? Instigator->GetController() : nullptr;

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		World,
		WeaponData->BaseDamage,
		WeaponData->BaseDamage * 0.1f, // MinDamage
		DetonationPoint,
		InnerRadiusCm,
		OuterRadiusCm,
		1.0f, // DamageFalloff exponent
		UDamageType::StaticClass(),
		TArray<AActor*>(), // IgnoreActors
		DamageCauser,
		InstigatorController,
		ECC_Visibility);

	// Spawn shrapnel fragments
	if (WeaponData->ShrapnelCount > 0)
	{
		for (int32 i = 0; i < WeaponData->ShrapnelCount; ++i)
		{
			const FVector FragDir = FMath::VRand();
			const float FragSpeed = FMath::RandRange(20000.0f, 50000.0f); // cm/s

			// Trace for shrapnel hit
			const float FragRange = OuterRadiusCm * 1.5f;
			const FVector FragEnd = DetonationPoint + FragDir * FragRange;

			FHitResult FragHit;
			FCollisionQueryParams FragParams(SCENE_QUERY_STAT(SHShrapnel), true);
			FragParams.bReturnPhysicalMaterial = true;
			if (Instigator)
			{
				FragParams.AddIgnoredActor(Instigator);
			}
			if (DamageCauser)
			{
				FragParams.AddIgnoredActor(DamageCauser);
			}

			if (World->LineTraceSingleByChannel(FragHit, DetonationPoint, FragEnd, ECC_Visibility, FragParams))
			{
				// Apply shrapnel damage with distance falloff
				const float FragDist = FragHit.Distance;
				const float DistRatio = FMath::Clamp(FragDist / FragRange, 0.0f, 1.0f);
				const float FragDamage = WeaponData->ShrapnelDamage * (1.0f - DistRatio);

				if (FragDamage > 0.0f && FragHit.GetActor())
				{
					FPointDamageEvent DamageEvent;
					DamageEvent.Damage = FragDamage;
					DamageEvent.HitInfo = FragHit;
					DamageEvent.ShotDirection = FragDir;

					FragHit.GetActor()->TakeDamage(
						FragDamage,
						DamageEvent,
						InstigatorController,
						DamageCauser);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Physical material mapping
// ---------------------------------------------------------------------------

ESHPenetrationMaterial USHBallisticsSystem::PhysMaterialToSHMaterial(const UPhysicalMaterial* PhysMat)
{
	if (!PhysMat)
	{
		return ESHPenetrationMaterial::Dirt;
	}

	// Map based on surface type or physical material name
	// In production, this would use a data table or the SurfaceType enum
	const FString MatName = PhysMat->GetName().ToLower();

	if (MatName.Contains(TEXT("wood")))     return ESHPenetrationMaterial::Wood;
	if (MatName.Contains(TEXT("drywall")))  return ESHPenetrationMaterial::Drywall;
	if (MatName.Contains(TEXT("plaster")))  return ESHPenetrationMaterial::Drywall;
	if (MatName.Contains(TEXT("sand")))     return ESHPenetrationMaterial::Sandbag;
	if (MatName.Contains(TEXT("concrete"))) return ESHPenetrationMaterial::Concrete;
	if (MatName.Contains(TEXT("brick")))    return ESHPenetrationMaterial::Concrete;
	if (MatName.Contains(TEXT("steel")))    return ESHPenetrationMaterial::Steel;
	if (MatName.Contains(TEXT("metal")))    return ESHPenetrationMaterial::Steel;
	if (MatName.Contains(TEXT("iron")))     return ESHPenetrationMaterial::Steel;
	if (MatName.Contains(TEXT("glass")))    return ESHPenetrationMaterial::Glass;
	if (MatName.Contains(TEXT("flesh")))    return ESHPenetrationMaterial::Flesh;
	if (MatName.Contains(TEXT("body")))     return ESHPenetrationMaterial::Flesh;
	if (MatName.Contains(TEXT("dirt")))     return ESHPenetrationMaterial::Dirt;
	if (MatName.Contains(TEXT("earth")))    return ESHPenetrationMaterial::Dirt;
	if (MatName.Contains(TEXT("mud")))      return ESHPenetrationMaterial::Dirt;

	return ESHPenetrationMaterial::Dirt;
}
