#include "Commands/PIECommands.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "PlayInEditorDataTypes.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "LevelEditor.h"
#include "SLevelViewport.h"

void FPIECommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // pie.start — queues a Play-In-Editor request for the next tick and returns immediately.
    Registry.Register(TEXT("pie.start"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GUnrealEd)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GUnrealEd is not available");
                return nullptr;
            }

            EPlaySessionWorldType WorldType = EPlaySessionWorldType::PlayInEditor;
            // Default to in-viewport play (matches the toolbar Play button) instead of
            // spawning a separate PIE window.
            EPlayModeType PlayModeKind = PlayMode_InViewPort;

            if (Params.IsValid())
            {
                FString ModeStr;
                if (Params->TryGetStringField(TEXT("mode"), ModeStr))
                {
                    if (ModeStr.Equals(TEXT("SIE"), ESearchCase::IgnoreCase))
                    {
                        WorldType = EPlaySessionWorldType::SimulateInEditor;
                    }
                    else if (ModeStr.Equals(TEXT("Window"), ESearchCase::IgnoreCase)
                          || ModeStr.Equals(TEXT("Floating"), ESearchCase::IgnoreCase))
                    {
                        PlayModeKind = PlayMode_InEditorFloating;
                    }
                    else if (!ModeStr.IsEmpty()
                          && !ModeStr.Equals(TEXT("PIE"), ESearchCase::IgnoreCase)
                          && !ModeStr.Equals(TEXT("Viewport"), ESearchCase::IgnoreCase))
                    {
                        OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                        OutError.Message = FString::Printf(
                            TEXT("Unsupported mode '%s'. Accepted: PIE, Viewport, Window, SIE"), *ModeStr);
                        return nullptr;
                    }
                }
            }

            // Defer the actual PIE start so the dispatcher lambda returns immediately.
            AsyncTask(ENamedThreads::GameThread, [WorldType, PlayModeKind]()
            {
                if (!GUnrealEd) { return; }

                ULevelEditorPlaySettings* PlaySettings = NewObject<ULevelEditorPlaySettings>();
                PlaySettings->LastExecutedPlayModeType = PlayModeKind;

                FRequestPlaySessionParams PlayParams;
                PlayParams.WorldType = WorldType;
                PlayParams.EditorPlaySettings = PlaySettings;

                // Anchor PIE inside the active level viewport when in-viewport play was
                // requested. Without this, RequestPlaySession spawns a separate PIE window
                // regardless of the LastExecutedPlayModeType hint.
                if (PlayModeKind == PlayMode_InViewPort
                    && FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
                {
                    FLevelEditorModule& LevelEditorModule =
                        FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
                    TSharedPtr<SLevelViewport> ActiveLevelViewport =
                        LevelEditorModule.GetFirstActiveLevelViewport();
                    if (ActiveLevelViewport.IsValid())
                    {
                        PlayParams.DestinationSlateViewport = ActiveLevelViewport;
                    }
                }

                GUnrealEd->RequestPlaySession(PlayParams);
            });

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("queued"), true);
            return Result;
        });

    // pie.stop — queues an end-play request for the next tick and returns immediately.
    Registry.Register(TEXT("pie.stop"),
        [](const TSharedPtr<FJsonObject>& /*Params*/, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GUnrealEd)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GUnrealEd is not available");
                return nullptr;
            }

            AsyncTask(ENamedThreads::GameThread, []()
            {
                if (GUnrealEd) { GUnrealEd->RequestEndPlayMap(); }
            });

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("queued"), true);
            return Result;
        });
}
