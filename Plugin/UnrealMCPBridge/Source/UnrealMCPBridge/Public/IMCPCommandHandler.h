#pragma once

#include "CoreMinimal.h"
#include "MCPProtocol.h"
#include "Dom/JsonObject.h"

class FMCPCommandRegistry;

// Signature for a command implementation.
// All handlers are invoked on the GameThread.
using FMCPCommandFunction = TFunction<
    TSharedPtr<FJsonObject>(const TSharedPtr<FJsonObject>& Params, MCPProtocol::FMCPError& OutError)
>;

// Interface for domain-specific command handler bundles.
class IMCPCommandHandler
{
public:
    virtual ~IMCPCommandHandler() = default;

    // Called once at subsystem startup to register all commands into the registry.
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) = 0;
};
