#include "ArenaWeaponComponent.h"
#include "ArenaGame.h"
#include "ArenaProjectilePool.h"
#include "ArenaRules.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/StaticMesh.h"
#include "Engine/StreamableManager.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UArenaWeaponComponent::UArenaWeaponComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	WeaponData = TSoftObjectPtr<UArenaWeaponData>(FSoftObjectPath(TEXT("/Game/Weapons/DA_Rifle.DA_Rifle")));
}

void UArenaWeaponComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		Ammo = Stats.MagazineSize;
	}

	if (WeaponData.IsNull())
	{
		LoadWeaponMesh();
		return;
	}
	// Runs on every machine: the server needs the stats, clients need the mesh.
	DataHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		WeaponData.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnWeaponDataLoaded));
}

void UArenaWeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TSharedPtr<FStreamableHandle>* Handle : { &DataHandle, &MeshHandle })
	{
		if (Handle->IsValid())
		{
			(*Handle)->CancelHandle();
			Handle->Reset();
		}
	}
	Super::EndPlay(EndPlayReason);
}

void UArenaWeaponComponent::OnWeaponDataLoaded()
{
	if (const UArenaWeaponData* Data = WeaponData.Get())
	{
		Stats = Data->Stats;
		if (GetOwner()->HasAuthority())
		{
			Ammo = Stats.MagazineSize;
		}
	}
	else
	{
		UE_LOG(LogArena, Verbose, TEXT("%s not found, using built-in weapon stats"), *WeaponData.ToString());
	}
	LoadWeaponMesh();
}

void UArenaWeaponComponent::LoadWeaponMesh()
{
	if (GetNetMode() == NM_DedicatedServer || Stats.WeaponMesh.IsNull())
	{
		return; // purely cosmetic
	}
	MeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		Stats.WeaponMesh.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ThisClass::OnWeaponMeshLoaded));
}

void UArenaWeaponComponent::OnWeaponMeshLoaded()
{
	UStaticMesh* LoadedMesh = Stats.WeaponMesh.Get();
	AActor* Owner = GetOwner();
	if (!LoadedMesh || !Owner || WeaponMeshComponent)
	{
		return;
	}

	WeaponMeshComponent = NewObject<UStaticMeshComponent>(Owner, TEXT("WeaponMesh"));
	WeaponMeshComponent->SetStaticMesh(LoadedMesh);
	WeaponMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	WeaponMeshComponent->SetupAttachment(Owner->GetRootComponent());
	WeaponMeshComponent->SetRelativeLocation(FVector(45.f, 35.f, 20.f));
	WeaponMeshComponent->SetRelativeScale3D(FVector(0.6f, 0.1f, 0.1f));
	WeaponMeshComponent->RegisterComponent();
}

void UArenaWeaponComponent::GetMuzzle(FVector& OutOrigin, FVector& OutDirection) const
{
	const APawn* Pawn = CastChecked<APawn>(GetOwner());
	OutDirection = Pawn->GetControlRotation().Vector();
	OutOrigin = Pawn->GetActorLocation() + OutDirection * 80.f + FVector(0.f, 0.f, 40.f);
}

void UArenaWeaponComponent::RequestFire()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (bReloading || Ammo <= 0 || !ArenaRules::CanFire(Now, LastLocalFireTime, Stats.FireInterval))
	{
		return;
	}
	LastLocalFireTime = Now;

	FVector Origin, Direction;
	GetMuzzle(Origin, Direction);

	if (GetOwner()->HasAuthority())
	{
		FireAuthoritative(Origin, Direction);
	}
	else
	{
		ServerFire(Origin, Direction);
	}
}

bool UArenaWeaponComponent::ServerFire_Validate(const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction)
{
	// Quantised normals are within ~1e-4 of unit length, well inside IsNormalized()'s tolerance.
	return ArenaRules::IsFireRequestPlausible(GetOwner()->GetActorLocation(), Origin, Direction, MaxOriginOffset);
}

void UArenaWeaponComponent::ServerFire_Implementation(const FVector_NetQuantize& Origin, const FVector_NetQuantizeNormal& Direction)
{
	FireAuthoritative(Origin, Direction);
}

void UArenaWeaponComponent::FireAuthoritative(const FVector& Origin, const FVector& Direction)
{
	const double Now = GetWorld()->GetTimeSeconds();
	// 0.8: accept requests bunched together by jitter, reject a client firing >25% fast.
	if (bReloading || Ammo <= 0 || !ArenaRules::CanFire(Now, LastServerFireTime, Stats.FireInterval, 0.8f))
	{
		return;
	}
	LastServerFireTime = Now;
	--Ammo;

	if (UArenaProjectilePool* Pool = GetWorld()->GetSubsystem<UArenaProjectilePool>())
	{
		Pool->Launch(Origin, Direction.GetSafeNormal(), Stats, Cast<APawn>(GetOwner()));
	}

	if (Ammo <= 0)
	{
		StartReload();
	}
}

void UArenaWeaponComponent::StartReload()
{
	bReloading = true;
	GetWorld()->GetTimerManager().SetTimer(ReloadTimer, this, &ThisClass::FinishReload, Stats.ReloadSeconds, false);
}

void UArenaWeaponComponent::FinishReload()
{
	Ammo = Stats.MagazineSize;
	bReloading = false;
}

void UArenaWeaponComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	// Only the owner's HUD shows ammo; nobody else needs the bytes.
	DOREPLIFETIME_CONDITION(UArenaWeaponComponent, Ammo, COND_OwnerOnly);
	DOREPLIFETIME_CONDITION(UArenaWeaponComponent, bReloading, COND_OwnerOnly);
}
