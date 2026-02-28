#include "Commands/Profiling/EpicUnrealMCPProfilingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPProfilingUtils.h"

#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/NetProfiler.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/Log.h"
#include "TraceServices/Model/Memory.h"
#include "TraceServices/Model/Regions.h"
#include "TraceServices/Model/Bookmarks.h"
#include "TraceServices/Model/Diagnostics.h"
#include "TraceServices/Model/Modules.h"
#include "TraceServices/Model/ContextSwitches.h"
#include "TraceServices/Model/TasksProfiler.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/StackSamples.h"
#include "TraceServices/Model/Screenshot.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Containers/Tables.h"

// ────────────────────────────────────────────────────────────
// net_stats (network profiling)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetNetStats(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::INetProfilerProvider* NetProvider =
		CurrentSession->ReadProvider<TraceServices::INetProfilerProvider>(
			TraceServices::GetNetProfilerProviderName());

	if (!NetProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Network profiler not available. Record with channels including 'net'."));
	}

	int32 ConnectionIndex = -1;
	if (Params->HasField(TEXT("connection_index")))
	{
		ConnectionIndex = static_cast<int32>(Params->GetNumberField(TEXT("connection_index")));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Game instances
	TArray<TSharedPtr<FJsonValue>> InstancesArray;

	NetProvider->ReadGameInstances(
		[&](const TraceServices::FNetProfilerGameInstance& Instance)
		{
			TSharedPtr<FJsonObject> InstObj = MakeShared<FJsonObject>();
			InstObj->SetNumberField(TEXT("index"), Instance.GameInstanceIndex);
			InstObj->SetStringField(TEXT("name"),
				Instance.InstanceName ? FString(Instance.InstanceName) : TEXT(""));
			InstObj->SetBoolField(TEXT("is_server"), Instance.bIsServer);

			uint32 ConnCount = NetProvider->GetConnectionCount(Instance.GameInstanceIndex);
			InstObj->SetNumberField(TEXT("connections"), ConnCount);

			TArray<TSharedPtr<FJsonValue>> ConnsArray;
			NetProvider->ReadConnections(Instance.GameInstanceIndex,
				[&](const TraceServices::FNetProfilerConnection& Conn)
				{
					TSharedPtr<FJsonObject> ConnObj = MakeShared<FJsonObject>();
					ConnObj->SetNumberField(TEXT("index"), Conn.ConnectionIndex);
					ConnObj->SetStringField(TEXT("name"),
						Conn.Name ? FString(Conn.Name) : TEXT(""));

					uint32 OutPackets = NetProvider->GetPacketCount(
						Conn.ConnectionIndex, TraceServices::ENetProfilerConnectionMode::Outgoing);
					uint32 InPackets = NetProvider->GetPacketCount(
						Conn.ConnectionIndex, TraceServices::ENetProfilerConnectionMode::Incoming);
					ConnObj->SetNumberField(TEXT("out_packets"), OutPackets);
					ConnObj->SetNumberField(TEXT("in_packets"), InPackets);

					ConnsArray.Add(MakeShared<FJsonValueObject>(ConnObj));
				});

			InstObj->SetArrayField(TEXT("connections_list"), ConnsArray);
			InstancesArray.Add(MakeShared<FJsonValueObject>(InstObj));
		});

	Result->SetArrayField(TEXT("game_instances"), InstancesArray);

	// Packet aggregation for specific connection
	if (ConnectionIndex >= 0)
	{
		uint32 OutPacketCount = NetProvider->GetPacketCount(
			ConnectionIndex, TraceServices::ENetProfilerConnectionMode::Outgoing);

		if (OutPacketCount > 0)
		{
			TraceServices::ITable<TraceServices::FNetProfilerAggregatedStats>* OutTable =
				NetProvider->CreateAggregation(
					ConnectionIndex, TraceServices::ENetProfilerConnectionMode::Outgoing,
					0, OutPacketCount, 0, ~0u);

			if (OutTable)
			{
				TArray<TSharedPtr<FJsonValue>> OutAggArray;
				auto* OutReader = OutTable->CreateReader();
				int32 AggCount = 0;

				while (OutReader->IsValid() && AggCount < 30)
				{
					const TraceServices::FNetProfilerAggregatedStats* Row = OutReader->GetCurrentRow();
					if (Row)
					{
						TSharedPtr<FJsonObject> AggObj = MakeShared<FJsonObject>();

						NetProvider->ReadEventType(Row->EventTypeIndex,
							[&](const TraceServices::FNetProfilerEventType& EvType)
							{
								AggObj->SetStringField(TEXT("name"),
									EvType.Name ? FString(EvType.Name) : TEXT("Unknown"));
							});

						AggObj->SetNumberField(TEXT("count"), Row->InstanceCount);
						AggObj->SetNumberField(TEXT("total_bits"), Row->TotalInclusive);
						AggObj->SetNumberField(TEXT("avg_bits"), Row->AverageInclusive);

						OutAggArray.Add(MakeShared<FJsonValueObject>(AggObj));
						AggCount++;
					}
					OutReader->NextRow();
				}

				delete OutReader;
				delete OutTable;

				Result->SetArrayField(TEXT("outgoing_aggregation"), OutAggArray);
			}
		}
	}

	return Result;
}

