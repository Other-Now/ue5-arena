#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ArenaProjectilePool.generated.h"

class AArenaProjectile;
struct FArenaWeaponStats;

// Server-side free list of projectile actors. Spawning and destroying a
// replicated actor per shot costs component registration, a net channel
// open/close and GC churn; reusing a fixed set avoids all three.
// `arena.UsePool 0` switches back to spawn/destroy for the A/B measurement.
UCLASS()
class ARENAGAME_API UArenaProjectilePool : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	// Server only. Returns the launched projectile, or nullptr on clients.
	AArenaProjectile* Launch(const FVector& Origin, const FVector& Direction,
		const FArenaWeaponStats& Stats, APawn* Instigator);

	// Server only. Fires ShotsPerSecond projectiles from random points for
	// Seconds, independent of players, so frame cost is reproducible.
	void StartStress(float ShotsPerSecond, float Seconds);

	int32 NumLive() const { return Live; }
	int32 NumFree() const { return Free.Num(); }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	AArenaProjectile* SpawnProjectile();
	void Release(AArenaProjectile* Projectile);
	void StressStep();

	UPROPERTY()
	TArray<TObjectPtr<AArenaProjectile>> Free;

	int32 Live = 0;

	FTimerHandle StressTimer;
	double StressEndTime = 0.0;
	float StressRate = 0.f;
	float StressCarry = 0.f;
};
