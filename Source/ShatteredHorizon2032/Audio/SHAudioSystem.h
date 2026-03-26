// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHAudioSystem.generated.h"

class USoundSubmix;
class USoundEffectSubmixPreset;
class USoundBase;
class USHBallisticsSystem;

// ─────────────────────────────────────────────────────────────
//  Combat event record
// ─────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FSHCombatAudioEvent
{
	GENERATED_BODY()

	/** World location where the combat event occurred. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Combat")
	FVector Location = FVector::ZeroVector;

	/** Normalized intensity of this event [0..1]. */
	UPROPERTY(BlueprintReadOnly, Category = "Audio|Combat")
	float Intensity = 0.f;

	/** World time when this event was registered. */
	UPROPERTY()
	float Timestamp = 0.f;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnCombatIntensityChanged,
	float, NewIntensity);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnAuditoryExclusionChanged,
	bool, bIsActive);

// ─────────────────────────────────────────────────────────────
//  USHAudioSystem — World Subsystem
// ─────────────────────────────────────────────────────────────

/**
 * USHAudioSystem
 *
 * World subsystem that coordinates all audio mixing for the game.
 * Drives a dynamic mix based on combat intensity: gunfire events,
 * explosions, and suppression levels contribute to an aggregate
 * combat intensity value (0..1) that controls mix bus levels.
 *
 * At peak combat stress (from the fatigue system), applies auditory
 * exclusion — selective frequency dropout via submix effects — to
 * simulate the real-world phenomenon of perceptual narrowing.
 *
 * Also provides extended Doppler modelling for passing supersonic
 * projectiles, building on the existing ballistics crack system.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHAudioSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  Subsystem lifecycle
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	/** Called every frame from a world tick delegate. */
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Combat event registration
	// ------------------------------------------------------------------

	/**
	 * Register a combat audio event at a world location.
	 * This feeds the combat intensity tracker — events decay over time.
	 *
	 * @param Location   World-space position of the combat event.
	 * @param Intensity  Magnitude of the event [0..1]. Gunfire ~0.1, explosion ~0.5-1.0.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio")
	void RegisterCombatEvent(FVector Location, float Intensity);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the current aggregate combat intensity [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio")
	float GetCombatIntensity() const { return CurrentCombatIntensity; }

	/** Get the current stress level driving auditory exclusion [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio")
	float GetStressLevel() const { return CurrentStressLevel; }

	/** Returns true if auditory exclusion is currently active. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio")
	bool IsAuditoryExclusionActive() const { return bAuditoryExclusionActive; }

	/** Get the active gunfire event count within the tracking window. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio")
	int32 GetActiveGunfireCount() const { return ActiveGunfireCount; }

	/** Get the active explosion count within the tracking window. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio")
	int32 GetActiveExplosionCount() const { return ActiveExplosionCount; }

	// ------------------------------------------------------------------
	//  Stress / Auditory exclusion
	// ------------------------------------------------------------------

	/**
	 * Set the stress level that controls auditory exclusion.
	 * Intended to be called by the fatigue system or suppression system.
	 *
	 * @param Level  Normalized stress [0..1]. Above StressThreshold triggers exclusion.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio")
	void SetStressLevel(float Level);

	// ------------------------------------------------------------------
	//  Doppler / Supersonic
	// ------------------------------------------------------------------

	/**
	 * Compute a Doppler pitch shift for a projectile passing the listener.
	 * Extends the existing supersonic crack system with proper pitch modelling.
	 *
	 * @param ProjectilePosition  Current projectile world position (cm).
	 * @param ProjectileVelocity  Current projectile velocity (cm/s).
	 * @param ListenerPosition    Listener world position (cm).
	 * @return Pitch multiplier (1.0 = no shift, >1.0 = approaching, <1.0 = receding).
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Doppler")
	static float ComputeDopplerPitchShift(
		const FVector& ProjectilePosition,
		const FVector& ProjectileVelocity,
		const FVector& ListenerPosition);

	/**
	 * Play a supersonic crack with proper Doppler at the listener.
	 * Should be called when a projectile passes within near-miss range.
	 *
	 * @param CrackSound          Sound asset to play.
	 * @param ProjectilePosition  Position at closest approach (cm).
	 * @param ProjectileVelocity  Velocity at closest approach (cm/s).
	 * @param ListenerPosition    Listener position (cm).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Doppler")
	void PlaySupersonicCrackWithDoppler(
		USoundBase* CrackSound,
		const FVector& ProjectilePosition,
		const FVector& ProjectileVelocity,
		const FVector& ListenerPosition);

	// ------------------------------------------------------------------
	//  Crack-Thump propagation
	// ------------------------------------------------------------------

	/**
	 * Play a gunshot with proper crack-thump separation.
	 *
	 * Doctrine: For supersonic rounds, the listener hears two distinct sounds:
	 * 1) The supersonic CRACK arrives first (travels with the bullet's shock wave)
	 * 2) The muzzle THUMP arrives later (travels at speed of sound: distance / 343 m/s)
	 *
	 * A skilled player can estimate shooter distance from the delay between
	 * crack and thump. This is THE defining audio signature of real combat.
	 *
	 * @param MuzzleLocation      World position of the weapon muzzle (cm).
	 * @param BulletPassLocation  Closest point the bullet passed to the listener (cm).
	 * @param ProjectileSpeed     Bullet velocity at pass point (cm/s).
	 * @param MuzzleSound         Muzzle report (thump) sound asset.
	 * @param CrackSound          Supersonic crack sound asset.
	 * @param bIsSuppressed       If true, muzzle report is greatly reduced.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|CrackThump")
	void PlayCrackThump(
		const FVector& MuzzleLocation,
		const FVector& BulletPassLocation,
		float ProjectileSpeed,
		USoundBase* MuzzleSound,
		USoundBase* CrackSound,
		bool bIsSuppressed = false);

	/**
	 * Compute the delay between crack and thump for a given geometry.
	 *
	 * @param MuzzleLocation      World muzzle position (cm).
	 * @param ListenerPosition    Listener position (cm).
	 * @param ProjectileSpeed     Speed of the projectile at listener pass (cm/s).
	 * @return Delay in seconds (thump arrives this much later than crack). 0 if subsonic.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|CrackThump")
	static float ComputeCrackThumpDelay(
		const FVector& MuzzleLocation,
		const FVector& ListenerPosition,
		float ProjectileSpeed);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio")
	FOnCombatIntensityChanged OnCombatIntensityChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio")
	FOnAuditoryExclusionChanged OnAuditoryExclusionChanged;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Duration (seconds) a combat event remains relevant before full decay. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float EventDecayTime = 8.f;

	/** Maximum number of combat events to track simultaneously. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	int32 MaxTrackedEvents = 64;

	/** Radius (cm) around the listener within which events contribute at full weight. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float FullWeightRadius = 5000.f;

	/** Maximum radius (cm) within which events have any contribution. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float MaxEventRadius = 25000.f;

	/** Intensity threshold for an event to be counted as gunfire (below explosion). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float GunfireIntensityThreshold = 0.05f;

	/** Intensity threshold for an event to be counted as an explosion. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float ExplosionIntensityThreshold = 0.4f;

	/** Interpolation speed for combat intensity smoothing. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float IntensitySmoothingSpeed = 3.f;

	/** Stress level above which auditory exclusion begins. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float StressThreshold = 0.7f;

	/** How quickly auditory exclusion ramps up (seconds to full effect). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float ExclusionRampUpTime = 1.5f;

	/** How quickly auditory exclusion fades out (seconds to clear). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float ExclusionRampDownTime = 3.f;

	/** Low-pass filter cutoff frequency when exclusion is fully active (Hz). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float ExclusionLowPassCutoff = 800.f;

	/** High-pass filter cutoff frequency when exclusion is fully active (Hz). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float ExclusionHighPassCutoff = 2000.f;

	/** Volume reduction of the world mix bus at maximum combat intensity (dB). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float MaxWorldBusAttenuation = -6.f;

	/** Volume boost of the combat mix bus at maximum combat intensity (dB). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Config")
	float MaxCombatBusBoost = 3.f;

	/** Submix for world/ambient audio. Assigned in editor. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Submixes")
	TObjectPtr<USoundSubmix> WorldAudioSubmix;

	/** Submix for combat audio (weapons, explosions). Assigned in editor. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Submixes")
	TObjectPtr<USoundSubmix> CombatAudioSubmix;

	/** Submix for dialogue / radio. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Submixes")
	TObjectPtr<USoundSubmix> DialogueAudioSubmix;

	/** Submix effect preset applied during auditory exclusion (low-pass). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Submixes")
	TObjectPtr<USoundEffectSubmixPreset> ExclusionFilterPreset;

	/** Speed of sound in cm/s for Doppler calculations. */
	static constexpr float SpeedOfSoundCmS = 34300.f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------
	void TickCombatEvents(float DeltaSeconds);
	void TickCombatIntensity(float DeltaSeconds);
	void TickAuditoryExclusion(float DeltaSeconds);
	void TickMixBusLevels(float DeltaSeconds);
	float ComputeRawIntensity() const;
	FVector GetListenerPosition() const;

	/** Bind to world tick delegate. */
	void BindToWorldTick();
	FDelegateHandle TickDelegateHandle;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Ring buffer of active combat events. */
	TArray<FSHCombatAudioEvent> ActiveEvents;

	/** Smoothed aggregate combat intensity [0..1]. */
	float CurrentCombatIntensity = 0.f;

	/** Raw (unsmoothed) intensity from this frame. */
	float RawCombatIntensity = 0.f;

	/** Current stress level from external systems [0..1]. */
	float CurrentStressLevel = 0.f;

	/** Current auditory exclusion strength [0..1]. 0 = none, 1 = fully active. */
	float ExclusionStrength = 0.f;

	/** Whether auditory exclusion is currently active. */
	bool bAuditoryExclusionActive = false;

	/** Previous frame's exclusion state, for delegate broadcast. */
	bool bWasExclusionActive = false;

	/** Tracked event counts for the current frame. */
	int32 ActiveGunfireCount = 0;
	int32 ActiveExplosionCount = 0;

	/** Current world bus volume offset (dB). */
	float CurrentWorldBusOffset = 0.f;

	/** Current combat bus volume offset (dB). */
	float CurrentCombatBusOffset = 0.f;

	/** Whether the exclusion submix effect has been pushed. */
	bool bExclusionEffectApplied = false;
};
