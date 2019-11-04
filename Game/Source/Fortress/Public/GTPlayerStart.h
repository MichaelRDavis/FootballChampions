// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "GTPlayerStart.generated.h"

UCLASS()
class FORTRESS_API AGTPlayerStart : public APlayerStart
{
	GENERATED_BODY()
	
public:
	AGTPlayerStart(const FObjectInitializer& ObjectInitializer);

	/** Which team can spawn at this point */
	UPROPERTY(EditInstanceOnly, Category = Team)
	int32 Team;
};