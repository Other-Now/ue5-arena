#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "ArenaWeaponData.h"
#include "ArenaWeaponComponent.generated.h"

class UStaticMeshComponent;
struct FStreamableHandle;

// Projectile weapon. The owning client does a cheap local check (ammo,
// cooldown) and sends a validated server RPC; the server re-checks everything
// with its own clock and spawns the projectile. Never ticks.
UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class ARENAGAME_API UArenaWeaponComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaWeaponComponent();

	// Call on the owning client for players, or on the server for AI.
	void RequestFire();

	int32 GetAmmo() const { return Ammo; }
	bool IsReloading() const { return bReloading; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Optional data asset. If it doesn't exist (fresh checkout), the defaults
	// in Stats are used, so the game runs with zero content.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	TSoftObjectPtr<UArenaWeaponData> WeaponData;

	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	FArenaWeaponStats Stats;

	// Max distance between the client's claimed muzzle and the server's view
	// of the pawn: walk speed (600) * a generous 300 ms RTT, plus the muzzle offset.
	UPROPERTY(EditDefaultsOnly, Category = "Weapon")
	float MaxOriginOffset = 350.f;

	UPROPERTY(Replicated)
	int32 Ammo = 0;

	UPROPERTY(Replicated)
	bool bReloading = false;

	// Reliable: shots are low-rate (<10/s) and a silently dropped shot under
	// packet loss feels worse than the retransmit delay.
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerFire(const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction);

private:
	void FireAuthoritative(const FVector& Origin, const FVector& Direction);
	void GetMuzzle(FVector& OutOrigin, FVector& OutDirection) const;
	void StartReload();
	void FinishReload();

	void OnWeaponDataLoaded();
	void LoadWeaponMesh();
	void OnWeaponMeshLoaded();

	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> WeaponMeshComponent;

	TSharedPtr<FStreamableHandle> DataHandle;
	TSharedPtr<FStreamableHandle> MeshHandle;
	FTimerHandle ReloadTimer;
	double LastServerFireTime = -1e9;
	double LastLocalFireTime = -1e9;
};
