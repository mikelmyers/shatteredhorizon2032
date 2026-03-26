// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCoverSystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"

// =====================================================================
//  Subsystem lifecycle
// =====================================================================

void USHCoverSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitDefaultPenetrationValues();
}

void USHCoverSystem::Deinitialize()
{
	DestructibleCoverMap.Empty();
	DynamicCoverMap.Empty();
	Super::Deinitialize();
}

void USHCoverSystem::InitDefaultPenetrationValues()
{
	if (MaterialPenetrationResistanceMap.Num() > 0)
	{
		return;
	}

	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Concrete, 8.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Steel, 10.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Brick, 6.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Wood, 2.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Sandbag, 5.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Earth, 7.f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Vegetation, 0.1f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Glass, 0.5f);
	MaterialPenetrationResistanceMap.Add(ESHCoverMaterial::Vehicle, 4.f);
}

// =====================================================================
//  Cover evaluation
// =====================================================================

FSHCoverEvaluation USHCoverSystem::EvaluateCoverFromDirection(const FVector& Position, const FVector& ThreatDirection) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FSHCoverEvaluation();
	}

	return EvaluateSingleDirection(Position, ThreatDirection.GetSafeNormal(), World);
}

FSHCoverEvaluation USHCoverSystem::EvaluateCoverMultiThreat(const FVector& Position, const TArray<FVector>& ThreatLocations) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FSHCoverEvaluation();
	}

	FSHCoverEvaluation BestEval;
	BestEval.Position = Position;
	int32 TotalProtected = 0;
	int32 TotalDirections = ThreatLocations.Num();
	float BestHeight = 0.f;
	float WorstScore = 1.f;

	for (const FVector& ThreatLoc : ThreatLocations)
	{
		const FVector Dir = (Position - ThreatLoc).GetSafeNormal();
		FSHCoverEvaluation Eval = EvaluateSingleDirection(Position, Dir, World);

		if (Eval.Quality != ESHCoverQuality::None)
		{
			TotalProtected++;
		}
		if (Eval.CoverHeight > BestHeight)
		{
			BestHeight = Eval.CoverHeight;
		}
		if (Eval.CoverScore < WorstScore)
		{
			WorstScore = Eval.CoverScore;
			// Take the worst-case evaluation as the aggregate.
			BestEval.Quality = Eval.Quality;
			BestEval.CoverType = Eval.CoverType;
			BestEval.Material = Eval.Material;
			BestEval.CoverActor = Eval.CoverActor;
		}

		BestEval.bCanLeanLeft |= Eval.bCanLeanLeft;
		BestEval.bCanLeanRight |= Eval.bCanLeanRight;
	}

	BestEval.CoverHeight = BestHeight;
	BestEval.ProtectedDirections = TotalProtected;
	BestEval.TotalDirectionsEvaluated = TotalDirections;
	BestEval.CoverScore = ComputeCoverScore(BestEval);

	// Check destructible cover integrity.
	if (BestEval.CoverActor.IsValid())
	{
		const FSHDestructibleCover* Destructible = DestructibleCoverMap.Find(BestEval.CoverActor.Get());
		if (Destructible)
		{
			BestEval.bIsIntact = Destructible->Integrity > 0.f;
			BestEval.CoverType = ESHCoverType::Destructible;
		}
	}

	return BestEval;
}

