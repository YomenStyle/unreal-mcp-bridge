#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles animgraph.* JSON-RPC methods (AnimBlueprint state machine authoring).
class FAnimGraphCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
