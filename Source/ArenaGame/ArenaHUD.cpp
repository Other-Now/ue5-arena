#include "ArenaHUD.h"
#include "ArenaCharacter.h"
#include "ArenaGameState.h"
#include "ArenaHealthComponent.h"
#include "ArenaPlayerState.h"
#include "ArenaProjectilePool.h"
#include "ArenaWeaponComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"

void AArenaHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	float Y = 40.f;
	auto Line = [&](const FString& Text)
	{
		DrawText(Text, FLinearColor::White, 40.f, Y, Font, 1.2f);
		Y += 26.f;
	};

	if (const AArenaCharacter* Character = Cast<AArenaCharacter>(GetOwningPawn()))
	{
		const UArenaHealthComponent* Health = Character->GetHealthComponent();
		const UArenaWeaponComponent* Weapon = Character->GetWeaponComponent();
		Line(FString::Printf(TEXT("HP   %.0f / %.0f"), Health->GetHealth(), Health->GetMaxHealth()));
		Line(Weapon->IsReloading()
			? FString(TEXT("AMMO reloading..."))
			: FString::Printf(TEXT("AMMO %d"), Weapon->GetAmmo()));
	}
	else
	{
		Line(TEXT("Respawning..."));
	}

	if (const AArenaPlayerState* PS = GetOwningPlayerController()->GetPlayerState<AArenaPlayerState>())
	{
		Line(FString::Printf(TEXT("K/D  %d / %d"), PS->Kills, PS->Deaths));
	}
	if (const AArenaGameState* GS = GetWorld()->GetGameState<AArenaGameState>())
	{
		Line(FString::Printf(TEXT("TIME %d:%02d"), GS->RemainingSeconds / 60, GS->RemainingSeconds % 60));
	}
	if (GetNetMode() != NM_Client)
	{
		if (const UArenaProjectilePool* Pool = GetWorld()->GetSubsystem<UArenaProjectilePool>())
		{
			Line(FString::Printf(TEXT("pool live %d  free %d"), Pool->NumLive(), Pool->NumFree()));
		}
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	DrawRect(FLinearColor::White, CX - 2.f, CY - 2.f, 4.f, 4.f);
}