FSHCoverEvaluation USHCoverSystem::EvaluateSingleDirection(const FVector& Position, const FVector& ThreatDir, const UWorld* World) const
{
	FSHCoverEvaluation Eval;
	Eval.Position = Position;

	// Cast rays at multiple heights to determine cover quality:
	// - Ankle height (30 cm): prone cover
	// - Crouch height (100 cm): crouch cover
	// - Standing height (180 cm): standing cover
	const float HeightSamples[] = { 30.f, CrouchHeight, StandingHeight };
	const int32 NumSamples = UE_ARRAY_COUNT(HeightSamples);

	FCollisionQueryParams QueryParams;
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = true;

	// Direction from the threat to the position (incoming fire direction).
	const FVector IncomingDir = -ThreatDir;

	int32 BlockedSamples = 0;
	float MaxBlockedHeight = 0.f;
	AActor* BestCoverActor = nullptr;
	const UPhysicalMaterial* HitPhysMat = nullptr;
	float HitThickness = 0.f;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const FVector SamplePos = Position + FVector(0.f, 0.f, HeightSamples[i]);
		const FVector TraceStart = SamplePos + IncomingDir * CoverCheckDistance;
		const FVector TraceEnd = SamplePos;

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, CoverTraceChannel, QueryParams))
		{
			// Something between the threat direction and this height sample.
			BlockedSamples++;
			MaxBlockedHeight = HeightSamples[i];

			if (Hit.GetActor())
			{
				BestCoverActor = Hit.GetActor();
			}
			if (Hit.PhysMaterial.IsValid())
			{
				HitPhysMat = Hit.PhysMaterial.Get();
			}

			// Estimate thickness with a reverse trace.
			FHitResult ReverseHit;
			if (World->LineTraceSingleByChannel(ReverseHit, TraceEnd, TraceStart, CoverTraceChannel, QueryParams))
			{
				const float EstimatedThickness = FVector::Dist(Hit.ImpactPoint, ReverseHit.ImpactPoint);
				if (EstimatedThickness > HitThickness)
				{
					HitThickness = EstimatedThickness;
				}
			}
		}
	}

	Eval.CoverHeight = MaxBlockedHeight;
	Eval.CoverThickness = HitThickness;
	Eval.Quality = HeightToCoverQuality(MaxBlockedHeight);
	Eval.CoverActor = BestCoverActor;

	if (BlockedSamples > 0)
	{
		Eval.ProtectedDirections = 1;
	}
	Eval.TotalDirectionsEvaluated = 1;

	// Determine cover type from physical material.
	if (HitPhysMat)
	{
		Eval.Material = PhysMatToCoverMaterial(HitPhysMat);
	}
	else if (BlockedSamples > 0)
	{
		Eval.Material = ESHCoverMaterial::Concrete; // Default assumption.
	}

	// Cover vs concealment: vegetation is concealment only.
	if (Eval.Material == ESHCoverMaterial::Vegetation || Eval.Material == ESHCoverMaterial::Glass)
	{
		Eval.CoverType = ESHCoverType::SoftCover;
	}
	else if (BlockedSamples > 0)
	{
		// Check if this cover is in the destructible map.
		if (BestCoverActor && DestructibleCoverMap.Contains(BestCoverActor))
		{
			Eval.CoverType = ESHCoverType::Destructible;
			const FSHDestructibleCover& DC = DestructibleCoverMap[BestCoverActor];
			Eval.bIsIntact = DC.Integrity > 0.f;
		}
		else
		{
			Eval.CoverType = ESHCoverType::HardCover;
		}
	}

	// Lean evaluation: check if there is an edge to lean around.
	// Cast a ray offset laterally. If it does NOT hit, there is open space to lean into.
	const FVector Right = FVector::CrossProduct(FVector::UpVector, ThreatDir).GetSafeNormal();
	{
		const FVector LeanPos = Position + Right * LeanOffset + FVector(0, 0, CrouchHeight);
		const FVector LeanTraceStart = LeanPos + IncomingDir * CoverCheckDistance;
		FHitResult LeanHit;
		if (!World->LineTraceSingleByChannel(LeanHit, LeanTraceStart, LeanPos, CoverTraceChannel, QueryParams))
		{
			Eval.bCanLeanRight = (BlockedSamples > 0); // Can lean if we have cover to lean from.
		}
	}
	{
		const FVector LeanPos = Position - Right * LeanOffset + FVector(0, 0, CrouchHeight);
		const FVector LeanTraceStart = LeanPos + IncomingDir * CoverCheckDistance;
		FHitResult LeanHit;
		if (!World->LineTraceSingleByChannel(LeanHit, LeanTraceStart, LeanPos, CoverTraceChannel, QueryParams))
		{
			Eval.bCanLeanLeft = (BlockedSamples > 0);
		}
	}

	Eval.CoverScore = ComputeCoverScore(Eval);
	return Eval;
}

ESHCoverQuality USHCoverSystem::HeightToCoverQuality(float Height) const
{
	if (Height >= StandingHeight * 0.9f)
		return ESHCoverQuality::High;
	if (Height >= CrouchHeight * 0.9f)
		return ESHCoverQuality::Medium;
	if (Height >= 30.f)
		return ESHCoverQuality::Low;
	return ESHCoverQuality::None;
}

