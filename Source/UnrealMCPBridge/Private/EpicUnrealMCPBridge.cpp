#include "EpicUnrealMCPBridge.h"
#include "MCPServerRunnable.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/IPv4/IPv4Address.h"
#include "Interfaces/IPv4/IPv4Endpoint.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "EditorAssetLibrary.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "GameFramework/Actor.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
// Add Blueprint related includes
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Factories/BlueprintFactory.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
// UE5.5 correct includes
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "UObject/Field.h"
#include "UObject/FieldPath.h"
// Blueprint Graph specific includes
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputAction.h"
#include "K2Node_Self.h"
#include "GameFramework/InputSettings.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
// Include our new command handler classes
#include "Commands/EpicUnrealMCPEditorCommands.h"
#include "Commands/EpicUnrealMCPBlueprintCommands.h"
#include "Commands/EpicUnrealMCPBlueprintGraphCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "Commands/EpicUnrealMCPDataTableCommands.h"
#include "Commands/EpicUnrealMCPAssetCommands.h"
#include "Commands/EpicUnrealMCPDataAssetCommands.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "Misc/Base64.h"

// Default settings
#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_SERVER_PORT 55557

UEpicUnrealMCPBridge::UEpicUnrealMCPBridge()
{
    EditorCommands = MakeShared<FEpicUnrealMCPEditorCommands>();
    BlueprintCommands = MakeShared<FEpicUnrealMCPBlueprintCommands>();
    BlueprintGraphCommands = MakeShared<FEpicUnrealMCPBlueprintGraphCommands>();
    MaterialCommands = MakeShared<FEpicUnrealMCPMaterialCommands>();
    DataTableCommands = MakeShared<FEpicUnrealMCPDataTableCommands>();
    AssetCommands = MakeShared<FEpicUnrealMCPAssetCommands>();
    DataAssetCommands = MakeShared<FEpicUnrealMCPDataAssetCommands>();
    WidgetCommands = MakeShared<FEpicUnrealMCPWidgetCommands>();
}

UEpicUnrealMCPBridge::~UEpicUnrealMCPBridge()
{
    EditorCommands.Reset();
    BlueprintCommands.Reset();
    BlueprintGraphCommands.Reset();
    MaterialCommands.Reset();
    DataTableCommands.Reset();
    AssetCommands.Reset();
    DataAssetCommands.Reset();
    WidgetCommands.Reset();
}

// Initialize subsystem
void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Initializing"));
    
    bIsRunning = false;
    ListenerSocket = nullptr;
    ConnectionSocket = nullptr;
    ServerThread = nullptr;
    Port = MCP_SERVER_PORT;
    FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

    // Start the server automatically
    StartServer();
}

// Clean up resources when subsystem is destroyed
void UEpicUnrealMCPBridge::Deinitialize()
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Shutting down"));
    StopServer();
}

// Start the MCP server
void UEpicUnrealMCPBridge::StartServer()
{
    if (bIsRunning)
    {
        UE_LOG(LogTemp, Warning, TEXT("EpicUnrealMCPBridge: Server is already running"));
        return;
    }

    // Create socket subsystem
    ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    if (!SocketSubsystem)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to get socket subsystem"));
        return;
    }

    // Create listener socket
    TSharedPtr<FSocket> NewListenerSocket = MakeShareable(SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
    if (!NewListenerSocket.IsValid())
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create listener socket"));
        return;
    }

    // Allow address reuse for quick restarts
    NewListenerSocket->SetReuseAddr(true);
    NewListenerSocket->SetNonBlocking(true);

    // Bind to address
    FIPv4Endpoint Endpoint(ServerAddress, Port);
    if (!NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to bind listener socket to %s:%d"), *ServerAddress.ToString(), Port);
        return;
    }

    // Start listening
    if (!NewListenerSocket->Listen(5))
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to start listening"));
        return;
    }

    ListenerSocket = NewListenerSocket;
    bIsRunning = true;
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server started on %s:%d"), *ServerAddress.ToString(), Port);

    // Start server thread
    ServerThread = FRunnableThread::Create(
        new FMCPServerRunnable(this, ListenerSocket),
        TEXT("UnrealMCPServerThread"),
        0, TPri_Normal
    );

    if (!ServerThread)
    {
        UE_LOG(LogTemp, Error, TEXT("EpicUnrealMCPBridge: Failed to create server thread"));
        StopServer();
        return;
    }
}

