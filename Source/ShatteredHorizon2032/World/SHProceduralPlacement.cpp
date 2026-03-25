// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHProceduralPlacement.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogSH_ProceduralPlacement);

USHProceduralPlacement::USHProceduralPlacement()
{
}

void USHProceduralPlacement::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
}

void USHProceduralPlacement::Deinitialize()
{
	ClearGeneratedContent();
	Super::Deinitialize();
}

bool USHProceduralPlacement::ShouldCreateSubsystem(UObject* Outer) const
{
	return true;
}

// =========================================================================
//  Primary API
// =========================================================================

void USHProceduralPlacement::SetSeed(int32 NewSeed)
{
	CurrentSeed = NewSeed;
	RandomStream.Initialize(NewSeed);
}

void USHProceduralPlacement::GenerateBattlefield(int32 Seed, const TArray<FSHPlacementRule>& Rules)
{
	ClearGeneratedContent();
	SetSeed(Seed);
	ActiveRules = Rules;

	UE_LOG(LogSH_ProceduralPlacement, Log, TEXT("Generating battlefield with seed %d, %d rules"), Seed, Rules.Num());

	// Separate enemy position rules for special tactical handling.
	TArray<FSHPlacementRule> EnemyRules;
	TArray<FSHPlacementRule> StandardRules;

	for (const FSHPlacementRule& Rule : Rules)
	{
		if (Rule.Category == ESHPlacementCategory::EnemyPosition)
		{
			EnemyRules.Add(Rule);
		}
		else
		{
			StandardRules.Add(Rule);
		}
	}

	// Generate standard categories first (cover, vegetation, debris, etc.).
	for (const FSHPlacementRule& Rule : StandardRules)
	{
		if (!Rule.ActorClass)
		{
			continue;
		}

		const int32 Count = RandomStream.RandRange(Rule.MinCount, Rule.MaxCount);
		TArray<FTransform> Transforms;
		FindValidSpawnLocations(Rule, Count, Transforms);

		for (const FTransform& Transform : Transforms)
		{
			SpawnPlacedActor(Rule.ActorClass, Transform, Rule.Category);
		}
	}

	// Generate enemy positions with tactical awareness.
	if (EnemyRules.Num() > 0)
	{
		GenerateEnemyPositions(EnemyRules, RandomStream);
	}

	// Count total placed.
	int32 TotalPlaced = 0;
	for (const FSHPlacementResult& Result : PlacementResults)
	{
		if (Result.bWasPlaced) { ++TotalPlaced; }
	}

	UE_LOG(LogSH_ProceduralPlacement, Log, TEXT("Generation complete: %d actors placed"), TotalPlaced);
	OnGenerationComplete.Broadcast(TotalPlaced);
}

void USHProceduralPlacement::GenerateCategory(ESHPlacementCategory Category, int32 Seed)
{
	FRandomStream RNG(Seed);

	TArray<FSHPlacementRule> CategoryRules;
	for (const FSHPlacementRule& Rule : ActiveRules)
	{
		if (Rule.Category == Category)
		{
			CategoryRules.Add(Rule);
		}
	}

	if (Category == ESHPlacementCategory::EnemyPosition)
	{
		GenerateEnemyPositions(CategoryRules, RNG);
		return;
	}

	for (const FSHPlacementRule& Rule : CategoryRules)
	{
		if (!Rule.ActorClass) { continue; }

		const int32 Count = RNG.RandRange(Rule.MinCount, Rule.MaxCount);
		TArray<FTransform> Transforms;
		FindValidSpawnLocations(Rule, Count, Transforms);

		for (const FTransform& Transform : Transforms)
		{
			SpawnPlacedActor(Rule.ActorClass, Transform, Rule.Category);
		}
	}
}

void USHProceduralPlacement::ClearGeneratedContent()
{
	for (auto& Pair : PlacedActorsByCategory)
	{
		for (const TWeakObjectPtr<AActor>& ActorPtr : Pair.Value)
		{
			if (ActorPtr.IsValid())
			{
				ActorPtr->Destroy();
			}
		}
	}

	PlacedActorsByCategory.Empty();
	PlacedLocationsByCategory.Empty();
	PlacementResults.Empty();
}