ESHCoverMaterial USHCoverSystem::PhysMatToCoverMaterial(const UPhysicalMaterial* PhysMat) const
{
	if (!PhysMat)
	{
		return ESHCoverMaterial::Concrete;
	}

	// Map physical material surface type to cover material.
	// This relies on the project's physical material naming convention.
	const FString Name = PhysMat->GetName().ToLower();

	if (Name.Contains(TEXT("concrete")) || Name.Contains(TEXT("cement")))
		return ESHCoverMaterial::Concrete;
	if (Name.Contains(TEXT("steel")) || Name.Contains(TEXT("metal")))
		return ESHCoverMaterial::Steel;
	if (Name.Contains(TEXT("brick")))
		return ESHCoverMaterial::Brick;
	if (Name.Contains(TEXT("wood")))
		return ESHCoverMaterial::Wood;
	if (Name.Contains(TEXT("sand")))
		return ESHCoverMaterial::Sandbag;
	if (Name.Contains(TEXT("earth")) || Name.Contains(TEXT("dirt")) || Name.Contains(TEXT("soil")))
		return ESHCoverMaterial::Earth;
	if (Name.Contains(TEXT("foliage")) || Name.Contains(TEXT("grass")) || Name.Contains(TEXT("leaf")))
		return ESHCoverMaterial::Vegetation;
	if (Name.Contains(TEXT("glass")))
		return ESHCoverMaterial::Glass;
	if (Name.Contains(TEXT("vehicle")) || Name.Contains(TEXT("car")))
		return ESHCoverMaterial::Vehicle;

	return ESHCoverMaterial::Concrete;
}

float USHCoverSystem::ComputeCoverScore(const FSHCoverEvaluation& Eval) const
{
	float Score = 0.f;

	// Quality contributes most.
	switch (Eval.Quality)
	{
	case ESHCoverQuality::High:   Score += 0.5f; break;
	case ESHCoverQuality::Medium: Score += 0.35f; break;
	case ESHCoverQuality::Low:    Score += 0.15f; break;
	default: return 0.f;
	}

	// Hard cover is much better than soft.
	if (Eval.CoverType == ESHCoverType::HardCover)
		Score += 0.25f;
	else if (Eval.CoverType == ESHCoverType::Destructible)
		Score += 0.2f;
	else
		Score += 0.05f; // Concealment is still something.

	// Protection ratio bonus.
	if (Eval.TotalDirectionsEvaluated > 0)
	{
		const float ProtectionRatio = static_cast<float>(Eval.ProtectedDirections) / Eval.TotalDirectionsEvaluated;
		Score += ProtectionRatio * 0.15f;
	}

	// Lean bonus (positional flexibility).
	if (Eval.bCanLeanLeft || Eval.bCanLeanRight)
	{
		Score += 0.05f;
	}

	// Intactness.
	if (!Eval.bIsIntact)
	{
		Score *= 0.3f;
	}

	return FMath::Clamp(Score, 0.f, 1.f);
}

// =====================================================================
//  Quick queries
// =====================================================================

bool USHCoverSystem::IsPositionInCover(const FVector& Position, const FVector& ThreatDirection) const
{
	const FSHCoverEvaluation Eval = EvaluateCoverFromDirection(Position, ThreatDirection);
	return Eval.Quality != ESHCoverQuality::None;
}

float USHCoverSystem::GetCoverScore(const FVector& Position, const FVector& ThreatDirection) const
{
	const FSHCoverEvaluation Eval = EvaluateCoverFromDirection(Position, ThreatDirection);
	return Eval.CoverScore;
}

bool USHCoverSystem::IsConcealment(const FVector& Position, const FVector& ThreatDirection) const
{
	const FSHCoverEvaluation Eval = EvaluateCoverFromDirection(Position, ThreatDirection);
	return Eval.Quality != ESHCoverQuality::None && Eval.CoverType == ESHCoverType::SoftCover;
}

// =====================================================================
//  Destructible cover
// =====================================================================

void USHCoverSystem::RegisterDestructibleCover(AActor* CoverActor, ESHCoverMaterial Material, float MaxDurability, ESHCoverQuality BaseQuality)
{
	if (!IsValid(CoverActor))
	{
		return;
	}

	FSHDestructibleCover Entry;
	Entry.CoverActor = CoverActor;
	Entry.Material = Material;
	Entry.MaxDurability = MaxDurability;
	Entry.Integrity = 1.f;
	Entry.AccumulatedDamage = 0.f;
	Entry.BaseQuality = BaseQuality;

	DestructibleCoverMap.Add(CoverActor, Entry);
}

