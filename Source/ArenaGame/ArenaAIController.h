#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "ArenaAIController.generated.h"

UENUM()
enum class EArenaBotState : uint8
{
	Idle,
	Chase,
	Attack
};

// Three-state bot: Idle -> Chase (pathfind to nearest visible enemy) -> Attack
// (in range with line of sight). Decisions run on a timer every ThinkInterval
// rather than every frame; `arena.BotThinkOnTick 1` switches to per-frame
// thinking for the before/after measurement in docs/MEASURE.md.
UCLASS()
class ARENAGAME_API AArenaAIController : public AAIController
{
	GENERATED_BODY()

public:
	AArenaAIController();

	EArenaBotState GetBotState() const { return State; }

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditDefaultsOnly, Category = "Bot")
	float SightRadius = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot")
	float AttackRadius = 1200.f;

	UPROPERTY(EditDefaultsOnly, Category = "Bot")
	float ThinkInterval = 0.25f;

private:
	void Think();
	APawn* FindNearestEnemy() const;

	EArenaBotState State = EArenaBotState::Idle;
	TWeakObjectPtr<APawn> Target;
	FTimerHandle ThinkTimer;
};
