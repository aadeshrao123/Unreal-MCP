#include "EpicUnrealMCPModule.h"
#include "Modules/ModuleManager.h"

void FEpicUnrealMCPModule::StartupModule()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge Module has started"));
}

void FEpicUnrealMCPModule::ShutdownModule()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCPBridge Module has shut down"));
}

IMPLEMENT_MODULE(FEpicUnrealMCPModule, UnrealMCPBridge)
