#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "ArenaGameState.generated.h"

// Match-wide state every client needs. Written only by the game mode.
UCLASS()
class ARENAGAME_API AArenaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(Replicated)
	int32 RemainingSeconds = 0;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};
