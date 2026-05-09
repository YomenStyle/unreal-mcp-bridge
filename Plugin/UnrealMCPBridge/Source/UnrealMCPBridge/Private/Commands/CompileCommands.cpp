#include "Commands/CompileCommands.h"
#include "MCPCommandRegistry.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"
#include "ILiveCodingModule.h"

void FCompileCommandHandler::RegisterCommands(FMCPCommandRegistry& Registry)
{
    // compile.trigger_live_coding — requests a Live Coding compile
    Registry.Register(TEXT("compile.trigger_live_coding"),
        [](const TSharedPtr<FJsonObject>& /*Params*/, MCPProtocol::FMCPError& OutError) -> TSharedPtr<FJsonObject>
        {
            if (!FModuleManager::Get().IsModuleLoaded(LIVE_CODING_MODULE_NAME))
            {
                OutError.Code = -32010; // user-defined: LiveCoding unavailable
                OutError.Message = TEXT("LiveCoding module is not loaded");
                return nullptr;
            }

            ILiveCodingModule& LiveCoding =
                FModuleManager::GetModuleChecked<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);

            if (!LiveCoding.IsEnabledForSession())
            {
                // Attempt to enable for this session before compiling
                if (LiveCoding.CanEnableForSession())
                {
                    LiveCoding.EnableForSession(true);
                }
                else
                {
                    OutError.Code = -32010;
                    OutError.Message = TEXT("LiveCoding cannot be enabled for this session");
                    return nullptr;
                }
            }

            ELiveCodingCompileResult CompileResult = ELiveCodingCompileResult::NotStarted;
            LiveCoding.Compile(ELiveCodingCompileFlags::None, &CompileResult);

            const bool bTriggered = (CompileResult == ELiveCodingCompileResult::InProgress
                                  || CompileResult == ELiveCodingCompileResult::Success
                                  || CompileResult == ELiveCodingCompileResult::NoChanges);

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("triggered"), bTriggered);
            return Result;
        });

    // compile.status — returns current Live Coding compilation state
    Registry.Register(TEXT("compile.status"),
        [](const TSharedPtr<FJsonObject>& /*Params*/, MCPProtocol::FMCPError& /*OutError*/) -> TSharedPtr<FJsonObject>
        {
            bool bIsCompiling = false;
            bool bIsEnabled   = false;

            if (FModuleManager::Get().IsModuleLoaded(LIVE_CODING_MODULE_NAME))
            {
                ILiveCodingModule& LiveCoding =
                    FModuleManager::GetModuleChecked<ILiveCodingModule>(LIVE_CODING_MODULE_NAME);
                bIsCompiling = LiveCoding.IsCompiling();
                bIsEnabled   = LiveCoding.IsEnabledForSession();
            }

            auto Result = MakeShared<FJsonObject>();
            Result->SetBoolField(TEXT("is_compiling"), bIsCompiling);
            Result->SetBoolField(TEXT("is_enabled"),   bIsEnabled);
            return Result;
        });
}