TArray<AActor*> USHProceduralPlacement::GetPlacedActors(ESHPlacementCategory Category) const
{
	TArray<AActor*> Result;
	if (const TArray<TWeakObjectPtr<AActor>>* Actors = PlacedActorsByCategory.Find(Category))
	{
		for (const TWeakObjectPtr<AActor>& Ptr : *Actors)
		{
			if (Ptr.IsValid())
			{
				Result.Add(Ptr.Get());
			}
		}
	}
	return Result;
}

// =========================================================================
//  Spawn location generation
// =========================================================================

void USHProceduralPlacement::FindValidSpawnLocations(const FSHPlacementRule& Rule, int32 Count,
													  TArray<FTransform>& OutTransforms)
{
	// Use Poisson disk sampling for natural distribution.
	TArray<FVector2D> CandidatePoints;
	const FVector2D Center2D(BattlefieldCenter.X, BattlefieldCenter.Y);
	PoissonDiskSample(Center2D, Rule.SpawnRadius, Rule.MinSpacing, Count * 3, CandidatePoints);

	int32 Placed = 0;
	for (const FVector2D& Point : CandidatePoints)
	{
		if (Placed >= Count)
		{
			break;
		}

		FVector Location(Point.X, Point.Y, BattlefieldCenter.Z + TraceStartHeight);

		if (!ValidateLocation(Location, Rule))
		{
			continue;
		}

		FRotator Rotation = FRotator::ZeroRotator;
		if (Rule.bAlignToSurface)
		{
			if (!AlignToSurface(Location, Rotation))
			{
				continue;
			}
		}
		else
		{
			// Just find ground Z.
			FHitResult Hit;
			FVector TraceEnd = Location - FVector(0.f, 0.f, TraceLength);
			if (GetWorld()->LineTraceSingleByChannel(Hit, Location, TraceEnd, GroundTraceChannel))
			{
				Location = Hit.ImpactPoint;
			}
			else
			{
				continue;
			}
		}

		if (Rule.bRandomRotation)
		{
			Rotation.Yaw = RandomStream.FRandRange(0.f, 360.f);
		}

		if (Rule.bCheckLineOfSight)
		{
			const TArray<FVector> ApproachRoutes = GetApproachRoutePoints();
			bool bHasSightline = false;
			for (const FVector& Route : ApproachRoutes)
			{
				if (CheckSightlines(Location + FVector(0.f, 0.f, SightlineEyeHeight), Route))
				{
					bHasSightline = true;
					break;
				}
			}
			if (!bHasSightline)
			{
				continue;
			}
		}

		OutTransforms.Add(FTransform(Rotation, Location, FVector::OneVector));
		++Placed;
	}
}

bool USHProceduralPlacement::ValidateLocation(FVector Location, const FSHPlacementRule& Rule) const
{
	// Terrain preference.
	if (Rule.PreferredTerrain != ESHPreferredTerrain::Any)
	{
		if (!MatchesTerrainPreference(Location, Rule.PreferredTerrain))
		{
			return false;
		}
	}

	// Slope check for flat terrain preference.
	if (Rule.PreferredTerrain == ESHPreferredTerrain::Flat)
	{
		if (GetSlopeAngleAt(Location) > MaxFlatSlopeAngle)
		{
			return false;
		}
	}

	// Water check.
	if (IsInWater(Location))
	{
		return false;
	}

	// Building overlap.
	if (OverlapsBuilding(Location))
	{
		return false;
	}

	// Spacing from same category.
	if (!MeetsSpacingRequirement(Location, Rule.Category, Rule.MinSpacing))
	{
		return false;
	}

	return true;
}

bool USHProceduralPlacement::AlignToSurface(FVector& Location, FRotator& OutRotation) const
{
	FHitResult Hit;
	const FVector Start = FVector(Location.X, Location.Y, Location.Z);
	const FVector End = Start - FVector(0.f, 0.f, TraceLength);

	if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel))
	{
		return false;
	}

	Location = Hit.ImpactPoint;

	// Compute rotation aligned to surface normal.
	const FVector UpVector = Hit.ImpactNormal;
	const FVector ForwardVector = FVector::CrossProduct(FVector::RightVector, UpVector).GetSafeNormal();
	OutRotation = FRotationMatrix::MakeFromXZ(ForwardVector, UpVector).Rotator();

	return true;
}

