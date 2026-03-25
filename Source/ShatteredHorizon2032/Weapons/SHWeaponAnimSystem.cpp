// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWeaponAnimSystem.h"

USHWeaponAnimSystem::USHWeaponAnimSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;

	/* --- Default bob configs per stance --- */
	BobStanding.HorizontalAmplitude = 1.2f;
	BobStanding.VerticalAmplitude = 0.8f;
	BobStanding.FrequencyMultiplier = 1.0f;

	BobCrouching.HorizontalAmplitude = 0.7f;
	BobCrouching.VerticalAmplitude = 0.5f;
	BobCrouching.FrequencyMultiplier = 0.8f;

	BobProne.HorizontalAmplitude = 0.3f;
	BobProne.VerticalAmplitude = 0.2f;
	BobProne.FrequencyMultiplier = 0.5f;
}

void USHWeaponAnimSystem::BeginPlay()
{
	Super::BeginPlay();

	/* Seed idle sway with a random offset so multiple weapons don't sway in sync. */
	IdleSwayTime = FMath::FRand() * 100.0f;
	BreathingTime = FMath::FRand() * 10.0f;
}

/* -----------------------------------------------------------------------
 *  Tick — drives all procedural sub-systems and composes output transform
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	/* Reset accumulators for this frame. */
	ProceduralLocationOffset = FVector::ZeroVector;
	ProceduralRotationOffset = FRotator::ZeroRotator;

	/* Run each procedural layer. Each writes into the accumulators additively. */
	TickRecoil(DeltaTime);
	TickMuzzleClimb(DeltaTime);
	TickWeaponBob(DeltaTime);
	TickADSTransition(DeltaTime);
	TickReloadMotion(DeltaTime);
	TickBreathingSway(DeltaTime);
	TickIdleSway(DeltaTime);
}

/* -----------------------------------------------------------------------
 *  Public API
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::SetRecoilPattern(const FSHRecoilPattern& InPattern)
{
	ActiveRecoilPattern = InPattern;
}

void USHWeaponAnimSystem::OnShotFired(int32 ShotIndex)
{
	/* --- Visual recoil kick --- */
	const bool bFirstShot = (ShotIndex == 0);
	const float FirstShotMul = bFirstShot ? ActiveRecoilPattern.FirstShotMultiplier : 1.0f;

	/* Vertical kick (always up). */
	const float VerticalKick = ActiveRecoilPattern.VerticalRecoil * FirstShotMul * RecoilKickPitchScale;

	/* Horizontal kick from pattern array (wrapping) or random. */
	float HorizontalKick = 0.0f;
	if (ActiveRecoilPattern.HorizontalPattern.Num() > 0)
	{
		const int32 PatternIndex = ShotIndex % ActiveRecoilPattern.HorizontalPattern.Num();
		HorizontalKick = ActiveRecoilPattern.HorizontalPattern[PatternIndex] * FirstShotMul * RecoilKickYawScale;
	}
	else
	{
		/* Fallback: random horizontal with base magnitude. */
		HorizontalKick = FMath::FRandRange(-1.0f, 1.0f) * ActiveRecoilPattern.HorizontalRecoil * FirstShotMul * RecoilKickYawScale;
	}

	/* Pushback (weapon slides toward camera). */
	const float Pushback = RecoilPushbackCm * FirstShotMul;

	/* Apply as impulse velocity into the spring system. */
	RecoilKickVelocity.X += VerticalKick * 30.0f;   // Pitch impulse
	RecoilKickVelocity.Y += HorizontalKick * 30.0f;  // Yaw impulse
	RecoilKickVelocity.Z += Pushback * 30.0f;         // Pushback impulse

	/* --- Muzzle climb accumulation --- */
	MuzzleClimbAccumulated += ActiveRecoilPattern.VerticalRecoil * FirstShotMul;
	MuzzleClimbAccumulated = FMath::Min(MuzzleClimbAccumulated, MaxMuzzleClimbDegrees);

	MuzzleClimbHorizontal += HorizontalKick * 0.5f;
}

