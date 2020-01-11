// Fill out your copyright notice in the Description page of Project Settings.

#include "BBlitzball.h"
#include "BCharacter.h"
#include "BPlayerState.h"
#include "BBlitzballBase.h"
#include "BGoal.h"
#include "BGameState.h"
#include "BGameMode.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ABBlitzball::ABBlitzball()
{
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	CollisionComp->SetupAttachment(GetRootComponent());
	CollisionComp->SetSimulatePhysics(true);

	BlitzballMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	BlitzballMesh->SetupAttachment(CollisionComp);

	GoalScore = 100;

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

void ABBlitzball::SetLastPlayer(ABCharacter* NewPlayer)
{
	Pawn = NewPlayer;
	Player = Cast<ABPlayerState>(NewPlayer->GetPlayerState());
	HitTime = GetWorld()->GetTimeSeconds();
}

void ABBlitzball::Score(ABGoal* Goal)
{
	ABGameState* Game = Cast<ABGameState>(GetWorld()->GetGameState());
	if (Player && Game)
	{
		if (Player->GetTeamNumber() != Goal->GetTeamNumber())
		{
			if (Player->GetTeamNumber() == 0)
			{
				Game->BlueTeamGoals++;
			}
			else if(Player->GetTeamNumber() == 1)
			{
				Game->RedTeamGoals++;
			}

			Player->ScoreGoal(Player, 100);
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

			Player->ScoreOwnGoal(Player, 100);
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