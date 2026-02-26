#include "Commands/EpicUnrealMCPProfilingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPProfilingUtils.h"

#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/NetProfiler.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/Log.h"
#include "TraceServices/Model/Memory.h"
#include "TraceServices/Model/Regions.h"
#include "TraceServices/Model/Bookmarks.h"
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

	if (!MemProvider->IsInitialized())
	{
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
