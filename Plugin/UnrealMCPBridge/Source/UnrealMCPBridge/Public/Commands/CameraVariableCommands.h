#pragma once

#include "CoreMinimal.h"
#include "IMCPCommandHandler.h"

// Handles camvar.* JSON-RPC methods — create, populate and inspect Gameplay Camera System
// (GCS) UCameraVariableCollection assets: the typed CameraVariable defaults used to share a
// value (offset / boom / fov / blend alpha) across multiple camera rigs via parameter binding.
//
// The GCS editor's variable authoring is not exposed to Python/Blueprint (UCameraVariableCollection
// ::Variables is a bare UPROPERTY with no script flags, and the variable value/name fields are
// editor-only), so these commands replicate the CameraVariableCollectionEditorToolkit create path
// directly in C++ (NewObject into the collection + append to the public Variables array).
class FCameraVariableCommandHandler : public IMCPCommandHandler
{
public:
    virtual void RegisterCommands(FMCPCommandRegistry& Registry) override;
};
