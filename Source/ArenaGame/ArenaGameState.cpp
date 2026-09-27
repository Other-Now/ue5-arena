#include "ArenaGameState.h"
#include "Net/UnrealNetwork.h"

void AArenaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaGameState, RemainingSeconds);
}
