#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ArenaCharacter.generated.h"

class UArenaHealthComponent;
class UArenaWeaponComponent;
class UCameraComponent;
class USpringArmComponent;
class UStaticMeshComponent;
struct FInputActionValue;

// Player and bot pawn. Gameplay lives in components (health, weapon); the
// character only routes input and reports death to the game mode. Movement
// prediction and correction come from CharacterMovementComponent unchanged.
UCLASS()
class ARENAGAME_API AArenaCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AArenaCharacter();

	UArenaHealthComponent* GetHealthComponent() const { return Health; }
	UArenaWeaponComponent* GetWeaponComponent() const { return Weapon; }

protected:
	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;

	// Engine basic-shape stand-in for a skeletal mesh; keeps the repo asset-free.
	UPROPERTY(VisibleAnywhere, Category = "Visual")
	TObjectPtr<UStaticMeshComponent> Body;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UArenaHealthComponent> Health;

	UPROPERTY(VisibleAnywhere, Category = "Gameplay")
	TObjectPtr<UArenaWeaponComponent> Weapon;

private:
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void Fire(const FInputActionValue& Value);

	UFUNCTION()
	void HandleDied(AController* Killer);
};