void USHCoverSystem::DamageCover(AActor* CoverActor, float DamageAmount)
{
	FSHDestructibleCover* Entry = DestructibleCoverMap.Find(CoverActor);
	if (!Entry)
	{
		return;
	}

	Entry->AccumulatedDamage += DamageAmount;
	Entry->Integrity = FMath::Clamp(1.f - (Entry->AccumulatedDamage / Entry->MaxDurability), 0.f, 1.f);

	OnCoverDamaged.Broadcast(CoverActor, Entry->Integrity);

	if (Entry->Integrity <= 0.f)
	{
		OnCoverDestroyed.Broadcast(CoverActor, Entry->Material);
		// Keep the entry so queries return "destroyed" rather than "unknown".
	}
}

float USHCoverSystem::GetCoverIntegrity(const AActor* CoverActor) const
{
	const FSHDestructibleCover* Entry = DestructibleCoverMap.Find(const_cast<AActor*>(CoverActor));
	return Entry ? Entry->Integrity : -1.f;
}

bool USHCoverSystem::IsCoverDestroyed(const AActor* CoverActor) const
{
	const FSHDestructibleCover* Entry = DestructibleCoverMap.Find(const_cast<AActor*>(CoverActor));
	return Entry ? (Entry->Integrity <= 0.f) : false;
}

// =====================================================================
//  Dynamic cover
// =====================================================================

void USHCoverSystem::RegisterDynamicCover(AActor* CoverActor, ESHCoverQuality Quality, ESHCoverType Type, ESHCoverMaterial Material, FVector Extents)
{
	if (!IsValid(CoverActor))
	{
		return;
	}

	FSHDynamicCoverEntry Entry;
	Entry.CoverActor = CoverActor;
	Entry.Quality = Quality;
	Entry.CoverType = Type;
	Entry.Material = Material;
	Entry.Extents = Extents;

	DynamicCoverMap.Add(CoverActor, Entry);
}

void USHCoverSystem::UnregisterDynamicCover(AActor* CoverActor)
{
	DynamicCoverMap.Remove(CoverActor);
}

// =====================================================================
//  Lean system
// =====================================================================

bool USHCoverSystem::CanLean(const AActor* Character, ESHLeanDirection Direction) const
{
	if (!IsValid(Character) || Direction == ESHLeanDirection::None)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector CharLocation = Character->GetActorLocation();
	const FVector Forward = Character->GetActorForwardVector();
	const FVector Right = Character->GetActorRightVector();

	// Check that there IS cover in front of the character.
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Character);

	FHitResult ForwardHit;
	const FVector ForwardTraceEnd = CharLocation + Forward * CoverCheckDistance;
	if (!World->LineTraceSingleByChannel(ForwardHit, CharLocation, ForwardTraceEnd, CoverTraceChannel, Params))
	{
		return false; // No cover in front to lean from.
	}

	// Check that the lean direction is clear (no wall to lean into).
	const float LeanSign = (Direction == ESHLeanDirection::Right) ? 1.f : -1.f;
	const FVector LeanTarget = CharLocation + Right * LeanSign * LeanOffset;

	FHitResult LateralHit;
	if (World->LineTraceSingleByChannel(LateralHit, CharLocation, LeanTarget, CoverTraceChannel, Params))
	{
		return false; // Something blocking the lean.
	}

	// Check that leaning exposes to the forward direction (there is no cover at the lean position).
	const FVector LeanForwardEnd = LeanTarget + Forward * CoverCheckDistance;
	FHitResult LeanForwardHit;
	// Lean is useful ONLY if the leaned position actually has sightline forward.
	// If leaning around a corner and there's still cover blocking the forward view,
	// the lean doesn't accomplish anything. Validate that the lean position has
	// either no obstruction or obstruction far enough to shoot past.
	if (World->LineTraceSingleByChannel(LeanForwardHit, LeanTarget, LeanForwardEnd, CoverTraceChannel, Params))
	{
		// There IS something in the forward direction from the lean position.
		// Lean is only useful if the obstruction is far enough to shoot past (>100cm).
		// If cover wraps around the corner, the lean doesn't help.
		if (LeanForwardHit.Distance < 100.f)
		{
			return false; // Cover wraps around corner — lean doesn't expose.
		}
	}

	// Lean position has either clear sightline or enough room to shoot.
	return true;
}

