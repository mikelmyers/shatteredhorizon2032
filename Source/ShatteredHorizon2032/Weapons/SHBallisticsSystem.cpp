// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHBallisticsSystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/DamageType.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

/* -----------------------------------------------------------------------
 *  Lifecycle
 * --------------------------------------------------------------------- */

void USHBallisticsSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitializeMaterialTable();
}

void USHBallisticsSystem::Deinitialize()
{
	Super::Deinitialize();
}

void USHBallisticsSystem::InitializeMaterialTable()
{
	MaterialTable.Empty();

	//                                         Material                        Density   RicochetAngle  RicochetProb  SpeedRetain
	MaterialTable.Add({ ESHPenetrableMaterial::Drywall,   0.08f,  5.0f,   0.05f,  0.10f });
	MaterialTable.Add({ ESHPenetrableMaterial::Glass,     0.10f,  8.0f,   0.10f,  0.15f });
	MaterialTable.Add({ ESHPenetrableMaterial::Wood,      0.25f,  12.0f,  0.15f,  0.25f });
	MaterialTable.Add({ ESHPenetrableMaterial::Dirt,      0.35f,  10.0f,  0.10f,  0.20f });
	MaterialTable.Add({ ESHPenetrableMaterial::Sandbag,   0.50f,  8.0f,   0.05f,  0.15f });
	MaterialTable.Add({ ESHPenetrableMaterial::Concrete,  0.75f,  20.0f,  0.40f,  0.35f });
	MaterialTable.Add({ ESHPenetrableMaterial::Steel,     1.00f,  30.0f,  0.60f,  0.40f });
	MaterialTable.Add({ ESHPenetrableMaterial::Flesh,     0.05f,  0.0f,   0.00f,  0.00f });
}

/* -----------------------------------------------------------------------
 *  Simulation Step
 * --------------------------------------------------------------------- */

void USHBallisticsSystem::StepSimulation(
	FVector& Position,
	FVector& Velocity,
	const FSHBallisticCoefficients& BC,
	float DeltaTime) const
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	// Sub-stepping for accuracy at high velocities
	// Use a fixed sub-step of ~2ms for stable integration at supersonic speeds
	constexpr float MaxSubStep = 0.002f;
	const int32 NumSubSteps = FMath::CeilToInt(DeltaTime / MaxSubStep);
	const float SubDelta = DeltaTime / static_cast<float>(NumSubSteps);

	for (int32 i = 0; i < NumSubSteps; ++i)
	{
		// --- Forces ---

		// 1. Gravity (downward in UE Z-up)
		const FVector GravityForce(0.0f, 0.0f, -GravityCmS2);

		// 2. Aerodynamic drag
		const FVector DragAccel = CalculateDragForce(Velocity, BC);

		// 3. Wind
		const FVector WindAccel = CalculateWindForce(Velocity, BC);

		// --- Integration (Velocity Verlet) ---
		const FVector TotalAccel = GravityForce + DragAccel + WindAccel;

		// Update position: x(t+dt) = x(t) + v(t)*dt + 0.5*a(t)*dt^2
		Position += Velocity * SubDelta + 0.5f * TotalAccel * SubDelta * SubDelta;

		// Update velocity: v(t+dt) = v(t) + a(t)*dt
		Velocity += TotalAccel * SubDelta;
	}
}

