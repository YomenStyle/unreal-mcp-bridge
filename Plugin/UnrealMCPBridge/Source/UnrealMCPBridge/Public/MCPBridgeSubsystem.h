#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "IMCPCommandHandler.h"
#include "MCPCommandRegistry.h"
#include "MCPServerRunnable.h"
#include "MCPBridgeSubsystem.generated.h"

// Editor subsystem that owns the TCP listener lifetime.
UCLASS()
class UNREALMCPBRIDGE_API UMCPBridgeSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Stop then (re)start the listener according to the current UMCPSettings.
    // Called by UMCPSettings::PostEditChangeProperty so toggling the setting in
    // Project Settings takes effect immediately — no editor restart required.
    void RestartFromSettings();

private:
    // Start the TCP listener if settings allow it (bEnabled && Port > 0).
    // Idempotent: does nothing if already running or disabled.
    void StartListener();

    // Stop the TCP listener if running. Safe to call multiple times.
    void StopListener();

    // Gracefully stop the server; safe to call multiple times (idempotent).
    void HandleEditorExit();

    TUniquePtr<FMCPServerRunnable> Server;
    TSharedPtr<FMCPCommandRegistry> Registry;
    FDelegateHandle OnExitHandle;

    // Domain handler instances — domain-implementer populates these.
    TArray<TSharedPtr<IMCPCommandHandler>> Handlers;
};
