#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ArenaHUD.generated.h"

// Canvas HUD (no UMG assets): health, ammo, score, clock, crosshair, and on the
// host the projectile pool counters used during profiling.
UCLASS()
class ARENAGAME_API AArenaHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
