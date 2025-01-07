// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProyectoGitVoxV1GameMode.h"
#include "ProyectoGitVoxV1Character.h"
#include "UObject/ConstructorHelpers.h"

AProyectoGitVoxV1GameMode::AProyectoGitVoxV1GameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
