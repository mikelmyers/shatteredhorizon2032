// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCameraSystem.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"

USHCameraSystem::USHCameraSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void USHCameraSystem::BeginPlay()
{
	Super::BeginPlay();

	if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
	{
		// Find the first-person camera on the owning character.
		OwnerCamera = Owner->FindComponentByClass<UCameraComponent>();
	}

	CurrentFOV = DefaultHipFOV;
	TargetFOV = DefaultHipFOV;
}

void USHCameraSystem::TickComponent(float DeltaTime, ELevelTick TickType,
									FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!OwnerCamera)
	{
		return;
	}

	TickHeadBob(DeltaTime);
	TickFOVTransition(DeltaTime);
	TickSuppressionFX(DeltaTime);
	TickScreenPunch(DeltaTime);

	// Single writer for the camera's relative transform: compose head bob and
	// lean offset into one location, and lean roll + screen punch into one
	// rotation. (Previously the character also wrote this transform from a
	// different tick group, so bob silently wiped the lean offset.)
	const FVector FinalOffset = CurrentBobOffset + FVector(0.f, LeanOffsetY, 0.f);
	OwnerCamera->SetRelativeLocation(FinalOffset);

	const FRotator FinalRotation(
		PunchRotation.Pitch,
		PunchRotation.Yaw,
		LeanRoll + PunchRotation.Roll);
	OwnerCamera->SetRelativeRotation(FinalRotation);

	// Apply FOV.
	OwnerCamera->SetFieldOfView(CurrentFOV);
}

// =========================================================================
//  External triggers
// =========================================================================

void USHCameraSystem::SetCameraContext(const FSHCameraContext& InContext)
{
	Context = InContext;
}

void USHCameraSystem::SetLeanOffset(float InLeanOffsetY, float InLeanRoll)
{
	LeanOffsetY = InLeanOffsetY;
	LeanRoll = InLeanRoll;
}

void USHCameraSystem::OnNearbyExplosion(const FVector& ExplosionOrigin, float MaxRadius)
{
	if (!GetOwner())
	{
		return;
	}

	const float Distance = FVector::Dist(GetOwner()->GetActorLocation(), ExplosionOrigin);
	if (Distance >= MaxRadius)
	{
		return;
	}

	// Inverse-square intensity falloff.
	const float NormalizedDist = Distance / MaxRadius;
	const float Intensity = FMath::Clamp(1.f - (NormalizedDist * NormalizedDist), 0.f, 1.f);

	if (ExplosionShakeClass && Intensity > 0.05f)
	{
		if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(Owner->GetController()))
			{
				PC->ClientStartCameraShake(ExplosionShakeClass, Intensity);
			}
		}
	}

	// Also apply a screen punch away from the explosion.
	const FVector Direction = (GetOwner()->GetActorLocation() - ExplosionOrigin).GetSafeNormal();
	ApplyScreenPunch(Direction, Intensity * 2.f);
}

void USHCameraSystem::ApplyScreenPunch(const FVector& DamageDirection, float Intensity)
{
	if (!GetOwner())
	{
		return;
	}

	// Convert world direction to local pitch/yaw punch.
	const FRotator OwnerRotation = GetOwner()->GetActorRotation();
	const FVector LocalDir = OwnerRotation.UnrotateVector(DamageDirection);

	const float PunchPitch = -LocalDir.Z * MaxPunchAngle * Intensity;
	const float PunchYaw = LocalDir.Y * MaxPunchAngle * Intensity;

	PunchVelocity += FRotator(PunchPitch, PunchYaw, 0.f) * PunchRecoverySpeed;
}

void USHCameraSystem::ApplyLandingDip(float Intensity)
{
	Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
	if (Intensity <= 0.f)
	{
		return;
	}

	// Inject a downward pitch impulse into the screen-punch spring, which then
	// recovers via TickScreenPunch — a quick dip-and-settle on impact.
	PunchVelocity += FRotator(-MaxPunchAngle * Intensity, 0.f, 0.f) * PunchRecoverySpeed;
}

// =========================================================================
//  Tick helpers
// =========================================================================

