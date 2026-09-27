#include "ArenaHealthComponent.h"
#include "ArenaRules.h"
#include "Net/UnrealNetwork.h"

UArenaHealthComponent::UArenaHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UArenaHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner && Owner->HasAuthority())
	{
		Health = MaxHealth;
		Owner->OnTakeAnyDamage.AddDynamic(this, &ThisClass::HandleTakeAnyDamage);
	}
}

void UArenaHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float Damage,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (IsDead())
	{
		return;
	}

	const float Old = Health;
	Health = ArenaRules::ApplyDamage(Health, Damage, MaxHealth);
	if (Health == Old)
	{
		return;
	}

	// OnRep does not run on the server, so a listen-server host needs this
	// broadcast to update its own HUD.
	OnHealthChanged.Broadcast(Health, Health - Old);
	if (IsDead())
	{
		OnDied.Broadcast(InstigatedBy);
	}
}

void UArenaHealthComponent::OnRep_Health(float OldHealth)
{
	OnHealthChanged.Broadcast(Health, Health - OldHealth);
}

void UArenaHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UArenaHealthComponent, Health);
}
