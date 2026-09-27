#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogArena, Log, All);

// Custom object channel declared in Config/DefaultEngine.ini ("Projectile").
// Everything blocks it by default; the Projectile profile ignores itself so
// projectiles never collide with each other.
#define ECC_Projectile ECC_GameTraceChannel1
