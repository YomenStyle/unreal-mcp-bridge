#include "Commands/PIECommands.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"
#include "Editor/EditorEngine.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "PlayInEditorDataTypes.h"

void FPIECommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // pie.start — begins a Play-In-Editor session
    Registry.Register(TEXT("pie.start"),
        [](const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GUnrealEd)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GUnrealEd is not available");
                return nullptr;
            }

            // Derive play world type from optional "mode" param (defaults to PIE)
            EPlaySessionWorldType WorldType = EPlaySessionWorldType::PlayInEditor;
            if (Params.IsValid())
            {
                FString ModeStr;
                if (Params->TryGetStringField(TEXT("mode"), ModeStr))
                {
                    if (ModeStr.Equals(TEXT("SIE"), ESearchCase::IgnoreCase))
                    {
                        WorldType = EPlaySessionWorldType::SimulateInEditor;
                    }
                    else if (!ModeStr.IsEmpty()
                          && !ModeStr.Equals(TEXT("PIE"), ESearchCase::IgnoreCase))
                    {
                        OutError.Code = MCPProtocol::FMCPError::InvalidParams;
                        OutError.Message = FString::Printf(
                            TEXT("Unsupported mode '%s'. Accepted values: PIE, SIE"), *ModeStr);
                        return nullptr;
                    }
                }
            }

            FRequestPlaySessionParams PlayParams;
            PlayParams.WorldType = WorldType;
            GUnrealEd->RequestPlaySession(PlayParams);

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("started"), true);
            return Result;
        });

    // pie.stop — ends the active Play-In-Editor session
    Registry.Register(TEXT("pie.stop"),
        [](const TSharedPtr<FJsonObject>& /*Params*/, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!GUnrealEd)
            {
                OutError.Code = MCPProtocol::FMCPError::InternalError;
                OutError.Message = TEXT("GUnrealEd is not available");
                return nullptr;
            }

            GUnrealEd->RequestEndPlayMap();

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("stopped"), true);
            return Result;
        });
}
