#include "MCPBridgeSubsystem.h"
#include "UnrealMCPBridgeModule.h"
#include "MCPSettings.h"
#include "Editor.h"
#include "Misc/CoreDelegates.h"
#include "Commands/EditorCommands.h"
#include "Commands/BlueprintCommands.h"
#include "Commands/AssetCommands.h"
#include "Commands/CompileCommands.h"
#include "Commands/PIECommands.h"

void UMCPBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UMCPSettings* Settings = GetDefault<UMCPSettings>();
    if (!Settings->bEnabled || Settings->Port <= 0)
    {
        UE_LOG(LogMCPBridge, Log,
            TEXT("MCPBridgeSubsystem: disabled (bEnabled=%s, Port=%d). Set both in Project Settings to activate."),
            Settings->bEnabled ? TEXT("true") : TEXT("false"),
            Settings->Port);
        return;
    }

    // Create the command registry that all domain handlers will populate.
    Registry = MakeShared<FMCPCommandRegistry>();

    // Register domain command handlers.
    Handlers.Add(MakeShared<FEditorCommandHandler>());
    Handlers.Add(MakeShared<FBlueprintCommandHandler>());
    Handlers.Add(MakeShared<FAssetCommandHandler>());
    Handlers.Add(MakeShared<FCompileCommandHandler>());
    Handlers.Add(MakeShared<FPIECommandHandler>());

    for (const TSharedPtr<IMCPCommandHandler>& Handler : Handlers)
    {
        if (Handler.IsValid())
        {
            Handler->RegisterCommands(*Registry);
        }
    }

    // Start the TCP listener.
    Server = MakeUnique<FMCPServerRunnable>(
        Settings->Host,
        Settings->Port,
        Settings->MaxLineBytes,
        Settings->GameThreadDispatchTimeoutSeconds,
        Registry.ToSharedRef());
    Server->StartThread();

    // Ensure clean shutdown when the editor exits.
    OnExitHandle = FCoreDelegates::OnPreExit.AddUObject(this, &UMCPBridgeSubsystem::HandleEditorExit);

    UE_LOG(LogMCPBridge, Log,
        TEXT("MCPBridgeSubsystem: started listener on %s:%d."), *Settings->Host, Settings->Port);
}

void UMCPBridgeSubsystem::Deinitialize()
{
    HandleEditorExit();
    Super::Deinitialize();
}

void UMCPBridgeSubsystem::HandleEditorExit()
{
    if (Server)
    {
        UE_LOG(LogMCPBridge, Log, TEXT("MCPBridgeSubsystem: stopping server."));
        Server->RequestStopAndWait();
        Server.Reset();
    }

    if (OnExitHandle.IsValid())
    {
        FCoreDelegates::OnPreExit.Remove(OnExitHandle);
        OnExitHandle.Reset();
    }
}