bool USHProceduralPlacement::CheckSightlines(FVector FromLocation, FVector ToTarget) const
{
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	return !GetWorld()->LineTraceSingleByChannel(Hit, FromLocation, ToTarget, SightlineTraceChannel, Params);
}

// =========================================================================
//  Poisson disk sampling
// =========================================================================

void USHProceduralPlacement::PoissonDiskSample(FVector2D Center, float Radius, float MinDistance,
												int32 MaxPoints, TArray<FVector2D>& OutPoints)
{
	OutPoints.Empty();

	if (MinDistance <= 0.f || MaxPoints <= 0)
	{
		return;
	}

	// Bridson's algorithm for Poisson disk sampling.
	TArray<FVector2D> ActiveList;

	// Start with center point.
	ActiveList.Add(Center);
	OutPoints.Add(Center);

	while (ActiveList.Num() > 0 && OutPoints.Num() < MaxPoints)
	{
		// Pick random active point.
		const int32 ActiveIdx = RandomStream.RandRange(0, ActiveList.Num() - 1);
		const FVector2D ActivePoint = ActiveList[ActiveIdx];

		bool bFoundCandidate = false;

		for (int32 Attempt = 0; Attempt < PoissonMaxAttempts; ++Attempt)
		{
			// Generate random candidate between MinDistance and 2*MinDistance from active point.
			const float Angle = RandomStream.FRandRange(0.f, UE_TWO_PI);
			const float Dist = RandomStream.FRandRange(MinDistance, MinDistance * 2.f);
			const FVector2D Candidate = ActivePoint + FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Dist;

			// Check within radius.
			if (FVector2D::Distance(Candidate, Center) > Radius)
			{
				continue;
			}

			// Check spacing from all existing points.
			bool bTooClose = false;
			for (const FVector2D& Existing : OutPoints)
			{
				if (FVector2D::DistSquared(Candidate, Existing) < MinDistance * MinDistance)
				{
					bTooClose = true;
					break;
				}
			}

			if (!bTooClose)
			{
				OutPoints.Add(Candidate);
				ActiveList.Add(Candidate);
				bFoundCandidate = true;
				break;
			}
		}

		if (!bFoundCandidate)
		{
			ActiveList.RemoveAtSwap(ActiveIdx);
		}
	}
}

// =========================================================================
//  Enemy tactical placement
// =========================================================================

void USHProceduralPlacement::GenerateEnemyPositions(const TArray<FSHPlacementRule>& EnemyRules,
													 FRandomStream& RNG)
{
	const TArray<FVector> ApproachRoutes = GetApproachRoutePoints();

	for (const FSHPlacementRule& Rule : EnemyRules)
	{
		if (!Rule.ActorClass) { continue; }

		const int32 TotalCount = RNG.RandRange(Rule.MinCount, Rule.MaxCount);

		// Organize into squads of 3-5.
		int32 Remaining = TotalCount;
		while (Remaining > 0)
		{
			const int32 SquadSize = FMath::Min(RNG.RandRange(MinSquadSize, MaxSquadSize), Remaining);

			// Find a valid squad center.
			bool bFoundCenter = false;
			for (int32 Retry = 0; Retry < MaxLocationRetries; ++Retry)
			{
				const float Angle = RNG.FRandRange(0.f, UE_TWO_PI);
				const float Dist = RNG.FRandRange(Rule.MinSpacing, Rule.SpawnRadius);
				FVector CandidateCenter = BattlefieldCenter +
					FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, TraceStartHeight);

				// Trace to ground.
				FHitResult Hit;
				if (!GetWorld()->LineTraceSingleByChannel(Hit, CandidateCenter,
					CandidateCenter - FVector(0.f, 0.f, TraceLength), GroundTraceChannel))
				{
					continue;
				}
				CandidateCenter = Hit.ImpactPoint;

				if (IsInWater(CandidateCenter) || OverlapsBuilding(CandidateCenter))
				{
					continue;
				}

				// Squad center needs sightlines to at least one approach route.
				const FVector EyePos = CandidateCenter + FVector(0.f, 0.f, SightlineEyeHeight);
				bool bHasSightline = false;
				for (const FVector& Route : ApproachRoutes)
				{
					if (CheckSightlines(EyePos, Route))
					{
						bHasSightline = true;
						break;
					}
				}
				if (!bHasSightline) { continue; }

				// Generate squad cluster.
				TArray<FTransform> SquadTransforms;
				GenerateSquadCluster(CandidateCenter, Rule, SquadSize, RNG, SquadTransforms);

				for (const FTransform& T : SquadTransforms)
				{
					SpawnPlacedActor(Rule.ActorClass, T, ESHPlacementCategory::EnemyPosition);
				}

				Remaining -= SquadTransforms.Num();
				bFoundCenter = true;
				break;
			}

			if (!bFoundCenter)
			{
				UE_LOG(LogSH_ProceduralPlacement, Warning,
					TEXT("Failed to find valid squad center after %d retries"), MaxLocationRetries);
				break;
			}
		}
	}
}