FTransform USHWeaponAnimSystem::GetProceduralTransform() const
{
	const FQuat RotationQuat = FQuat(FRotator(
		ProceduralRotationOffset.Pitch,
		ProceduralRotationOffset.Yaw,
		ProceduralRotationOffset.Roll));

	return FTransform(RotationQuat, ProceduralLocationOffset);
}

void USHWeaponAnimSystem::SetMovementState(float Speed, bool bSprinting, ESHStance Stance)
{
	CurrentMoveSpeed = Speed;
	bIsSprinting = bSprinting;
	CurrentStance = Stance;
}

void USHWeaponAnimSystem::SetADSAlpha(float Alpha)
{
	TargetADSAlpha = FMath::Clamp(Alpha, 0.0f, 1.0f);
}

void USHWeaponAnimSystem::SetFatigueLevel(float Level)
{
	FatigueLevel = FMath::Clamp(Level, 0.0f, 1.0f);
}

void USHWeaponAnimSystem::OnReloadStarted()
{
	bIsReloading = true;
}

void USHWeaponAnimSystem::OnReloadFinished()
{
	bIsReloading = false;
}

/* -----------------------------------------------------------------------
 *  Tick: Recoil — damped spring simulation
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickRecoil(float DeltaTime)
{
	/*
	 * Simple damped spring: F = -kx - cv
	 * k = RecoverySpeed^2  (spring stiffness)
	 * c = 2 * damping * RecoverySpeed  (damping coefficient)
	 */
	const float k = RecoilRecoverySpeed * RecoilRecoverySpeed;
	const float c = 2.0f * RecoilRecoveryDamping * RecoilRecoverySpeed;

	/* Acceleration = -k * displacement - c * velocity */
	const FVector Accel = -k * RecoilKickCurrent - c * RecoilKickVelocity;

	/* Semi-implicit Euler integration. */
	RecoilKickVelocity += Accel * DeltaTime;
	RecoilKickCurrent += RecoilKickVelocity * DeltaTime;

	/* Snap to zero when nearly settled to avoid micro-jitter. */
	if (RecoilKickCurrent.SizeSquared() < KINDA_SMALL_NUMBER && RecoilKickVelocity.SizeSquared() < KINDA_SMALL_NUMBER)
	{
		RecoilKickCurrent = FVector::ZeroVector;
		RecoilKickVelocity = FVector::ZeroVector;
	}

	/* Map spring output to transform:
	 *   X = pitch rotation offset (up kick)
	 *   Y = yaw rotation offset (horizontal kick)
	 *   Z = forward pushback (negative X in local weapon space)
	 */
	ProceduralRotationOffset.Pitch += RecoilKickCurrent.X;
	ProceduralRotationOffset.Yaw += RecoilKickCurrent.Y;
	ProceduralLocationOffset.X -= RecoilKickCurrent.Z; // Pushback along weapon forward axis
}

