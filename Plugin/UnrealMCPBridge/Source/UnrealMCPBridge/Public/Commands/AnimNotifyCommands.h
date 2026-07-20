#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles animnotify.* JSON-RPC methods — inspecting and retiming notifies on an animation asset.
// Editing an existing event in place (rather than remove + re-add) matters: a notify's Instanced
// sub-objects (e.g. a MotionWarping notify's RootMotionModifier) do not survive re-creation, so
// re-adding silently resets the animator's config.
class FAnimNotifyCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
