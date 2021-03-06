// Fill out your copyright notice in the Description page of Project Settings.

#include "BBlitzball.h"
#include "BCharacter.h"
#include "BPlayerState.h"
#include "BBlitzballBase.h"
#include "BGameState.h"
#include "BGameMode.h"
#include "BPlayerController.h"
#include "FCReplicatedPhysicsComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ABBlitzball::ABBlitzball()
{
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->SetupAttachment(GetRootComponent());
	CollisionComp->SetSimulatePhysics(true);
	CollisionComp->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionComp->OnComponentHit.AddDynamic(this, &ABBlitzball::OnHit);

	BlitzballMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	BlitzballMesh->SetupAttachment(CollisionComp);
	BlitzballMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	PhysicsReplication = CreateDefaultSubobject<UFCReplicatedPhysicsComponent>(TEXT("PhysicsReplication"));

	SetReplicates(true);
	NetPriority = 3.0f;
	bReplicateMovement = true;
	PrimaryActorTick.bCanEverTick = true;
}

void ABBlitzball::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ABBlitzball, Player, COND_None);
}

void ABBlitzball::BeginPlay()
{
	Super::BeginPlay();
}

void ABBlitzball::OnHit(UPrimitiveComponent* MyComp, AActor* OtherActor, UPrimitiveComponent* HitComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (OtherActor != nullptr)
	{
		ServerHeaderBall(OtherActor, Hit.Location);
	}
}

void ABBlitzball::SetLastPlayer(ABCharacter* NewPlayer)
{
	Pawn = NewPlayer;
	Player = Cast<ABPlayerState>(NewPlayer->GetPlayerState());
	if (Player != LastPlayer)
	{
		LastPlayer = Player;
	}

	PlayerController = Cast<ABPlayerController>(NewPlayer->GetController());
	HitTime = GetWorld()->GetTimeSeconds();
}

void ABBlitzball::Score(int32 TeamNumber)
{
	ABGameState* Game = Cast<ABGameState>(GetWorld()->GetGameState());
	ABGameMode* BGameMode = Cast<ABGameMode>(GetWorld()->GetAuthGameMode());
	if (Player && Game && BGameMode)
	{
		if (Player->GetTeamNumber() != TeamNumber)
		{
			if (Player->GetTeamNumber() == 0)
			{
				Game->BlueTeamGoals++;
			}
			else if(Player->GetTeamNumber() == 1)
			{
				Game->RedTeamGoals++;
			}

			Player->ScoreGoal(Player, BGameMode->GetGoalScore());
			Player->ScoreAssist(Player, LastPlayer, BGameMode->GetAssistScore());
			PlayerController->OnScored();
			BGameMode->RestartMatch();
		}
		else
		{
			if (Player->GetTeamNumber() == 0)
			{
				Game->RedTeamGoals++;
			}
			else if (Player->GetTeamNumber() == 1)
			{
				Game->BlueTeamGoals++;
			}

			Player->ScoreOwnGoal(Player, BGameMode->GetGoalScore());
			BGameMode->RestartMatch();
		}
	}
}

void ABBlitzball::SpawnAtBase()
{
	if (HomeBase)
	{
		HomeBase->ServerSpawnBlitzball();
	}
}

void ABBlitzball::HeaderBall(AActor* OtherActor, FVector HitLocation)
{
	if (OtherActor != nullptr)
	{
		ABCharacter* Character = Cast<ABCharacter>(OtherActor);
		if (Character)
		{
			FClosestPointOnPhysicsAsset CharacterPhysicsAssest;
			CharacterPhysicsAssest.BoneName = "head";
			CharacterPhysicsAssest.Normal = HitLocation.GetSafeNormal();
			CharacterPhysicsAssest.ClosestWorldPosition = HitLocation;
			CharacterPhysicsAssest.Distance = 0.0f;

			bool bIsGrounded = Character->GetCharacterMovement()->IsMovingOnGround();
			bool bFoundPoint = Character->GetMesh()->GetClosestPointOnPhysicsAsset(HitLocation, CharacterPhysicsAssest, false);
			if (!bIsGrounded && bFoundPoint)
			{
				const FVector PlayerDirection = Character->GetActorUpVector();
				CollisionComp->AddImpulse(PlayerDirection * HeaderImpulse);
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), HeaderSound, GetActorLocation());
			}
		}
	}
}

void ABBlitzball::ServerHeaderBall_Implementation(AActor* OtherActor, FVector HitLocation)
{
	HeaderBall(OtherActor, HitLocation);
}

bool ABBlitzball::ServerHeaderBall_Validate(AActor* OtherActor, FVector HitLocation)
{
	return true;
}