/* -----------------------------------------------------------------------
 *  Tick: Muzzle Climb — accumulated climb with mouse-fightable recovery
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickMuzzleClimb(float DeltaTime)
{
	if (FMath::IsNearlyZero(MuzzleClimbAccumulated) && FMath::IsNearlyZero(MuzzleClimbHorizontal))
	{
		return;
	}

	/*
	 * Recovery uses a power curve: recovery is faster near zero climb and slower
	 * at high climb, encouraging the player to actively counter with mouse input.
	 * The RecoveryRate from the recoil pattern serves as the base decay speed.
	 */
	const float EffectiveRecoveryRate = ActiveRecoilPattern.RecoveryRate > 0.0f
		? ActiveRecoilPattern.RecoveryRate
		: MuzzleClimbRecoveryRate;

	/* Normalized climb position for curve evaluation. */
	const float NormalizedClimb = FMath::Clamp(
		MuzzleClimbAccumulated / FMath::Max(MaxMuzzleClimbDegrees, 0.01f), 0.0f, 1.0f);

	/* Curved recovery: at low climb the recovery is fast, at high climb it slows. */
	const float CurveMultiplier = FMath::Pow(1.0f - NormalizedClimb, MuzzleClimbRecoveryCurveExp);
	const float RecoveryThisFrame = EffectiveRecoveryRate * (0.3f + 0.7f * CurveMultiplier) * DeltaTime;

	/* Decay vertical. */
	if (MuzzleClimbAccumulated > 0.0f)
	{
		MuzzleClimbAccumulated = FMath::Max(MuzzleClimbAccumulated - RecoveryThisFrame, 0.0f);
	}
	else
	{
		MuzzleClimbAccumulated = FMath::Min(MuzzleClimbAccumulated + RecoveryThisFrame, 0.0f);
	}

	/* Decay horizontal. */
	const float HorizRecovery = EffectiveRecoveryRate * 1.2f * DeltaTime;
	if (MuzzleClimbHorizontal > 0.0f)
	{
		MuzzleClimbHorizontal = FMath::Max(MuzzleClimbHorizontal - HorizRecovery, 0.0f);
	}
	else
	{
		MuzzleClimbHorizontal = FMath::Min(MuzzleClimbHorizontal + HorizRecovery, 0.0f);
	}

	/* Apply to rotation offset. */
	ProceduralRotationOffset.Pitch += MuzzleClimbAccumulated;
	ProceduralRotationOffset.Yaw += MuzzleClimbHorizontal;
}

/* -----------------------------------------------------------------------
 *  Tick: Weapon Bob — velocity and stance driven
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickWeaponBob(float DeltaTime)
{
	/* Determine target blend alpha based on movement. */
	const float SpeedFraction = FMath::Clamp(CurrentMoveSpeed / BobMaxSpeedReference, 0.0f, 1.0f);
	const float TargetBlend = (SpeedFraction > 0.01f) ? SpeedFraction : 0.0f;

	/* Smoothly blend bob in/out. */
	BobBlendAlpha = FMath::FInterpTo(BobBlendAlpha, TargetBlend, DeltaTime, BobBlendSpeed);

	if (BobBlendAlpha < KINDA_SMALL_NUMBER)
	{
		BobOffset = FVector::ZeroVector;
		return;
	}

	/* Get stance-appropriate config. */
	const FSHWeaponBobConfig& Config = GetBobConfigForStance(CurrentStance);

	/* Calculate frequency. */
	const float SprintMul = bIsSprinting ? BobSprintMultiplier : 1.0f;
	const float Frequency = BobBaseFrequency * Config.FrequencyMultiplier * SprintMul;

	/* Advance time proportionally to speed. */
	BobTime += DeltaTime * Frequency * SpeedFraction;

	/* Figure-eight bob pattern:
	 *   Horizontal: sin(time)
	 *   Vertical:   sin(2 * time)  — twice the frequency for a natural gait feel
	 */
	const float HorizBob = FMath::Sin(BobTime * PI * 2.0f) * Config.HorizontalAmplitude * SprintMul;
	const float VertBob = FMath::Sin(BobTime * PI * 4.0f) * Config.VerticalAmplitude * SprintMul;

	BobOffset.X = 0.0f;
	BobOffset.Y = HorizBob * BobBlendAlpha;
	BobOffset.Z = VertBob * BobBlendAlpha;

	/* Reduce bob while ADS. */
	const float ADSBobReduction = FMath::Lerp(1.0f, 0.15f, CurrentADSAlpha);
	BobOffset *= ADSBobReduction;

	ProceduralLocationOffset += BobOffset;

	/* Slight roll from horizontal bob for added immersion. */
	ProceduralRotationOffset.Roll += HorizBob * 0.3f * BobBlendAlpha * ADSBobReduction;
}

