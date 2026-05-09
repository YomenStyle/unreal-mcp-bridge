#include "Commands/EditorCommands.h"
#include "Commands/CommandJsonHelpers.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace
{
    // Default upper bound for editor.list_actors when max_count is not supplied by the caller.
    constexpr int32 DefaultActorListLimit = 1000;
}

void FEditorCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // editor.get_status — returns current level name, selection counts, PIE state
    Registry.Register(TEXT("editor.get_status"),
        [](const TSharedPtr<FJsonObject>& /*Params*/, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GEditor)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GEditor is not available");
                return nullptr;
            }

            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (!World)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("Editor world is not available");
                return nullptr;
            }

            int32 TotalActorCount = 0;
            for (TActorIterator<AActor> It(World); It; ++It)
            {
                ++TotalActorCount;
            }

            const int32 SelectedActorCount = GEditor->GetSelectedActorCount();
            const bool bIsPIERunning = World->IsPlayInEditor();

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("current_level"), World->GetMapName());
            Result->SetNumberField(TEXT("selected_actor_count"), static_cast<double>(SelectedActorCount));
            Result->SetNumberField(TEXT("total_actor_count"), static_cast<double>(TotalActorCount));
            Result->SetBoolField(TEXT("is_pie_running"), bIsPIERunning);
            return Result;
        });

    // editor.list_actors — iterates actors with optional class filter and count cap
    Registry.Register(TEXT("editor.list_actors"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GEditor)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GEditor is not available");
                return nullptr;
            }

            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (!World)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("Editor world is not available");
                return nullptr;
            }

            FString FilterClass;
            int32 MaxCount = DefaultActorListLimit;

            if (Params.IsValid())
            {
                Params->TryGetStringField(TEXT("filter_class"), FilterClass);

                double MaxCountVal = 0.0;
                if (Params->TryGetNumberField(TEXT("max_count"), MaxCountVal))
                {
                    MaxCount = static_cast<int32>(MaxCountVal);
                    if (MaxCount <= 0)
                    {
                        OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                        OutError.Message = TEXT("max_count must be a positive integer");
                        return nullptr;
                    }
                }
            }

            TArray<TSharedPtr<FJsonValue>> ActorArray;

            for (TActorIterator<AActor> It(World); It; ++It)
            {
                AActor* Actor = *It;

                // Apply class name filter if provided
                if (!FilterClass.IsEmpty())
                {
                    const FString ClassName = Actor->GetClass()->GetName();
                    if (!ClassName.Equals(FilterClass, ESearchCase::IgnoreCase))
                    {
                        continue;
                    }
                }

                if (ActorArray.Num() >= MaxCount)
                {
                    break;
                }

                ActorArray.Add(MakeShared<FJsonValueObject>(UnrealMCPBridge::Json::MakeActorJson(Actor)));
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetArrayField(TEXT("actors"), ActorArray);
            return Result;
        });

    // editor.spawn_actor — loads a UClass by path and spawns it in the editor world
    Registry.Register(TEXT("editor.spawn_actor"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!Params.IsValid())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("params object is required");
                return nullptr;
            }

            FString ClassPath;
            if (!Params->TryGetStringField(TEXT("class_path"), ClassPath) || ClassPath.IsEmpty())
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = TEXT("class_path is required and must be non-empty");
                return nullptr;
            }

            if (!GEditor)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GEditor is not available");
                return nullptr;
            }

            UWorld* World = GEditor->GetEditorWorldContext().World();
            if (!World)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("Editor world is not available");
                return nullptr;
            }

            UClass* ActorClass = LoadObject<UClass>(nullptr, *ClassPath);
            if (!ActorClass)
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Could not load class: %s"), *ClassPath);
                return nullptr;
            }

            if (!ActorClass->IsChildOf(AActor::StaticClass()))
            {
                OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                OutError.Message = FString::Printf(TEXT("Class is not an AActor subclass: %s"), *ClassPath);
                return nullptr;
            }

            FVector Location = FVector::ZeroVector;
            FRotator Rotation = FRotator::ZeroRotator;

            const TArray<TSharedPtr<FJsonValue>>* LocArray = nullptr;
            if (Params->TryGetArrayField(TEXT("location"), LocArray) && LocArray && LocArray->Num() == 3)
            {
                Location.X = static_cast<float>((*LocArray)[0]->AsNumber());
                Location.Y = static_cast<float>((*LocArray)[1]->AsNumber());
                Location.Z = static_cast<float>((*LocArray)[2]->AsNumber());
            }

            const TArray<TSharedPtr<FJsonValue>>* RotArray = nullptr;
            if (Params->TryGetArrayField(TEXT("rotation"), RotArray) && RotArray && RotArray->Num() == 3)
            {
                Rotation.Pitch = static_cast<float>((*RotArray)[0]->AsNumber());
                Rotation.Yaw   = static_cast<float>((*RotArray)[1]->AsNumber());
                Rotation.Roll  = static_cast<float>((*RotArray)[2]->AsNumber());
            }

            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            AActor* SpawnedActor = World->SpawnActor<AActor>(ActorClass, Location, Rotation, SpawnParams);
            if (!SpawnedActor)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("SpawnActor returned null");
                return nullptr;
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetStringField(TEXT("actor_name"), SpawnedActor->GetName());
            Result->SetStringField(TEXT("actor_path"), SpawnedActor->GetPathName());
            return Result;
        });
}
