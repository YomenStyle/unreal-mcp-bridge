#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles asset.* JSON-RPC methods.
class FAssetCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