/* -----------------------------------------------------------------------
 *  Tick: ADS Transition — position offset lerp
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickADSTransition(float DeltaTime)
{
	/* Smoothly interpolate toward target ADS alpha. */
	CurrentADSAlpha = FMath::FInterpTo(CurrentADSAlpha, TargetADSAlpha, DeltaTime, ADSLerpSpeed);

	if (CurrentADSAlpha < KINDA_SMALL_NUMBER)
	{
		return;
	}

	/* Lerp position and rotation offsets. */
	ProceduralLocationOffset += ADSPositionOffset * CurrentADSAlpha;

	ProceduralRotationOffset.Pitch += ADSRotationOffset.Pitch * CurrentADSAlpha;
	ProceduralRotationOffset.Yaw += ADSRotationOffset.Yaw * CurrentADSAlpha;
	ProceduralRotationOffset.Roll += ADSRotationOffset.Roll * CurrentADSAlpha;
}

/* -----------------------------------------------------------------------
 *  Tick: Reload Motion — weapon dip and tilt
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickReloadMotion(float DeltaTime)
{
	const float TargetAlpha = bIsReloading ? 1.0f : 0.0f;
	ReloadMotionAlpha = FMath::FInterpTo(ReloadMotionAlpha, TargetAlpha, DeltaTime, ReloadMotionSpeed);

	if (ReloadMotionAlpha < KINDA_SMALL_NUMBER)
	{
		return;
	}

	/*
	 * Reload motion profile:
	 *   - Weapon dips downward (negative Z).
	 *   - Weapon tilts (roll) to simulate magazine manipulation.
	 *   - Slight forward push to sell the arm extension.
	 *
	 * Use a smoothstep-like shape for a natural feel:
	 *   smoothAlpha rises quickly, holds, then falls.
	 */
	const float SmoothAlpha = FMath::SmoothStep(0.0f, 1.0f, ReloadMotionAlpha);

	ProceduralLocationOffset.Z -= ReloadDipAmountCm * SmoothAlpha;
	ProceduralLocationOffset.X += ReloadDipAmountCm * 0.3f * SmoothAlpha; // Slight forward push
	ProceduralRotationOffset.Roll += ReloadTiltDegrees * SmoothAlpha;
	ProceduralRotationOffset.Pitch -= ReloadTiltDegrees * 0.4f * SmoothAlpha; // Slight downward tilt
}

/* -----------------------------------------------------------------------
 *  Tick: Breathing Sway — fatigue-modulated, active during ADS/scope
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickBreathingSway(float DeltaTime)
{
	/* Breathing sway is primarily visible when ADS. Scale it with ADS alpha. */
	if (CurrentADSAlpha < 0.1f)
	{
		BreathingSwayOffset = FVector::ZeroVector;
		return;
	}

	/* Calculate amplitude based on fatigue. */
	const float Amplitude = BreathingSwayBaseAmplitude
		+ BreathingSwayFatigueAmplitude * FatigueLevel;

	/* Calculate frequency based on fatigue (tired = faster breathing). */
	const float Frequency = BreathingFrequency
		+ BreathingFatigueFrequencyBonus * FatigueLevel;

	BreathingTime += DeltaTime * Frequency;

	/*
	 * Breathing is an elliptical pattern:
	 *   Vertical: slow sinusoid (primary breathing motion).
	 *   Horizontal: slower sinusoid at an irrational ratio to avoid repeating loops.
	 */
	const float VerticalSway = FMath::Sin(BreathingTime * PI * 2.0f) * Amplitude;
	const float HorizontalSway = FMath::Sin(BreathingTime * PI * 2.0f * 0.618f) * Amplitude * 0.6f;

	BreathingSwayOffset.X = 0.0f;
	BreathingSwayOffset.Y = HorizontalSway;
	BreathingSwayOffset.Z = VerticalSway;

	/* Scale by ADS alpha so it fades in during scope transition. */
	ProceduralLocationOffset += BreathingSwayOffset * CurrentADSAlpha;

	/* Slight rotation from breathing. */
	ProceduralRotationOffset.Pitch += VerticalSway * 0.3f * CurrentADSAlpha;
	ProceduralRotationOffset.Yaw += HorizontalSway * 0.2f * CurrentADSAlpha;
}

