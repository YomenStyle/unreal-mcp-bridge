#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles blueprint.* JSON-RPC methods.
class FBlueprintCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
