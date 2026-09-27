#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ArenaGameMode.generated.h"

class AArenaCharacter;

// Server-only rules: spawns bots, scores kills, respawns, runs the match clock.
// Bot count comes from the URL, e.g. `open Arena?listen?Bots=16`.
UCLASS()
class ARENAGAME_API AArenaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AArenaGameMode();

	void OnCharacterKilled(AArenaCharacter* Victim, AController* Killer);

protected:
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	int32 NumBots = 3;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	float RespawnDelay = 3.f;

	UPROPERTY(EditDefaultsOnly, Category = "Arena")
	int32 MatchSeconds = 300;

private:
	void SpawnBot();
	void TickMatchClock();

	FTimerHandle ClockTimer;
};
