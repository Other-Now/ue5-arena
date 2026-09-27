#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "ArenaProjectile.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;

// Everything a client needs to simulate a projectile itself. Replicated once per
// launch instead of streaming position updates while it flies.
USTRUCT()
struct FArenaProjectileLaunch
{
	GENERATED_BODY()

	// 1 cm precision, ~20 bits/axis in practice instead of 32.
	UPROPERTY()
	FVector_NetQuantize Origin;

	// 16 bits/axis.
	UPROPERTY()
	FVector_NetQuantizeNormal Direction;

	UPROPERTY()
	float Speed = 0.f;

	// Bumped on every launch so a pooled actor re-fired with identical
	// parameters still produces a changed struct and a fresh OnRep.
	UPROPERTY()
	uint8 Sequence = 0;

	UPROPERTY()
	bool bActive = false;
};

// Pooled, server-authoritative projectile. The server owns hit detection and
// damage; clients run the same ProjectileMovement locally from the replicated
// launch state for visuals only. Acquired/released by UArenaProjectilePool.
UCLASS()
class ARENAGAME_API AArenaProjectile : public AActor
{
	GENERATED_BODY()

public:
	AArenaProjectile();

	// Server only.
	void Launch(const FVector& Origin, const FVector& Direction, float Speed, float InDamage,
		float LifeSeconds, APawn* InInstigator);

	// Server only. Hides the actor and hands it back to the pool.
	void Deactivate();

	bool IsLaunched() const { return LaunchState.bActive; }

	DECLARE_DELEGATE_OneParam(FOnReleased, AArenaProjectile*);
	FOnReleased OnReleased;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<USphereComponent> Collision;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UPROPERTY(VisibleAnywhere, Category = "Projectile")
	TObjectPtr<UProjectileMovementComponent> Movement;

	UPROPERTY(ReplicatedUsing = OnRep_LaunchState)
	FArenaProjectileLaunch LaunchState;

	UFUNCTION()
	void OnRep_LaunchState();

	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		FVector NormalImpulse, const FHitResult& Hit);

private:
	void ApplyLaunchLocally();
	void SetLiveLocally(bool bLive);

	float Damage = 0.f;
	FTimerHandle LifeTimer;
};
