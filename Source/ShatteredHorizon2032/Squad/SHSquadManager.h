// Copyright Shattered Horizon 2032. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHSquadOrder.h"
#include "SHSquadMember.h"
#include "SHSquadManager.generated.h"

class ACharacter;

/* ───────────────────────────────────────────────────────────── */
/*  Formation Types                                             */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHFormationType : uint8
{
	Column			UMETA(DisplayName = "Column"),
	Line			UMETA(DisplayName = "Line"),
	Wedge			UMETA(DisplayName = "Wedge"),
	File			UMETA(DisplayName = "File"),
	StaggeredColumn	UMETA(DisplayName = "Staggered Column")
};

/* ───────────────────────────────────────────────────────────── */
/*  Squad Cohesion Data                                         */
/* ───────────────────────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSquadCohesion
{
	GENERATED_BODY()

	/** Average distance (cm) from members to the squad centroid. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Cohesion")
	float AverageSpread = 0.f;

	/** Maximum distance (cm) any single member is from the leader. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Cohesion")
	float MaxMemberDistance = 0.f;

	/** Number of members with line-of-sight to the squad leader. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Cohesion")
	int32 MembersWithLOSToLeader = 0;

	/** 0 = completely dispersed, 1 = tight formation. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Cohesion")
	float CohesionScore = 1.f;
};

/* ───────────────────────────────────────────────────────────── */
/*  Delegates                                                   */
/* ───────────────────────────────────────────────────────────── */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnSquadMemberStatusChanged, ASHSquadMember*, Member, ESHWoundState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnContactReported, const FSHContactReport&, Report);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnFormationChanged, ESHFormationType, NewFormation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnSquadMoraleChanged, float, NewMorale);

/* ───────────────────────────────────────────────────────────── */
/*  USHSquadManager — Squad Management Component                */
/* ───────────────────────────────────────────────────────────── */

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHSquadManager : public UActorComponent
{
	GENERATED_BODY()

public:
	USHSquadManager();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

public:
	/* ── Squad Composition ──────────────────────────────────── */

	/** The player character acting as team leader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Composition")
	TWeakObjectPtr<ACharacter> PlayerLeader;

	/** Maximum squad size (excluding the player leader). */
	static constexpr int32 MaxSquadSize = 4;

	/** Register a member into the squad. Returns the assigned index or -1 if full. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Composition")
	int32 AddSquadMember(ASHSquadMember* NewMember);

	/** Remove a member (e.g. KIA confirmed, extracted). */
	UFUNCTION(BlueprintCallable, Category = "Squad|Composition")
	void RemoveSquadMember(ASHSquadMember* Member);

	/** Get all living squad members. */
	UFUNCTION(BlueprintPure, Category = "Squad|Composition")
	TArray<ASHSquadMember*> GetAliveMembers() const;

	/** Get combat-effective members only. */
	UFUNCTION(BlueprintPure, Category = "Squad|Composition")
	TArray<ASHSquadMember*> GetCombatEffectiveMembers() const;

	/** Get members of a specific buddy team. */
	UFUNCTION(BlueprintPure, Category = "Squad|Composition")
	TArray<ASHSquadMember*> GetBuddyTeam(ESHBuddyTeam Team) const;

	/** Get the full members array (including KIA). */
	UFUNCTION(BlueprintPure, Category = "Squad|Composition")
	const TArray<ASHSquadMember*>& GetSquadMembers() const { return SquadMembers; }

	/** Number of combat-effective members. */
	UFUNCTION(BlueprintPure, Category = "Squad|Composition")
	int32 GetEffectiveStrength() const;

	/* ── Orders ─────────────────────────────────────────────── */

	/** Issue an order to the entire squad. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void IssueSquadOrder(const FSHSquadOrder& Order);

	/** Issue an order to a specific member by index. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void IssueOrderToMember(int32 MemberIndex, const FSHSquadOrder& Order);

	/** Issue an order to a specific buddy team. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void IssueOrderToBuddyTeam(ESHBuddyTeam Team, const FSHSquadOrder& Order);

	/** Convenience: issue a move order to the full squad. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderMoveTo(const FVector& Location);

	/** Convenience: issue a hold position order. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderHoldPosition();

	/** Convenience: issue suppressive fire on a target location. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderSuppress(const FVector& TargetLocation, AActor* TargetActor = nullptr);

	/** Convenience: issue a flank order. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderFlank(bool bFlankLeft);

	/** Convenience: issue fall back. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderFallBack();

	/** Convenience: issue breach and clear. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderBreachAndClear(const FVector& DoorLocation, AActor* DoorActor = nullptr);

	/** Convenience: rally on leader. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Orders")
	void OrderRally();

	/* ── Formation ──────────────────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation")
	ESHFormationType CurrentFormation = ESHFormationType::Wedge;

	/** Spacing between formation positions in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation")
	float FormationSpacing = 400.f;

	/** Set the squad formation type. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Formation")
	void SetFormation(ESHFormationType NewFormation);

	/** Get the world-space formation position for a given member index. */
	UFUNCTION(BlueprintPure, Category = "Squad|Formation")
	FVector GetFormationPosition(int32 MemberIndex) const;

	/** Should formation auto-adapt to terrain? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation")
	bool bAutoAdaptFormation = true;

	/** Maximum formation spacing before cohesion is considered broken. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Formation")
	float MaxFormationSpread = 2000.f;

	/* ── Cohesion ───────────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Cohesion")
	FSHSquadCohesion Cohesion;

	/* ── Morale (Squad-Level) ───────────────────────────────── */

	/** Aggregate squad morale derived from individual members. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Morale")
	float SquadMorale = 80.f;

	/* ── Casualty Tracking ──────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Casualties")
	int32 WoundedCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Casualties")
	int32 KIACount = 0;

	/* ── Contact Reports ────────────────────────────────────── */

	/** Recent contact reports (ring buffer, kept for tactical awareness). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Contacts")
	TArray<FSHContactReport> RecentContacts;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Contacts")
	int32 MaxContactReports = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Contacts")
	float ContactReportExpiry = 30.f;

	/** Called by squad members when they spot contacts. */
	void OnContactReported(ASHSquadMember* Reporter, const FSHContactReport& Report);

	/* ── Delegates ──────────────────────────────────────────── */

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnSquadMemberStatusChanged OnSquadMemberStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnContactReported OnContactReportedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnFormationChanged OnFormationChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnSquadMoraleChanged OnSquadMoraleChanged;

private:
	UPROPERTY()
	TArray<ASHSquadMember*> SquadMembers;

	/* ── Internal Tick Helpers ──────────────────────────────── */
	void UpdateCohesion();
	void UpdateSquadMorale();
	void UpdateCasualties();
	void UpdateFormationAdaptation();
	void ExpireOldContacts();

	/** Compute a local-space formation offset for a member index. */
	FVector ComputeFormationOffset(int32 Index, ESHFormationType Formation) const;

	/** Adapt formation based on terrain (narrow passages -> file, open -> wedge). */
	ESHFormationType EvaluateTerrainFormation() const;

	/** Internal handler bound to member wound state changes. */
	UFUNCTION()
	void HandleMemberWoundStateChanged(ASHSquadMember* Member, ESHWoundState NewState);
};
