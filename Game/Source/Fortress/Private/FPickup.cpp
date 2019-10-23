// Fill out your copyright notice in the Description page of Project Settings.
#include "FPickup.h"
#include "Components/SphereComponent.h"

AFPickup::AFPickup()
{
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	CollisionComp->SetupAttachment(GetRootComponent());
	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AFPickup::OnOverlapBegin);

	SetReplicates(true);
	RespawnTime = 30.0f;
	bAlwaysRelevant = true;
	NetUpdateFrequency = 1.0f;
	PrimaryActorTick.bCanEverTick = true;
}

void AFPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

void AFPickup::BeginPlay()
{
	Super::BeginPlay();
}

void AFPickup::OnOverlapBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherCOmp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{

}

