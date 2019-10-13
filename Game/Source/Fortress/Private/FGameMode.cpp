// Fill out your copyright notice in the Description page of Project Settings.

#include "FGameMode.h"
#include "FPlayerController.h"
#include "FCharacter.h"

AFGameMode::AFGameMode()
{
	PlayerControllerClass = AFPlayerController::StaticClass();
}

UClass* AFGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	AFPlayerController* PlayerController = Cast<AFPlayerController>(InController);
	if (PlayerController)
	{
		return CharacterClass;
	}

	return DefaultPawnClass;
}