#include "ArenaAIController.h"
#include "ArenaCharacter.h"
#include "ArenaHealthComponent.h"
#include "ArenaWeaponComponent.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "TimerManager.h"

static TAutoConsoleVariable<int32> CVarBotThinkOnTick(
	TEXT("arena.BotThinkOnTick"), 0,
	TEXT("1 = bots re-evaluate every frame (baseline), 0 = on a staggered timer. Applies to bots possessed after the change."));

AArenaAIController::AArenaAIController()
{
	bWantsPlayerState = true; // bots appear on the scoreboard
	// AAIController::Tick updates control rotation toward the focus, so the
	// controller keeps ticking; only the decision logic moves to a timer.
	PrimaryActorTick.bCanEverTick = true;
}

void AArenaAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	State = EArenaBotState::Idle;
	Target.Reset();

	if (CVarBotThinkOnTick.GetValueOnGameThread() == 0)
	{
		// Random first delay spreads N bots across frames instead of all
		// thinking on the same one.
		GetWorldTimerManager().SetTimer(ThinkTimer, this, &ThisClass::Think, ThinkInterval, true,
			FMath::FRandRange(0.f, ThinkInterval));
	}
}

void AArenaAIController::OnUnPossess()
{
	GetWorldTimerManager().ClearTimer(ThinkTimer);
	Super::OnUnPossess();
}

void AArenaAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (GetPawn() && !ThinkTimer.IsValid())
	{
		Think();
	}
}

static bool IsAlive(const APawn* Pawn)
{
	const AArenaCharacter* Character = Cast<AArenaCharacter>(Pawn);
	return IsValid(Character) && !Character->GetHealthComponent()->IsDead();
}

APawn* AArenaAIController::FindNearestEnemy() const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaBot_FindEnemy);

	const APawn* Me = GetPawn();
	APawn* Best = nullptr;
	float BestDistSq = FMath::Square(SightRadius);
	for (TActorIterator<AArenaCharacter> It(GetWorld()); It; ++It)
	{
		AArenaCharacter* Other = *It;
		if (Other == Me || !IsAlive(Other))
		{
			continue;
		}
		const float DistSq = FVector::DistSquared(Me->GetActorLocation(), Other->GetActorLocation());
		if (DistSq < BestDistSq && LineOfSightTo(Other))
		{
			Best = Other;
			BestDistSq = DistSq;
		}
	}
	return Best;
}

void AArenaAIController::Think()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(ArenaBot_Think);

	APawn* Me = GetPawn();
	if (!Me)
	{
		return;
	}

	APawn* Current = Target.Get();
	if (!IsAlive(Current) ||
		FVector::DistSquared(Me->GetActorLocation(), Current->GetActorLocation()) > FMath::Square(SightRadius))
	{
		Current = FindNearestEnemy();
	}
	const bool bNewTarget = Current != Target.Get();
	Target = Current;

	if (!Current)
	{
		if (State != EArenaBotState::Idle)
		{
			StopMovement();
			ClearFocus(EAIFocusPriority::Gameplay);
			State = EArenaBotState::Idle;
		}
		return;
	}

	SetFocus(Current); // control rotation (and so aim) tracks the target

	const float Dist = FVector::Dist(Me->GetActorLocation(), Current->GetActorLocation());
	if (Dist > AttackRadius)
	{
		// MoveToActor follows a moving goal by itself; only re-issue it when
		// the goal changes, not on every think.
		if (State != EArenaBotState::Chase || bNewTarget)
		{
			MoveToActor(Current, AttackRadius * 0.6f);
		}
		State = EArenaBotState::Chase;
		return;
	}

	if (State == EArenaBotState::Chase)
	{
		StopMovement();
	}
	State = EArenaBotState::Attack;
	if (LineOfSightTo(Current))
	{
		if (UArenaWeaponComponent* Weapon = Me->FindComponentByClass<UArenaWeaponComponent>())
		{
			Weapon->RequestFire();
		}
	}
}
