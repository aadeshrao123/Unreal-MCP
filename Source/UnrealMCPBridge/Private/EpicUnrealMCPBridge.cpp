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
#include "Async/Async.h"
#include "IPythonScriptPlugin.h"
#include "PythonScriptTypes.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Editor/EditorPerformanceSettings.h"

#define MCP_SERVER_HOST "127.0.0.1"
#define MCP_DEFAULT_PORT 55557
#define MCP_PORT_SCAN_RANGE 100

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
	EnhancedInputCommands = MakeShared<FEpicUnrealMCPEnhancedInputCommands>();
	ProfilingCommands = MakeShared<FEpicUnrealMCPProfilingCommands>();
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
	EnhancedInputCommands.Reset();
	ProfilingCommands.Reset();
}

void UEpicUnrealMCPBridge::Initialize(FSubsystemCollectionBase& Collection)
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Initializing"));

	bIsRunning = false;
	ListenerSocket = nullptr;
	ConnectionSocket = nullptr;
	ServerThread = nullptr;

	// Read optional base port from DefaultEngine.ini [UnrealMCP] Port
	int32 ConfigPort = MCP_DEFAULT_PORT;
	GConfig->GetInt(TEXT("UnrealMCP"), TEXT("Port"), ConfigPort, GEngineIni);
	if (ConfigPort < 1 || ConfigPort > 65535)
	{
		ConfigPort = MCP_DEFAULT_PORT;
	}
	Port = static_cast<uint16>(ConfigPort);

	FIPv4Address::Parse(MCP_SERVER_HOST, ServerAddress);

	StartServer();
}

void UEpicUnrealMCPBridge::Deinitialize()
{
	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Shutting down"));
	DeletePortFile();
	StopServer();
}

void UEpicUnrealMCPBridge::StartServer()
{
	if (bIsRunning)
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Server is already running"));
		return;
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to get socket subsystem"));
		return;
	}

	// Try to bind to an available port, starting from the configured base port
	const uint16 StartPort = Port;
	bool bBound = false;

	for (uint16 Offset = 0; Offset < MCP_PORT_SCAN_RANGE; ++Offset)
	{
		const uint16 TryPort = StartPort + Offset;

		// Guard against uint16 overflow
		if (TryPort < StartPort)
		{
			break;
		}

		TSharedPtr<FSocket> NewListenerSocket = MakeShareable(
			SocketSubsystem->CreateSocket(NAME_Stream, TEXT("UnrealMCPListener"), false));
		if (!NewListenerSocket.IsValid())
		{
			UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to create listener socket"));
			return;
		}

		// Do NOT use SetReuseAddr(true) — on Windows, SO_REUSEADDR allows multiple
		// processes to bind the same port simultaneously, defeating port scanning.
		// Without it, Bind() correctly fails when another editor holds the port,
		// and the scan moves to the next port. TIME_WAIT ports are skipped too,
		// which is fine since the scan will just pick the next available one.
		NewListenerSocket->SetNonBlocking(true);

		FIPv4Endpoint Endpoint(ServerAddress, TryPort);
		if (NewListenerSocket->Bind(*Endpoint.ToInternetAddr()))
		{
			if (NewListenerSocket->Listen(5))
			{
				Port = TryPort;
				ListenerSocket = NewListenerSocket;
				bBound = true;
				break;
			}

			UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Bound to port %d but Listen() failed"), TryPort);
		}
		else if (Offset == 0)
		{
			UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Port %d in use, scanning for available port..."), TryPort);
		}

		// Socket will be cleaned up by TSharedPtr going out of scope
	}

	if (!bBound)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to bind to any port in range %d-%d"),
			StartPort, StartPort + MCP_PORT_SCAN_RANGE - 1);
		return;
	}

	bIsRunning = true;
	WritePortFile();

	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Server started on %s:%d"), *ServerAddress.ToString(), Port);

	ServerThread = FRunnableThread::Create(
		new FMCPServerRunnable(this, ListenerSocket),
		TEXT("UnrealMCPServerThread"),
		0, TPri_Normal);

	if (!ServerThread)
	{
		UE_LOG(LogTemp, Error, TEXT("UnrealMCP: Failed to create server thread"));
		StopServer();
		return;
	}

	// Disable the editor's background CPU throttle while the MCP server is active.
	// Without this, the editor drops to ~3 FPS when not in the foreground, causing
	// ~333ms delays on every game-thread command dispatch.
	UEditorPerformanceSettings* PerfSettings = GetMutableDefault<UEditorPerformanceSettings>();
	bOriginalThrottleSetting = PerfSettings->bThrottleCPUWhenNotForeground;
	PerfSettings->bThrottleCPUWhenNotForeground = false;
	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Disabled background CPU throttling for responsive command execution"));
}

void UEpicUnrealMCPBridge::StopServer()
{
	if (!bIsRunning)
	{
		return;
	}

	bIsRunning = false;

	// Restore the original background CPU throttle setting
	GetMutableDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground = bOriginalThrottleSetting;

	if (ServerThread)
	{
		ServerThread->Kill(true);
		delete ServerThread;
		ServerThread = nullptr;
	}

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

	UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Server stopped"));
}

void UEpicUnrealMCPBridge::WritePortFile() const
{
	FString PortDir = FPaths::ProjectSavedDir() / TEXT("UnrealMCP");
	IFileManager::Get().MakeDirectory(*PortDir, true);

	FString PortFilePath = PortDir / TEXT("port.txt");
	FString PortString = FString::Printf(TEXT("%d"), Port);

	if (FFileHelper::SaveStringToFile(PortString, *PortFilePath))
	{
		UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Wrote port %d to %s"), Port, *PortFilePath);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UnrealMCP: Failed to write port file at %s"), *PortFilePath);
	}
}

