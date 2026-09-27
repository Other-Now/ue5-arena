#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ArenaWeaponData.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FArenaWeaponStats
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float Damage = 20.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "0.02"))
	float FireInterval = 0.15f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ProjectileSpeed = 4000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ProjectileLifeSeconds = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon", meta = (ClampMin = "1"))
	int32 MagazineSize = 20;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	float ReloadSeconds = 1.5f;

	// Soft reference: the mesh is only loaded (asynchronously) when a weapon
	// using it is actually equipped, not when this struct's owner is loaded.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> WeaponMesh =
		TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
};

// Designer-tunable weapon definition. Create one in the editor under
// /Game/Weapons (Content Browser > Miscellaneous > Data Asset > ArenaWeaponData);
// that folder is scanned and always cooked (see DefaultGame.ini).
UCLASS(BlueprintType)
class ARENAGAME_API UArenaWeaponData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon")
	FArenaWeaponStats Stats;
};
