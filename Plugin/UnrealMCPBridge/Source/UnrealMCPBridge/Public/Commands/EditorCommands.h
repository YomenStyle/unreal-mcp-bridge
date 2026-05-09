#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles editor.* JSON-RPC methods.
class FEditorCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