// ────────────────────────────────────────────────────────────
// loading (asset loading analysis)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetLoading(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ILoadTimeProfilerProvider* LoadProvider =
		CurrentSession->ReadProvider<TraceServices::ILoadTimeProfilerProvider>(
			TraceServices::GetLoadTimeProfilerProviderName());

	if (!LoadProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Load time profiler not available. Record with channels including 'loadtime'."));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	int32 MaxResults = 50;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 500);
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Package details
	TraceServices::ITable<TraceServices::FPackagesTableRow>* PkgTable =
		LoadProvider->CreatePackageDetailsTable(IntervalStart, IntervalEnd);

	if (PkgTable)
	{
		TArray<TSharedPtr<FJsonValue>> PackagesArray;
		auto* PkgReader = PkgTable->CreateReader();
		int32 Count = 0;

		while (PkgReader->IsValid() && Count < MaxResults)
		{
			const TraceServices::FPackagesTableRow* Row = PkgReader->GetCurrentRow();
			if (Row && Row->PackageInfo)
			{
				FString PkgName = Row->PackageInfo->Name ? FString(Row->PackageInfo->Name) : TEXT("Unknown");

				if (!NameFilter.IsEmpty() && !PkgName.Contains(NameFilter))
				{
					PkgReader->NextRow();
					continue;
				}

				TSharedPtr<FJsonObject> PkgObj = MakeShared<FJsonObject>();
				PkgObj->SetStringField(TEXT("name"), PkgName);
				PkgObj->SetNumberField(TEXT("size"), static_cast<double>(Row->TotalSerializedSize));
				PkgObj->SetNumberField(TEXT("exports"), static_cast<double>(Row->SerializedExportsCount));
				PkgObj->SetNumberField(TEXT("main_ms"), SafeDouble(Row->MainThreadTime * 1000.0));
				PkgObj->SetNumberField(TEXT("async_ms"), SafeDouble(Row->AsyncLoadingThreadTime * 1000.0));

				PackagesArray.Add(MakeShared<FJsonValueObject>(PkgObj));
				Count++;
			}
			PkgReader->NextRow();
		}

		delete PkgReader;
		delete PkgTable;

		Result->SetNumberField(TEXT("package_count"), Count);
		Result->SetArrayField(TEXT("packages"), PackagesArray);
	}

	// Requests
	TraceServices::ITable<TraceServices::FRequestsTableRow>* ReqTable =
		LoadProvider->CreateRequestsTable(IntervalStart, IntervalEnd);

	if (ReqTable)
	{
		TArray<TSharedPtr<FJsonValue>> RequestsArray;
		auto* ReqReader = ReqTable->CreateReader();
		int32 Count = 0;

		while (ReqReader->IsValid() && Count < 30)
		{
			const TraceServices::FRequestsTableRow* Row = ReqReader->GetCurrentRow();
			if (Row)
			{
				TSharedPtr<FJsonObject> ReqObj = MakeShared<FJsonObject>();
				ReqObj->SetStringField(TEXT("name"), Row->Name ? FString(Row->Name) : TEXT("Unknown"));
				ReqObj->SetNumberField(TEXT("duration_ms"), SafeDouble(Row->Duration * 1000.0));
				ReqObj->SetNumberField(TEXT("packages"), Row->Packages.Num());

				RequestsArray.Add(MakeShared<FJsonValueObject>(ReqObj));
				Count++;
			}
			ReqReader->NextRow();
		}

		delete ReqReader;
		delete ReqTable;

		Result->SetArrayField(TEXT("requests"), RequestsArray);
	}

	return Result;
}

