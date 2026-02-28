#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "Http.h"
#include "Json.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPMaterialCommands.h"
#include "Commands/EpicUnrealMCPDataTableCommands.h"
#include "Commands/EpicUnrealMCPAssetCommands.h"
#include "Commands/EpicUnrealMCPDataAssetCommands.h"
#include "Commands/EpicUnrealMCPWidgetCommands.h"
#include "Commands/EpicUnrealMCPEnhancedInputCommands.h"
#include "Commands/Profiling/EpicUnrealMCPProfilingCommands.h"
#include "EpicUnrealMCPBridge.generated.h"

class FMCPServerRunnable;

/**
 * Editor subsystem for MCP Bridge
 * Handles communication between external tools and the Unreal Editor
 * through a TCP socket connection. Commands are received as JSON and
 * routed to appropriate command handlers.
 */
UCLASS()
class UNREALMCPBRIDGE_API UEpicUnrealMCPBridge : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UEpicUnrealMCPBridge();
	virtual ~UEpicUnrealMCPBridge();

	// UEditorSubsystem implementation
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// Server functions
	void StartServer();
	void StopServer();
	bool IsRunning() const { return bIsRunning; }

	// Command execution
	FString ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params);

	// Returns the port the server is currently listening on
	uint16 GetPort() const { return Port; }

private:
	// Port file management for Python MCP server auto-discovery
	void WritePortFile() const;
	void DeletePortFile() const;

	// Server state
	bool bIsRunning;
	TSharedPtr<FSocket> ListenerSocket;
	
	TSharedPtr<FSocket> ConnectionSocket;
	FRunnableThread* ServerThread;

	// Server configuration
	FIPv4Address ServerAddress;
	uint16 Port;

	// Saved editor throttle setting, restored when MCP server stops
	bool bOriginalThrottleSetting;

	// Command handler instances
	TSharedPtr<FEpicUnrealMCPEditorCommands> EditorCommands;
	TSharedPtr<FEpicUnrealMCPBlueprintCommands> BlueprintCommands;
	TSharedPtr<FEpicUnrealMCPBlueprintGraphCommands> BlueprintGraphCommands;
	TSharedPtr<FEpicUnrealMCPMaterialCommands> MaterialCommands;
	TSharedPtr<FEpicUnrealMCPDataTableCommands> DataTableCommands;
	TSharedPtr<FEpicUnrealMCPAssetCommands> AssetCommands;
	TSharedPtr<FEpicUnrealMCPDataAssetCommands> DataAssetCommands;
	TSharedPtr<FEpicUnrealMCPWidgetCommands> WidgetCommands;
	TSharedPtr<FEpicUnrealMCPEnhancedInputCommands> EnhancedInputCommands;
	TSharedPtr<FEpicUnrealMCPProfilingCommands> ProfilingCommands;
};