#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles statetree.* JSON-RPC methods — programmatic authoring of StateTree assets
// (states, evaluators/tasks, transitions, property bindings, compile) via the
// StateTree editor Builder API. Mirrors the AnimGraph command handler pattern.
class FStateTreeCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