void USHProceduralPlacement::GenerateSquadCluster(FVector SquadCenter, const FSHPlacementRule& Rule,
												   int32 SquadSize, FRandomStream& RNG,
												   TArray<FTransform>& OutTransforms)
{
	for (int32 i = 0; i < SquadSize; ++i)
	{
		for (int32 Retry = 0; Retry < 50; ++Retry)
		{
			const float Angle = RNG.FRandRange(0.f, UE_TWO_PI);
			const float Dist = RNG.FRandRange(200.f, SquadClusterRadius);
			FVector Candidate = SquadCenter + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);

			FRotator Rotation = FRotator::ZeroRotator;
			if (!AlignToSurface(Candidate, Rotation))
			{
				continue;
			}

			if (Rule.bRandomRotation)
			{
				Rotation.Yaw = RNG.FRandRange(0.f, 360.f);
			}

			// Check spacing from already-placed squad members.
			bool bTooClose = false;
			for (const FTransform& Existing : OutTransforms)
			{
				if (FVector::Dist(Candidate, Existing.GetLocation()) < 150.f)
				{
					bTooClose = true;
					break;
				}
			}
			if (bTooClose) { continue; }

			OutTransforms.Add(FTransform(Rotation, Candidate, FVector::OneVector));
			break;
		}
	}
}

bool USHProceduralPlacement::ValidateApproachCoverage(const TArray<FTransform>& Positions,
													   const TArray<FVector>& ApproachRoutes) const
{
	if (ApproachRoutes.Num() == 0) { return true; }

	int32 CoveredRoutes = 0;
	for (const FVector& Route : ApproachRoutes)
	{
		for (const FTransform& Pos : Positions)
		{
			if (CheckSightlines(Pos.GetLocation() + FVector(0.f, 0.f, SightlineEyeHeight), Route))
			{
				++CoveredRoutes;
				break;
			}
		}
	}

	// Require at least half the approach routes to be covered.
	return CoveredRoutes >= (ApproachRoutes.Num() / 2);
}

bool USHProceduralPlacement::HasNearbyCover(FVector Location, float SearchRadius) const
{
	// Check if any cover actors exist nearby.
	if (const TArray<TWeakObjectPtr<AActor>>* CoverActors = PlacedActorsByCategory.Find(ESHPlacementCategory::Cover))
	{
		for (const TWeakObjectPtr<AActor>& Actor : *CoverActors)
		{
			if (Actor.IsValid() && FVector::Dist(Location, Actor->GetActorLocation()) <= SearchRadius)
			{
				return true;
			}
		}
	}

	// Also check for world geometry that could serve as cover via overlap.
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SearchRadius);
	if (GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_WorldStatic, Sphere))
	{
		return Overlaps.Num() > 0;
	}

	return false;
}

// =========================================================================
//  Spawning
// =========================================================================

AActor* USHProceduralPlacement::SpawnPlacedActor(TSubclassOf<AActor> ActorClass,
												  const FTransform& Transform,
												  ESHPlacementCategory Category)
{
	FSHPlacementResult Result;
	Result.ActorClass = ActorClass;
	Result.SpawnTransform = Transform;
	Result.Category = Category;

	if (!GetWorld() || !ActorClass)
	{
		Result.bWasPlaced = false;
		PlacementResults.Add(Result);
		return nullptr;
	}

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* Spawned = GetWorld()->SpawnActor<AActor>(ActorClass, &Transform, Params);
	if (Spawned)
	{
		Result.bWasPlaced = true;
		PlacedActorsByCategory.FindOrAdd(Category).Add(Spawned);
		PlacedLocationsByCategory.FindOrAdd(Category).Add(Transform.GetLocation());
	}
	else
	{
		Result.bWasPlaced = false;
	}

	PlacementResults.Add(Result);
	return Spawned;
}