// Stop the MCP server
void UEpicUnrealMCPBridge::StopServer()
{
    if (!bIsRunning)
    {
        return;
    }

    bIsRunning = false;

    // Clean up thread
    if (ServerThread)
    {
        ServerThread->Kill(true);
        delete ServerThread;
        ServerThread = nullptr;
    }

    // Close sockets
    if (ConnectionSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket.Get());
        ConnectionSocket.Reset();
    }

    if (ListenerSocket.IsValid())
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket.Get());
        ListenerSocket.Reset();
    }

    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Server stopped"));
}

// ---------------------------------------------------------------------------
// Python execution helper — mirrors init_unreal.py behavior
// ---------------------------------------------------------------------------
static TSharedPtr<FJsonObject> ExecutePythonCode(const FString& Code)
{
	TSharedPtr<FJsonObject> Result = MakeShareable(new FJsonObject);

	if (Code.IsEmpty())
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Empty code"));
		return Result;
	}

	IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
	if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Python scripting not available"));
		return Result;
	}

	// Base64-encode user code so it can be safely embedded in the wrapper
	FTCHARToUTF8 Utf8Code(*Code);
	TArray<uint8> CodeBytes((const uint8*)Utf8Code.Get(), Utf8Code.Length());
	FString Base64Code = FBase64::Encode(CodeBytes);

	// Wrapper replicates init_unreal.py: exec() in isolated namespace,
	// capture stdout, look for 'result' variable, print JSON with marker.
	FString WrapperCode = FString::Printf(TEXT(
		"import unreal, json, sys, traceback, base64\n"
		"from io import StringIO\n"
		"_c = StringIO()\n"
		"_os, _oe = sys.stdout, sys.stderr\n"
		"sys.stdout = _c\n"
		"sys.stderr = _c\n"
		"_gl = {'unreal': unreal, '__builtins__': __builtins__}\n"
		"_lc = {}\n"
		"_mcp_out = ''\n"
		"try:\n"
		"    _code = base64.b64decode('%s').decode('utf-8')\n"
		"    exec(_code, _gl, _lc)\n"
		"    if 'result' in _lc:\n"
		"        _r = _lc['result']\n"
		"        try:\n"
		"            json.dumps(_r, default=str)\n"
		"        except (TypeError, ValueError):\n"
		"            _r = str(_r)\n"
		"    else:\n"
		"        _o = _c.getvalue().strip()\n"
		"        _r = _o if _o else 'OK'\n"
		"    _mcp_out = json.dumps({'result': _r}, default=str)\n"
		"except Exception:\n"
		"    _mcp_out = json.dumps({'error': traceback.format_exc()}, default=str)\n"
		"finally:\n"
		"    sys.stdout = _os\n"
		"    sys.stderr = _oe\n"
		"print('__MCPRESULT__' + _mcp_out)\n"
	), *Base64Code);

	FPythonCommandEx PyCmd;
	PyCmd.Command = WrapperCode;
	PyCmd.ExecutionMode = EPythonCommandExecutionMode::ExecuteFile;
	PyCmd.FileExecutionScope = EPythonFileExecutionScope::Public;

	PythonPlugin->ExecPythonCommandEx(PyCmd);

	// Find our marker in captured log output
	FString JsonResult;
	for (const FPythonLogOutputEntry& Entry : PyCmd.LogOutput)
	{
		if (Entry.Output.StartsWith(TEXT("__MCPRESULT__")))
		{
			JsonResult = Entry.Output.Mid(13);
			break;
		}
	}

	if (JsonResult.IsEmpty())
	{
		Result->SetBoolField(TEXT("success"), false);
		FString ErrorMsg = (!PyCmd.CommandResult.IsEmpty() && PyCmd.CommandResult != TEXT("None"))
			? PyCmd.CommandResult
			: TEXT("No result from Python execution");
		Result->SetStringField(TEXT("error"), ErrorMsg);
		return Result;
	}

	// Parse the JSON result
	TSharedPtr<FJsonObject> ParsedResult;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonResult);
	if (FJsonSerializer::Deserialize(Reader, ParsedResult))
	{
		if (ParsedResult->HasField(TEXT("error")))
		{
			Result->SetBoolField(TEXT("success"), false);
			Result->SetStringField(TEXT("error"), ParsedResult->GetStringField(TEXT("error")));
		}
		else
		{
			Result->SetBoolField(TEXT("success"), true);
			TSharedPtr<FJsonValue> ResultValue = ParsedResult->TryGetField(TEXT("result"));
			if (ResultValue.IsValid())
			{
				Result->SetField(TEXT("result"), ResultValue);
			}
		}
	}
	else
	{
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("result"), JsonResult);
	}

	return Result;
}

