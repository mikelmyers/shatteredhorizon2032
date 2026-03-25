// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHHitFeedback.h"
#include "ShatteredHorizon2032/Core/SHCameraSystem.h"
#include "NiagaraSystem.h"
#include "NiagaraFunctionLibrary.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/KismetMathLibrary.h"

// =====================================================================
//  Constructor
// =====================================================================

USHHitFeedback::USHHitFeedback()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHHitFeedback::BeginPlay()
{
	Super::BeginPlay();

	// Cache sibling camera system for screen punch.
	if (AActor* Owner = GetOwner())
	{
		CachedCameraSystem = Owner->FindComponentByClass<USHCameraSystem>();
	}
}

// =====================================================================
//  Damage hooks
// =====================================================================

void USHHitFeedback::OnDamageReceived(const FSHDamageInfo& Info, const FSHDamageResult& Result)
{
	if (Result.DamageDealt <= 0.f)
	{
		return;
	}

	// 1. Screen punch — directional camera nudge.
	ApplyScreenPunch(Info, Result);

	// 2. Hit indicator — broadcast for UI.
	BroadcastHitIndicator(Info, Result);

	// 3. Impact VFX at the hit location.
	const ESHPenetrableMaterial SurfaceMaterial = DetermineSurfaceMaterial(Info.ImpactLocation);
	const FVector ImpactNormal = -Info.DamageDirection.GetSafeNormal();
	SpawnImpactVFX(Info.ImpactLocation, ImpactNormal, SurfaceMaterial);
}

void USHHitFeedback::OnDamageDealt(const FSHDamageInfo& Info, const FSHDamageResult& Result)
{
	if (Result.DamageDealt <= 0.f)
	{
		return;
	}

	// 1. Hit marker delegate for UI.
	OnHitMarkerTriggered.Broadcast(Result.bIsLethal);

	// 2. Enemy flinch montage on the target.
	// OnDamageDealt is called on the instigator's component. FSHDamageInfo
	// tracks the shooter as Instigator, so we locate the target character
	// via a small sphere overlap at the impact location.
	TriggerFlinchOnTarget(Info, Result);
}

// =====================================================================
//  Screen Punch
// =====================================================================

void USHHitFeedback::ApplyScreenPunch(const FSHDamageInfo& Info, const FSHDamageResult& Result)
{
	if (!CachedCameraSystem)
	{
		return;
	}

	// Scale intensity by damage proportion and lethal bonus.
	float Intensity = PunchIntensity * (Result.DamageDealt / 50.f);

	if (Result.bIsLethal)
	{
		Intensity *= LethalPunchMultiplier;
	}

	// Head hits feel harder.
	if (Result.HitZone == ESHHitZone::Head)
	{
		Intensity *= 1.5f;
	}

	Intensity = FMath::Clamp(Intensity, 0.f, MaxPunchIntensity);

	CachedCameraSystem->ApplyScreenPunch(Info.DamageDirection, Intensity);
}

// =====================================================================
//  Hit Indicator
// =====================================================================

void USHHitFeedback::BroadcastHitIndicator(const FSHDamageInfo& Info, const FSHDamageResult& Result)
{
	if (Result.DamageDealt < IndicatorMinDamage)
	{
		return;
	}

	// Compute the screen-space angle from the camera forward to the damage source.
	FVector DamageOrigin = Info.ImpactLocation - Info.DamageDirection * 500.f;
	if (Info.Instigator.IsValid())
	{
		DamageOrigin = Info.Instigator->GetActorLocation();
	}

	float Angle = 0.f;
	if (!ComputeScreenAngle(DamageOrigin, Angle))
	{
		return;
	}

	// Intensity is normalized: 1.0 for a 100-damage hit, scaled linearly.
	const float Intensity = FMath::Clamp(Result.DamageDealt / 100.f, 0.1f, 1.0f);

	OnHitIndicator.Broadcast(Angle, Intensity, IndicatorDuration);
}

