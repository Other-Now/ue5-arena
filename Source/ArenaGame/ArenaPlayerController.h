#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ArenaPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

// Owns the Enhanced Input actions and mapping context. They are built in code
// rather than as .uasset files so the repository has no binary input assets
// and the bindings are reviewable in a diff. They live on the controller, not
// the pawn, so they survive respawns.
UCLASS()
class ARENAGAME_API AArenaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Builds the actions on first call and registers the mapping context with
	// this local player's Enhanced Input subsystem. Idempotent.
	void EnsureInputReady();

	UPROPERTY()
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY()
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY()
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY()
	TObjectPtr<UInputAction> FireAction;

private:
	void BuildInput();

	UPROPERTY()
	TObjectPtr<UInputMappingContext> MappingContext;
};
