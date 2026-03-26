// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SHInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class USHInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * ISHInteractable
 *
 * Interface for world objects the player can interact with:
 * doors, ammo crates, weapon pickups, vehicle entry, intel documents, etc.
 */
class SHATTEREDHORIZON2032_API ISHInteractable
{
	GENERATED_BODY()

public:
	/**
	 * Called when the player presses the interact key while looking at this actor.
	 * @param Interactor  The controller performing the interaction.
	 * @return True if the interaction was consumed.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SH|Interaction")
	bool Interact(APlayerController* Interactor);

	/**
	 * Get a short display string for the interaction prompt (e.g., "Pick up M4A1", "Open door").
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SH|Interaction")
	FText GetInteractionPrompt() const;

	/**
	 * Whether this object can currently be interacted with.
	 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "SH|Interaction")
	bool CanInteract(APlayerController* Interactor) const;
};
