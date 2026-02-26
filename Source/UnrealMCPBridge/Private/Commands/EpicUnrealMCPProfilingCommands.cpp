#include "Commands/EpicUnrealMCPProfilingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPProfilingUtils.h"

#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Trace/DataStream.h"
#include "TraceServices/ITraceServicesModule.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/Model/AnalysisSession.h"
#include "Misc/Paths.h"

// Shared log category definition (declared in EpicUnrealMCPProfilingUtils.h)
DEFINE_LOG_CATEGORY(LogMCPProfiling);

// ────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────

FEpicUnrealMCPProfilingCommands::FEpicUnrealMCPProfilingCommands()
{
}

FEpicUnrealMCPProfilingCommands::~FEpicUnrealMCPProfilingCommands()
{
	if (bIsRecording)
	{
		FTraceAuxiliary::Stop();
		bIsRecording = false;
	}

	if (CurrentSession.IsValid())
	{
		CurrentSession->Stop(true);
		CurrentSession.Reset();
	}
	AnalysisService.Reset();
}

// ────────────────────────────────────────────────────────────
// Command dispatcher
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleCommand(
	const FString& CommandType,
	const TSharedPtr<FJsonObject>& Params)
{
	if (CommandType == TEXT("performance_start_trace"))
	{
		return HandleStartTrace(Params);
	}
	else if (CommandType == TEXT("performance_stop_trace"))
	{
		return HandleStopTrace(Params);
	}
	else if (CommandType == TEXT("performance_analyze_insight"))
	{
		return HandleAnalyzeInsight(Params);
	}

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown profiling command: %s"), *CommandType));
}

// ────────────────────────────────────────────────────────────
// Session helpers
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::EnsureSession() const
{
	if (!CurrentSession.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("No trace loaded. Use performance_stop_trace (after recording) or performance_analyze_insight(query=\"load\") first."));
	}
	return nullptr;
}

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::EnsureAnalysisService()
{
	if (!AnalysisService.IsValid())
	{
		ITraceServicesModule& TraceServicesModule =
			FModuleManager::LoadModuleChecked<ITraceServicesModule>(TEXT("TraceServices"));

		AnalysisService = TraceServicesModule.GetAnalysisService();

		if (!AnalysisService.IsValid())
		{
			return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
				TEXT("Failed to get TraceServices analysis service"));
		}
	}
	return nullptr;
}

// ────────────────────────────────────────────────────────────
// performance_analyze_insight (unified query dispatcher)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleAnalyzeInsight(
	const TSharedPtr<FJsonObject>& Params)
{
	FString Query;
	if (!Params->TryGetStringField(TEXT("query"), Query))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'query'. Valid: load, diagnose, summary, bottlenecks, hotpath, compare, spikes, search, histogram, flame, ")
			TEXT("worst_frames, frame_details, timer_stats, butterfly, threads, counters, ")
			TEXT("net_stats, loading, logs, memory, regions, bookmarks, ")
			TEXT("session, modules, file_io, tasks, context_switches, allocations, stack_samples, screenshots"));
	}

	if (Query == TEXT("load"))
	{
		return HandleLoadTrace(Params);
	}

	// All other queries need a loaded session
	if (TSharedPtr<FJsonObject> Err = EnsureSession())
	{
		return Err;
	}

	// Smart analysis queries (compact output)
	if (Query == TEXT("bottlenecks"))  return HandleGetBottlenecks(Params);
	if (Query == TEXT("hotpath"))      return HandleGetHotpath(Params);
	if (Query == TEXT("compare"))      return HandleGetCompare(Params);
	if (Query == TEXT("spikes"))       return HandleGetSpikes(Params);
	if (Query == TEXT("search"))       return HandleGetSearch(Params);
	if (Query == TEXT("histogram"))    return HandleGetHistogram(Params);
	if (Query == TEXT("diagnose"))     return HandleGetDiagnose(Params);
	if (Query == TEXT("flame"))        return HandleGetFlame(Params);

	// Standard queries
	if (Query == TEXT("summary"))         return HandleGetSummary(Params);
	if (Query == TEXT("worst_frames"))    return HandleGetWorstFrames(Params);
	if (Query == TEXT("frame_details"))   return HandleGetFrameDetails(Params);
	if (Query == TEXT("timer_stats"))     return HandleGetTimerStats(Params);
	if (Query == TEXT("butterfly"))       return HandleGetButterfly(Params);
	if (Query == TEXT("threads"))         return HandleGetThreads(Params);
	if (Query == TEXT("counters"))        return HandleGetCounters(Params);

	// Provider queries
	if (Query == TEXT("net_stats"))       return HandleGetNetStats(Params);
	if (Query == TEXT("loading"))         return HandleGetLoading(Params);
	if (Query == TEXT("logs"))            return HandleGetLogs(Params);
	if (Query == TEXT("memory"))          return HandleGetMemory(Params);
	if (Query == TEXT("regions"))         return HandleGetRegions(Params);
	if (Query == TEXT("bookmarks"))       return HandleGetBookmarks(Params);

	// Extended provider queries
	if (Query == TEXT("session"))            return HandleGetSession(Params);
	if (Query == TEXT("modules"))            return HandleGetModules(Params);
	if (Query == TEXT("file_io"))            return HandleGetFileIO(Params);
	if (Query == TEXT("tasks"))              return HandleGetTasks(Params);
	if (Query == TEXT("context_switches"))   return HandleGetContextSwitches(Params);
	if (Query == TEXT("allocations"))        return HandleGetAllocations(Params);
	if (Query == TEXT("stack_samples"))      return HandleGetStackSamples(Params);
	if (Query == TEXT("screenshots"))        return HandleGetScreenshots(Params);

	return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
		FString::Printf(TEXT("Unknown query: %s"), *Query));
}

