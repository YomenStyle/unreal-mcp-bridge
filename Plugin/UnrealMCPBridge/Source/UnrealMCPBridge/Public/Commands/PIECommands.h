#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles pie.* JSON-RPC methods.
class FPIECommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
