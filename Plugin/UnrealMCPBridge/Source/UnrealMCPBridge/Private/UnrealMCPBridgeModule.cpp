#include "UnrealMCPBridgeModule.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogMCPBridge);

void FUnrealMCPBridgeModule::StartupModule()
{
    UE_LOG(LogMCPBridge, Log, TEXT("UnrealMCPBridge module started."));
}

void FUnrealMCPBridgeModule::ShutdownModule()
{
    UE_LOG(LogMCPBridge, Log, TEXT("UnrealMCPBridge module shut down."));
}

IMPLEMENT_MODULE(FUnrealMCPBridgeModule, UnrealMCPBridge)
