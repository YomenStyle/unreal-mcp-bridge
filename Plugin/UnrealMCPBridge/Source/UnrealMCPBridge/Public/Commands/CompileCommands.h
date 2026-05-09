#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles compile.* JSON-RPC methods.
class FCompileCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