// ────────────────────────────────────────────────────────────
// logs
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetLogs(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ILogProvider* LogProviderPtr =
		CurrentSession->ReadProvider<TraceServices::ILogProvider>(
			TraceServices::GetLogProviderName());

	if (!LogProviderPtr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Log provider not available. Record with channels including 'log'."));
	}

	const TraceServices::ILogProvider& LogProvider = *LogProviderPtr;

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	int32 MaxResults = 100;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 1000);
	}

	FString CategoryFilter;
	Params->TryGetStringField(TEXT("filter"), CategoryFilter);

	FString VerbosityFilter;
	Params->TryGetStringField(TEXT("verbosity"), VerbosityFilter);

	TArray<TSharedPtr<FJsonValue>> MessagesArray;
	int32 Count = 0;

	LogProvider.EnumerateMessages(IntervalStart, IntervalEnd,
		[&](const TraceServices::FLogMessageInfo& Msg)
		{
			if (Count >= MaxResults)
			{
				return;
			}

			if (!CategoryFilter.IsEmpty() && Msg.Category)
			{
				FString CatName(Msg.Category->Name);
				if (!CatName.Contains(CategoryFilter))
				{
					return;
				}
			}

			if (!VerbosityFilter.IsEmpty())
			{
				FString VerbStr;
				switch (Msg.Verbosity)
				{
				case ELogVerbosity::Fatal:       VerbStr = TEXT("Fatal"); break;
				case ELogVerbosity::Error:       VerbStr = TEXT("Error"); break;
				case ELogVerbosity::Warning:     VerbStr = TEXT("Warning"); break;
				case ELogVerbosity::Display:     VerbStr = TEXT("Display"); break;
				case ELogVerbosity::Log:         VerbStr = TEXT("Log"); break;
				case ELogVerbosity::Verbose:     VerbStr = TEXT("Verbose"); break;
				case ELogVerbosity::VeryVerbose: VerbStr = TEXT("VeryVerbose"); break;
				default:                         VerbStr = TEXT("Unknown"); break;
				}

				if (!VerbStr.Contains(VerbosityFilter))
				{
					return;
				}
			}

			TSharedPtr<FJsonObject> MsgObj = MakeShared<FJsonObject>();
			MsgObj->SetNumberField(TEXT("time"), Msg.Time);
			MsgObj->SetStringField(TEXT("msg"),
				Msg.Message ? FString(Msg.Message) : TEXT(""));

			if (Msg.Category && Msg.Category->Name)
			{
				MsgObj->SetStringField(TEXT("cat"), FString(Msg.Category->Name));
			}

			MessagesArray.Add(MakeShared<FJsonValueObject>(MsgObj));
			Count++;
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total"), static_cast<double>(LogProvider.GetMessageCount()));
	Result->SetNumberField(TEXT("shown"), Count);
	Result->SetArrayField(TEXT("messages"), MessagesArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// memory (LLM memory tag tracking)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetMemory(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IMemoryProvider* MemProvider =
		CurrentSession->ReadProvider<TraceServices::IMemoryProvider>(
			TraceServices::GetMemoryProviderName());

	if (!MemProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Memory provider not available. Record with channels including 'memtag'."));
	}

	// Memory provider uses FProviderLock — must acquire provider-level read scope
	MemProvider->BeginRead();

	if (!MemProvider->IsInitialized())
	{
		MemProvider->EndRead();
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Memory provider not initialized (no memory data in trace)."));
	}

	FString TagFilter;
	Params->TryGetStringField(TEXT("filter"), TagFilter);

	int32 TrackerId = 0;
	if (Params->HasField(TEXT("tracker")))
	{
		TrackerId = static_cast<int32>(Params->GetNumberField(TEXT("tracker")));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Trackers
	TArray<TSharedPtr<FJsonValue>> TrackersArray;
	MemProvider->EnumerateTrackers(
		[&](const TraceServices::FMemoryTrackerInfo& Tracker)
		{
			TSharedPtr<FJsonObject> TObj = MakeShared<FJsonObject>();
			TObj->SetNumberField(TEXT("id"), Tracker.Id);
			TObj->SetStringField(TEXT("name"), Tracker.Name);
			TrackersArray.Add(MakeShared<FJsonValueObject>(TObj));
		});
	Result->SetArrayField(TEXT("trackers"), TrackersArray);

	// Tags with latest values
	double TraceEnd = CurrentSession->GetDurationSeconds();
	double SampleStart = FMath::Max(0.0, TraceEnd - 1.0);

	TArray<TSharedPtr<FJsonValue>> TagsArray;
	MemProvider->EnumerateTags(
		[&](const TraceServices::FMemoryTagInfo& Tag)
		{
			FString TagName = Tag.Name;
			if (!TagFilter.IsEmpty() && !TagName.Contains(TagFilter))
			{
				return;
			}

			TSharedPtr<FJsonObject> TagObj = MakeShared<FJsonObject>();
			TagObj->SetStringField(TEXT("name"), TagName);

			int64 LatestValue = 0;
			bool bHasValue = false;

			MemProvider->EnumerateTagSamples(
				static_cast<TraceServices::FMemoryTrackerId>(TrackerId),
				Tag.Id, SampleStart, TraceEnd, true,
				[&](double Time, double Duration, const TraceServices::FMemoryTagSample& Sample)
				{
					LatestValue = Sample.Value;
					bHasValue = true;
				});

			if (bHasValue)
			{
				TagObj->SetNumberField(TEXT("mb"),
					SafeDouble(static_cast<double>(LatestValue) / (1024.0 * 1024.0)));
			}

			TagsArray.Add(MakeShared<FJsonValueObject>(TagObj));
		});

	Result->SetNumberField(TEXT("tag_count"), TagsArray.Num());
	Result->SetArrayField(TEXT("tags"), TagsArray);

	MemProvider->EndRead();
	return Result;
}

// ────────────────────────────────────────────────────────────
// regions
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetRegions(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IRegionProvider* RegionProviderPtr =
		CurrentSession->ReadProvider<TraceServices::IRegionProvider>(
			TraceServices::GetRegionProviderName());

	if (!RegionProviderPtr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Region provider not available. Record with channels including 'region'."));
	}

	// Region provider uses FProviderLock — must acquire provider-level read scope
	RegionProviderPtr->BeginRead();

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	FString CategoryFilter;
	Params->TryGetStringField(TEXT("filter"), CategoryFilter);

	int32 MaxResults = 100;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 1000);
	}

	TArray<TSharedPtr<FJsonValue>> RegionsArray;
	int32 Count = 0;

	const TraceServices::IRegionTimeline& DefaultTimeline = RegionProviderPtr->GetDefaultTimeline();
	DefaultTimeline.EnumerateRegions(IntervalStart, IntervalEnd,
		[&](const TraceServices::FTimeRegion& Region) -> bool
		{
			if (Count >= MaxResults)
			{
				return false;
			}

			FString RegionName;
			FString RegionCategory;

			if (Region.Timer)
			{
				if (Region.Timer->Name)
				{
					RegionName = Region.Timer->Name;
				}
				if (Region.Timer->Category && Region.Timer->Category->Name)
				{
					RegionCategory = Region.Timer->Category->Name;
				}
			}

			if (!CategoryFilter.IsEmpty() && !RegionCategory.Contains(CategoryFilter)
				&& !RegionName.Contains(CategoryFilter))
			{
				return true;
			}

			TSharedPtr<FJsonObject> RegObj = MakeShared<FJsonObject>();
			RegObj->SetStringField(TEXT("name"), RegionName);
			RegObj->SetStringField(TEXT("category"), RegionCategory);
			RegObj->SetNumberField(TEXT("ms"),
				SafeDouble((Region.EndTime - Region.BeginTime) * 1000.0));

			RegionsArray.Add(MakeShared<FJsonValueObject>(RegObj));
			Count++;
			return true;
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total"), static_cast<double>(RegionProviderPtr->GetRegionCount()));
	Result->SetNumberField(TEXT("shown"), Count);
	Result->SetArrayField(TEXT("regions"), RegionsArray);

	RegionProviderPtr->EndRead();
	return Result;
}

// ────────────────────────────────────────────────────────────
// bookmarks
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetBookmarks(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IBookmarkProvider* BookmarkProviderPtr =
		CurrentSession->ReadProvider<TraceServices::IBookmarkProvider>(
			TraceServices::GetBookmarkProviderName());

	if (!BookmarkProviderPtr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Bookmark provider not available. Record with channels including 'bookmark'."));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	FString TextFilter;
	Params->TryGetStringField(TEXT("filter"), TextFilter);

	int32 MaxResults = 100;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 1000);
	}

	TArray<TSharedPtr<FJsonValue>> BookmarksArray;
	int32 Count = 0;

	BookmarkProviderPtr->EnumerateBookmarks(IntervalStart, IntervalEnd,
		[&](const TraceServices::FBookmark& Bookmark)
		{
			if (Count >= MaxResults)
			{
				return;
			}

			FString Text = Bookmark.Text ? FString(Bookmark.Text) : TEXT("");

			if (!TextFilter.IsEmpty() && !Text.Contains(TextFilter))
			{
				return;
			}

			TSharedPtr<FJsonObject> BmObj = MakeShared<FJsonObject>();
			BmObj->SetNumberField(TEXT("time"), Bookmark.Time);
			BmObj->SetStringField(TEXT("text"), Text);

			BookmarksArray.Add(MakeShared<FJsonValueObject>(BmObj));
			Count++;
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total"),
		static_cast<double>(BookmarkProviderPtr->GetBookmarkCount()));
	Result->SetNumberField(TEXT("shown"), Count);
	Result->SetArrayField(TEXT("bookmarks"), BookmarksArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// session (diagnostics / session metadata)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetSession(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IDiagnosticsProvider* DiagProvider =
		TraceServices::ReadDiagnosticsProvider(*CurrentSession);

	if (!DiagProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Diagnostics provider not available in this trace."));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	if (DiagProvider->IsSessionInfoAvailable())
	{
		const TraceServices::FSessionInfo& Info = DiagProvider->GetSessionInfo();

		Result->SetStringField(TEXT("platform"), Info.Platform);
		Result->SetStringField(TEXT("app_name"), Info.AppName);
		Result->SetStringField(TEXT("project"), Info.ProjectName);
		Result->SetStringField(TEXT("command_line"), Info.CommandLine);
		Result->SetStringField(TEXT("branch"), Info.Branch);
		Result->SetStringField(TEXT("build_version"), Info.BuildVersion);
		Result->SetNumberField(TEXT("changelist"), static_cast<double>(Info.Changelist));
		Result->SetStringField(TEXT("config"), LexToString(Info.ConfigurationType));
		Result->SetStringField(TEXT("target"), LexToString(Info.TargetType));

		Result->SetNumberField(TEXT("duration_seconds"), CurrentSession->GetDurationSeconds());
		Result->SetStringField(TEXT("trace_path"), LoadedTracePath);
	}
	else
	{
		Result->SetStringField(TEXT("message"),
			TEXT("Session info not available (trace may not include diagnostics channel)."));
		Result->SetNumberField(TEXT("duration_seconds"), CurrentSession->GetDurationSeconds());
		Result->SetStringField(TEXT("trace_path"), LoadedTracePath);
	}

	return Result;
}

// ────────────────────────────────────────────────────────────
// modules (loaded modules and symbol resolution stats)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetModules(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IModuleProvider* ModProvider =
		TraceServices::ReadModuleProvider(*CurrentSession);

	if (!ModProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Module provider not available in this trace."));
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	int32 MaxResults = 50;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 500);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_modules"), ModProvider->GetNumModules());

	// Symbol resolution stats
	TraceServices::IModuleProvider::FStats Stats;
	ModProvider->GetStats(&Stats);

	TSharedPtr<FJsonObject> SymStats = MakeShared<FJsonObject>();
	SymStats->SetNumberField(TEXT("modules_discovered"), Stats.ModulesDiscovered);
	SymStats->SetNumberField(TEXT("modules_loaded"), Stats.ModulesLoaded);
	SymStats->SetNumberField(TEXT("modules_failed"), Stats.ModulesFailed);
	SymStats->SetNumberField(TEXT("symbols_discovered"), Stats.SymbolsDiscovered);
	SymStats->SetNumberField(TEXT("symbols_resolved"), Stats.SymbolsResolved);
	SymStats->SetNumberField(TEXT("symbols_failed"), Stats.SymbolsFailed);
	SymStats->SetBoolField(TEXT("finished_resolving"), ModProvider->HasFinishedResolving());
	Result->SetObjectField(TEXT("symbol_stats"), SymStats);

	// Module list
	TArray<TSharedPtr<FJsonValue>> ModulesArray;
	int32 Count = 0;

	ModProvider->EnumerateModules(0,
		[&](const TraceServices::FModule& Module)
		{
			if (Count >= MaxResults)
			{
				return;
			}

			FString ModName(Module.Name);
			if (!NameFilter.IsEmpty() && !ModName.Contains(NameFilter))
			{
				return;
			}

			TSharedPtr<FJsonObject> ModObj = MakeShared<FJsonObject>();
			ModObj->SetStringField(TEXT("name"), ModName);
			ModObj->SetStringField(TEXT("full_name"), FString(Module.FullName));
			ModObj->SetNumberField(TEXT("base"), static_cast<double>(Module.Base));
			ModObj->SetNumberField(TEXT("size"), static_cast<double>(Module.Size));

			TraceServices::EModuleStatus ModStatus = Module.Status.load(std::memory_order_relaxed);
			FString StatusStr = TraceServices::ModuleStatusToString(ModStatus);
			ModObj->SetStringField(TEXT("status"), StatusStr);

			// Per-module symbol stats
			uint32 Resolved = Module.Stats.Resolved.load(std::memory_order_relaxed);
			uint32 Failed = Module.Stats.Failed.load(std::memory_order_relaxed);
			uint32 Discovered = Module.Stats.Discovered.load(std::memory_order_relaxed);
			if (Discovered > 0)
			{
				ModObj->SetNumberField(TEXT("symbols_discovered"), Discovered);
				ModObj->SetNumberField(TEXT("symbols_resolved"), Resolved);
				ModObj->SetNumberField(TEXT("symbols_failed"), Failed);
			}

			ModulesArray.Add(MakeShared<FJsonValueObject>(ModObj));
			Count++;
		});

	Result->SetNumberField(TEXT("shown"), Count);
	Result->SetArrayField(TEXT("modules"), ModulesArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// file_io (file I/O activity)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetFileIO(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IFileActivityProvider* FileProvider =
		TraceServices::ReadFileActivityProvider(*CurrentSession);

	if (!FileProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("File activity provider not available. Record with channels including 'loadtime'."));
	}

	FString PathFilter;
	Params->TryGetStringField(TEXT("filter"), PathFilter);

	int32 MaxResults = 50;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 500);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// File activity table — aggregated file-level stats
	const TraceServices::ITable<TraceServices::FFileActivity>& ActivityTable =
		FileProvider->GetFileActivityTable();

	struct FFileIOStats
	{
		FString Path;
		int32 ReadCount = 0;
		int64 TotalReadBytes = 0;
		int32 WriteCount = 0;
		int64 TotalWriteBytes = 0;
		int32 OpenCount = 0;
		double TotalReadMs = 0.0;
		double TotalWriteMs = 0.0;
		bool bHadFailure = false;
	};

	TMap<int32, FFileIOStats> FileStats;

	auto* Reader = ActivityTable.CreateReader();
	while (Reader->IsValid())
	{
		const TraceServices::FFileActivity* Row = Reader->GetCurrentRow();
		if (Row && Row->File)
		{
			FString FilePath = Row->File->Path ? FString(Row->File->Path) : TEXT("<unknown>");

			if (!PathFilter.IsEmpty() && !FilePath.Contains(PathFilter))
			{
				Reader->NextRow();
				continue;
			}

			FFileIOStats& Stats = FileStats.FindOrAdd(Row->File->Id);
			if (Stats.Path.IsEmpty())
			{
				Stats.Path = FilePath;
			}

			double DurationMs = SafeDouble((Row->EndTime - Row->StartTime) * 1000.0);

			switch (Row->ActivityType)
			{
			case TraceServices::FileActivityType_Read:
				Stats.ReadCount++;
				Stats.TotalReadBytes += Row->Size;
				Stats.TotalReadMs += DurationMs;
				break;
			case TraceServices::FileActivityType_Write:
				Stats.WriteCount++;
				Stats.TotalWriteBytes += Row->Size;
				Stats.TotalWriteMs += DurationMs;
				break;
			case TraceServices::FileActivityType_Open:
			case TraceServices::FileActivityType_ReOpen:
				Stats.OpenCount++;
				break;
			default:
				break;
			}

			if (Row->Failed)
			{
				Stats.bHadFailure = true;
			}
		}
		Reader->NextRow();
	}
	delete Reader;

	// Sort by total read time descending (most impactful first)
	TArray<FFileIOStats> SortedStats;
	for (auto& Pair : FileStats)
	{
		SortedStats.Add(MoveTemp(Pair.Value));
	}
	SortedStats.Sort([](const FFileIOStats& A, const FFileIOStats& B)
	{
		return (A.TotalReadMs + A.TotalWriteMs) > (B.TotalReadMs + B.TotalWriteMs);
	});

	TArray<TSharedPtr<FJsonValue>> FilesArray;
	int32 TotalReads = 0;
	int32 TotalWrites = 0;
	int64 TotalBytesRead = 0;
	int64 TotalBytesWritten = 0;

	for (int32 i = 0; i < SortedStats.Num(); ++i)
	{
		const FFileIOStats& Stats = SortedStats[i];
		TotalReads += Stats.ReadCount;
		TotalWrites += Stats.WriteCount;
		TotalBytesRead += Stats.TotalReadBytes;
		TotalBytesWritten += Stats.TotalWriteBytes;

		if (i < MaxResults)
		{
			TSharedPtr<FJsonObject> FileObj = MakeShared<FJsonObject>();
			FileObj->SetStringField(TEXT("path"), Stats.Path);

			if (Stats.ReadCount > 0)
			{
				FileObj->SetNumberField(TEXT("reads"), Stats.ReadCount);
				FileObj->SetNumberField(TEXT("read_mb"),
					SafeDouble(static_cast<double>(Stats.TotalReadBytes) / (1024.0 * 1024.0)));
				FileObj->SetNumberField(TEXT("read_ms"), SafeDouble(Stats.TotalReadMs));
			}
			if (Stats.WriteCount > 0)
			{
				FileObj->SetNumberField(TEXT("writes"), Stats.WriteCount);
				FileObj->SetNumberField(TEXT("write_mb"),
					SafeDouble(static_cast<double>(Stats.TotalWriteBytes) / (1024.0 * 1024.0)));
				FileObj->SetNumberField(TEXT("write_ms"), SafeDouble(Stats.TotalWriteMs));
			}
			if (Stats.bHadFailure)
			{
				FileObj->SetBoolField(TEXT("had_failure"), true);
			}

			FilesArray.Add(MakeShared<FJsonValueObject>(FileObj));
		}
	}

	// Summary
	Result->SetNumberField(TEXT("total_files"), FileStats.Num());
	Result->SetNumberField(TEXT("total_reads"), TotalReads);
	Result->SetNumberField(TEXT("total_writes"), TotalWrites);
	Result->SetNumberField(TEXT("total_read_mb"),
		SafeDouble(static_cast<double>(TotalBytesRead) / (1024.0 * 1024.0)));
	Result->SetNumberField(TEXT("total_write_mb"),
		SafeDouble(static_cast<double>(TotalBytesWritten) / (1024.0 * 1024.0)));
	Result->SetNumberField(TEXT("shown"), FilesArray.Num());
	Result->SetArrayField(TEXT("files"), FilesArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// tasks (task graph profiling)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetTasks(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ITasksProvider* TaskProvider =
		TraceServices::ReadTasksProvider(*CurrentSession);

	if (!TaskProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Tasks provider not available. The trace may not contain task profiling data."));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	int32 MaxResults = 50;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 500);
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_tasks"), static_cast<double>(TaskProvider->GetNumTasks()));

	// Collect tasks sorted by execution time
	struct FTaskEntry
	{
		FString DebugName;
		double StartedMs;
		double DurationMs;
		double WaitMs;
		int32 Prerequisites;
		int32 Subsequents;
		int32 NestedTasks;
	};

	TArray<FTaskEntry> AllTasks;

	TaskProvider->EnumerateTasks(IntervalStart, IntervalEnd,
		TraceServices::ETaskEnumerationOption::Alive,
		[&](const TraceServices::FTaskInfo& Task) -> TraceServices::ETaskEnumerationResult
		{
			FString TaskName = Task.DebugName ? FString(Task.DebugName) : TEXT("<unnamed>");

			if (!NameFilter.IsEmpty() && !TaskName.Contains(NameFilter))
			{
				return TraceServices::ETaskEnumerationResult::Continue;
			}

			double StartedSec = Task.StartedTimestamp;
			double FinishedSec = Task.FinishedTimestamp;
			double CreatedSec = Task.CreatedTimestamp;
			double LaunchedSec = Task.LaunchedTimestamp;

			double DurationMs = 0.0;
			if (StartedSec > 0.0 && FinishedSec > StartedSec)
			{
				DurationMs = (FinishedSec - StartedSec) * 1000.0;
			}

			double WaitMs = 0.0;
			if (CreatedSec > 0.0 && StartedSec > CreatedSec)
			{
				WaitMs = (StartedSec - CreatedSec) * 1000.0;
			}

			FTaskEntry Entry;
			Entry.DebugName = TaskName;
			Entry.StartedMs = StartedSec * 1000.0;
			Entry.DurationMs = DurationMs;
			Entry.WaitMs = WaitMs;
			Entry.Prerequisites = Task.Prerequisites.Num();
			Entry.Subsequents = Task.Subsequents.Num();
			Entry.NestedTasks = Task.NestedTasks.Num();

			AllTasks.Add(MoveTemp(Entry));
			return TraceServices::ETaskEnumerationResult::Continue;
		});

	// Sort by duration descending (slowest first)
	AllTasks.Sort([](const FTaskEntry& A, const FTaskEntry& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	TArray<TSharedPtr<FJsonValue>> TasksArray;
	int32 Limit = FMath::Min(AllTasks.Num(), MaxResults);
	double TotalDurationMs = 0.0;
	double TotalWaitMs = 0.0;

	for (int32 i = 0; i < AllTasks.Num(); ++i)
	{
		TotalDurationMs += AllTasks[i].DurationMs;
		TotalWaitMs += AllTasks[i].WaitMs;

		if (i < Limit)
		{
			TSharedPtr<FJsonObject> TaskObj = MakeShared<FJsonObject>();
			TaskObj->SetStringField(TEXT("name"), AllTasks[i].DebugName);
			TaskObj->SetNumberField(TEXT("duration_ms"), SafeDouble(AllTasks[i].DurationMs));

			if (AllTasks[i].WaitMs > 0.01)
			{
				TaskObj->SetNumberField(TEXT("wait_ms"), SafeDouble(AllTasks[i].WaitMs));
			}
			if (AllTasks[i].Prerequisites > 0)
			{
				TaskObj->SetNumberField(TEXT("prerequisites"), AllTasks[i].Prerequisites);
			}
			if (AllTasks[i].Subsequents > 0)
			{
				TaskObj->SetNumberField(TEXT("subsequents"), AllTasks[i].Subsequents);
			}
			if (AllTasks[i].NestedTasks > 0)
			{
				TaskObj->SetNumberField(TEXT("nested"), AllTasks[i].NestedTasks);
			}

			TasksArray.Add(MakeShared<FJsonValueObject>(TaskObj));
		}
	}

	Result->SetNumberField(TEXT("tasks_in_range"), AllTasks.Num());
	Result->SetNumberField(TEXT("total_execution_ms"), SafeDouble(TotalDurationMs));
	Result->SetNumberField(TEXT("total_wait_ms"), SafeDouble(TotalWaitMs));
	Result->SetNumberField(TEXT("shown"), TasksArray.Num());
	Result->SetArrayField(TEXT("tasks"), TasksArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// context_switches (CPU core scheduling)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetContextSwitches(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IContextSwitchesProvider* CSProvider =
		TraceServices::ReadContextSwitchesProvider(*CurrentSession);

	if (!CSProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Context switches provider not available. Requires admin/elevated trace with platform events."));
	}

	// Context switches provider uses FProviderLock — must acquire provider-level read scope
	CSProvider->BeginRead();

	if (!CSProvider->HasData())
	{
		CSProvider->EndRead();
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Context switches provider has no data. Requires admin/elevated trace with platform events."));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Enumerate CPU cores
	TArray<TSharedPtr<FJsonValue>> CoresArray;
	CSProvider->EnumerateCpuCores(
		[&](const TraceServices::FCpuCoreInfo& Core)
		{
			TSharedPtr<FJsonObject> CoreObj = MakeShared<FJsonObject>();
			CoreObj->SetNumberField(TEXT("core"), Core.CoreNumber);

			// Count events on this core in the time range
			int32 EventCount = 0;
			CSProvider->EnumerateCpuCoreEvents(Core.CoreNumber, IntervalStart, IntervalEnd,
				[&](const TraceServices::FCpuCoreEvent& Event)
					-> TraceServices::EContextSwitchEnumerationResult
				{
					EventCount++;
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				});

			CoreObj->SetNumberField(TEXT("events"), EventCount);
			CoresArray.Add(MakeShared<FJsonValueObject>(CoreObj));
		});

	Result->SetNumberField(TEXT("cpu_cores"), CoresArray.Num());
	Result->SetArrayField(TEXT("cores"), CoresArray);

	// Thread context switch analysis — find threads with most context switches
	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	struct FThreadCSInfo
	{
		FString Name;
		uint32 ThreadId;
		int32 SwitchCount;
	};

	TArray<FThreadCSInfo> ThreadStats;

	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& Thread)
		{
			int32 SwitchCount = 0;
			CSProvider->EnumerateContextSwitches(Thread.Id, IntervalStart, IntervalEnd,
				[&](const TraceServices::FContextSwitch& CS)
					-> TraceServices::EContextSwitchEnumerationResult
				{
					SwitchCount++;
					return TraceServices::EContextSwitchEnumerationResult::Continue;
				});

			if (SwitchCount > 0)
			{
				FThreadCSInfo Info;
				Info.Name = Thread.Name ? FString(Thread.Name) : TEXT("<unnamed>");
				Info.ThreadId = Thread.Id;
				Info.SwitchCount = SwitchCount;
				ThreadStats.Add(MoveTemp(Info));
			}
		});

	// Sort by switch count descending
	ThreadStats.Sort([](const FThreadCSInfo& A, const FThreadCSInfo& B)
	{
		return A.SwitchCount > B.SwitchCount;
	});

	TArray<TSharedPtr<FJsonValue>> ThreadsArray;
	int32 Limit = FMath::Min(ThreadStats.Num(), 30);
	for (int32 i = 0; i < Limit; ++i)
	{
		TSharedPtr<FJsonObject> TObj = MakeShared<FJsonObject>();
		TObj->SetStringField(TEXT("thread"), ThreadStats[i].Name);
		TObj->SetNumberField(TEXT("context_switches"), ThreadStats[i].SwitchCount);
		ThreadsArray.Add(MakeShared<FJsonValueObject>(TObj));
	}

	Result->SetNumberField(TEXT("threads_with_switches"), ThreadStats.Num());
	Result->SetArrayField(TEXT("thread_switches"), ThreadsArray);

	CSProvider->EndRead();
	return Result;
}

// ────────────────────────────────────────────────────────────
// allocations (memory allocation timeline)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetAllocations(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IAllocationsProvider* AllocProvider =
		TraceServices::ReadAllocationsProvider(*CurrentSession);

	if (!AllocProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Allocations provider not available. Record with channels including 'memalloc'."));
	}

	// Allocations provider uses FProviderLock — must acquire provider-level read scope
	AllocProvider->BeginRead();

	if (!AllocProvider->IsInitialized())
	{
		AllocProvider->EndRead();
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Allocations provider not initialized (no allocation data in trace)."));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	int32 NumPoints = AllocProvider->GetTimelineNumPoints();
	Result->SetNumberField(TEXT("timeline_points"), NumPoints);
	Result->SetBoolField(TEXT("has_alloc_events"), AllocProvider->HasAllocationEvents());
	Result->SetBoolField(TEXT("has_swap_events"), AllocProvider->HasSwapOpEvents());
	Result->SetNumberField(TEXT("page_size"), static_cast<double>(AllocProvider->GetPlatformPageSize()));

	// Get timeline index range for our interval
	int32 StartIdx = 0;
	int32 EndIdx = NumPoints;
	AllocProvider->GetTimelineIndexRange(IntervalStart, IntervalEnd, StartIdx, EndIdx);

	// Peak memory
	uint64 PeakMemory = 0;
	uint64 MinMemory = UINT64_MAX;
	AllocProvider->EnumerateTimeline(
		TraceServices::IAllocationsProvider::ETimelineU64::MaxTotalAllocatedMemory,
		StartIdx, EndIdx,
		[&](double Time, double Duration, uint64 Value)
		{
			if (Value > PeakMemory)
			{
				PeakMemory = Value;
			}
		});

	AllocProvider->EnumerateTimeline(
		TraceServices::IAllocationsProvider::ETimelineU64::MinTotalAllocatedMemory,
		StartIdx, EndIdx,
		[&](double Time, double Duration, uint64 Value)
		{
			if (Value < MinMemory)
			{
				MinMemory = Value;
			}
		});

	if (MinMemory == UINT64_MAX)
	{
		MinMemory = 0;
	}

	Result->SetNumberField(TEXT("peak_memory_mb"),
		SafeDouble(static_cast<double>(PeakMemory) / (1024.0 * 1024.0)));
	Result->SetNumberField(TEXT("min_memory_mb"),
		SafeDouble(static_cast<double>(MinMemory) / (1024.0 * 1024.0)));
	Result->SetNumberField(TEXT("memory_growth_mb"),
		SafeDouble(static_cast<double>(PeakMemory - MinMemory) / (1024.0 * 1024.0)));

	// Peak live allocation count
	uint32 PeakLiveAllocs = 0;
	AllocProvider->EnumerateTimeline(
		TraceServices::IAllocationsProvider::ETimelineU32::MaxLiveAllocations,
		StartIdx, EndIdx,
		[&](double Time, double Duration, uint32 Value)
		{
			if (Value > PeakLiveAllocs)
			{
				PeakLiveAllocs = Value;
			}
		});
	Result->SetNumberField(TEXT("peak_live_allocations"), static_cast<double>(PeakLiveAllocs));

	// Alloc/free event counts
	uint32 TotalAllocEvents = 0;
	uint32 TotalFreeEvents = 0;
	AllocProvider->EnumerateTimeline(
		TraceServices::IAllocationsProvider::ETimelineU32::AllocEvents,
		StartIdx, EndIdx,
		[&](double Time, double Duration, uint32 Value)
		{
			TotalAllocEvents += Value;
		});

	AllocProvider->EnumerateTimeline(
		TraceServices::IAllocationsProvider::ETimelineU32::FreeEvents,
		StartIdx, EndIdx,
		[&](double Time, double Duration, uint32 Value)
		{
			TotalFreeEvents += Value;
		});

	Result->SetNumberField(TEXT("alloc_events"), static_cast<double>(TotalAllocEvents));
	Result->SetNumberField(TEXT("free_events"), static_cast<double>(TotalFreeEvents));
	Result->SetNumberField(TEXT("net_allocations"),
		static_cast<double>(static_cast<int64>(TotalAllocEvents) - static_cast<int64>(TotalFreeEvents)));

	// Swap memory (if available)
	if (AllocProvider->HasSwapOpEvents())
	{
		uint64 PeakSwap = 0;
		AllocProvider->EnumerateTimeline(
			TraceServices::IAllocationsProvider::ETimelineU64::MaxTotalSwapMemory,
			StartIdx, EndIdx,
			[&](double Time, double Duration, uint64 Value)
			{
				if (Value > PeakSwap)
				{
					PeakSwap = Value;
				}
			});

		Result->SetNumberField(TEXT("peak_swap_mb"),
			SafeDouble(static_cast<double>(PeakSwap) / (1024.0 * 1024.0)));
	}

	// Heap info
	TArray<TSharedPtr<FJsonValue>> HeapsArray;
	AllocProvider->EnumerateRootHeaps(
		[&](HeapId Id,
			const TraceServices::IAllocationsProvider::FHeapSpec& Heap)
		{
			TSharedPtr<FJsonObject> HeapObj = MakeShared<FJsonObject>();
			HeapObj->SetNumberField(TEXT("id"), static_cast<double>(Id));
			HeapObj->SetStringField(TEXT("name"), Heap.Name);
			HeapsArray.Add(MakeShared<FJsonValueObject>(HeapObj));
		});

	if (HeapsArray.Num() > 0)
	{
		Result->SetArrayField(TEXT("heaps"), HeapsArray);
	}

	// Memory tags (allocation tags)
	TArray<TSharedPtr<FJsonValue>> TagsArray;
	AllocProvider->EnumerateTags(
		[&](const TCHAR* FullPath, const TCHAR* Name,
			TraceServices::TagIdType Id,
			TraceServices::TagIdType ParentId)
		{
			// Only show leaf/named tags, not all intermediates
			if (Name && FCString::Strlen(Name) > 0)
			{
				TSharedPtr<FJsonObject> TagObj = MakeShared<FJsonObject>();
				TagObj->SetStringField(TEXT("name"), FString(Name));
				TagObj->SetStringField(TEXT("path"), FString(FullPath));
				TagsArray.Add(MakeShared<FJsonValueObject>(TagObj));
			}
		});

	if (TagsArray.Num() > 0)
	{
		Result->SetNumberField(TEXT("tag_count"), TagsArray.Num());
		// Limit to first 50 tags to avoid huge output
		if (TagsArray.Num() > 50)
		{
			TagsArray.SetNum(50);
			Result->SetBoolField(TEXT("tags_truncated"), true);
		}
		Result->SetArrayField(TEXT("tags"), TagsArray);
	}

	AllocProvider->EndRead();
	return Result;
}

// ────────────────────────────────────────────────────────────
// stack_samples (CPU sampling profiler)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetStackSamples(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IStackSamplesProvider* SamplesProvider =
		TraceServices::ReadStackSamplesProvider(*CurrentSession);

	if (!SamplesProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Stack samples provider not available. The trace may not contain CPU sampling data."));
	}

	// Stack samples provider uses FProviderLock — must acquire provider-level read scope
	SamplesProvider->BeginRead();

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	int32 MaxResults = 50;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 500);
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_frames"), SamplesProvider->GetStackFrameCount());
	Result->SetNumberField(TEXT("total_threads"), SamplesProvider->GetThreadCount());
	Result->SetNumberField(TEXT("total_timelines"), SamplesProvider->GetTimelineCount());

	// Thread info
	TArray<TSharedPtr<FJsonValue>> ThreadsArray;
	SamplesProvider->EnumerateThreads(
		[&](const TraceServices::FStackSampleThread& Thread)
		{
			TSharedPtr<FJsonObject> TObj = MakeShared<FJsonObject>();
			TObj->SetStringField(TEXT("name"), FString(Thread.Name));
			TObj->SetNumberField(TEXT("system_thread_id"), static_cast<double>(Thread.SystemThreadId));
			TObj->SetBoolField(TEXT("has_timeline"), Thread.Timeline != nullptr);
			ThreadsArray.Add(MakeShared<FJsonValueObject>(TObj));
		});

	Result->SetArrayField(TEXT("threads"), ThreadsArray);

	// Sampled stack frames — aggregate by function name for a "flat profile"
	struct FSampleAggregate
	{
		FString FunctionName;
		FString ModuleName;
		int32 SampleCount = 0;
	};

	TMap<FString, FSampleAggregate> FunctionSamples;
	int32 TotalSamples = 0;

	SamplesProvider->EnumerateStackFrames(
		[&](const TraceServices::FStackSampleFrame& Frame)
		{
			TotalSamples++;

			FString FuncName = TEXT("<unresolved>");
			FString ModName = TEXT("");

			if (Frame.Symbol)
			{
				auto SymResult = Frame.Symbol->GetResult();
				if (SymResult == TraceServices::ESymbolQueryResult::OK)
				{
					FuncName = Frame.Symbol->Name ? FString(Frame.Symbol->Name) : TEXT("<unnamed>");
					ModName = Frame.Symbol->Module ? FString(Frame.Symbol->Module) : TEXT("");
				}
			}

			if (!NameFilter.IsEmpty() && !FuncName.Contains(NameFilter) && !ModName.Contains(NameFilter))
			{
				return;
			}

			FSampleAggregate& Agg = FunctionSamples.FindOrAdd(FuncName);
			if (Agg.FunctionName.IsEmpty())
			{
				Agg.FunctionName = FuncName;
				Agg.ModuleName = ModName;
			}
			Agg.SampleCount++;
		});

	// Sort by sample count
	TArray<FSampleAggregate> SortedSamples;
	for (auto& Pair : FunctionSamples)
	{
		SortedSamples.Add(MoveTemp(Pair.Value));
	}
	SortedSamples.Sort([](const FSampleAggregate& A, const FSampleAggregate& B)
	{
		return A.SampleCount > B.SampleCount;
	});

	TArray<TSharedPtr<FJsonValue>> SamplesArray;
	int32 Limit = FMath::Min(SortedSamples.Num(), MaxResults);
	for (int32 i = 0; i < Limit; ++i)
	{
		TSharedPtr<FJsonObject> SObj = MakeShared<FJsonObject>();
		SObj->SetStringField(TEXT("function"), SortedSamples[i].FunctionName);
		if (!SortedSamples[i].ModuleName.IsEmpty())
		{
			SObj->SetStringField(TEXT("module"), SortedSamples[i].ModuleName);
		}
		SObj->SetNumberField(TEXT("samples"), SortedSamples[i].SampleCount);

		if (TotalSamples > 0)
		{
			SObj->SetNumberField(TEXT("pct"),
				SafeDouble(static_cast<double>(SortedSamples[i].SampleCount) /
					static_cast<double>(TotalSamples) * 100.0));
		}

		SamplesArray.Add(MakeShared<FJsonValueObject>(SObj));
	}

	Result->SetNumberField(TEXT("unique_functions"), SortedSamples.Num());
	Result->SetNumberField(TEXT("total_samples"), TotalSamples);
	Result->SetNumberField(TEXT("shown"), SamplesArray.Num());
	Result->SetArrayField(TEXT("hotspots"), SamplesArray);

	SamplesProvider->EndRead();
	return Result;
}

// ────────────────────────────────────────────────────────────
// screenshots (screenshot metadata from trace)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetScreenshots(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IScreenshotProvider* ScreenshotProvider = nullptr;

	// Screenshot provider uses a reference return, not pointer
	try
	{
		ScreenshotProvider = &TraceServices::ReadScreenshotProvider(*CurrentSession);
	}
	catch (...)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Screenshot provider not available. Record with channels including 'screenshot'."));
	}

	if (!ScreenshotProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Screenshot provider not available."));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Try to get screenshots by iterating IDs (provider has GetScreenshot(uint32 Id))
	TArray<TSharedPtr<FJsonValue>> ScreenshotsArray;
	int32 MaxAttempts = 1000;

	for (uint32 Id = 0; Id < static_cast<uint32>(MaxAttempts); ++Id)
	{
		TSharedPtr<const TraceServices::FScreenshot> Shot = ScreenshotProvider->GetScreenshot(Id);
		if (!Shot.IsValid())
		{
			continue;
		}

		TSharedPtr<FJsonObject> ShotObj = MakeShared<FJsonObject>();
		ShotObj->SetNumberField(TEXT("id"), static_cast<double>(Shot->Id));
		ShotObj->SetStringField(TEXT("name"), FString(Shot->Name));
		ShotObj->SetNumberField(TEXT("timestamp"), Shot->Timestamp);
		ShotObj->SetNumberField(TEXT("width"), Shot->Width);
		ShotObj->SetNumberField(TEXT("height"), Shot->Height);
		ShotObj->SetNumberField(TEXT("size_kb"),
			SafeDouble(static_cast<double>(Shot->Size) / 1024.0));

		ScreenshotsArray.Add(MakeShared<FJsonValueObject>(ShotObj));
	}

	Result->SetNumberField(TEXT("count"), ScreenshotsArray.Num());
	Result->SetArrayField(TEXT("screenshots"), ScreenshotsArray);

	if (ScreenshotsArray.Num() == 0)
	{
		Result->SetStringField(TEXT("message"),
			TEXT("No screenshots found. Screenshots are captured when using 'screenshot' trace channel."));
	}

	return Result;
}
