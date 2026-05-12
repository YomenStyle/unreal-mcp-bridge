#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MCPSettings.generated.h"

// Project Settings > Plugins > Unreal MCP Bridge.
// Stored under [/Script/UnrealMCPBridge.MCPSettings] in DefaultEngine.ini.
UCLASS(config=Engine, defaultconfig, meta=(DisplayName="Unreal MCP Bridge"))
class UNREALMCPBRIDGE_API UMCPSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    // Enable the TCP listener on startup.
    UPROPERTY(EditAnywhere, config, Category="MCP")
    bool bEnabled = false;

    // Listener bind address.
    UPROPERTY(EditAnywhere, config, Category="MCP")
    FString Host = TEXT("127.0.0.1");

    // Listener port. 0 means disabled; set a value > 0 to activate.
    UPROPERTY(EditAnywhere, config, Category="MCP")
    int32 Port = 0;

    // Seconds to wait for a GameThread dispatch before returning InternalError.
    UPROPERTY(EditAnywhere, config, Category="MCP")
    int32 GameThreadDispatchTimeoutSeconds = 30;

    // Maximum accepted JSON line length in bytes.
    UPROPERTY(EditAnywhere, config, Category="MCP")
    int32 MaxLineBytes = 16777216;

    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
};