// =========================================================================
//  Terrain helpers
// =========================================================================

float USHProceduralPlacement::GetSlopeAngleAt(FVector Location) const
{
	FHitResult Hit;
	const FVector Start = FVector(Location.X, Location.Y, Location.Z + TraceStartHeight);
	const FVector End = Start - FVector(0.f, 0.f, TraceLength);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, GroundTraceChannel))
	{
		const float DotUp = FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector);
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotUp, -1.f, 1.f)));
	}

	return 90.f; // No ground = vertical.
}

bool USHProceduralPlacement::IsInWater(FVector Location) const
{
	// Check for water volume via overlap.
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Point = FCollisionShape::MakeSphere(10.f);
	FCollisionQueryParams Params;
	Params.bReturnPhysicalMaterial = false;

	if (GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_PhysicsBody, Point, Params))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor() && Overlap.GetActor()->Tags.Contains(FName(TEXT("Water"))))
			{
				return true;
			}
		}
	}
	return false;
}

bool USHProceduralPlacement::OverlapsBuilding(FVector Location, float Tolerance) const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Box = FCollisionShape::MakeBox(FVector(Tolerance));

	if (GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_WorldStatic, Box))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor() && Overlap.GetActor()->Tags.Contains(FName(TEXT("Building"))))
			{
				return true;
			}
		}
	}
	return false;
}

bool USHProceduralPlacement::MeetsSpacingRequirement(FVector Location, ESHPlacementCategory Category,
													  float MinSpacing) const
{
	if (const TArray<FVector>* Locations = PlacedLocationsByCategory.Find(Category))
	{
		const float MinSpacingSq = MinSpacing * MinSpacing;
		for (const FVector& Existing : *Locations)
		{
			if (FVector::DistSquared(Location, Existing) < MinSpacingSq)
			{
				return false;
			}
		}
	}
	return true;
}

bool USHProceduralPlacement::IsNearRoad(FVector Location, float Tolerance) const
{
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(Tolerance);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, Location, FQuat::Identity, ECC_WorldStatic, Sphere))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor() && Overlap.GetActor()->Tags.Contains(FName(TEXT("Road"))))
			{
				return true;
			}
		}
	}
	return false;
}

bool USHProceduralPlacement::IsNearBuilding(FVector Location, float Tolerance) const
{
	return OverlapsBuilding(Location, Tolerance);
}

bool USHProceduralPlacement::MatchesTerrainPreference(FVector Location, ESHPreferredTerrain Preference) const
{
	switch (Preference)
	{
	case ESHPreferredTerrain::Flat:
		return GetSlopeAngleAt(Location) <= MaxFlatSlopeAngle;
	case ESHPreferredTerrain::Elevated:
		return Location.Z > (AverageTerrainElevation + ElevatedThreshold);
	case ESHPreferredTerrain::LowGround:
		return Location.Z < (AverageTerrainElevation + LowGroundThreshold);
	case ESHPreferredTerrain::NearRoad:
		return IsNearRoad(Location);
	case ESHPreferredTerrain::NearBuilding:
		return IsNearBuilding(Location);
	case ESHPreferredTerrain::Any:
	default:
		return true;
	}
}

TArray<FVector> USHProceduralPlacement::GetApproachRoutePoints() const
{
	TArray<FVector> Routes;

	// Generate approach route points in a ring around the battlefield center.
	// These represent likely player approach directions.
	for (int32 i = 0; i < ApproachRoutePointCount; ++i)
	{
		const float Angle = (static_cast<float>(i) / ApproachRoutePointCount) * UE_TWO_PI;
		const float Dist = 20000.f; // 200m out from center.
		FVector Point = BattlefieldCenter + FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, 0.f);

		// Trace to ground.
		FHitResult Hit;
		const FVector Start = FVector(Point.X, Point.Y, BattlefieldCenter.Z + TraceStartHeight);
		if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start - FVector(0.f, 0.f, TraceLength), GroundTraceChannel))
		{
			Point = Hit.ImpactPoint + FVector(0.f, 0.f, SightlineEyeHeight);
		}

		Routes.Add(Point);
	}

	return Routes;
}