// Execute a command received from a client
FString UEpicUnrealMCPBridge::ExecuteCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);
    
    // Create a promise to wait for the result
    TPromise<FString> Promise;
    TFuture<FString> Future = Promise.GetFuture();
    
    // Queue execution on Game Thread
    AsyncTask(ENamedThreads::GameThread, [this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
    {
        TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
        
        try
        {
            TSharedPtr<FJsonObject> ResultJson;
            
            if (CommandType == TEXT("ping"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
            }
            // Python execution & health check
            else if (CommandType == TEXT("execute_python"))
            {
                ResultJson = ExecutePythonCode(Params->GetStringField(TEXT("code")));
            }
            else if (CommandType == TEXT("health_check"))
            {
                ResultJson = MakeShareable(new FJsonObject);
                ResultJson->SetBoolField(TEXT("success"), true);
                ResultJson->SetStringField(TEXT("status"), TEXT("ok"));
                ResultJson->SetStringField(TEXT("editor"), TEXT("UnrealEngine5"));
            }
            // Editor Commands (actor manipulation + world queries)
            else if (CommandType == TEXT("get_actors_in_level") ||
                     CommandType == TEXT("find_actors_by_name") ||
                     CommandType == TEXT("spawn_actor") ||
                     CommandType == TEXT("delete_actor") ||
                     CommandType == TEXT("set_actor_transform") ||
                     CommandType == TEXT("spawn_blueprint_actor") ||
                     CommandType == TEXT("get_selected_actors") ||
                     CommandType == TEXT("get_world_info") ||
                     CommandType == TEXT("spawn_actor_from_class") ||
                     CommandType == TEXT("get_actor_properties") ||
                     CommandType == TEXT("take_screenshot"))
            {
                ResultJson = EditorCommands->HandleCommand(CommandType, Params);
            }
            // Asset Commands
            else if (CommandType == TEXT("find_assets") ||
                     CommandType == TEXT("list_assets") ||
                     CommandType == TEXT("open_asset") ||
                     CommandType == TEXT("get_asset_info") ||
                     CommandType == TEXT("get_asset_properties") ||
                     CommandType == TEXT("set_asset_property") ||
                     CommandType == TEXT("find_references") ||
                     CommandType == TEXT("duplicate_asset") ||
                     CommandType == TEXT("rename_asset") ||
                     CommandType == TEXT("delete_asset") ||
                     CommandType == TEXT("save_asset") ||
                     CommandType == TEXT("save_all") ||
                     CommandType == TEXT("import_asset") ||
                     CommandType == TEXT("import_assets_batch") ||
                     CommandType == TEXT("get_selected_assets") ||
                     CommandType == TEXT("sync_browser"))
            {
                ResultJson = AssetCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Commands
            else if (CommandType == TEXT("create_blueprint") ||
                     CommandType == TEXT("search_parent_classes") ||
                     CommandType == TEXT("add_component_to_blueprint") ||
                     CommandType == TEXT("set_physics_properties") ||
                     CommandType == TEXT("compile_blueprint") ||
                     CommandType == TEXT("set_static_mesh_properties") ||
                     CommandType == TEXT("set_mesh_material_color") ||
                     CommandType == TEXT("get_available_materials") ||
                     CommandType == TEXT("apply_material_to_actor") ||
                     CommandType == TEXT("apply_material_to_blueprint") ||
                     CommandType == TEXT("get_actor_material_info") ||
                     CommandType == TEXT("get_blueprint_material_info") ||
                     CommandType == TEXT("read_blueprint_content") ||
                     CommandType == TEXT("analyze_blueprint_graph") ||
                     CommandType == TEXT("get_blueprint_variable_details") ||
                     CommandType == TEXT("get_blueprint_function_details") ||
                     CommandType == TEXT("get_blueprint_class_defaults"))
            {
                ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
            }
            // Material Commands
            else if (CommandType == TEXT("create_material") ||
                     CommandType == TEXT("create_material_instance") ||
                     CommandType == TEXT("build_material_graph") ||
                     CommandType == TEXT("get_material_info") ||
                     CommandType == TEXT("recompile_material") ||
                     CommandType == TEXT("set_material_properties") ||
                     CommandType == TEXT("add_material_comments") ||
                     CommandType == TEXT("get_material_graph_nodes") ||
                     CommandType == TEXT("get_material_expression_info") ||
                     CommandType == TEXT("get_material_property_connections") ||
                     CommandType == TEXT("add_material_expression") ||
                     CommandType == TEXT("set_material_expression_property") ||
                     CommandType == TEXT("move_material_expression") ||
                     CommandType == TEXT("duplicate_material_expression") ||
                     CommandType == TEXT("connect_material_expressions") ||
                     CommandType == TEXT("delete_material_expression") ||
                     CommandType == TEXT("layout_material_expressions") ||
                     CommandType == TEXT("get_material_instance_parameters") ||
                     CommandType == TEXT("set_material_instance_parameter") ||
                     CommandType == TEXT("get_material_errors"))
            {
                ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
            }
            // Data Table Commands
            else if (CommandType == TEXT("get_data_table_rows") ||
                     CommandType == TEXT("get_data_table_row") ||
                     CommandType == TEXT("get_data_table_schema") ||
                     CommandType == TEXT("add_data_table_row") ||
                     CommandType == TEXT("update_data_table_row") ||
                     CommandType == TEXT("delete_data_table_row") ||
                     CommandType == TEXT("duplicate_data_table_row") ||
                     CommandType == TEXT("rename_data_table_row"))
            {
                ResultJson = DataTableCommands->HandleCommand(CommandType, Params);
            }
            // Data Asset Commands
            else if (CommandType == TEXT("list_data_asset_classes") ||
                     CommandType == TEXT("create_data_asset") ||
                     CommandType == TEXT("get_data_asset_properties") ||
                     CommandType == TEXT("set_data_asset_property") ||
                     CommandType == TEXT("set_data_asset_properties") ||
                     CommandType == TEXT("list_data_assets") ||
                     CommandType == TEXT("get_property_valid_types"))
            {
                ResultJson = DataAssetCommands->HandleCommand(CommandType, Params);
            }
            // Blueprint Graph Commands
            else if (CommandType == TEXT("add_blueprint_node") ||
                     CommandType == TEXT("connect_nodes") ||
                     CommandType == TEXT("create_variable") ||
                     CommandType == TEXT("set_blueprint_variable_properties") ||
                     CommandType == TEXT("add_event_node") ||
                     CommandType == TEXT("delete_node") ||
                     CommandType == TEXT("set_node_property") ||
                     CommandType == TEXT("create_function") ||
                     CommandType == TEXT("add_function_input") ||
                     CommandType == TEXT("add_function_output") ||
                     CommandType == TEXT("delete_function") ||
                     CommandType == TEXT("rename_function"))
            {
                ResultJson = BlueprintGraphCommands->HandleCommand(CommandType, Params);
            }
            // Widget Blueprint Commands
            else if (CommandType == TEXT("get_widget_tree") ||
                     CommandType == TEXT("add_widget") ||
                     CommandType == TEXT("remove_widget") ||
                     CommandType == TEXT("move_widget") ||
                     CommandType == TEXT("rename_widget") ||
                     CommandType == TEXT("duplicate_widget") ||
                     CommandType == TEXT("get_widget_properties") ||
                     CommandType == TEXT("set_widget_properties") ||
                     CommandType == TEXT("get_slot_properties") ||
                     CommandType == TEXT("set_slot_properties") ||
                     CommandType == TEXT("list_widget_types"))
            {
                ResultJson = WidgetCommands->HandleCommand(CommandType, Params);
            }
            else
            {
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), FString::Printf(TEXT("Unknown command: %s"), *CommandType));
                
                FString ResultString;
                TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
                FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
                Promise.SetValue(ResultString);
                return;
            }
            
            // Check if the result contains an error
            bool bSuccess = true;
            FString ErrorMessage;
            
            if (ResultJson->HasField(TEXT("success")))
            {
                bSuccess = ResultJson->GetBoolField(TEXT("success"));
                if (!bSuccess && ResultJson->HasField(TEXT("error")))
                {
                    ErrorMessage = ResultJson->GetStringField(TEXT("error"));
                }
            }
            
            if (bSuccess)
            {
                // Set success status and include the result
                ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
                ResponseJson->SetObjectField(TEXT("result"), ResultJson);
            }
            else
            {
                // Set error status and include the error message
                ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
                ResponseJson->SetStringField(TEXT("error"), ErrorMessage);
            }
        }
        catch (const std::exception& e)
        {
            ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
            ResponseJson->SetStringField(TEXT("error"), UTF8_TO_TCHAR(e.what()));
        }
        
        FString ResultString;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
        FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
        Promise.SetValue(ResultString);
    });
    
    return Future.Get();
}