void USHCameraSystem::TickHeadBob(float DeltaTime)
{
	const float Speed = Context.GroundSpeed;
	const bool bMoving = Speed > 10.f;

	if (!bMoving)
	{
		// Smoothly return to zero bob.
		CurrentBobOffset = FMath::VInterpTo(CurrentBobOffset, FVector::ZeroVector, DeltaTime, 8.f);
		return;
	}

	// Select amplitude based on sprint state.
	float Amplitude = Context.bIsSprinting ? SprintBobAmplitude : WalkBobAmplitude;

	// Reduce bob during ADS.
	if (Context.bIsADS)
	{
		Amplitude *= ADSBobReduction;
	}

	// Scale amplitude with speed (walk to sprint blend).
	const float SpeedFactor = FMath::GetMappedRangeValueClamped(
		FVector2D(50.f, 600.f), FVector2D(0.3f, 1.f), Speed);
	Amplitude *= SpeedFactor;

	// Advance bob timer.
	BobTimer += DeltaTime * BobFrequency * (Context.bIsSprinting ? 1.3f : 1.f);

	const float BobZ = FMath::Sin(BobTimer * UE_TWO_PI) * Amplitude;
	const float BobY = FMath::Cos(BobTimer * UE_TWO_PI * 0.5f) * Amplitude * 0.5f;

	const FVector TargetBob(0.f, BobY, BobZ);
	CurrentBobOffset = FMath::VInterpTo(CurrentBobOffset, TargetBob, DeltaTime, 10.f);
}

void USHCameraSystem::TickFOVTransition(float DeltaTime)
{
	TargetFOV = Context.bIsADS ? Context.ADSFOV : Context.HipFOV;

	if (TargetFOV <= 0.f)
	{
		TargetFOV = DefaultHipFOV;
	}

	// Widen FOV while sprinting (hip only) to convey speed. Suppressed by ADS.
	if (Context.bIsSprinting && !Context.bIsADS)
	{
		TargetFOV += SprintFOVBoost;
	}

	// Smooth FOV transition.
	const float TransitionSpeed = Context.ADSTransitionTime > 0.f
		? 1.f / Context.ADSTransitionTime
		: FOVInterpSpeed;

	CurrentFOV = FMath::FInterpTo(CurrentFOV, TargetFOV, DeltaTime, TransitionSpeed);
}

void USHCameraSystem::TickSuppressionFX(float DeltaTime)
{
	const float TargetVignette = Context.Suppression * MaxVignetteIntensity;
	const float TargetDesat = Context.Suppression * MaxDesaturation;

	CurrentVignetteIntensity = FMath::FInterpTo(CurrentVignetteIntensity, TargetVignette,
												DeltaTime, SuppressionFXInterpSpeed);
	CurrentDesaturation = FMath::FInterpTo(CurrentDesaturation, TargetDesat,
										   DeltaTime, SuppressionFXInterpSpeed);

	// Trigger suppression camera shake at medium+ levels — start it ONCE on the
	// rising edge rather than restarting it every frame (which stacked into a
	// constant max-intensity jitter). Re-arms once suppression drops back below
	// the threshold.
	if (Context.Suppression > 0.4f && SuppressionShakeClass)
	{
		if (!bSuppressionShakeActive)
		{
			if (ACharacter* Owner = Cast<ACharacter>(GetOwner()))
			{
				if (APlayerController* PC = Cast<APlayerController>(Owner->GetController()))
				{
					const float ShakeScale = FMath::GetMappedRangeValueClamped(
						FVector2D(0.4f, 1.f), FVector2D(0.2f, 1.f), Context.Suppression);
					PC->ClientStartCameraShake(SuppressionShakeClass, ShakeScale);
					bSuppressionShakeActive = true;
				}
			}
		}
	}
	else
	{
		bSuppressionShakeActive = false;
	}
}

void USHCameraSystem::TickScreenPunch(float DeltaTime)
{
	// Spring-damper recovery.
	PunchRotation += PunchVelocity * DeltaTime;
	PunchVelocity = FMath::RInterpTo(PunchVelocity, FRotator::ZeroRotator,
									  DeltaTime, PunchRecoverySpeed);
	PunchRotation = FMath::RInterpTo(PunchRotation, FRotator::ZeroRotator,
									  DeltaTime, PunchRecoverySpeed * 0.5f);
}