/* -----------------------------------------------------------------------
 *  Tick: Idle Sway — Perlin noise driven, visible when standing still
 * --------------------------------------------------------------------- */

void USHWeaponAnimSystem::TickIdleSway(float DeltaTime)
{
	/* Idle sway is visible when NOT moving. Fade out with movement. */
	const float SpeedFraction = FMath::Clamp(CurrentMoveSpeed / FMath::Max(BobMaxSpeedReference, 1.0f), 0.0f, 1.0f);
	const float IdleBlend = 1.0f - FMath::Clamp(SpeedFraction * 4.0f, 0.0f, 1.0f); // Quick fade-out

	if (IdleBlend < KINDA_SMALL_NUMBER)
	{
		IdleSwayOffset = FVector::ZeroVector;
		IdleSwayRotation = FRotator::ZeroRotator;
		return;
	}

	IdleSwayTime += DeltaTime * IdleSwayFrequency;

	/*
	 * Sample Perlin noise at different offsets for each axis to get
	 * uncorrelated, organic-feeling motion.
	 */
	const float NoiseX = SamplePerlinNoise1D(IdleSwayTime);
	const float NoiseY = SamplePerlinNoise1D(IdleSwayTime + 137.5f);   // Offset to decorrelate
	const float NoiseZ = SamplePerlinNoise1D(IdleSwayTime + 293.7f);
	const float NoisePitch = SamplePerlinNoise1D(IdleSwayTime + 412.3f);
	const float NoiseYaw = SamplePerlinNoise1D(IdleSwayTime + 571.9f);

	/* Reduce idle sway when ADS (scoped breathing takes over). */
	const float ADSReduction = FMath::Lerp(1.0f, 0.2f, CurrentADSAlpha);
	const float FinalBlend = IdleBlend * ADSReduction;

	IdleSwayOffset.X = 0.0f;
	IdleSwayOffset.Y = NoiseY * IdleSwayAmplitude * FinalBlend;
	IdleSwayOffset.Z = NoiseZ * IdleSwayAmplitude * FinalBlend;

	IdleSwayRotation.Pitch = NoisePitch * IdleSwayRotationDegrees * FinalBlend;
	IdleSwayRotation.Yaw = NoiseYaw * IdleSwayRotationDegrees * FinalBlend;
	IdleSwayRotation.Roll = NoiseX * IdleSwayRotationDegrees * 0.5f * FinalBlend;

	ProceduralLocationOffset += IdleSwayOffset;
	ProceduralRotationOffset += IdleSwayRotation;
}

/* -----------------------------------------------------------------------
 *  Helpers
 * --------------------------------------------------------------------- */

const FSHWeaponBobConfig& USHWeaponAnimSystem::GetBobConfigForStance(ESHStance InStance) const
{
	switch (InStance)
	{
	case ESHStance::Crouching:
		return BobCrouching;
	case ESHStance::Prone:
		return BobProne;
	case ESHStance::Standing:
	default:
		return BobStanding;
	}
}

float USHWeaponAnimSystem::SamplePerlinNoise1D(float X) const
{
	/*
	 * Multi-octave Perlin noise using FMath::PerlinNoise1D.
	 * Three octaves for organic feel: base + 0.5 * higher freq + 0.25 * highest freq.
	 * Returns a value roughly in [-1, 1].
	 */
	const float Octave1 = FMath::PerlinNoise1D(X);
	const float Octave2 = FMath::PerlinNoise1D(X * 2.13f) * 0.5f;
	const float Octave3 = FMath::PerlinNoise1D(X * 4.37f) * 0.25f;

	/* Normalize the sum: max theoretical magnitude = 1 + 0.5 + 0.25 = 1.75. */
	return (Octave1 + Octave2 + Octave3) / 1.75f;
}