void UEpicUnrealMCPBridge::DeletePortFile() const
{
	FString PortFilePath = FPaths::ProjectSavedDir() / TEXT("UnrealMCP") / TEXT("port.txt");

	if (IFileManager::Get().FileExists(*PortFilePath))
	{
		IFileManager::Get().Delete(*PortFilePath);
		UE_LOG(LogTemp, Display, TEXT("UnrealMCP: Deleted port file at %s"), *PortFilePath);
	}
}

// Base64-encodes user code and runs it via the Python scripting plugin
// with stdout capture and JSON result extraction.
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

	FTCHARToUTF8 Utf8Code(*Code);
	TArray<uint8> CodeBytes((const uint8*)Utf8Code.Get(), Utf8Code.Length());
	FString Base64Code = FBase64::Encode(CodeBytes);

	// exec() in isolated namespace, capture stdout, extract 'result' variable
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

// Helper to serialize a response JSON object to a string
static FString SerializeResponse(const TSharedPtr<FJsonObject>& ResponseJson)
{
	FString ResultString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
	FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
	return ResultString;
}

FString UEpicUnrealMCPBridge::ExecuteCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	UE_LOG(LogTemp, Display, TEXT("EpicUnrealMCPBridge: Executing command: %s"), *CommandType);

	// Fast-path: commands that don't touch UObjects are handled immediately
	// on the server thread, bypassing the game thread dispatch entirely.
	if (CommandType == TEXT("ping"))
	{
		TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
		TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
		ResultJson->SetStringField(TEXT("message"), TEXT("pong"));
		ResponseJson->SetObjectField(TEXT("result"), ResultJson);
		return SerializeResponse(ResponseJson);
	}

	if (CommandType == TEXT("health_check"))
	{
		TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);
		ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
		TSharedPtr<FJsonObject> ResultJson = MakeShareable(new FJsonObject);
		ResultJson->SetBoolField(TEXT("success"), true);
		ResultJson->SetStringField(TEXT("status"), TEXT("ok"));
		ResultJson->SetStringField(TEXT("editor"), TEXT("UnrealEngine5"));
		ResponseJson->SetObjectField(TEXT("result"), ResultJson);
		return SerializeResponse(ResponseJson);
	}

	// All other commands require the game thread for UObject/Editor API access
	TPromise<FString> Promise;
	TFuture<FString> Future = Promise.GetFuture();

	AsyncTask(ENamedThreads::GameThread,
		[this, CommandType, Params, Promise = MoveTemp(Promise)]() mutable
	{
		TSharedPtr<FJsonObject> ResponseJson = MakeShareable(new FJsonObject);

		try
		{
			TSharedPtr<FJsonObject> ResultJson;

			if (CommandType == TEXT("execute_python"))
			{
				ResultJson = ExecutePythonCode(Params->GetStringField(TEXT("code")));
			}
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
				CommandType == TEXT("get_blueprint_class_defaults") ||
				CommandType == TEXT("set_blueprint_class_defaults"))
			{
				ResultJson = BlueprintCommands->HandleCommand(CommandType, Params);
			}
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
				CommandType == TEXT("get_material_errors") ||
				CommandType == TEXT("list_material_expression_types"))
			{
				ResultJson = MaterialCommands->HandleCommand(CommandType, Params);
			}
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
			else if (CommandType == TEXT("create_input_action") ||
				CommandType == TEXT("get_input_action") ||
				CommandType == TEXT("set_input_action_properties") ||
				CommandType == TEXT("add_input_action_trigger") ||
				CommandType == TEXT("add_input_action_modifier") ||
				CommandType == TEXT("remove_input_action_trigger") ||
				CommandType == TEXT("remove_input_action_modifier") ||
				CommandType == TEXT("list_input_actions") ||
				CommandType == TEXT("create_input_mapping_context") ||
				CommandType == TEXT("get_input_mapping_context") ||
				CommandType == TEXT("add_key_mapping") ||
				CommandType == TEXT("remove_key_mapping") ||
				CommandType == TEXT("set_key_mapping") ||
				CommandType == TEXT("add_mapping_trigger") ||
				CommandType == TEXT("add_mapping_modifier") ||
				CommandType == TEXT("remove_mapping_trigger") ||
				CommandType == TEXT("remove_mapping_modifier") ||
				CommandType == TEXT("list_input_mapping_contexts") ||
				CommandType == TEXT("list_trigger_types") ||
				CommandType == TEXT("list_modifier_types") ||
				CommandType == TEXT("list_input_keys"))
			{
				ResultJson = EnhancedInputCommands->HandleCommand(CommandType, Params);
			}
			else if (CommandType == TEXT("performance_start_trace") ||
				CommandType == TEXT("performance_stop_trace") ||
				CommandType == TEXT("performance_analyze_insight"))
			{
				ResultJson = ProfilingCommands->HandleCommand(CommandType, Params);
			}
			else
			{
				ResponseJson->SetStringField(TEXT("status"), TEXT("error"));
				ResponseJson->SetStringField(TEXT("error"),
					FString::Printf(TEXT("Unknown command: %s"), *CommandType));

				FString ResultString;
				TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultString);
				FJsonSerializer::Serialize(ResponseJson.ToSharedRef(), Writer);
				Promise.SetValue(ResultString);
				return;
			}

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
				ResponseJson->SetStringField(TEXT("status"), TEXT("success"));
				ResponseJson->SetObjectField(TEXT("result"), ResultJson);
			}
			else
			{
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
