#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles camerarig.* JSON-RPC methods — inspection and property editing of Gameplay
// Camera System (GCS) UCameraRigAsset node trees (e.g. tuning a Boom Arm's offset or a
// lens node's FOV) via UObject reflection. Structural node creation is left to the editor.
class FCameraRigCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
