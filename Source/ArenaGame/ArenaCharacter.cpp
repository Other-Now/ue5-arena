#include "ArenaCharacter.h"
#include "ArenaAIController.h"
#include "ArenaGameMode.h"
#include "ArenaHealthComponent.h"
#include "ArenaPlayerController.h"
#include "ArenaWeaponComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"

AArenaCharacter::AArenaCharacter()
{
	PrimaryActorTick.bCanEverTick = false;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Shooter-style: the body always faces where the camera aims, so the muzzle
	// direction (control rotation) and what the player sees agree.
	bUseControllerRotationYaw = true;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->MaxWalkSpeed = 600.f;
	GetCharacterMovement()->JumpZVelocity = 600.f;

	// Default for characters is 100 Hz. 60 is plenty for movement that the
	// client predicts anyway, and cuts per-pawn bandwidth.
	SetNetUpdateFrequency(60.f);
	SetMinNetUpdateFrequency(15.f);

	SpawnCollisionHandlingMethod = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	AIControllerClass = AArenaAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::Disabled; // the game mode possesses bots explicitly

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 350.f;
	CameraBoom->SocketOffset = FVector(0.f, 60.f, 60.f);
	CameraBoom->bUsePawnControlRotation = true;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetRelativeScale3D(FVector(0.8f, 0.8f, 1.9f));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (Cylinder.Succeeded())
	{
		Body->SetStaticMesh(Cylinder.Object);
	}

	Health = CreateDefaultSubobject<UArenaHealthComponent>(TEXT("Health"));
	Weapon = CreateDefaultSubobject<UArenaWeaponComponent>(TEXT("Weapon"));
}

void AArenaCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (HasAuthority())
	{
		Health->OnDied.AddDynamic(this, &ThisClass::HandleDied);
	}
}

void AArenaCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	AArenaPlayerController* PC = Cast<AArenaPlayerController>(GetController());
	UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!PC || !Input)
	{
		return;
	}
	PC->EnsureInputReady();

	Input->BindAction(PC->MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
	Input->BindAction(PC->LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
	Input->BindAction(PC->JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
	Input->BindAction(PC->JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	// Triggered fires every frame while held; the weapon's cooldown turns that
	// into automatic fire at Stats.FireInterval.
	Input->BindAction(PC->FireAction, ETriggerEvent::Triggered, this, &ThisClass::Fire);
}

void AArenaCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	const FRotationMatrix YawOnly(FRotator(0.f, GetControlRotation().Yaw, 0.f));
	AddMovementInput(YawOnly.GetUnitAxis(EAxis::X), Axis.Y);
	AddMovementInput(YawOnly.GetUnitAxis(EAxis::Y), Axis.X);
}

void AArenaCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Axis = Value.Get<FVector2D>();
	AddControllerYawInput(Axis.X);
	AddControllerPitchInput(Axis.Y);
}

void AArenaCharacter::Fire(const FInputActionValue& Value)
{
	Weapon->RequestFire();
}

void AArenaCharacter::HandleDied(AController* Killer)
{
	if (AArenaGameMode* GameMode = GetWorld()->GetAuthGameMode<AArenaGameMode>())
	{
		GameMode->OnCharacterKilled(this, Killer);
	}
}
