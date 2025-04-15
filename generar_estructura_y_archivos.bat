@echo off
setlocal enabledelayedexpansion

REM Cambiar si tu proyecto se llama diferente
set "PROJECT=ProyectoGitVoxV1"

REM Crear estructura de carpetas
set DIRS=^
Source\%PROJECT%\Voxel\Chunk ^
Source\%PROJECT%\Voxel\World ^
Source\%PROJECT%\Voxel\Render ^
Source\%PROJECT%\Voxel\Serialization ^
Source\%PROJECT%\AI\Core ^
Source\%PROJECT%\AI\FSM

for %%D in (%DIRS%) do (
    echo Creando carpeta %%D
    mkdir %%D 2>nul
)

REM Crear archivos con contenido base

REM === VoxelChunk ===
(
echo #pragma once
echo struct FVoxel {
echo     uint8 BlockID;
echo     uint8 Metadata;
echo };
echo class VoxelChunk {
echo public:
echo     VoxelChunk();
echo     void Generate();
echo };
) > Source\%PROJECT%\Voxel\Chunk\VoxelChunk.h

(
echo #include "VoxelChunk.h"
echo VoxelChunk::VoxelChunk() {}
echo void VoxelChunk::Generate() {}
) > Source\%PROJECT%\Voxel\Chunk\VoxelChunk.cpp

REM === VoxelWorldManager ===
(
echo #pragma once
echo #include "VoxelChunk.h"
echo class VoxelWorldManager {
echo public:
echo     void LoadChunk();
echo };
) > Source\%PROJECT%\Voxel\World\VoxelWorldManager.h

(
echo #include "VoxelWorldManager.h"
echo void VoxelWorldManager::LoadChunk() {}
) > Source\%PROJECT%\Voxel\World\VoxelWorldManager.cpp

REM === VoxelMeshGenerator ===
(
echo #pragma once
echo class VoxelChunk;
echo class UProceduralMeshComponent;
echo class VoxelMeshGenerator {
echo public:
echo     static void GenerateMesh(const VoxelChunk* chunk, UProceduralMeshComponent* meshComponent);
echo };
) > Source\%PROJECT%\Voxel\Render\VoxelMeshGenerator.h

(
echo #include "VoxelMeshGenerator.h"
echo void VoxelMeshGenerator::GenerateMesh(const VoxelChunk* chunk, UProceduralMeshComponent* meshComponent) {}
) > Source\%PROJECT%\Voxel\Render\VoxelMeshGenerator.cpp

REM === VoxelSerializer ===
(
echo #pragma once
echo class VoxelChunk;
echo class VoxelSerializer {
echo public:
echo     static void SaveChunk(const VoxelChunk* chunk);
echo     static void LoadChunk(VoxelChunk* chunk);
echo };
) > Source\%PROJECT%\Voxel\Serialization\VoxelSerializer.h

(
echo #include "VoxelSerializer.h"
echo void VoxelSerializer::SaveChunk(const VoxelChunk* chunk) {}
echo void VoxelSerializer::LoadChunk(VoxelChunk* chunk) {}
) > Source\%PROJECT%\Voxel\Serialization\VoxelSerializer.cpp

REM === DevicePerformanceDetector ===
(
echo #pragma once
echo enum class EPerformanceProfile : uint8 { LOW, MEDIUM, HIGH, ULTRA };
echo class DevicePerformanceDetector {
echo public:
echo     static EPerformanceProfile DetectPerformanceProfile();
echo };
) > Source\%PROJECT%\AI\Core\DevicePerformanceDetector.h

(
echo #include "DevicePerformanceDetector.h"
echo EPerformanceProfile DevicePerformanceDetector::DetectPerformanceProfile() {
echo     return EPerformanceProfile::MEDIUM;
echo }
) > Source\%PROJECT%\AI\Core\DevicePerformanceDetector.cpp

REM === AIVoxelAgent ===
(
echo #pragma once
echo #include "CoreMinimal.h"
echo #include "GameFramework/Character.h"
echo #include "AIVoxelAgent.generated.h"
echo UCLASS()
echo class AAIVoxelAgent : public ACharacter {
echo     GENERATED_BODY()
echo public:
echo     AAIVoxelAgent();
echo };
) > Source\%PROJECT%\AI\Core\AIVoxelAgent.h

(
echo #include "AIVoxelAgent.h"
echo AAIVoxelAgent::AAIVoxelAgent() {}
) > Source\%PROJECT%\AI\Core\AIVoxelAgent.cpp

REM === AIStateComponent ===
(
echo #pragma once
echo #include "Components/ActorComponent.h"
echo #include "AIStateComponent.generated.h"
echo UENUM()
echo enum class EAIState : uint8 { Idle, Wander, Gather, Flee };
echo UCLASS()
echo class UAIStateComponent : public UActorComponent {
echo     GENERATED_BODY()
echo public:
echo     void TickFSM(float DeltaTime);
echo private:
echo     EAIState CurrentState;
echo };
) > Source\%PROJECT%\AI\FSM\AIStateComponent.h

(
echo #include "AIStateComponent.h"
echo void UAIStateComponent::TickFSM(float DeltaTime) {}
) > Source\%PROJECT%\AI\FSM\AIStateComponent.cpp

echo ✅ Archivos y carpetas generados con éxito.
pause