// ────────────────────────────────────────────────────────────
// performance_start_trace
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleStartTrace(
	const TSharedPtr<FJsonObject>& Params)
{
	if (bIsRecording)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Already recording a trace. Call performance_stop_trace first."));
	}

	FString FilePath;
	if (!Params->TryGetStringField(TEXT("file_path"), FilePath))
	{
		FilePath = FPaths::ProfilingDir() / FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT(".utrace");
	}

	FString Channels = TEXT("default");
	Params->TryGetStringField(TEXT("channels"), Channels);

	UE_LOG(LogMCPProfiling, Display, TEXT("Starting trace: %s (channels: %s)"), *FilePath, *Channels);

	FTraceAuxiliary::FOptions Options;
	Options.bTruncateFile = true;
	Options.bExcludeTail = false;

	bool bStarted = FTraceAuxiliary::Start(
		FTraceAuxiliary::EConnectionType::File,
		*FilePath,
		*Channels,
		&Options);

	if (!bStarted)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to start trace recording to: %s"), *FilePath));
	}

	bIsRecording = true;
	RecordingTracePath = FilePath;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("trace_path"), FilePath);
	Result->SetStringField(TEXT("channels"), Channels);
	Result->SetStringField(TEXT("message"), TEXT("Trace recording started. Call performance_stop_trace when done."));

	return Result;
}

// ────────────────────────────────────────────────────────────
// performance_stop_trace
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleStopTrace(
	const TSharedPtr<FJsonObject>& Params)
{
	if (!bIsRecording)
	{
		if (CurrentSession.IsValid())
		{
			CurrentSession->Stop(true);
			CurrentSession.Reset();
			LoadedTracePath.Empty();

			TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
			Result->SetBoolField(TEXT("success"), true);
			Result->SetStringField(TEXT("message"), TEXT("Analysis session unloaded."));
			return Result;
		}
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("No trace is being recorded and no session is loaded."));
	}

	FTraceAuxiliary::Stop();
	bIsRecording = false;

	FString TracePath = RecordingTracePath;
	RecordingTracePath.Empty();

	UE_LOG(LogMCPProfiling, Display, TEXT("Trace recording stopped: %s"), *TracePath);

	bool bAutoLoad = true;
	if (Params->HasField(TEXT("auto_load")))
	{
		bAutoLoad = Params->GetBoolField(TEXT("auto_load"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("trace_path"), TracePath);

	if (bAutoLoad)
	{
		if (CurrentSession.IsValid())
		{
			CurrentSession->Stop(true);
			CurrentSession.Reset();
		}

		if (TSharedPtr<FJsonObject> Err = EnsureAnalysisService())
		{
			return Err;
		}

		CurrentSession = AnalysisService->Analyze(*TracePath);
		if (!CurrentSession.IsValid())
		{
			Result->SetStringField(TEXT("message"),
				FString::Printf(TEXT("Recording stopped but failed to load trace: %s"), *TracePath));
			return Result;
		}

		LoadedTracePath = TracePath;
		{
			TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);
			Result->SetNumberField(TEXT("duration_seconds"), CurrentSession->GetDurationSeconds());
		}
		Result->SetBoolField(TEXT("loaded"), true);
		Result->SetStringField(TEXT("message"), TEXT("Recording stopped and trace loaded. Use performance_analyze_insight to query."));
	}
	else
	{
		Result->SetBoolField(TEXT("loaded"), false);
		Result->SetStringField(TEXT("message"), TEXT("Recording stopped. Use performance_analyze_insight(query=\"load\") to load."));
	}

	return Result;
}

// ────────────────────────────────────────────────────────────
// load
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleLoadTrace(
	const TSharedPtr<FJsonObject>& Params)
{
	FString TracePath;
	if (!Params->TryGetStringField(TEXT("trace_path"), TracePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'trace_path' for load query"));
	}

	if (!FPaths::FileExists(TracePath))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Trace file not found: %s"), *TracePath));
	}

	if (CurrentSession.IsValid())
	{
		CurrentSession->Stop(true);
		CurrentSession.Reset();
	}

	if (TSharedPtr<FJsonObject> Err = EnsureAnalysisService())
	{
		return Err;
	}

	UE_LOG(LogMCPProfiling, Display, TEXT("Loading trace: %s"), *TracePath);

	CurrentSession = AnalysisService->Analyze(*TracePath);
	if (!CurrentSession.IsValid())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Failed to analyze trace: %s"), *TracePath));
	}

	LoadedTracePath = TracePath;

	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("trace_path"), TracePath);
	Result->SetNumberField(TEXT("duration_seconds"), CurrentSession->GetDurationSeconds());
	Result->SetStringField(TEXT("message"),
		TEXT("Trace loaded. Recommended: start with 'bottlenecks' for compact frame analysis, ")
		TEXT("then 'hotpath' to drill into categories. Also: summary, worst_frames, compare, ")
		TEXT("frame_details, timer_stats, butterfly, threads, counters, ")
		TEXT("net_stats, loading, logs, memory, regions, bookmarks"));

	return Result;
}
