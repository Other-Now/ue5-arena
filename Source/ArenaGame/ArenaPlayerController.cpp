#include "ArenaPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"

void AArenaPlayerController::BuildInput()
{
	MappingContext = NewObject<UInputMappingContext>(this, TEXT("IMC_Arena"));

	auto MakeAction = [this](const TCHAR* Name, EInputActionValueType Type)
	{
		UInputAction* Action = NewObject<UInputAction>(this, Name);
		Action->ValueType = Type;
		return Action;
	};
	MoveAction = MakeAction(TEXT("IA_Move"), EInputActionValueType::Axis2D);
	LookAction = MakeAction(TEXT("IA_Look"), EInputActionValueType::Axis2D);
	JumpAction = MakeAction(TEXT("IA_Jump"), EInputActionValueType::Boolean);
	FireAction = MakeAction(TEXT("IA_Fire"), EInputActionValueType::Boolean);

	auto Map = [this](UInputAction* Action, const FKey& Key, bool bSwizzle, bool bNegate)
	{
		FEnhancedActionKeyMapping& Mapping = MappingContext->MapKey(Action, Key);
		if (bSwizzle)
		{
			// Default order YXZ: a 1D key press on X becomes the Y (forward) axis.
			Mapping.Modifiers.Add(NewObject<UInputModifierSwizzleAxis>(MappingContext));
		}
		if (bNegate)
		{
			Mapping.Modifiers.Add(NewObject<UInputModifierNegate>(MappingContext));
		}
	};

	// Move: X = right, Y = forward (same convention as the Third Person template).
	Map(MoveAction, EKeys::W, true, false);
	Map(MoveAction, EKeys::S, true, true);
	Map(MoveAction, EKeys::D, false, false);
	Map(MoveAction, EKeys::A, false, true);

	FEnhancedActionKeyMapping& Look = MappingContext->MapKey(LookAction, EKeys::Mouse2D);
	UInputModifierNegate* InvertY = NewObject<UInputModifierNegate>(MappingContext);
	InvertY->bX = false;
	InvertY->bZ = false;
	Look.Modifiers.Add(InvertY);

	Map(JumpAction, EKeys::SpaceBar, false, false);
	Map(FireAction, EKeys::LeftMouseButton, false, false);
}

void AArenaPlayerController::EnsureInputReady()
{
	if (!MappingContext)
	{
		BuildInput();
	}

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (!Subsystem->HasMappingContext(MappingContext))
		{
			Subsystem->AddMappingContext(MappingContext, 0);
		}
	}
}
