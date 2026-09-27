#include "ArenaGameMode.h"
#include "ArenaAIController.h"
#include "ArenaCharacter.h"
#include "ArenaGame.h"
#include "ArenaGameState.h"
#include "ArenaHUD.h"
#include "ArenaPlayerController.h"
#include "ArenaPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

AArenaGameMode::AArenaGameMode()
{
	DefaultPawnClass = AArenaCharacter::StaticClass();
	PlayerControllerClass = AArenaPlayerController::StaticClass();
	PlayerStateClass = AArenaPlayerState::StaticClass();
	GameStateClass = AArenaGameState::StaticClass();
	HUDClass = AArenaHUD::StaticClass();
}

void AArenaGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	NumBots = UGameplayStatics::GetIntOption(Options, TEXT("Bots"), NumBots);
}

void AArenaGameMode::StartPlay()
{
	Super::StartPlay();

	for (int32 i = 0; i < NumBots; ++i)
	{
		SpawnBot();
	}

	if (AArenaGameState* GS = GetGameState<AArenaGameState>())
	{
		GS->RemainingSeconds = MatchSeconds;
	}
	GetWorldTimerManager().SetTimer(ClockTimer, this, &ThisClass::TickMatchClock, 1.f, true);
}

void AArenaGameMode::SpawnBot()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaAIController* Bot = GetWorld()->SpawnActor<AArenaAIController>(AArenaAIController::StaticClass(), Params);
	if (Bot)
	{
		// Same path as human players: pick a start, spawn DefaultPawnClass, possess.
		RestartPlayer(Bot);
	}
}

void AArenaGameMode::OnCharacterKilled(AArenaCharacter* Victim, AController* Killer)
{
	AController* VictimController = Victim->GetController();

	if (Killer && Killer != VictimController)
	{
		if (AArenaPlayerState* PS = Killer->GetPlayerState<AArenaPlayerState>())
		{
			PS->AddKill();
		}
	}
	if (VictimController)
	{
		if (AArenaPlayerState* PS = VictimController->GetPlayerState<AArenaPlayerState>())
		{
			PS->AddDeath();
		}
	}

	Victim->DetachFromControllerPendingDestroy();
	Victim->SetLifeSpan(0.1f); // not Destroy(): we're inside the damage callback chain

	if (VictimController)
	{
		// Weak: the controller may leave (player disconnects) during the delay.
		TWeakObjectPtr<AController> WeakController = VictimController;
		FTimerHandle Handle;
		GetWorldTimerManager().SetTimer(Handle, FTimerDelegate::CreateWeakLambda(this, [this, WeakController]()
		{
			if (AController* C = WeakController.Get())
			{
				RestartPlayer(C);
			}
		}), RespawnDelay, false);
	}
}

void AArenaGameMode::TickMatchClock()
{
	AArenaGameState* GS = GetGameState<AArenaGameState>();
	if (!GS)
	{
		return;
	}
	if (--GS->RemainingSeconds > 0)
	{
		return;
	}

	// Round over: log the winner and start a new round in place.
	const AArenaPlayerState* Best = nullptr;
	for (APlayerState* PS : GS->PlayerArray)
	{
		const AArenaPlayerState* APS = Cast<AArenaPlayerState>(PS);
		if (APS && (!Best || APS->Kills > Best->Kills))
		{
			Best = APS;
		}
	}
	UE_LOG(LogArena, Log, TEXT("Round over. Winner: %s"), Best ? *Best->GetPlayerName() : TEXT("nobody"));

	for (APlayerState* PS : GS->PlayerArray)
	{
		if (AArenaPlayerState* APS = Cast<AArenaPlayerState>(PS))
		{
			APS->ResetScore();
		}
	}
	GS->RemainingSeconds = MatchSeconds;
}
