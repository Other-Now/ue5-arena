#include "ArenaProjectilePool.h"
#include "ArenaGame.h"
#include "ArenaProjectile.h"
#include "ArenaWeaponData.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarUsePool(
	TEXT("arena.UsePool"), 1,
	TEXT("1 = reuse pooled projectiles, 0 = spawn and destroy one actor per shot (baseline)."));

static TAutoConsoleVariable<int32> CVarPoolPrewarm(
	TEXT("arena.PoolPrewarm"), 64,
	TEXT("Projectiles spawned up front when the world begins play (server)."));

static FAutoConsoleCommandWithWorldAndArgs GArenaStressCommand(
	TEXT("arena.Stress"),
	TEXT("arena.Stress [ShotsPerSecond=500] [Seconds=10]. Run on the server/listen-host window."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		if (!World || World->GetNetMode() == NM_Client)
		{
			UE_LOG(LogArena, Warning, TEXT("arena.Stress must run on the server or listen host."));
			return;
		}
		const float Rate = Args.Num() > 0 ? FCString::Atof(*Args[0]) : 500.f;
		const float Seconds = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 10.f;
		if (UArenaProjectilePool* Pool = World->GetSubsystem<UArenaProjectilePool>())
		{
			Pool->StartStress(Rate, Seconds);
		}
	}));

bool UArenaProjectilePool::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UArenaProjectilePool::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	if (InWorld.GetNetMode() == NM_Client)
	{
		return;
	}

	const int32 Prewarm = CVarPoolPrewarm.GetValueOnGameThread();
	Free.Reserve(Prewarm);
	for (int32 i = 0; i < Prewarm; ++i)
	{
		if (AArenaProjectile* P = SpawnProjectile())
		{
			Free.Add(P);
		}
	}
}

AArenaProjectile* UArenaProjectilePool::SpawnProjectile()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaPool_Spawn);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AArenaProjectile* P = GetWorld()->SpawnActor<AArenaProjectile>(
		AArenaProjectile::StaticClass(), FTransform::Identity, Params);
	if (P)
	{
		P->OnReleased.BindUObject(this, &ThisClass::Release);
	}
	return P;
}

AArenaProjectile* UArenaProjectilePool::Launch(const FVector& Origin, const FVector& Direction,
	const FArenaWeaponStats& Stats, APawn* Instigator)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaPool_Launch);

	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		return nullptr;
	}

	AArenaProjectile* P = nullptr;
	if (CVarUsePool.GetValueOnGameThread() != 0)
	{
		while (!P && Free.Num() > 0)
		{
			P = Free.Pop(EAllowShrinking::No);
			if (!IsValid(P))
			{
				P = nullptr; // destroyed behind our back (level streaming, etc.)
			}
		}
	}
	if (!P)
	{
		P = SpawnProjectile();
	}
	if (!P)
	{
		return nullptr;
	}

	++Live;
	P->Launch(Origin, Direction, Stats.ProjectileSpeed, Stats.Damage, Stats.ProjectileLifeSeconds, Instigator);
	return P;
}

void UArenaProjectilePool::Release(AArenaProjectile* Projectile)
{
	--Live;
	if (CVarUsePool.GetValueOnGameThread() != 0)
	{
		Free.Push(Projectile);
	}
	else
	{
		Projectile->Destroy();
	}
}

void UArenaProjectilePool::StartStress(float ShotsPerSecond, float Seconds)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}
	StressRate = FMath::Max(0.f, ShotsPerSecond);
	StressCarry = 0.f;
	StressEndTime = World->GetTimeSeconds() + Seconds;
	World->GetTimerManager().SetTimer(StressTimer, this, &ThisClass::StressStep, 1.f / 60.f, true);
	UE_LOG(LogArena, Log, TEXT("Stress: %.0f shots/s for %.1fs, pool %s"), StressRate, Seconds,
		CVarUsePool.GetValueOnGameThread() ? TEXT("ON") : TEXT("OFF"));
}

void UArenaProjectilePool::StressStep()
{
	UWorld* World = GetWorld();
	if (!World || World->GetTimeSeconds() >= StressEndTime)
	{
		if (World)
		{
			World->GetTimerManager().ClearTimer(StressTimer);
		}
		UE_LOG(LogArena, Log, TEXT("Stress done. Live=%d Free=%d"), Live, Free.Num());
		return;
	}

	// Harmless projectiles fired horizontally from a ring around the origin.
	FArenaWeaponStats Stats;
	Stats.Damage = 0.f;

	StressCarry += StressRate / 60.f;
	const int32 Shots = FMath::FloorToInt(StressCarry);
	StressCarry -= Shots;
	for (int32 i = 0; i < Shots; ++i)
	{
		const FVector Origin(FMath::FRandRange(-2000.f, 2000.f), FMath::FRandRange(-2000.f, 2000.f), 300.f);
		const float Yaw = FMath::FRandRange(0.f, 360.f);
		Launch(Origin, FRotator(0.f, Yaw, 0.f).Vector(), Stats, nullptr);
	}
}
