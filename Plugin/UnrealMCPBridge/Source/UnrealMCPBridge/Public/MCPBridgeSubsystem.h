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

private:
    // Gracefully stop the server; safe to call multiple times (idempotent).
    void HandleEditorExit();

    TUniquePtr<FMCPServerRunnable> Server;
    TSharedPtr<FMCPCommandRegistry> Registry;
    FDelegateHandle OnExitHandle;

    // Domain handler instances — domain-implementer populates these.
    TArray<TSharedPtr<IMCPCommandHandler>> Handlers;
};