bool USHHitFeedback::ComputeScreenAngle(const FVector& WorldOrigin, float& OutAngle) const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}

	// Get the owning pawn's controller for camera orientation.
	const APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn)
	{
		return false;
	}

	const APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (!PC)
	{
		return false;
	}

	// Use the control rotation (camera facing direction).
	const FRotator ControlRotation = PC->GetControlRotation();
	const FVector Forward = ControlRotation.Vector();
	const FVector Right = FRotationMatrix(ControlRotation).GetScaledAxis(EAxis::Y);

	// Direction from owner to damage source, projected onto the horizontal plane.
	FVector ToSource = WorldOrigin - Owner->GetActorLocation();
	ToSource.Z = 0.f;

	if (ToSource.IsNearlyZero())
	{
		OutAngle = 0.f;
		return true;
	}

	ToSource.Normalize();

	// Project onto camera-local 2D axes.
	const FVector Forward2D = FVector(Forward.X, Forward.Y, 0.f).GetSafeNormal();
	const FVector Right2D = FVector(Right.X, Right.Y, 0.f).GetSafeNormal();

	const float DotForward = FVector::DotProduct(Forward2D, ToSource);
	const float DotRight = FVector::DotProduct(Right2D, ToSource);

	// Atan2 gives us the angle: 0 = forward/top, positive = clockwise.
	OutAngle = FMath::RadiansToDegrees(FMath::Atan2(DotRight, DotForward));

	// Normalize to [0, 360).
	if (OutAngle < 0.f)
	{
		OutAngle += 360.f;
	}

	return true;
}

// =====================================================================
//  Enemy Flinch
// =====================================================================

void USHHitFeedback::TriggerFlinchMontage(AActor* HitActor, ESHHitZone HitZone)
{
	if (!HitActor)
	{
		return;
	}

	// Look up the montage for this zone.
	const TSoftObjectPtr<UAnimMontage>* MontagePtr = FlinchMontages.Find(HitZone);
	if (!MontagePtr || MontagePtr->IsNull())
	{
		return;
	}

	// Synchronously load if not yet loaded. In production, these should be
	// preloaded via the asset manager during level streaming.
	UAnimMontage* Montage = MontagePtr->LoadSynchronous();
	if (!Montage)
	{
		return;
	}

	// Find the skeletal mesh / anim instance on the hit actor.
	ACharacter* HitCharacter = Cast<ACharacter>(HitActor);
	if (!HitCharacter)
	{
		return;
	}

	UAnimInstance* AnimInstance = HitCharacter->GetMesh() ? HitCharacter->GetMesh()->GetAnimInstance() : nullptr;
	if (!AnimInstance)
	{
		return;
	}

	// Only play if a flinch montage isn't already playing (avoid stacking).
	if (!AnimInstance->Montage_IsPlaying(Montage))
	{
		AnimInstance->Montage_Play(Montage, FlinchPlayRate);
	}
}

void USHHitFeedback::TriggerFlinchOnTarget(const FSHDamageInfo& Info, const FSHDamageResult& Result)
{
	if (!GetWorld())
	{
		return;
	}

	// Sphere overlap at the impact location to find the hit character.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwner());

	const float SearchRadius = 50.f;
	const bool bHit = GetWorld()->OverlapMultiByChannel(
		Overlaps,
		Info.ImpactLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(SearchRadius),
		QueryParams
	);

	if (bHit)
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* OverlapActor = Overlap.GetActor();
			if (OverlapActor && OverlapActor != GetOwner() && Cast<ACharacter>(OverlapActor))
			{
				TriggerFlinchMontage(OverlapActor, Result.HitZone);
				break; // Only flinch the first character found.
			}
		}
	}
}

// =====================================================================
//  Impact VFX
// =====================================================================

void USHHitFeedback::SpawnImpactVFX(const FVector& ImpactLocation, const FVector& ImpactNormal, ESHPenetrableMaterial SurfaceMaterial)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Look up material-specific VFX, fall back to default.
	UNiagaraSystem* VFXSystem = nullptr;

	if (const TSoftObjectPtr<UNiagaraSystem>* Found = ImpactVFXMap.Find(SurfaceMaterial))
	{
		if (!Found->IsNull())
		{
			VFXSystem = Found->LoadSynchronous();
		}
	}

	if (!VFXSystem && !DefaultImpactVFX.IsNull())
	{
		VFXSystem = DefaultImpactVFX.LoadSynchronous();
	}

	if (!VFXSystem)
	{
		return;
	}

	// Build rotation from impact normal so particles emit away from the surface.
	const FRotator SpawnRotation = ImpactNormal.Rotation();

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World,
		VFXSystem,
		ImpactLocation,
		SpawnRotation,
		ImpactVFXScale,
		/* bAutoDestroy = */ true,
		/* bAutoActivate = */ true,
		ENCPoolMethod::AutoRelease
	);
}

ESHPenetrableMaterial USHHitFeedback::DetermineSurfaceMaterial(const FVector& ImpactLocation) const
{
	// Default to Flesh for character hits. A more sophisticated implementation
	// would inspect the physical material from the hit result or trace at the
	// impact location. For now, we return Flesh since this component lives on
	// characters and OnDamageReceived implies the character was hit.
	return ESHPenetrableMaterial::Flesh;
}
