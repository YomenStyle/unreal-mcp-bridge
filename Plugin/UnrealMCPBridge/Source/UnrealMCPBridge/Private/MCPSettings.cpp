#include "MCPSettings.h"

// UDeveloperSettings subclass — config behaviour is driven by UPROPERTY metadata.
// PostEditChangeProperty lets setting changes apply live (no editor restart).

#if WITH_EDITOR
#include "Editor.h"
#include "MCPBridgeSubsystem.h"

void UMCPSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (GEditor)
    {
        if (UMCPBridgeSubsystem* Bridge = GEditor->GetEditorSubsystem<UMCPBridgeSubsystem>())
        {
            Bridge->RestartFromSettings();
        }
    }
}
#endif
