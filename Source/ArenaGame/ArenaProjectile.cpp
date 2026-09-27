#include "ArenaProjectile.h"
#include "ArenaGame.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

// A/B switch for the bandwidth measurement (docs/MEASURE.md). 0 = clients
// simulate from the one-off launch state (default). 1 = the naive approach:
// stream replicated movement at 30 Hz for the projectile's whole flight.
static TAutoConsoleVariable<int32> CVarProjectileReplicateMovement(
	TEXT("arena.ProjectileReplicateMovement"), 0,
	TEXT("1 = replicate projectile movement every net update (baseline for bandwidth comparison)."));

AArenaProjectile::AArenaProjectile()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	SetReplicatingMovement(false);
	// State only changes at launch/deactivate, and both call ForceNetUpdate(),
	// so a low polling rate costs nothing in responsiveness.
	SetNetUpdateFrequency(10.f);
	SetNetCullDistanceSquared(FMath::Square(8000.f));

	Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
	Collision->InitSphereRadius(12.f);
	Collision->SetCollisionProfileName(TEXT("Projectile"));
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Collision->SetGenerateOverlapEvents(false);
	Collision->CanCharacterStepUpOn = ECB_No;
	RootComponent = Collision;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetupAttachment(Collision);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetRelativeScale3D(FVector(0.2f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (Sphere.Succeeded())
	{
		Mesh->SetStaticMesh(Sphere.Object);
	}

	Movement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Movement"));
	Movement->UpdatedComponent = Collision;
	Movement->ProjectileGravityScale = 0.f;
	Movement->bRotationFollowsVelocity = true;
	Movement->bShouldBounce = false;
	Movement->InitialSpeed = 0.f;
	Movement->bAutoActivate = false;

	// Inactive = invisible mesh + no collision, but the *actor* is not hidden:
	// AActor::IsNetRelevantFor drops hidden actors with collision off, which
	// would stop the bActive=false update reaching clients and make every
	// reuse close and reopen the actor channel.
	Mesh->SetVisibility(false);
}

void AArenaProjectile::BeginPlay()
{
	Super::BeginPlay();
	Collision->OnComponentHit.AddDynamic(this, &ThisClass::OnHit);
}

void AArenaProjectile::Launch(const FVector& Origin, const FVector& Direction, float Speed,
	float InDamage, float LifeSeconds, APawn* InInstigator)
{
	check(HasAuthority());

	Damage = InDamage;
	SetInstigator(InInstigator);

	Collision->ClearMoveIgnoreActors();
	if (InInstigator)
	{
		Collision->IgnoreActorWhenMoving(InInstigator, true);
	}

	const bool bStreamMovement = CVarProjectileReplicateMovement.GetValueOnGameThread() != 0;
	SetReplicatingMovement(bStreamMovement);
	SetNetUpdateFrequency(bStreamMovement ? 30.f : 10.f);

	LaunchState.Origin = Origin;
	LaunchState.Direction = Direction;
	LaunchState.Speed = Speed;
	LaunchState.Sequence++;
	LaunchState.bActive = true;

	ApplyLaunchLocally();

	GetWorldTimerManager().SetTimer(LifeTimer, this, &ThisClass::Deactivate, LifeSeconds, false);
	ForceNetUpdate();
}

void AArenaProjectile::Deactivate()
{
	if (!HasAuthority() || !LaunchState.bActive)
	{
		return;
	}

	GetWorldTimerManager().ClearTimer(LifeTimer);
	LaunchState.bActive = false;
	SetLiveLocally(false);
	ForceNetUpdate();

	OnReleased.ExecuteIfBound(this);
}

void AArenaProjectile::OnRep_LaunchState()
{
	if (LaunchState.bActive)
	{
		ApplyLaunchLocally();
	}
	else
	{
		SetLiveLocally(false);
	}
}

void AArenaProjectile::ApplyLaunchLocally()
{
	const FVector Dir = LaunchState.Direction;
	SetActorLocationAndRotation(LaunchState.Origin, Dir.Rotation(), false, nullptr, ETeleportType::TeleportPhysics);

	if (!HasAuthority())
	{
		// Instigator replicates alongside the launch state; ignore our shooter
		// locally too so the client-side copy doesn't pop on the muzzle.
		Collision->ClearMoveIgnoreActors();
		if (APawn* Shooter = GetInstigator())
		{
			Collision->IgnoreActorWhenMoving(Shooter, true);
		}
	}

	SetLiveLocally(true);

	// ProjectileMovement clears UpdatedComponent when it stops on impact, so a
	// reused actor must re-attach it every launch.
	Movement->SetUpdatedComponent(Collision);
	Movement->Activate(true);
	Movement->Velocity = Dir * LaunchState.Speed;
	Movement->UpdateComponentVelocity();
}

void AArenaProjectile::SetLiveLocally(bool bLive)
{
	Mesh->SetVisibility(bLive);
	Collision->SetCollisionEnabled(bLive ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	if (!bLive)
	{
		Movement->StopMovementImmediately();
		Movement->Deactivate();
	}
}

void AArenaProjectile::OnHit(UPrimitiveComponent* HitComp, AActor* OtherActor, UPrimitiveComponent* OtherComp,
	FVector NormalImpulse, const FHitResult& Hit)
{
	if (!LaunchState.bActive)
	{
		return;
	}

	if (!HasAuthority())
	{
		// Visual only: hide our local copy. The server decides whether it hit.
		SetLiveLocally(false);
		return;
	}

	if (OtherActor && OtherActor != GetInstigator() && Damage > 0.f)
	{
		UGameplayStatics::ApplyPointDamage(OtherActor, Damage, GetActorForwardVector(), Hit,
			GetInstigatorController(), this, UDamageType::StaticClass());
	}
	Deactivate();
}

void AArenaProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AArenaProjectile, LaunchState);
}