FVector USHBallisticsSystem::CalculateDragForce(const FVector& Velocity, const FSHBallisticCoefficients& BC) const
{
	const float Speed = Velocity.Size();
	if (Speed < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// Drag equation: F_drag = 0.5 * rho * v^2 * Cd * A
	// Acceleration = F_drag / mass
	// All units: mass in kg, area in m^2, velocity in cm/s -> convert

	const float SpeedMS = Speed / 100.0f;         // cm/s -> m/s
	const float AreaM2 = BC.CrossSectionCm2 / 10000.0f; // cm^2 -> m^2
	const float MassKg = BC.BulletMassGrams / 1000.0f;  // g -> kg

	if (MassKg < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// Use ballistic coefficient to modify drag:
	// BC = mass / (Cd * diameter^2) in standard form
	// Higher BC = less drag. We use the explicit Cd from the data.
	float Cd = BC.DragCoefficient;

	// Transonic drag rise (Mach 0.8 - 1.2) — increase drag coefficient
	const float Mach = SpeedMS / (SpeedOfSoundCmS / 100.0f); // Speed of sound in m/s
	if (Mach > 0.8f && Mach < 1.2f)
	{
		// Empirical drag rise in transonic regime
		const float TransonicFactor = 1.0f + 0.5f * FMath::Sin((Mach - 0.8f) / 0.4f * PI);
		Cd *= TransonicFactor;
	}
	else if (Mach >= 1.2f)
	{
		// Supersonic wave drag (simplified)
		Cd *= (1.0f + 0.15f / FMath::Max(Mach - 1.0f, 0.01f));
	}

	const float DragForceMag = 0.5f * AirDensityKgM3 * SpeedMS * SpeedMS * Cd * AreaM2;
	const float DragAccelMS2 = DragForceMag / MassKg;

	// Convert back to cm/s^2 and oppose velocity direction
	const FVector DragDir = -Velocity.GetSafeNormal();
	return DragDir * (DragAccelMS2 * 100.0f);
}

FVector USHBallisticsSystem::CalculateWindForce(const FVector& Velocity, const FSHBallisticCoefficients& BC) const
{
	if (CurrentWind.SpeedMPS < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const float MassKg = BC.BulletMassGrams / 1000.0f;
	const float AreaM2 = BC.CrossSectionCm2 / 10000.0f;

	if (MassKg < KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	// Wind as a crosswind component relative to bullet flight path
	const FVector BulletDir = Velocity.GetSafeNormal();

	// Effective wind with gust variation
	float EffectiveWindSpeed = CurrentWind.SpeedMPS;
	if (CurrentWind.GustVariation > 0.0f)
	{
		// Perlin-like variation using time. Use a simple sin approximation.
		const float GustFactor = 1.0f + CurrentWind.GustVariation *
			FMath::Sin(GetWorld() ? GetWorld()->GetTimeSeconds() * 0.7f : 0.0f);
		EffectiveWindSpeed *= GustFactor;
	}

	const FVector WindVelocityCmS = CurrentWind.Direction.GetSafeNormal() * (EffectiveWindSpeed * 100.0f);

	// Relative wind velocity to the bullet
	const FVector RelativeWind = WindVelocityCmS - Velocity;

	// Cross-wind component (perpendicular to bullet flight path)
	const FVector CrossWind = RelativeWind - (FVector::DotProduct(RelativeWind, BulletDir) * BulletDir);

	const float CrossWindSpeedMS = CrossWind.Size() / 100.0f;

	// Wind force: simplified aerodynamic force from crosswind
	// Use a reduced drag coefficient for lateral force (roughly Cd * 0.5)
	const float Cd = BC.DragCoefficient * 0.5f;
	const float WindForceMag = 0.5f * AirDensityKgM3 * CrossWindSpeedMS * CrossWindSpeedMS * Cd * AreaM2;
	const float WindAccelMS2 = WindForceMag / MassKg;

	if (CrossWind.IsNearlyZero())
	{
		return FVector::ZeroVector;
	}

	return CrossWind.GetSafeNormal() * (WindAccelMS2 * 100.0f);
}

/* -----------------------------------------------------------------------
 *  Damage Falloff
 * --------------------------------------------------------------------- */

float USHBallisticsSystem::CalculateDamageAtDistance(
	float BaseDamage,
	float FalloffStartCm,
	float MaxRangeCm,
	float MinDamageMultiplier,
	float DistanceTraveledCm)
{
	if (DistanceTraveledCm <= FalloffStartCm)
	{
		return BaseDamage;
	}

	const float FalloffRange = MaxRangeCm - FalloffStartCm;
	if (FalloffRange <= 0.0f)
	{
		return BaseDamage * MinDamageMultiplier;
	}

	const float Alpha = FMath::Clamp(
		(DistanceTraveledCm - FalloffStartCm) / FalloffRange, 0.0f, 1.0f);

	// Use a slight curve for more realistic falloff (not purely linear)
	const float CurvedAlpha = FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 1.5f);

	return BaseDamage * FMath::Lerp(1.0f, MinDamageMultiplier, CurvedAlpha);
}

/* -----------------------------------------------------------------------
 *  Impact Evaluation (Penetration + Ricochet)
 * --------------------------------------------------------------------- */

bool USHBallisticsSystem::EvaluateImpact(
	const FHitResult& HitResult,
	const FVector& IncomingVelocity,
	float CurrentDamage,
	const FSHPenetrationEntry& PenetrationData,
	float MuzzleVelocityCmS,
	FSHBallisticHitResult& OutResult) const
{
	OutResult = FSHBallisticHitResult();
	OutResult.HitResult = HitResult;
	OutResult.ImpactVelocity = IncomingVelocity.Size();
	OutResult.DamageAtImpact = CurrentDamage;

	// Get material properties for ricochet evaluation
	const ESHPenetrableMaterial MatType = ClassifyMaterial(HitResult);
	const FSHMaterialBallisticProps MatProps = GetMaterialProperties(MatType);

	// --- Ricochet check first (shallow angle hits ricochet before penetrating) ---
	FVector RicochetDir;
	float RicochetSpeedRetain;

	if (EvaluateRicochet(HitResult, IncomingVelocity, MatProps, RicochetDir, RicochetSpeedRetain))
	{
		OutResult.bRicocheted = true;
		OutResult.RicochetDirection = RicochetDir;
		OutResult.RicochetSpeed = IncomingVelocity.Size() * RicochetSpeedRetain;
		OutResult.RicochetDamage = CurrentDamage * RicochetSpeedRetain * 0.5f; // Damage reduction on ricochet
		return true;
	}

	// --- Penetration check ---
	if (PenetrationData.MaxPenetrationCm <= 0.0f)
	{
		// Cannot penetrate this material at all
		return false;
	}

	// Scale penetration capability with remaining velocity relative to muzzle
	const float VelocityRatio = (MuzzleVelocityCmS > 0.0f)
		? (IncomingVelocity.Size() / MuzzleVelocityCmS)
		: 0.0f;

	// Effective penetration depth scales with velocity^2 (kinetic energy)
	const float EffectivePenCm = PenetrationData.MaxPenetrationCm * VelocityRatio * VelocityRatio;

	// We need to know the thickness of what we hit.
	// Perform a reverse trace from behind the surface to estimate thickness.
	const FVector TraceDir = IncomingVelocity.GetSafeNormal();
	const float MaxProbeDistance = EffectivePenCm; // Only probe up to our penetration capability

	FHitResult ExitHit;
	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;

	const FVector ProbeStart = HitResult.ImpactPoint + TraceDir * (MaxProbeDistance + 1.0f);
	const FVector ProbeEnd = HitResult.ImpactPoint - TraceDir * 1.0f;

	bool bFoundExit = false;
	float ThicknessCm = MaxProbeDistance; // Assume worst case

	if (UWorld* World = GetWorld())
	{
		if (World->LineTraceSingleByChannel(ExitHit, ProbeStart, ProbeEnd,
			ECC_GameTraceChannel1, QueryParams))
		{
			ThicknessCm = FVector::Dist(HitResult.ImpactPoint, ExitHit.ImpactPoint);
			bFoundExit = true;
		}
	}

	// Can we penetrate?
	if (ThicknessCm > EffectivePenCm)
	{
		// Too thick — round stops inside
		return false;
	}

	// Calculate exit parameters
	const float PenetrationFraction = ThicknessCm / FMath::Max(EffectivePenCm, 0.01f);
	const float VelocityRetain = PenetrationData.VelocityRetention * (1.0f - PenetrationFraction * 0.5f);
	const float DamageRetain = PenetrationData.DamageRetention * (1.0f - PenetrationFraction * 0.5f);

	// Slight deflection through material
	const float DeflectionAngle = PenetrationFraction * 3.0f; // Up to 3 degrees deflection
	const FVector RandomPerp = FVector::CrossProduct(TraceDir,
		FMath::Abs(TraceDir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	const FVector DeflectedDir = TraceDir.RotateAngleAxis(
		FMath::FRandRange(-DeflectionAngle, DeflectionAngle), RandomPerp);

	OutResult.bPenetrated = true;
	OutResult.PostPenetrationVelocity = DeflectedDir * IncomingVelocity.Size() * VelocityRetain;
	OutResult.PostPenetrationDamage = CurrentDamage * DamageRetain;

	return true;
}

/* -----------------------------------------------------------------------
 *  Ricochet
 * --------------------------------------------------------------------- */

bool USHBallisticsSystem::EvaluateRicochet(
	const FHitResult& HitResult,
	const FVector& IncomingVelocity,
	const FSHMaterialBallisticProps& MaterialProps,
	FVector& OutRicochetDir,
	float& OutSpeedRetention) const
{
	if (MaterialProps.BaseRicochetProbability <= 0.0f)
	{
		return false;
	}

	const FVector InDir = IncomingVelocity.GetSafeNormal();
	const FVector Normal = HitResult.ImpactNormal;

	// Calculate angle of incidence (angle between incoming direction and surface)
	// Dot of -InDir and Normal gives cos(angle from normal)
	// We want the grazing angle (angle from surface, not from normal)
	const float CosAngleFromNormal = FVector::DotProduct(-InDir, Normal);
	const float AngleFromNormal = FMath::Acos(FMath::Clamp(CosAngleFromNormal, 0.0f, 1.0f));
	const float GrazingAngleDeg = 90.0f - FMath::RadiansToDegrees(AngleFromNormal);

	// Only ricochet at shallow (grazing) angles
	if (GrazingAngleDeg > MaterialProps.RicochetAngleThreshold)
	{
		return false;
	}

	// Probability increases as angle gets shallower
	const float AngleAlpha = 1.0f - (GrazingAngleDeg / FMath::Max(MaterialProps.RicochetAngleThreshold, 1.0f));
	const float RicochetProb = MaterialProps.BaseRicochetProbability * AngleAlpha;

	if (FMath::FRand() >= RicochetProb)
	{
		return false;
	}

	// Reflect the velocity off the surface
	OutRicochetDir = FMath::GetReflectionVector(InDir, Normal);

	// Add some random scatter (more scatter at steeper angles)
	const float ScatterDeg = FMath::FRandRange(1.0f, 5.0f) * (1.0f - AngleAlpha);
	const FVector RandomPerp = FVector::CrossProduct(OutRicochetDir,
		FMath::Abs(OutRicochetDir.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector).GetSafeNormal();
	OutRicochetDir = OutRicochetDir.RotateAngleAxis(
		FMath::FRandRange(-ScatterDeg, ScatterDeg), RandomPerp);
	OutRicochetDir.Normalize();

	OutSpeedRetention = MaterialProps.RicochetSpeedRetention;

	return true;
}

/* -----------------------------------------------------------------------
 *  Material Lookup
 * --------------------------------------------------------------------- */

FSHMaterialBallisticProps USHBallisticsSystem::GetMaterialProperties(ESHPenetrableMaterial Material) const
{
	for (const FSHMaterialBallisticProps& Props : MaterialTable)
	{
		if (Props.MaterialType == Material)
		{
			return Props;
		}
	}

	// Default — treat as wood
	FSHMaterialBallisticProps Default;
	Default.MaterialType = Material;
	Default.DensityFactor = 0.25f;
	Default.RicochetAngleThreshold = 12.0f;
	Default.BaseRicochetProbability = 0.15f;
	Default.RicochetSpeedRetention = 0.25f;
	return Default;
}

ESHPenetrableMaterial USHBallisticsSystem::ClassifyMaterial(const FHitResult& HitResult) const
{
	// Map UE physical material surface types to our penetration materials.
	// In a production setup, you'd use a data table or custom physical material subclass.
	if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get())
	{
		switch (PhysMat->SurfaceType)
		{
		case EPhysicalSurface::SurfaceType1:  return ESHPenetrableMaterial::Concrete;
		case EPhysicalSurface::SurfaceType2:  return ESHPenetrableMaterial::Steel;
		case EPhysicalSurface::SurfaceType3:  return ESHPenetrableMaterial::Wood;
		case EPhysicalSurface::SurfaceType4:  return ESHPenetrableMaterial::Drywall;
		case EPhysicalSurface::SurfaceType5:  return ESHPenetrableMaterial::Dirt;
		case EPhysicalSurface::SurfaceType6:  return ESHPenetrableMaterial::Sandbag;
		case EPhysicalSurface::SurfaceType7:  return ESHPenetrableMaterial::Glass;
		case EPhysicalSurface::SurfaceType8:  return ESHPenetrableMaterial::Flesh;
		default: break;
		}
	}

	// Default fallback based on hit component properties
	return ESHPenetrableMaterial::Concrete;
}

/* -----------------------------------------------------------------------
 *  Supersonic Crack
 * --------------------------------------------------------------------- */

bool USHBallisticsSystem::ShouldTriggerSupersonicCrack(
	const FVector& ProjectilePosition,
	const FVector& ProjectileVelocity,
	const FVector& ListenerPosition,
	float NearMissRadiusCm)
{
	const float Speed = ProjectileVelocity.Size();

	// Must be supersonic (speed of sound ~343 m/s = 34300 cm/s)
	if (Speed < SpeedOfSoundCmS)
	{
		return false;
	}

	// Calculate closest approach distance using point-to-line distance
	const FVector BulletDir = ProjectileVelocity.GetSafeNormal();
	const FVector ToListener = ListenerPosition - ProjectilePosition;

	// Project listener position onto bullet flight path
	const float ProjectionLength = FVector::DotProduct(ToListener, BulletDir);

	// Only trigger for bullets that have passed or are passing the listener (not approaching from far away)
	if (ProjectionLength < -NearMissRadiusCm)
	{
		return false;
	}

	// Perpendicular distance from bullet path to listener
	const FVector ClosestPoint = ProjectilePosition + BulletDir * ProjectionLength;
	const float Distance = FVector::Dist(ClosestPoint, ListenerPosition);

	return Distance <= NearMissRadiusCm;
}

/* -----------------------------------------------------------------------
 *  Crack-Thump Delay (Doctrine: time-delayed muzzle report encodes distance)
 * --------------------------------------------------------------------- */

float USHBallisticsSystem::ComputeCrackThumpDelay(
	const FVector& ShooterPosition,
	const FVector& ListenerPosition)
{
	// The supersonic crack is heard essentially at bullet-pass time.
	// The muzzle report (thump) travels at the speed of sound from the shooter.
	// Delay = distance(shooter, listener) / SpeedOfSound
	// This gives the player an instinctive range estimate.
	const float DistanceCm = FVector::Dist(ShooterPosition, ListenerPosition);
	const float DelaySec = DistanceCm / SpeedOfSoundCmS;

	return DelaySec;
}

float USHBallisticsSystem::ComputeMuzzleReportDelay(
	const FVector& ShooterPosition,
	const FVector& ListenerPosition)
{
	const float DistanceCm = FVector::Dist(ShooterPosition, ListenerPosition);
	return DistanceCm / SpeedOfSoundCmS;
}

/* -----------------------------------------------------------------------
 *  Fragmentation
 * --------------------------------------------------------------------- */

void USHBallisticsSystem::SpawnFragmentation(
	const FVector& Origin,
	const FSHFragmentationParams& Params,
	AController* Instigator,
	AActor* DamageCauser) const
{
	UWorld* World = GetWorld();
	if (!World || Params.FragmentCount <= 0)
	{
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.bReturnPhysicalMaterial = true;
	if (DamageCauser)
	{
		QueryParams.AddIgnoredActor(DamageCauser);
	}

	// Track actors already damaged to avoid double-hitting from multiple fragments
	TMap<AActor*, float> DamagedActors;

	for (int32 i = 0; i < Params.FragmentCount; ++i)
	{
		// Random direction in a sphere (biased slightly upward for ground detonation)
		FVector FragDir = FMath::VRandCone(FVector::UpVector, FMath::DegreesToRadians(85.0f));
		if (FMath::FRand() < 0.3f)
		{
			// Some fragments go downward too
			FragDir.Z = -FMath::Abs(FragDir.Z) * 0.5f;
		}
		FragDir.Normalize();

		FHitResult FragHit;
		const FVector TraceEnd = Origin + FragDir * Params.OuterRadiusCm;

		if (World->LineTraceSingleByChannel(FragHit, Origin, TraceEnd,
			ECC_GameTraceChannel1, QueryParams))
		{
			AActor* HitActor = FragHit.GetActor();
			if (!HitActor)
			{
				continue;
			}

			const float Distance = FragHit.Distance;

			// Damage falloff: full damage within inner radius, linear falloff to outer
			float DamageMult = 1.0f;
			if (Distance > Params.InnerRadiusCm)
			{
				const float FalloffRange = Params.OuterRadiusCm - Params.InnerRadiusCm;
				if (FalloffRange > 0.0f)
				{
					DamageMult = 1.0f - FMath::Clamp(
						(Distance - Params.InnerRadiusCm) / FalloffRange, 0.0f, 1.0f);
				}
			}

			const float FragDamage = Params.FragmentDamage * DamageMult;

			// Accumulate damage per actor (take highest fragment damage)
			if (float* ExistingDamage = DamagedActors.Find(HitActor))
			{
				*ExistingDamage = FMath::Max(*ExistingDamage, FragDamage);
			}
			else
			{
				DamagedActors.Add(HitActor, FragDamage);
			}
		}
	}

	// Apply accumulated damage
	for (const auto& Pair : DamagedActors)
	{
		if (Pair.Key && Pair.Value > 0.0f)
		{
			FPointDamageEvent DamageEvent;
			DamageEvent.Damage = Pair.Value;

			Pair.Key->TakeDamage(
				Pair.Value,
				FDamageEvent(DamageEvent.GetTypeID()),
				Instigator,
				DamageCauser);
		}
	}
}

/* -----------------------------------------------------------------------
 *  Tracer Utility
 * --------------------------------------------------------------------- */

bool USHBallisticsSystem::IsTracerRound(int32 RoundNumber, int32 TracerInterval)
{
	if (TracerInterval <= 0)
	{
		return false;
	}

	return (RoundNumber % TracerInterval) == 0;
}
