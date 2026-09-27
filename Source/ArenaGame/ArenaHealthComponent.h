#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ArenaHealthComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FArenaHealthChanged, float, NewHealth, float, Delta);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FArenaDied, AController*, Killer);

// Replicated health. Damage is applied only on the server (via the owner's
// OnTakeAnyDamage); clients learn about it through OnRep_Health. Never ticks.
UCLASS(ClassGroup = (Arena), meta = (BlueprintSpawnableComponent))
class ARENAGAME_API UArenaHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UArenaHealthComponent();

	UPROPERTY(BlueprintAssignable, Category = "Health")
	FArenaHealthChanged OnHealthChanged;

	// Server only.
	UPROPERTY(BlueprintAssignable, Category = "Health")
	FArenaDied OnDied;

	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	bool IsDead() const { return Health <= 0.f; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(EditDefaultsOnly, Category = "Health", meta = (ClampMin = "1"))
	float MaxHealth = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_Health)
	float Health = 100.f;

	UFUNCTION()
	void OnRep_Health(float OldHealth);

	UFUNCTION()
	void HandleTakeAnyDamage(AActor* DamagedActor, float Damage, const UDamageType* DamageType,
		AController* InstigatedBy, AActor* DamageCauser);
};