// =========================================================================
//  Default rule sets
// =========================================================================

TArray<FSHPlacementRule> USHProceduralPlacement::CreateDefaultCoverRules()
{
	TArray<FSHPlacementRule> Rules;

	// Sandbag positions.
	FSHPlacementRule Sandbags;
	Sandbags.Category = ESHPlacementCategory::Cover;
	Sandbags.MinCount = 15;
	Sandbags.MaxCount = 30;
	Sandbags.MinSpacing = 1500.f;
	Sandbags.PreferredTerrain = ESHPreferredTerrain::Flat;
	Sandbags.SpawnRadius = 40000.f;
	Sandbags.Weight = 1.f;
	Rules.Add(Sandbags);

	// Concrete barriers.
	FSHPlacementRule Barriers;
	Barriers.Category = ESHPlacementCategory::Cover;
	Barriers.MinCount = 8;
	Barriers.MaxCount = 15;
	Barriers.MinSpacing = 2000.f;
	Barriers.PreferredTerrain = ESHPreferredTerrain::NearRoad;
	Barriers.SpawnRadius = 35000.f;
	Barriers.Weight = 0.8f;
	Rules.Add(Barriers);

	// Vehicle wrecks.
	FSHPlacementRule Wrecks;
	Wrecks.Category = ESHPlacementCategory::Cover;
	Wrecks.MinCount = 3;
	Wrecks.MaxCount = 8;
	Wrecks.MinSpacing = 5000.f;
	Wrecks.PreferredTerrain = ESHPreferredTerrain::NearRoad;
	Wrecks.SpawnRadius = 40000.f;
	Wrecks.Weight = 0.5f;
	Rules.Add(Wrecks);

	return Rules;
}

TArray<FSHPlacementRule> USHProceduralPlacement::CreateDefaultVegetationRules()
{
	TArray<FSHPlacementRule> Rules;

	// Tree clusters.
	FSHPlacementRule Trees;
	Trees.Category = ESHPlacementCategory::Vegetation;
	Trees.MinCount = 40;
	Trees.MaxCount = 80;
	Trees.MinSpacing = 800.f;
	Trees.PreferredTerrain = ESHPreferredTerrain::Any;
	Trees.SpawnRadius = 50000.f;
	Trees.Weight = 1.f;
	Rules.Add(Trees);

	// Bushes / low vegetation.
	FSHPlacementRule Bushes;
	Bushes.Category = ESHPlacementCategory::Vegetation;
	Bushes.MinCount = 60;
	Bushes.MaxCount = 120;
	Bushes.MinSpacing = 400.f;
	Bushes.PreferredTerrain = ESHPreferredTerrain::Any;
	Bushes.SpawnRadius = 50000.f;
	Bushes.Weight = 1.f;
	Rules.Add(Bushes);

	return Rules;
}

TArray<FSHPlacementRule> USHProceduralPlacement::CreateDefaultEnemyPositionRules()
{
	TArray<FSHPlacementRule> Rules;

	// Infantry positions.
	FSHPlacementRule Infantry;
	Infantry.Category = ESHPlacementCategory::EnemyPosition;
	Infantry.MinCount = 20;
	Infantry.MaxCount = 40;
	Infantry.MinSpacing = 3000.f;
	Infantry.PreferredTerrain = ESHPreferredTerrain::Any;
	Infantry.SpawnRadius = 40000.f;
	Infantry.bCheckLineOfSight = true;
	Infantry.Weight = 1.f;
	Rules.Add(Infantry);

	// Elevated overwatch.
	FSHPlacementRule Overwatch;
	Overwatch.Category = ESHPlacementCategory::EnemyPosition;
	Overwatch.MinCount = 3;
	Overwatch.MaxCount = 6;
	Overwatch.MinSpacing = 8000.f;
	Overwatch.PreferredTerrain = ESHPreferredTerrain::Elevated;
	Overwatch.SpawnRadius = 45000.f;
	Overwatch.bCheckLineOfSight = true;
	Overwatch.Weight = 0.6f;
	Rules.Add(Overwatch);

	return Rules;
}