bool USHCoverSystem::ComputeLeanTransform(const AActor* Character, ESHLeanDirection Direction, FVector& OutOffset, FRotator& OutRotation) const
{
	OutOffset = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;

	if (!CanLean(Character, Direction))
	{
		return false;
	}

	const FVector Right = Character->GetActorRightVector();
	const float LeanSign = (Direction == ESHLeanDirection::Right) ? 1.f : -1.f;

	// Lateral offset.
	OutOffset = Right * LeanSign * LeanOffset;

	// Slight vertical dip and rotation to simulate natural lean posture.
	OutOffset.Z = -8.f; // Small downward offset.
	OutRotation.Roll = LeanSign * 15.f; // 15 degree body roll.

	return true;
}

// =====================================================================
//  Material penetration
// =====================================================================

float USHCoverSystem::GetMaterialPenetrationResistance(ESHCoverMaterial Material) const
{
	const float* Value = MaterialPenetrationResistanceMap.Find(Material);
	return Value ? *Value : 5.f;
}

bool USHCoverSystem::CanPenetratecover(ESHCoverMaterial Material, float ThicknessCm, int32 PenetrationRating, float& OutDamageMultiplier) const
{
	OutDamageMultiplier = 0.f;

	const float Resistance = GetMaterialPenetrationResistance(Material);

	// Effective resistance scales with thickness.
	// Baseline thickness is ~20 cm; thicker = harder to penetrate.
	const float ThicknessMultiplier = ThicknessCm / 20.f;
	const float EffectiveResistance = Resistance * FMath::Max(ThicknessMultiplier, 0.5f);

	const float PenFloat = static_cast<float>(PenetrationRating);

	if (PenFloat <= EffectiveResistance * 0.5f)
	{
		// Completely stopped.
		OutDamageMultiplier = 0.f;
		return false;
	}

	if (PenFloat >= EffectiveResistance)
	{
		// Clean penetration with moderate damage reduction.
		OutDamageMultiplier = FMath::Clamp(PenFloat / (EffectiveResistance + PenFloat), 0.2f, 0.9f);
		return true;
	}

	// Marginal penetration: low remaining energy.
	const float Ratio = (PenFloat - EffectiveResistance * 0.5f) / (EffectiveResistance * 0.5f);
	OutDamageMultiplier = Ratio * 0.3f; // Very reduced damage.
	return OutDamageMultiplier > 0.05f;
}

// =====================================================================
//  EQS integration
// =====================================================================

TArray<FSHCoverEvaluation> USHCoverSystem::FindBestCoverPositions(const FVector& Origin, float SearchRadius, const FVector& ThreatLocation, int32 MaxResults) const
{
	TArray<FSHCoverEvaluation> Results;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return Results;
	}

	// Generate candidate positions in a grid pattern within the search radius.
	const float GridSpacing = 150.f; // cm between sample points.
	const int32 GridHalf = FMath::CeilToInt(SearchRadius / GridSpacing);

	const FVector ThreatDir = (Origin - ThreatLocation).GetSafeNormal();

	TArray<FSHCoverEvaluation> Candidates;
	Candidates.Reserve(GridHalf * GridHalf * 4);

	for (int32 X = -GridHalf; X <= GridHalf; ++X)
	{
		for (int32 Y = -GridHalf; Y <= GridHalf; ++Y)
		{
			const FVector Candidate = Origin + FVector(X * GridSpacing, Y * GridSpacing, 0.f);
			const float DistSq = FVector::DistSquared2D(Candidate, Origin);
			if (DistSq > SearchRadius * SearchRadius)
			{
				continue;
			}

			// Ground trace to snap to navmesh/terrain height.
			FHitResult GroundHit;
			const FVector TraceStart = Candidate + FVector(0, 0, 200.f);
			const FVector TraceEnd = Candidate - FVector(0, 0, 500.f);
			FCollisionQueryParams GroundParams;
			if (!World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_WorldStatic, GroundParams))
			{
				continue;
			}

			const FVector GroundPos = GroundHit.ImpactPoint;
			const FVector DirToPos = (GroundPos - ThreatLocation).GetSafeNormal();

			FSHCoverEvaluation Eval = EvaluateSingleDirection(GroundPos, DirToPos, World);
			if (Eval.Quality != ESHCoverQuality::None)
			{
				Candidates.Add(MoveTemp(Eval));
			}
		}
	}

	// Sort by score descending.
	Candidates.Sort([](const FSHCoverEvaluation& A, const FSHCoverEvaluation& B)
	{
		return A.CoverScore > B.CoverScore;
	});

	// Return top N.
	const int32 Count = FMath::Min(Candidates.Num(), MaxResults);
	Results.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		Results.Add(MoveTemp(Candidates[i]));
	}

	return Results;
}
