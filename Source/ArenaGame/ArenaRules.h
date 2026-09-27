#pragma once

#include "CoreMinimal.h"

// Gameplay rules with no UObject/world dependency, so the automation tests can
// exercise them directly. Components call these; they never re-implement them.
namespace ArenaRules
{
	// Health after taking Damage. Clamped to [0, MaxHealth]; non-positive or NaN
	// damage is ignored (healing is not a damage event).
	inline float ApplyDamage(float Health, float Damage, float MaxHealth)
	{
		if (!(Damage > 0.f))
		{
			return FMath::Clamp(Health, 0.f, MaxHealth);
		}
		return FMath::Clamp(Health - Damage, 0.f, MaxHealth);
	}

	// Cooldown gate. Tolerance < 1 lets the server accept requests that arrive
	// slightly faster than FireInterval because of network jitter, while still
	// capping a modified client at ~1/Tolerance times the intended rate.
	inline bool CanFire(double Now, double LastFireTime, float FireInterval, float Tolerance = 1.f)
	{
		return Now - LastFireTime >= double(FireInterval) * double(Tolerance) - 1e-4;
	}

	// Server-side sanity check of a client's fire request. The server does not
	// trust the client's muzzle position: it must be close to where the server
	// thinks the pawn is (MaxOriginOffset covers speed * RTT), and the direction
	// must be a unit vector. Failing this in an RPC _Validate disconnects the client.
	inline bool IsFireRequestPlausible(const FVector& PawnLocation, const FVector& Origin,
		const FVector& Direction, float MaxOriginOffset)
	{
		if (Origin.ContainsNaN() || Direction.ContainsNaN())
		{
			return false;
		}
		if (!Direction.IsNormalized())
		{
			return false;
		}
		return FVector::DistSquared(PawnLocation, Origin) <= FMath::Square(MaxOriginOffset);
	}
}
