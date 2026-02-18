#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FEpicUnrealMCPModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static inline FEpicUnrealMCPModule& Get()
	{
		return FModuleManager::LoadModuleChecked<FEpicUnrealMCPModule>("UnrealMCPBridge");
	}

	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("UnrealMCPBridge");
	}
};
