#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ArenaPlayerState.generated.h"

// Per-player score. PlayerState (not the pawn) holds it because it survives
// death and respawn, and is replicated to everyone for the scoreboard.
UCLASS()
class ARENAGAME_API AArenaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated)
	int32 Kills = 0;

	UPROPERTY(Replicated)
	int32 Deaths = 0;

	void AddKill() { ++Kills; }
	void AddDeath() { ++Deaths; }
	void ResetScore() { Kills = 0; Deaths = 0; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
