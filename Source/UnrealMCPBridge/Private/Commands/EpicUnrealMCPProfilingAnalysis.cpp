#include "Commands/EpicUnrealMCPProfilingCommands.h"
#include "Commands/EpicUnrealMCPCommonUtils.h"
#include "EpicUnrealMCPProfilingUtils.h"

#include "TraceServices/Model/AnalysisSession.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/Counters.h"
#include "TraceServices/Containers/Tables.h"

// ════════════════════════════════════════════════════════════
//  SMART ANALYSIS QUERIES
//  These do the heavy lifting in C++ and return compact results.
// ════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────
// bottlenecks — Auto-categorize a frame's timing into buckets
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetBottlenecks(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	if (!Params->HasField(TEXT("frame_index")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'frame_index'"));
	}

	uint64 FrameIndex = static_cast<uint64>(Params->GetNumberField(TEXT("frame_index")));

	double TargetFps = 60.0;
	if (Params->HasField(TEXT("target_fps")))
	{
		TargetFps = FMath::Max(1.0, Params->GetNumberField(TEXT("target_fps")));
	}

	FString ThreadFilter = TEXT("GameThread");
	Params->TryGetStringField(TEXT("thread"), ThreadFilter);

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);

	const TraceServices::FFrame* Frame = FrameProvider.GetFrame(
		ETraceFrameType::TraceFrameType_Game, FrameIndex);

	if (!Frame)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Frame %llu not found"), FrameIndex));
	}

	double FrameStart = Frame->StartTime;
	double FrameEnd = Frame->EndTime;
	double FrameMs = (FrameEnd - FrameStart) * 1000.0;

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	// Category accumulators
	struct FCategoryBucket
	{
		double TotalMs = 0.0;
		FString TopEventName;
		double TopEventMs = 0.0;
		int32 EventCount = 0;
	};

	FCategoryBucket Buckets[static_cast<int32>(EProfilingCategory::MAX)];

	// Find the target thread and enumerate depth-0 and depth-1 events
	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
			if (!ThreadName.Contains(ThreadFilter))
			{
				return;
			}

			uint32 TimelineIndex = 0;
			if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
			{
				return;
			}

			TimingProvider->ReadTimeline(TimelineIndex,
				[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					// Auto-detect bucketing depth: find the shallowest depth
					// that has multiple events (skip root wrappers like
					// FEngineLoop::Tick -> Frame that are just single-event chains)
					int32 EventCountByDepth[6] = {};
					Timeline.EnumerateEvents(FrameStart, FrameEnd,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							if (Depth < 6)
							{
								double DurMs = (EndTime - StartTime) * 1000.0;
								if (std::isfinite(DurMs) && DurMs > 0.1)
								{
									EventCountByDepth[Depth]++;
								}
							}
							return Depth < 5 ? TraceServices::EEventEnumerate::Continue : TraceServices::EEventEnumerate::Continue;
						});

					uint32 BucketDepth = 0;
					for (int32 d = 0; d < 6; ++d)
					{
						if (EventCountByDepth[d] >= 2)
						{
							BucketDepth = d;
							break;
						}
						BucketDepth = d + 1;
					}
					BucketDepth = FMath::Min(BucketDepth, (uint32)5);

					// Now bucket events at the detected depth
					Timeline.EnumerateEvents(FrameStart, FrameEnd,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							if (Depth != BucketDepth)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
							if (!std::isfinite(DurMs) || DurMs < 0.01)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							FString TimerName;
							TimingProvider->ReadTimers(
								[&](const TraceServices::ITimingProfilerTimerReader& Reader)
								{
									const TraceServices::FTimingProfilerTimer* Timer =
										Reader.GetTimer(Event.TimerIndex);
									if (Timer && Timer->Name)
									{
										TimerName = Timer->Name;
									}
								});

							if (TimerName.IsEmpty())
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							EProfilingCategory Cat = CategorizeTimerName(TimerName);
							int32 CatIdx = static_cast<int32>(Cat);
							FCategoryBucket& Bucket = Buckets[CatIdx];

							Bucket.TotalMs += DurMs;
							Bucket.EventCount++;

							if (DurMs > Bucket.TopEventMs)
							{
								Bucket.TopEventMs = DurMs;
								Bucket.TopEventName = TimerName;
							}

							return TraceServices::EEventEnumerate::Continue;
						});
				});
		});

	// Sort categories by total time
	struct FSortedCategory
	{
		EProfilingCategory Category;
		double TotalMs;
		FString TopEvent;
		double TopMs;
		int32 Count;
	};

	TArray<FSortedCategory> SortedCats;
	for (int32 i = 0; i < static_cast<int32>(EProfilingCategory::MAX); ++i)
	{
		if (Buckets[i].TotalMs > 0.01)
		{
			SortedCats.Add({
				static_cast<EProfilingCategory>(i),
				Buckets[i].TotalMs,
				Buckets[i].TopEventName,
				Buckets[i].TopEventMs,
				Buckets[i].EventCount
			});
		}
	}

	SortedCats.Sort([](const FSortedCategory& A, const FSortedCategory& B)
	{
		return A.TotalMs > B.TotalMs;
	});

	// Build compact result
	double TargetMs = 1000.0 / TargetFps;

	TArray<TSharedPtr<FJsonValue>> CategoriesArray;
	for (const FSortedCategory& Cat : SortedCats)
	{
		TSharedPtr<FJsonObject> CatObj = MakeShared<FJsonObject>();
		CatObj->SetStringField(TEXT("name"), GetCategoryName(Cat.Category));
		CatObj->SetNumberField(TEXT("total_ms"), SafeDouble(Cat.TotalMs));
		CatObj->SetNumberField(TEXT("pct"), SafeDouble(FrameMs > 0.0 ? (Cat.TotalMs / FrameMs) * 100.0 : 0.0));
		CatObj->SetNumberField(TEXT("count"), Cat.Count);
		CatObj->SetStringField(TEXT("top_event"), Cat.TopEvent);
		CatObj->SetNumberField(TEXT("top_ms"), SafeDouble(Cat.TopMs));
		CategoriesArray.Add(MakeShared<FJsonValueObject>(CatObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("frame_index"), static_cast<double>(FrameIndex));
	Result->SetNumberField(TEXT("frame_ms"), SafeDouble(FrameMs));
	Result->SetNumberField(TEXT("target_fps"), TargetFps);
	Result->SetNumberField(TEXT("target_ms"), SafeDouble(TargetMs));
	Result->SetNumberField(TEXT("over_budget_ms"), SafeDouble(FrameMs - TargetMs));
	Result->SetStringField(TEXT("thread"), ThreadFilter);
	Result->SetArrayField(TEXT("categories"), CategoriesArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// hotpath — Drill into a specific category or event
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetHotpath(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	if (!Params->HasField(TEXT("frame_index")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'frame_index'"));
	}

	uint64 FrameIndex = static_cast<uint64>(Params->GetNumberField(TEXT("frame_index")));

	FString CategoryName;
	Params->TryGetStringField(TEXT("category"), CategoryName);

	FString EventName;
	Params->TryGetStringField(TEXT("event_name"), EventName);

	if (CategoryName.IsEmpty() && EventName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Provide 'category' (Animation, Slate, Network, etc.) or 'event_name' to drill into."));
	}

	int32 MaxResults = 20;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 100);
	}

	int32 MaxDepth = 5;
	if (Params->HasField(TEXT("max_depth")))
	{
		MaxDepth = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("max_depth"))), 1, 20);
	}

	FString ThreadFilter = TEXT("GameThread");
	Params->TryGetStringField(TEXT("thread"), ThreadFilter);

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const TraceServices::FFrame* Frame = FrameProvider.GetFrame(
		ETraceFrameType::TraceFrameType_Game, FrameIndex);

	if (!Frame)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Frame %llu not found"), FrameIndex));
	}

	double FrameStart = Frame->StartTime;
	double FrameEnd = Frame->EndTime;

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	// Resolve category filter to enum if specified
	EProfilingCategory TargetCategory = EProfilingCategory::Other;
	bool bFilterByCategory = !CategoryName.IsEmpty();
	if (bFilterByCategory)
	{
		// Map category string to enum
		for (int32 i = 0; i < static_cast<int32>(EProfilingCategory::MAX); ++i)
		{
			if (CategoryName.Equals(GetCategoryName(static_cast<EProfilingCategory>(i)), ESearchCase::IgnoreCase))
			{
				TargetCategory = static_cast<EProfilingCategory>(i);
				break;
			}
		}
	}

	// Collect events
	struct FEventEntry
	{
		FString Name;
		double DurationMs;
		uint32 Depth;
	};

	TArray<FEventEntry> MatchedEvents;

	// For event_name drill-down, we need to find the parent event's time range
	double ParentStartTime = FrameStart;
	double ParentEndTime = FrameEnd;
	bool bFoundParent = false;

	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
			if (!ThreadName.Contains(ThreadFilter))
			{
				return;
			}

			uint32 TimelineIndex = 0;
			if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
			{
				return;
			}

			// If drilling into a specific event, first find its time range
			if (!EventName.IsEmpty() && !bFoundParent)
			{
				TimingProvider->ReadTimeline(TimelineIndex,
					[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						Timeline.EnumerateEvents(FrameStart, FrameEnd,
							[&](double StartTime, double EndTime, uint32 Depth,
								const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
							{
								if (bFoundParent)
								{
									return TraceServices::EEventEnumerate::Stop;
								}

								FString TimerName;
								TimingProvider->ReadTimers(
									[&](const TraceServices::ITimingProfilerTimerReader& Reader)
									{
										const TraceServices::FTimingProfilerTimer* Timer =
											Reader.GetTimer(Event.TimerIndex);
										if (Timer && Timer->Name)
										{
											TimerName = Timer->Name;
										}
									});

								if (TimerName.Contains(EventName))
								{
									ParentStartTime = StartTime;
									ParentEndTime = EndTime;
									bFoundParent = true;
									return TraceServices::EEventEnumerate::Stop;
								}
								return TraceServices::EEventEnumerate::Continue;
							});
					});
			}

			// Now collect matching events
			TimingProvider->ReadTimeline(TimelineIndex,
				[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					Timeline.EnumerateEvents(
						!EventName.IsEmpty() ? ParentStartTime : FrameStart,
						!EventName.IsEmpty() ? ParentEndTime : FrameEnd,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							if (static_cast<int32>(Depth) > MaxDepth)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
							if (!std::isfinite(DurMs) || DurMs < 0.01)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							FString TimerName;
							TimingProvider->ReadTimers(
								[&](const TraceServices::ITimingProfilerTimerReader& Reader)
								{
									const TraceServices::FTimingProfilerTimer* Timer =
										Reader.GetTimer(Event.TimerIndex);
									if (Timer && Timer->Name)
									{
										TimerName = Timer->Name;
									}
								});

							if (TimerName.IsEmpty())
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							// Filter by category or event name
							if (bFilterByCategory)
							{
								if (CategorizeTimerName(TimerName) != TargetCategory)
								{
									return TraceServices::EEventEnumerate::Continue;
								}
							}
							else if (!EventName.IsEmpty())
							{
								// Skip the parent event itself, only show children
								if (Depth == 0 && TimerName.Contains(EventName))
								{
									return TraceServices::EEventEnumerate::Continue;
								}
							}

							MatchedEvents.Add({ TimerName, DurMs, Depth });
							return TraceServices::EEventEnumerate::Continue;
						});
				});
		});

	// Sort by duration descending
	MatchedEvents.Sort([](const FEventEntry& A, const FEventEntry& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	// Trim to max results
	if (MatchedEvents.Num() > MaxResults)
	{
		MatchedEvents.SetNum(MaxResults);
	}

	// Build result
	TArray<TSharedPtr<FJsonValue>> EventsArray;
	for (const FEventEntry& Evt : MatchedEvents)
	{
		TSharedPtr<FJsonObject> EvtObj = MakeShared<FJsonObject>();
		EvtObj->SetStringField(TEXT("name"), Evt.Name);
		EvtObj->SetNumberField(TEXT("ms"), SafeDouble(Evt.DurationMs));
		EvtObj->SetNumberField(TEXT("depth"), Evt.Depth);
		EventsArray.Add(MakeShared<FJsonValueObject>(EvtObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("frame_index"), static_cast<double>(FrameIndex));
	Result->SetNumberField(TEXT("total_matched"), MatchedEvents.Num());

	if (bFilterByCategory)
	{
		Result->SetStringField(TEXT("category"), CategoryName);
	}
	if (!EventName.IsEmpty())
	{
		Result->SetStringField(TEXT("event_name"), EventName);
		Result->SetBoolField(TEXT("parent_found"), bFoundParent);
	}
	Result->SetArrayField(TEXT("events"), EventsArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// compare — Compare a frame against trace-wide median
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetCompare(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	if (!Params->HasField(TEXT("frame_index")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'frame_index'"));
	}

	uint64 FrameIndex = static_cast<uint64>(Params->GetNumberField(TEXT("frame_index")));

	int32 MaxResults = 15;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 50);
	}

	double MinDeviationPct = 50.0;
	if (Params->HasField(TEXT("min_deviation_pct")))
	{
		MinDeviationPct = Params->GetNumberField(TEXT("min_deviation_pct"));
	}

	FString ThreadFilter = TEXT("GameThread");
	Params->TryGetStringField(TEXT("thread"), ThreadFilter);

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const TraceServices::FFrame* Frame = FrameProvider.GetFrame(
		ETraceFrameType::TraceFrameType_Game, FrameIndex);

	if (!Frame)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Frame %llu not found"), FrameIndex));
	}

	double FrameStart = Frame->StartTime;
	double FrameEnd = Frame->EndTime;
	double FrameMs = (FrameEnd - FrameStart) * 1000.0;

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	// Step 1: Get trace-wide median times using engine aggregation
	TraceServices::FCreateAggregationParams AggParams;
	AggParams.IntervalStart = 0.0;
	AggParams.IntervalEnd = CurrentSession->GetDurationSeconds();
	AggParams.CpuThreadFilter = [](uint32) { return true; };
	AggParams.SortBy = TraceServices::FCreateAggregationParams::ESortBy::TotalInclusiveTime;
	AggParams.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::Descending;
	AggParams.TableEntryLimit = 0; // No limit - we need all timers for comparison

	TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggTable =
		TimingProvider->CreateAggregation(AggParams);

	if (!AggTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create aggregation"));
	}

	// Build a map of timer name -> median inclusive time
	TMap<FString, double> MedianTimesMap;
	TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>* AggReader =
		AggTable->CreateReader();

	while (AggReader->IsValid())
	{
		const TraceServices::FTimingProfilerAggregatedStats* Row = AggReader->GetCurrentRow();
		if (Row && Row->Timer && Row->Timer->Name)
		{
			FString Name(Row->Timer->Name);
			double MedianMs = SafeDouble(Row->MedianInclusiveTime * 1000.0);
			MedianTimesMap.Add(Name, MedianMs);
		}
		AggReader->NextRow();
	}

	delete AggReader;
	delete AggTable;

	// Step 2: Get frame-specific event times (depth 0 only for top-level comparison)
	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	struct FFrameEvent
	{
		FString Name;
		double FrameMs;
		double MedianMs;
		double DeviationPct;
	};

	TArray<FFrameEvent> FrameEvents;

	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
			if (!ThreadName.Contains(ThreadFilter))
			{
				return;
			}

			uint32 TimelineIndex = 0;
			if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
			{
				return;
			}

			// Accumulate depth-0 events by name (some events appear multiple times)
			TMap<FString, double> FrameEventTimes;

			TimingProvider->ReadTimeline(TimelineIndex,
				[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					Timeline.EnumerateEvents(FrameStart, FrameEnd,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							if (Depth > 1)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
							if (!std::isfinite(DurMs) || DurMs < 0.1)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							FString TimerName;
							TimingProvider->ReadTimers(
								[&](const TraceServices::ITimingProfilerTimerReader& Reader)
								{
									const TraceServices::FTimingProfilerTimer* Timer =
										Reader.GetTimer(Event.TimerIndex);
									if (Timer && Timer->Name)
									{
										TimerName = Timer->Name;
									}
								});

							if (!TimerName.IsEmpty())
							{
								double& Existing = FrameEventTimes.FindOrAdd(TimerName, 0.0);
								Existing += DurMs;
							}

							return TraceServices::EEventEnumerate::Continue;
						});
				});

			// Compare against trace-wide medians
			for (auto& Pair : FrameEventTimes)
			{
				double* MedianPtr = MedianTimesMap.Find(Pair.Key);
				if (!MedianPtr || *MedianPtr < 0.01)
				{
					continue;
				}

				double DevPct = ((Pair.Value - *MedianPtr) / *MedianPtr) * 100.0;

				if (FMath::Abs(DevPct) >= MinDeviationPct)
				{
					FrameEvents.Add({ Pair.Key, Pair.Value, *MedianPtr, DevPct });
				}
			}
		});

	// Sort by absolute deviation descending
	FrameEvents.Sort([](const FFrameEvent& A, const FFrameEvent& B)
	{
		return FMath::Abs(A.DeviationPct) > FMath::Abs(B.DeviationPct);
	});

	if (FrameEvents.Num() > MaxResults)
	{
		FrameEvents.SetNum(MaxResults);
	}

	// Build result
	// Get trace-wide median frame time
	double TraceTotalFrameTime = 0.0;
	TArray<double> AllFrameTimes;
	uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& F)
		{
			double Ft = F.EndTime - F.StartTime;
			if (std::isfinite(Ft) && Ft > 0.0)
			{
				AllFrameTimes.Add(Ft);
			}
		});

	double TraceMedianMs = 0.0;
	if (AllFrameTimes.Num() > 0)
	{
		AllFrameTimes.Sort();
		TraceMedianMs = AllFrameTimes[AllFrameTimes.Num() / 2] * 1000.0;
	}

	TArray<TSharedPtr<FJsonValue>> OutliersArray;
	for (const FFrameEvent& Evt : FrameEvents)
	{
		TSharedPtr<FJsonObject> EvtObj = MakeShared<FJsonObject>();
		EvtObj->SetStringField(TEXT("name"), Evt.Name);
		EvtObj->SetNumberField(TEXT("frame_ms"), SafeDouble(Evt.FrameMs));
		EvtObj->SetNumberField(TEXT("trace_median_ms"), SafeDouble(Evt.MedianMs));
		EvtObj->SetNumberField(TEXT("deviation_pct"), SafeDouble(Evt.DeviationPct));
		EvtObj->SetStringField(TEXT("category"),
			GetCategoryName(CategorizeTimerName(Evt.Name)));
		OutliersArray.Add(MakeShared<FJsonValueObject>(EvtObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("frame_index"), static_cast<double>(FrameIndex));
	Result->SetNumberField(TEXT("frame_ms"), SafeDouble(FrameMs));
	Result->SetNumberField(TEXT("trace_median_ms"), SafeDouble(TraceMedianMs));
	Result->SetNumberField(TEXT("frame_vs_median_pct"),
		SafeDouble(TraceMedianMs > 0.0 ? ((FrameMs - TraceMedianMs) / TraceMedianMs) * 100.0 : 0.0));
	Result->SetNumberField(TEXT("outlier_count"), FrameEvents.Num());
	Result->SetArrayField(TEXT("outliers"), OutliersArray);

	return Result;
}

// ════════════════════════════════════════════════════════════
//  STANDARD ANALYSIS QUERIES
// ════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────
// summary
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetSummary(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IFrameProvider* FrameProviderPtr =
		CurrentSession->ReadProvider<TraceServices::IFrameProvider>(TraceServices::GetFrameProviderName());
	if (!FrameProviderPtr)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Frame provider not available"));
	}

	const TraceServices::IFrameProvider& FrameProvider = *FrameProviderPtr;
	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);
	const uint64 RenderFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Rendering);

	double TotalFrameTime = 0.0;
	double MinFrameTime = DBL_MAX;
	double MaxFrameTime = 0.0;
	TArray<double> FrameTimes;
	FrameTimes.Reserve(static_cast<int32>(FMath::Min(GameFrameCount, (uint64)1000000)));

	if (GameFrameCount > 0)
	{
		FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
			[&](const TraceServices::FFrame& Frame)
			{
				double FrameTime = Frame.EndTime - Frame.StartTime;
				if (!std::isfinite(FrameTime) || FrameTime < 0.0)
				{
					return;
				}
				TotalFrameTime += FrameTime;
				MinFrameTime = FMath::Min(MinFrameTime, FrameTime);
				MaxFrameTime = FMath::Max(MaxFrameTime, FrameTime);
				FrameTimes.Add(FrameTime);
			});
	}

	double P50FrameTime = 0.0;
	double P95FrameTime = 0.0;
	double P99FrameTime = 0.0;
	if (FrameTimes.Num() > 0)
	{
		FrameTimes.Sort();
		int32 N = FrameTimes.Num();
		P50FrameTime = FrameTimes[FMath::Clamp(FMath::FloorToInt32(N * 0.50f), 0, N - 1)];
		P95FrameTime = FrameTimes[FMath::Clamp(FMath::FloorToInt32(N * 0.95f), 0, N - 1)];
		P99FrameTime = FrameTimes[FMath::Clamp(FMath::FloorToInt32(N * 0.99f), 0, N - 1)];
	}

	int32 ValidFrameCount = FrameTimes.Num();
	double AvgFrameTime = ValidFrameCount > 0 ? TotalFrameTime / ValidFrameCount : 0.0;

	// Thread count
	const TraceServices::IThreadProvider* ThreadProviderPtr =
		CurrentSession->ReadProvider<TraceServices::IThreadProvider>(TraceServices::GetThreadProviderName());
	int32 ThreadCount = 0;
	if (ThreadProviderPtr)
	{
		ThreadProviderPtr->EnumerateThreads([&](const TraceServices::FThreadInfo&) { ThreadCount++; });
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("trace_path"), LoadedTracePath);
	Result->SetNumberField(TEXT("duration_seconds"), CurrentSession->GetDurationSeconds());
	Result->SetNumberField(TEXT("game_frame_count"), static_cast<double>(GameFrameCount));
	Result->SetNumberField(TEXT("render_frame_count"), static_cast<double>(RenderFrameCount));
	Result->SetNumberField(TEXT("thread_count"), ThreadCount);

	TSharedPtr<FJsonObject> FrameStats = MakeShared<FJsonObject>();
	FrameStats->SetNumberField(TEXT("avg_ms"), SafeDouble(AvgFrameTime * 1000.0));
	FrameStats->SetNumberField(TEXT("min_ms"), SafeDouble((MinFrameTime < DBL_MAX ? MinFrameTime : 0.0) * 1000.0));
	FrameStats->SetNumberField(TEXT("max_ms"), SafeDouble(MaxFrameTime * 1000.0));
	FrameStats->SetNumberField(TEXT("median_ms"), SafeDouble(P50FrameTime * 1000.0));
	FrameStats->SetNumberField(TEXT("p95_ms"), SafeDouble(P95FrameTime * 1000.0));
	FrameStats->SetNumberField(TEXT("p99_ms"), SafeDouble(P99FrameTime * 1000.0));
	FrameStats->SetNumberField(TEXT("avg_fps"), SafeDouble(AvgFrameTime > 0.0 ? 1.0 / AvgFrameTime : 0.0));
	Result->SetObjectField(TEXT("frame_stats"), FrameStats);

	return Result;
}

// ────────────────────────────────────────────────────────────
// worst_frames
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetWorstFrames(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);

	int32 MaxResults = 20;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 200);
	}

	double ThresholdMs = 0.0;
	if (Params->HasField(TEXT("threshold_ms")))
	{
		ThresholdMs = Params->GetNumberField(TEXT("threshold_ms"));
	}

	struct FFrameEntry
	{
		uint64 Index;
		double StartTime;
		double EndTime;
		double DurationMs;
	};

	TArray<FFrameEntry> AllFrames;
	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);
	AllFrames.Reserve(static_cast<int32>(FMath::Min(GameFrameCount, (uint64)1000000)));

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& Frame)
		{
			double DurationMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
			if (!std::isfinite(DurationMs) || DurationMs < 0.0)
			{
				return;
			}
			if (DurationMs >= ThresholdMs)
			{
				AllFrames.Add({ Frame.Index, Frame.StartTime, Frame.EndTime, DurationMs });
			}
		});

	AllFrames.Sort([](const FFrameEntry& A, const FFrameEntry& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	if (AllFrames.Num() > MaxResults)
	{
		AllFrames.SetNum(MaxResults);
	}

	// Use bottleneck-style compact output for each frame
	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);

	TArray<TSharedPtr<FJsonValue>> FrameArray;
	for (const FFrameEntry& Entry : AllFrames)
	{
		TSharedPtr<FJsonObject> FrameObj = MakeShared<FJsonObject>();
		FrameObj->SetNumberField(TEXT("frame_index"), static_cast<double>(Entry.Index));
		FrameObj->SetNumberField(TEXT("duration_ms"), Entry.DurationMs);

		if (TimingProvider)
		{
			// Find top 5 events only (compact)
			TMap<FString, double> TopEventMap;

			const TraceServices::IThreadProvider& ThreadProvider =
				TraceServices::ReadThreadProvider(*CurrentSession);

			ThreadProvider.EnumerateThreads(
				[&](const TraceServices::FThreadInfo& ThreadInfo)
				{
					FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
					if (!ThreadName.Contains(TEXT("GameThread")))
					{
						return;
					}

					uint32 TimelineIndex = 0;
					if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
					{
						return;
					}

					TimingProvider->ReadTimeline(TimelineIndex,
						[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
						{
							Timeline.EnumerateEvents(Entry.StartTime, Entry.EndTime,
								[&](double StartTime, double EndTime, uint32 Depth,
									const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
								{
									if (Depth == 0)
									{
										TimingProvider->ReadTimers(
											[&](const TraceServices::ITimingProfilerTimerReader& Reader)
											{
												const TraceServices::FTimingProfilerTimer* Timer =
													Reader.GetTimer(Event.TimerIndex);
												if (Timer && Timer->Name)
												{
													FString TimerName(Timer->Name);
													double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
													double& Existing = TopEventMap.FindOrAdd(TimerName, 0.0);
													Existing += DurMs;
												}
											});
									}
									return TraceServices::EEventEnumerate::Continue;
								});
						});
				});

			TArray<TPair<FString, double>> SortedEvents;
			for (auto& Pair : TopEventMap)
			{
				SortedEvents.Add(TPair<FString, double>(Pair.Key, Pair.Value));
			}
			SortedEvents.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B)
			{
				return A.Value > B.Value;
			});

			TArray<TSharedPtr<FJsonValue>> TopEventsArray;
			int32 EventLimit = FMath::Min(SortedEvents.Num(), 5);
			for (int32 i = 0; i < EventLimit; ++i)
			{
				TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
				EventObj->SetStringField(TEXT("name"), SortedEvents[i].Key);
				EventObj->SetNumberField(TEXT("ms"), SortedEvents[i].Value);
				TopEventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
			}
			FrameObj->SetArrayField(TEXT("top_events"), TopEventsArray);
		}

		FrameArray.Add(MakeShared<FJsonValueObject>(FrameObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_matching"), AllFrames.Num());
	Result->SetArrayField(TEXT("frames"), FrameArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// frame_details
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetFrameDetails(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	if (!Params->HasField(TEXT("frame_index")))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'frame_index'"));
	}

	uint64 FrameIndex = static_cast<uint64>(Params->GetNumberField(TEXT("frame_index")));
	int32 MaxDepth = 3;
	if (Params->HasField(TEXT("max_depth")))
	{
		MaxDepth = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("max_depth"))), 1, 20);
	}

	double MinDurationMs = 0.1;
	if (Params->HasField(TEXT("min_duration_ms")))
	{
		MinDurationMs = Params->GetNumberField(TEXT("min_duration_ms"));
	}

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);

	const TraceServices::FFrame* Frame = FrameProvider.GetFrame(
		ETraceFrameType::TraceFrameType_Game, FrameIndex);

	if (!Frame)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Frame %llu not found"), FrameIndex));
	}

	double FrameStart = Frame->StartTime;
	double FrameEnd = Frame->EndTime;

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	FString ThreadFilter;
	Params->TryGetStringField(TEXT("thread_name"), ThreadFilter);

	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	TArray<TSharedPtr<FJsonValue>> ThreadsArray;

	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));

			if (!ThreadFilter.IsEmpty() && !ThreadName.Contains(ThreadFilter))
			{
				return;
			}

			uint32 TimelineIndex = 0;
			if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
			{
				return;
			}

			struct FEventEntry
			{
				FString Name;
				double DurationMs;
				uint32 Depth;
			};

			TArray<FEventEntry> Events;

			TimingProvider->ReadTimeline(TimelineIndex,
				[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					Timeline.EnumerateEvents(FrameStart, FrameEnd,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							if (static_cast<int32>(Depth) > MaxDepth)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
							if (!std::isfinite(DurMs) || DurMs < MinDurationMs)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							FString TimerName;
							TimingProvider->ReadTimers(
								[&](const TraceServices::ITimingProfilerTimerReader& Reader)
								{
									const TraceServices::FTimingProfilerTimer* Timer =
										Reader.GetTimer(Event.TimerIndex);
									if (Timer && Timer->Name)
									{
										TimerName = Timer->Name;
									}
								});

							if (!TimerName.IsEmpty())
							{
								Events.Add({ TimerName, DurMs, Depth });
							}

							return TraceServices::EEventEnumerate::Continue;
						});
				});

			if (Events.Num() == 0)
			{
				return;
			}

			// Sort by duration descending for compact output
			Events.Sort([](const FEventEntry& A, const FEventEntry& B)
			{
				return A.DurationMs > B.DurationMs;
			});

			int32 EventLimit = FMath::Min(Events.Num(), 50);

			TArray<TSharedPtr<FJsonValue>> EventsArray;
			for (int32 i = 0; i < EventLimit; ++i)
			{
				TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
				EventObj->SetStringField(TEXT("name"), Events[i].Name);
				EventObj->SetNumberField(TEXT("ms"), Events[i].DurationMs);
				EventObj->SetNumberField(TEXT("depth"), Events[i].Depth);
				EventsArray.Add(MakeShared<FJsonValueObject>(EventObj));
			}

			TSharedPtr<FJsonObject> ThreadObj = MakeShared<FJsonObject>();
			ThreadObj->SetStringField(TEXT("thread"), ThreadName);
			ThreadObj->SetNumberField(TEXT("event_count"), Events.Num());
			ThreadObj->SetNumberField(TEXT("shown"), EventLimit);
			ThreadObj->SetArrayField(TEXT("events"), EventsArray);

			ThreadsArray.Add(MakeShared<FJsonValueObject>(ThreadObj));
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("frame_index"), static_cast<double>(FrameIndex));
	Result->SetNumberField(TEXT("frame_ms"), (FrameEnd - FrameStart) * 1000.0);
	Result->SetArrayField(TEXT("threads"), ThreadsArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// timer_stats (engine-built aggregation)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetTimerStats(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
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

	TraceServices::FCreateAggregationParams AggParams;
	AggParams.IntervalStart = IntervalStart;
	AggParams.IntervalEnd = IntervalEnd;
	AggParams.CpuThreadFilter = [](uint32) { return true; };
	AggParams.SortBy = TraceServices::FCreateAggregationParams::ESortBy::TotalInclusiveTime;
	AggParams.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::Descending;
	AggParams.TableEntryLimit = NameFilter.IsEmpty() ? MaxResults : 0;

	TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggTable =
		TimingProvider->CreateAggregation(AggParams);

	if (!AggTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create aggregation"));
	}

	TArray<TSharedPtr<FJsonValue>> StatsArray;
	TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>* Reader =
		AggTable->CreateReader();

	int32 Count = 0;
	while (Reader->IsValid() && Count < MaxResults)
	{
		const TraceServices::FTimingProfilerAggregatedStats* Row = Reader->GetCurrentRow();
		if (Row && Row->Timer && Row->Timer->Name)
		{
			FString TimerName(Row->Timer->Name);

			if (!NameFilter.IsEmpty() && !TimerName.Contains(NameFilter))
			{
				Reader->NextRow();
				continue;
			}

			TSharedPtr<FJsonObject> StatObj = MakeShared<FJsonObject>();
			StatObj->SetStringField(TEXT("name"), TimerName);
			StatObj->SetNumberField(TEXT("count"), static_cast<double>(Row->InstanceCount));
			StatObj->SetNumberField(TEXT("total_incl_ms"), SafeDouble(Row->TotalInclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("avg_incl_ms"), SafeDouble(Row->AverageInclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("median_incl_ms"), SafeDouble(Row->MedianInclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("max_incl_ms"),
				SafeDouble(Row->MaxInclusiveTime > -DBL_MAX ? Row->MaxInclusiveTime * 1000.0 : 0.0));
			StatObj->SetNumberField(TEXT("total_excl_ms"), SafeDouble(Row->TotalExclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("avg_excl_ms"), SafeDouble(Row->AverageExclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("median_excl_ms"), SafeDouble(Row->MedianExclusiveTime * 1000.0));
			StatObj->SetNumberField(TEXT("max_excl_ms"),
				SafeDouble(Row->MaxExclusiveTime > -DBL_MAX ? Row->MaxExclusiveTime * 1000.0 : 0.0));

			StatsArray.Add(MakeShared<FJsonValueObject>(StatObj));
			Count++;
		}
		Reader->NextRow();
	}

	delete Reader;
	delete AggTable;

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("timer_count"), Count);
	Result->SetArrayField(TEXT("timers"), StatsArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// butterfly (callers/callees analysis)
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetButterfly(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	FString TimerName;
	if (!Params->TryGetStringField(TEXT("timer_name"), TimerName) || TimerName.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'timer_name'. Provide the name of the function/scope to analyze."));
	}

	FString Mode = TEXT("both");
	Params->TryGetStringField(TEXT("mode"), Mode);

	int32 MaxDepth = 5;
	if (Params->HasField(TEXT("max_depth")))
	{
		MaxDepth = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("max_depth"))), 1, 10);
	}

	// Find the timer ID by name (substring match, prefer exact)
	uint32 FoundTimerId = uint32(-1);
	FString FoundTimerName;

	TimingProvider->ReadTimers(
		[&](const TraceServices::ITimingProfilerTimerReader& Reader)
		{
			uint32 TimerCount = Reader.GetTimerCount();
			for (uint32 i = 0; i < TimerCount; ++i)
			{
				const TraceServices::FTimingProfilerTimer* Timer = Reader.GetTimer(i);
				if (Timer && Timer->Name)
				{
					FString Name(Timer->Name);
					if (Name.Contains(TimerName))
					{
						if (Name == TimerName)
						{
							FoundTimerId = i;
							FoundTimerName = Name;
							return;
						}
						if (FoundTimerId == uint32(-1))
						{
							FoundTimerId = i;
							FoundTimerName = Name;
						}
					}
				}
			}
		});

	if (FoundTimerId == uint32(-1))
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Timer '%s' not found in this trace"), *TimerName));
	}

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), IntervalStart, IntervalEnd);

	TraceServices::FCreateButterflyParams ButterflyParams;
	ButterflyParams.IntervalStart = IntervalStart;
	ButterflyParams.IntervalEnd = IntervalEnd;
	ButterflyParams.CpuThreadFilter = [](uint32) { return true; };

	TraceServices::ITimingProfilerButterfly* Butterfly =
		TimingProvider->CreateButterfly(ButterflyParams);

	if (!Butterfly)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create butterfly analysis"));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("timer_name"), FoundTimerName);

	if (Mode == TEXT("callers") || Mode == TEXT("both"))
	{
		const TraceServices::FTimingProfilerButterflyNode& CallersRoot =
			Butterfly->GenerateCallersTree(FoundTimerId);
		Result->SetObjectField(TEXT("callers"), SerializeButterflyNode(CallersRoot, MaxDepth));
	}

	if (Mode == TEXT("callees") || Mode == TEXT("both"))
	{
		const TraceServices::FTimingProfilerButterflyNode& CalleesRoot =
			Butterfly->GenerateCalleesTree(FoundTimerId);
		Result->SetObjectField(TEXT("callees"), SerializeButterflyNode(CalleesRoot, MaxDepth));
	}

	delete Butterfly;

	return Result;
}

// ────────────────────────────────────────────────────────────
// threads
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetThreads(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);

	TArray<TSharedPtr<FJsonValue>> ThreadsArray;

	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			TSharedPtr<FJsonObject> ThreadObj = MakeShared<FJsonObject>();
			ThreadObj->SetNumberField(TEXT("id"), ThreadInfo.Id);
			ThreadObj->SetStringField(TEXT("name"),
				ThreadInfo.Name ? FString(ThreadInfo.Name) : TEXT("Unknown"));
			ThreadObj->SetStringField(TEXT("group"),
				ThreadInfo.GroupName ? FString(ThreadInfo.GroupName) : TEXT(""));

			if (TimingProvider)
			{
				uint32 TimelineIndex = 0;
				bool bHasTimeline = TimingProvider->GetCpuThreadTimelineIndex(
					ThreadInfo.Id, TimelineIndex);
				ThreadObj->SetBoolField(TEXT("has_timeline"), bHasTimeline);

				if (bHasTimeline)
				{
					TimingProvider->ReadTimeline(TimelineIndex,
						[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
						{
							ThreadObj->SetNumberField(TEXT("event_count"),
								static_cast<double>(Timeline.GetEventCount()));
						});
				}
			}

			ThreadsArray.Add(MakeShared<FJsonValueObject>(ThreadObj));
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("thread_count"), ThreadsArray.Num());
	Result->SetArrayField(TEXT("threads"), ThreadsArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// counters
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetCounters(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	const TraceServices::ICounterProvider& CounterProvider =
		TraceServices::ReadCounterProvider(*CurrentSession);

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	bool bIncludeValues = false;
	if (Params->HasField(TEXT("include_values")))
	{
		bIncludeValues = Params->GetBoolField(TEXT("include_values"));
	}

	double SampleStart = 0.0;
	double SampleEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, CurrentSession->GetDurationSeconds(), SampleStart, SampleEnd);

	int32 MaxSamples = 100;
	if (Params->HasField(TEXT("max_samples")))
	{
		MaxSamples = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("max_samples"))), 1, 10000);
	}

	TArray<TSharedPtr<FJsonValue>> CountersArray;

	CounterProvider.EnumerateCounters(
		[&](uint32 CounterId, const TraceServices::ICounter& Counter)
		{
			FString CounterName(Counter.GetName());

			if (!NameFilter.IsEmpty() && !CounterName.Contains(NameFilter))
			{
				return;
			}

			TSharedPtr<FJsonObject> CounterObj = MakeShared<FJsonObject>();
			CounterObj->SetNumberField(TEXT("id"), CounterId);
			CounterObj->SetStringField(TEXT("name"), CounterName);
			CounterObj->SetBoolField(TEXT("is_float"), Counter.IsFloatingPoint());

			if (bIncludeValues)
			{
				TArray<TSharedPtr<FJsonValue>> ValuesArray;
				int32 SampleCount = 0;

				if (Counter.IsFloatingPoint())
				{
					Counter.EnumerateFloatValues(SampleStart, SampleEnd, true,
						[&](double Time, double Value)
						{
							if (SampleCount >= MaxSamples)
							{
								return;
							}
							TSharedPtr<FJsonObject> SampleObj = MakeShared<FJsonObject>();
							SampleObj->SetNumberField(TEXT("t"), Time);
							SampleObj->SetNumberField(TEXT("v"), SafeDouble(Value));
							ValuesArray.Add(MakeShared<FJsonValueObject>(SampleObj));
							SampleCount++;
						});
				}
				else
				{
					Counter.EnumerateValues(SampleStart, SampleEnd, true,
						[&](double Time, int64 Value)
						{
							if (SampleCount >= MaxSamples)
							{
								return;
							}
							TSharedPtr<FJsonObject> SampleObj = MakeShared<FJsonObject>();
							SampleObj->SetNumberField(TEXT("t"), Time);
							SampleObj->SetNumberField(TEXT("v"), static_cast<double>(Value));
							ValuesArray.Add(MakeShared<FJsonValueObject>(SampleObj));
							SampleCount++;
						});
				}

				CounterObj->SetArrayField(TEXT("values"), ValuesArray);
			}

			CountersArray.Add(MakeShared<FJsonValueObject>(CounterObj));
		});

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("counter_count"), CountersArray.Num());
	Result->SetArrayField(TEXT("counters"), CountersArray);

	return Result;
}

// ════════════════════════════════════════════════════════════
//  ADDITIONAL SMART QUERIES
// ════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────
// spikes — Auto-detect worst frames AND categorize them
// Combines worst_frames + bottlenecks in one compact call.
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetSpikes(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	int32 MaxResults = 10;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 50);
	}

	double ThresholdMs = 0.0;
	if (Params->HasField(TEXT("threshold_ms")))
	{
		ThresholdMs = Params->GetNumberField(TEXT("threshold_ms"));
	}

	double TargetFps = 60.0;
	if (Params->HasField(TEXT("target_fps")))
	{
		TargetFps = FMath::Max(1.0, Params->GetNumberField(TEXT("target_fps")));
	}

	FString ThreadFilter = TEXT("GameThread");
	Params->TryGetStringField(TEXT("thread"), ThreadFilter);

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	// Step 1: Collect all frames and sort by duration
	struct FFrameEntry
	{
		uint64 Index;
		double StartTime;
		double EndTime;
		double DurationMs;
	};

	TArray<FFrameEntry> AllFrames;
	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);
	AllFrames.Reserve(static_cast<int32>(FMath::Min(GameFrameCount, (uint64)100000)));

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& Frame)
		{
			double DurationMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
			if (std::isfinite(DurationMs) && DurationMs > 0.0 && DurationMs >= ThresholdMs)
			{
				AllFrames.Add({ Frame.Index, Frame.StartTime, Frame.EndTime, DurationMs });
			}
		});

	AllFrames.Sort([](const FFrameEntry& A, const FFrameEntry& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	if (AllFrames.Num() > MaxResults)
	{
		AllFrames.SetNum(MaxResults);
	}

	// Step 2: For each worst frame, run category bucketing
	double TargetMs = 1000.0 / TargetFps;

	TArray<TSharedPtr<FJsonValue>> SpikesArray;
	for (const FFrameEntry& Entry : AllFrames)
	{
		// Category buckets for this frame
		struct FCategoryBucket
		{
			double TotalMs = 0.0;
			FString TopEventName;
			double TopEventMs = 0.0;
		};

		FCategoryBucket Buckets[static_cast<int32>(EProfilingCategory::MAX)];

		ThreadProvider.EnumerateThreads(
			[&](const TraceServices::FThreadInfo& ThreadInfo)
			{
				FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
				if (!ThreadName.Contains(ThreadFilter))
				{
					return;
				}

				uint32 TimelineIndex = 0;
				if (!TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
				{
					return;
				}

				TimingProvider->ReadTimeline(TimelineIndex,
					[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						// Auto-detect bucketing depth (same logic as bottlenecks)
						int32 EventCountByDepth[6] = {};
						Timeline.EnumerateEvents(Entry.StartTime, Entry.EndTime,
							[&](double StartTime, double EndTime, uint32 Depth,
								const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
							{
								if (Depth < 6)
								{
									double DurMs = (EndTime - StartTime) * 1000.0;
									if (std::isfinite(DurMs) && DurMs > 0.1)
									{
										EventCountByDepth[Depth]++;
									}
								}
								return TraceServices::EEventEnumerate::Continue;
							});

						uint32 BucketDepth = 0;
						for (int32 d = 0; d < 6; ++d)
						{
							if (EventCountByDepth[d] >= 2)
							{
								BucketDepth = d;
								break;
							}
							BucketDepth = d + 1;
						}
						BucketDepth = FMath::Min(BucketDepth, (uint32)5);

						// Bucket events at detected depth
						Timeline.EnumerateEvents(Entry.StartTime, Entry.EndTime,
							[&](double StartTime, double EndTime, uint32 Depth,
								const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
							{
								if (Depth != BucketDepth)
								{
									return TraceServices::EEventEnumerate::Continue;
								}

								double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
								if (!std::isfinite(DurMs) || DurMs < 0.01)
								{
									return TraceServices::EEventEnumerate::Continue;
								}

								FString TimerName;
								TimingProvider->ReadTimers(
									[&](const TraceServices::ITimingProfilerTimerReader& Reader)
									{
										const TraceServices::FTimingProfilerTimer* Timer =
											Reader.GetTimer(Event.TimerIndex);
										if (Timer && Timer->Name)
										{
											TimerName = Timer->Name;
										}
									});

								if (TimerName.IsEmpty())
								{
									return TraceServices::EEventEnumerate::Continue;
								}

								EProfilingCategory Cat = CategorizeTimerName(TimerName);
								int32 CatIdx = static_cast<int32>(Cat);
								FCategoryBucket& Bucket = Buckets[CatIdx];

								Bucket.TotalMs += DurMs;
								if (DurMs > Bucket.TopEventMs)
								{
									Bucket.TopEventMs = DurMs;
									Bucket.TopEventName = TimerName;
								}

								return TraceServices::EEventEnumerate::Continue;
							});
					});
			});

		// Build compact spike entry: frame info + top 3 categories
		TSharedPtr<FJsonObject> SpikeObj = MakeShared<FJsonObject>();
		SpikeObj->SetNumberField(TEXT("frame"), static_cast<double>(Entry.Index));
		SpikeObj->SetNumberField(TEXT("ms"), SafeDouble(Entry.DurationMs));
		SpikeObj->SetNumberField(TEXT("over_budget_ms"), SafeDouble(Entry.DurationMs - TargetMs));

		// Sort categories and take top 3
		struct FSortedCat
		{
			EProfilingCategory Category;
			double TotalMs;
			FString TopEvent;
			double TopMs;
		};

		TArray<FSortedCat> SortedCats;
		for (int32 i = 0; i < static_cast<int32>(EProfilingCategory::MAX); ++i)
		{
			if (Buckets[i].TotalMs > 0.1)
			{
				SortedCats.Add({
					static_cast<EProfilingCategory>(i),
					Buckets[i].TotalMs,
					Buckets[i].TopEventName,
					Buckets[i].TopEventMs
				});
			}
		}

		SortedCats.Sort([](const FSortedCat& A, const FSortedCat& B)
		{
			return A.TotalMs > B.TotalMs;
		});

		TArray<TSharedPtr<FJsonValue>> CatsArray;
		int32 CatLimit = FMath::Min(SortedCats.Num(), 3);
		for (int32 i = 0; i < CatLimit; ++i)
		{
			TSharedPtr<FJsonObject> CatObj = MakeShared<FJsonObject>();
			CatObj->SetStringField(TEXT("cat"), GetCategoryName(SortedCats[i].Category));
			CatObj->SetNumberField(TEXT("ms"), SafeDouble(SortedCats[i].TotalMs));
			CatObj->SetStringField(TEXT("top"), SortedCats[i].TopEvent);
			CatObj->SetNumberField(TEXT("top_ms"), SafeDouble(SortedCats[i].TopMs));
			CatsArray.Add(MakeShared<FJsonValueObject>(CatObj));
		}
		SpikeObj->SetArrayField(TEXT("cats"), CatsArray);

		SpikesArray.Add(MakeShared<FJsonValueObject>(SpikeObj));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("target_fps"), TargetFps);
	Result->SetNumberField(TEXT("target_ms"), SafeDouble(TargetMs));
	Result->SetNumberField(TEXT("spike_count"), SpikesArray.Num());
	Result->SetArrayField(TEXT("spikes"), SpikesArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// search — Find a specific timer across all frames
// Returns: which frames it appears in, min/avg/max/p95, worst frame index.
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetSearch(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	FString TimerFilter;
	if (!Params->TryGetStringField(TEXT("filter"), TimerFilter) || TimerFilter.IsEmpty())
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			TEXT("Missing 'filter'. Provide a timer name or substring to search for."));
	}

	FString ThreadFilter = TEXT("GameThread");
	Params->TryGetStringField(TEXT("thread"), ThreadFilter);

	int32 MaxFrameResults = 10;
	if (Params->HasField(TEXT("count")))
	{
		MaxFrameResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 50);
	}

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	const TraceServices::IThreadProvider& ThreadProvider =
		TraceServices::ReadThreadProvider(*CurrentSession);

	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	// Find target thread's timeline index
	uint32 TargetTimelineIndex = UINT32_MAX;
	ThreadProvider.EnumerateThreads(
		[&](const TraceServices::FThreadInfo& ThreadInfo)
		{
			if (TargetTimelineIndex != UINT32_MAX)
			{
				return;
			}

			FString ThreadName(ThreadInfo.Name ? ThreadInfo.Name : TEXT("Unknown"));
			if (!ThreadName.Contains(ThreadFilter))
			{
				return;
			}

			uint32 TimelineIndex = 0;
			if (TimingProvider->GetCpuThreadTimelineIndex(ThreadInfo.Id, TimelineIndex))
			{
				TargetTimelineIndex = TimelineIndex;
			}
		});

	if (TargetTimelineIndex == UINT32_MAX)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(
			FString::Printf(TEXT("Thread '%s' not found"), *ThreadFilter));
	}

	// Scan all frames for the timer
	struct FTimerOccurrence
	{
		uint64 FrameIndex;
		double DurationMs;
		double FrameDurationMs;
	};

	TArray<FTimerOccurrence> Occurrences;
	TArray<double> AllDurations;
	TSet<FString> MatchedTimerNames;

	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& Frame)
		{
			double FrameMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
			if (!std::isfinite(FrameMs) || FrameMs <= 0.0)
			{
				return;
			}

			// Accumulate all matching timer instances within this frame
			double FrameTimerTotalMs = 0.0;
			bool bFoundInFrame = false;

			TimingProvider->ReadTimeline(TargetTimelineIndex,
				[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
				{
					Timeline.EnumerateEvents(Frame.StartTime, Frame.EndTime,
						[&](double StartTime, double EndTime, uint32 Depth,
							const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
						{
							// Only check first few depths to avoid excessive scanning
							if (Depth > 6)
							{
								return TraceServices::EEventEnumerate::Continue;
							}

							FString TimerName;
							TimingProvider->ReadTimers(
								[&](const TraceServices::ITimingProfilerTimerReader& Reader)
								{
									const TraceServices::FTimingProfilerTimer* Timer =
										Reader.GetTimer(Event.TimerIndex);
									if (Timer && Timer->Name)
									{
										TimerName = Timer->Name;
									}
								});

							if (!TimerName.IsEmpty() && TimerName.Contains(TimerFilter))
							{
								double DurMs = SafeDouble((EndTime - StartTime) * 1000.0);
								if (std::isfinite(DurMs) && DurMs > 0.001)
								{
									FrameTimerTotalMs += DurMs;
									bFoundInFrame = true;
									MatchedTimerNames.Add(TimerName);
								}
							}

							return TraceServices::EEventEnumerate::Continue;
						});
				});

			if (bFoundInFrame && FrameTimerTotalMs > 0.001)
			{
				Occurrences.Add({ Frame.Index, FrameTimerTotalMs, FrameMs });
				AllDurations.Add(FrameTimerTotalMs);
			}
		});

	if (Occurrences.Num() == 0)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetStringField(TEXT("filter"), TimerFilter);
		Result->SetNumberField(TEXT("frames_found"), 0);
		Result->SetStringField(TEXT("message"), TEXT("Timer not found in any frame."));
		return Result;
	}

	// Compute stats
	AllDurations.Sort();
	int32 N = AllDurations.Num();

	double TotalMs = 0.0;
	for (double D : AllDurations)
	{
		TotalMs += D;
	}

	double MinMs = AllDurations[0];
	double MaxMs = AllDurations[N - 1];
	double AvgMs = TotalMs / N;
	double MedianMs = AllDurations[N / 2];
	double P95Ms = AllDurations[FMath::Clamp(FMath::FloorToInt32(N * 0.95f), 0, N - 1)];
	double P99Ms = AllDurations[FMath::Clamp(FMath::FloorToInt32(N * 0.99f), 0, N - 1)];

	// Sort occurrences by timer duration to find worst frames
	Occurrences.Sort([](const FTimerOccurrence& A, const FTimerOccurrence& B)
	{
		return A.DurationMs > B.DurationMs;
	});

	// Build worst frames list
	TArray<TSharedPtr<FJsonValue>> WorstArray;
	int32 WorstLimit = FMath::Min(Occurrences.Num(), MaxFrameResults);
	for (int32 i = 0; i < WorstLimit; ++i)
	{
		TSharedPtr<FJsonObject> FrameObj = MakeShared<FJsonObject>();
		FrameObj->SetNumberField(TEXT("frame"), static_cast<double>(Occurrences[i].FrameIndex));
		FrameObj->SetNumberField(TEXT("timer_ms"), SafeDouble(Occurrences[i].DurationMs));
		FrameObj->SetNumberField(TEXT("frame_ms"), SafeDouble(Occurrences[i].FrameDurationMs));
		FrameObj->SetNumberField(TEXT("pct_of_frame"),
			SafeDouble(Occurrences[i].FrameDurationMs > 0.0
				? (Occurrences[i].DurationMs / Occurrences[i].FrameDurationMs) * 100.0
				: 0.0));
		WorstArray.Add(MakeShared<FJsonValueObject>(FrameObj));
	}

	// Matched timer names list
	TArray<TSharedPtr<FJsonValue>> NamesArray;
	for (const FString& Name : MatchedTimerNames)
	{
		NamesArray.Add(MakeShared<FJsonValueString>(Name));
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetStringField(TEXT("filter"), TimerFilter);
	Result->SetNumberField(TEXT("frames_found"), Occurrences.Num());
	Result->SetNumberField(TEXT("total_frames"), static_cast<double>(GameFrameCount));
	Result->SetArrayField(TEXT("matched_timers"), NamesArray);

	TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
	Stats->SetNumberField(TEXT("min_ms"), SafeDouble(MinMs));
	Stats->SetNumberField(TEXT("max_ms"), SafeDouble(MaxMs));
	Stats->SetNumberField(TEXT("avg_ms"), SafeDouble(AvgMs));
	Stats->SetNumberField(TEXT("median_ms"), SafeDouble(MedianMs));
	Stats->SetNumberField(TEXT("p95_ms"), SafeDouble(P95Ms));
	Stats->SetNumberField(TEXT("p99_ms"), SafeDouble(P99Ms));
	Result->SetObjectField(TEXT("stats"), Stats);

	Result->SetArrayField(TEXT("worst_frames"), WorstArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// histogram — Frame time distribution for quick pattern detection
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetHistogram(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	double BucketSizeMs = 0.0;
	if (Params->HasField(TEXT("bucket_size_ms")))
	{
		BucketSizeMs = Params->GetNumberField(TEXT("bucket_size_ms"));
	}

	double TargetFps = 60.0;
	if (Params->HasField(TEXT("target_fps")))
	{
		TargetFps = FMath::Max(1.0, Params->GetNumberField(TEXT("target_fps")));
	}

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);

	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);

	// Collect all frame times
	TArray<double> FrameTimes;
	FrameTimes.Reserve(static_cast<int32>(FMath::Min(GameFrameCount, (uint64)1000000)));

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& Frame)
		{
			double DurationMs = (Frame.EndTime - Frame.StartTime) * 1000.0;
			if (std::isfinite(DurationMs) && DurationMs > 0.0)
			{
				FrameTimes.Add(DurationMs);
			}
		});

	if (FrameTimes.Num() == 0)
	{
		TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), true);
		Result->SetNumberField(TEXT("total_frames"), 0);
		return Result;
	}

	FrameTimes.Sort();

	double TargetMs = 1000.0 / TargetFps;

	if (BucketSizeMs <= 0.0)
	{
		// Auto-size buckets based on target FPS
		// Use half the target frame time as bucket size for good granularity
		BucketSizeMs = FMath::Max(1.0, FMath::CeilToDouble(TargetMs / 4.0));
	}

	// Build histogram buckets
	// Find max frame time to determine bucket count
	double MaxMs = FrameTimes.Last();
	int32 NumBuckets = FMath::CeilToInt32(MaxMs / BucketSizeMs) + 1;
	NumBuckets = FMath::Min(NumBuckets, 50); // Cap at 50 buckets for sanity

	TArray<int32> BucketCounts;
	BucketCounts.SetNumZeroed(NumBuckets);

	for (double FrameMs : FrameTimes)
	{
		int32 BucketIdx = FMath::Clamp(FMath::FloorToInt32(FrameMs / BucketSizeMs), 0, NumBuckets - 1);
		BucketCounts[BucketIdx]++;
	}

	// Build result with non-empty buckets only
	TArray<TSharedPtr<FJsonValue>> BucketsArray;
	for (int32 i = 0; i < NumBuckets; ++i)
	{
		if (BucketCounts[i] == 0)
		{
			continue;
		}

		double RangeStart = i * BucketSizeMs;
		double RangeEnd = (i + 1) * BucketSizeMs;

		TSharedPtr<FJsonObject> BucketObj = MakeShared<FJsonObject>();
		BucketObj->SetStringField(TEXT("range"),
			FString::Printf(TEXT("%.0f-%.0fms"), RangeStart, RangeEnd));
		BucketObj->SetNumberField(TEXT("count"), BucketCounts[i]);
		BucketObj->SetNumberField(TEXT("pct"),
			SafeDouble((static_cast<double>(BucketCounts[i]) / FrameTimes.Num()) * 100.0));
		BucketsArray.Add(MakeShared<FJsonValueObject>(BucketObj));
	}

	// Summary stats
	int32 N = FrameTimes.Num();
	int32 OnBudget = 0;
	int32 SlightlyOver = 0;
	int32 Over2x = 0;
	int32 Over4x = 0;

	for (double FrameMs : FrameTimes)
	{
		if (FrameMs <= TargetMs)
		{
			OnBudget++;
		}
		else if (FrameMs <= TargetMs * 2.0)
		{
			SlightlyOver++;
		}
		else if (FrameMs <= TargetMs * 4.0)
		{
			Over2x++;
		}
		else
		{
			Over4x++;
		}
	}

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("total_frames"), N);
	Result->SetNumberField(TEXT("bucket_size_ms"), BucketSizeMs);
	Result->SetNumberField(TEXT("target_ms"), SafeDouble(TargetMs));

	// Budget summary — very compact
	TSharedPtr<FJsonObject> Budget = MakeShared<FJsonObject>();
	Budget->SetStringField(TEXT("on_budget"),
		FString::Printf(TEXT("%d (%.1f%%)"), OnBudget, (OnBudget * 100.0) / N));
	Budget->SetStringField(TEXT("slightly_over"),
		FString::Printf(TEXT("%d (%.1f%%)"), SlightlyOver, (SlightlyOver * 100.0) / N));
	Budget->SetStringField(TEXT("over_2x"),
		FString::Printf(TEXT("%d (%.1f%%)"), Over2x, (Over2x * 100.0) / N));
	Budget->SetStringField(TEXT("over_4x"),
		FString::Printf(TEXT("%d (%.1f%%)"), Over4x, (Over4x * 100.0) / N));
	Result->SetObjectField(TEXT("budget"), Budget);

	Result->SetArrayField(TEXT("buckets"), BucketsArray);

	return Result;
}

// ════════════════════════════════════════════════════════════
//  PROFESSIONAL ANALYSIS QUERIES
// ════════════════════════════════════════════════════════════

// ────────────────────────────────────────────────────────────
// diagnose — Full auto-diagnosis report with findings & recommendations
// One call = verdict, findings, category breakdown, top exclusive timers
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetDiagnose(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	double TargetFps = 60.0;
	if (Params->HasField(TEXT("target_fps")))
	{
		TargetFps = FMath::Max(1.0, Params->GetNumberField(TEXT("target_fps")));
	}
	double TargetMs = 1000.0 / TargetFps;

	// ── Step 1: Frame statistics ─────────────────────────────

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);

	TArray<double> FrameTimes;
	FrameTimes.Reserve(static_cast<int32>(FMath::Min(GameFrameCount, (uint64)100000)));

	int32 OnBudget = 0;
	int32 StartupSpikes = 0;
	int32 TotalSpikes = 0;

	FrameProvider.EnumerateFrames(ETraceFrameType::TraceFrameType_Game, 0, GameFrameCount,
		[&](const TraceServices::FFrame& Frame)
		{
			double Ms = (Frame.EndTime - Frame.StartTime) * 1000.0;
			if (!std::isfinite(Ms) || Ms <= 0.0)
			{
				return;
			}

			FrameTimes.Add(Ms);

			if (Ms <= TargetMs)
			{
				OnBudget++;
			}
			if (Ms > TargetMs * 4.0)
			{
				TotalSpikes++;
				if (Frame.Index < 20)
				{
					StartupSpikes++;
				}
			}
		});

	if (FrameTimes.Num() == 0)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("No valid frames in trace"));
	}

	FrameTimes.Sort();
	int32 N = FrameTimes.Num();

	double TotalMs = 0.0;
	for (double Ms : FrameTimes)
	{
		TotalMs += Ms;
	}

	double AvgMs = TotalMs / N;
	double MedianMs = FrameTimes[N / 2];
	double P95Ms = FrameTimes[FMath::Clamp(FMath::FloorToInt32(N * 0.95f), 0, N - 1)];
	double P99Ms = FrameTimes[FMath::Clamp(FMath::FloorToInt32(N * 0.99f), 0, N - 1)];
	double MaxMs = FrameTimes.Last();
	double OnBudgetPct = (static_cast<double>(OnBudget) / N) * 100.0;

	// ── Step 2: GPU bound detection ──────────────────────────

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);

	bool bHasGpu = false;
	double GpuAvgMs = 0.0;
	FString GpuQueueName;

	if (TimingProvider && TimingProvider->HasGpuTiming())
	{
		// Find the primary GPU queue
		uint32 GpuTimelineIndex = UINT32_MAX;
		TimingProvider->EnumerateGpuQueues(
			[&](const TraceServices::FGpuQueueInfo& QueueInfo)
			{
				if (GpuTimelineIndex == UINT32_MAX)
				{
					GpuTimelineIndex = QueueInfo.TimelineIndex;
					GpuQueueName = QueueInfo.Name ? FString(QueueInfo.Name) : TEXT("GPU");
				}
			});

		if (GpuTimelineIndex != UINT32_MAX)
		{
			bHasGpu = true;

			// Sample GPU frame times from ~50 evenly spaced frames
			int32 SampleCount = FMath::Min(N, 50);
			int32 Step = FMath::Max(1, N / SampleCount);
			double GpuTotalSampled = 0.0;
			int32 GpuSamples = 0;

			for (int32 SampleIdx = 0; SampleIdx < SampleCount; ++SampleIdx)
			{
				int32 FrameIdx = SampleIdx * Step;
				if (FrameIdx >= static_cast<int32>(GameFrameCount))
				{
					break;
				}

				const TraceServices::FFrame* Frame = FrameProvider.GetFrame(
					ETraceFrameType::TraceFrameType_Game, FrameIdx);
				if (!Frame)
				{
					continue;
				}

				double MaxGpuEventMs = 0.0;
				TimingProvider->ReadTimeline(GpuTimelineIndex,
					[&](const TraceServices::ITimingProfilerProvider::Timeline& Timeline)
					{
						Timeline.EnumerateEvents(Frame->StartTime, Frame->EndTime,
							[&](double StartTime, double EndTime, uint32 Depth,
								const TraceServices::FTimingProfilerEvent& Event) -> TraceServices::EEventEnumerate
							{
								if (Depth == 0)
								{
									double DurMs = (EndTime - StartTime) * 1000.0;
									if (std::isfinite(DurMs) && DurMs > MaxGpuEventMs)
									{
										MaxGpuEventMs = DurMs;
									}
								}
								return TraceServices::EEventEnumerate::Continue;
							});
					});

				if (MaxGpuEventMs > 0.0)
				{
					GpuTotalSampled += MaxGpuEventMs;
					GpuSamples++;
				}
			}

			if (GpuSamples > 0)
			{
				GpuAvgMs = GpuTotalSampled / GpuSamples;
			}
		}
	}

	// ── Step 3: Category breakdown via aggregation ───────────

	struct FCatStats
	{
		double TotalExclMs = 0.0;
		FString TopTimer;
		double TopTimerExclMs = 0.0;
		int32 TimerCount = 0;
	};
	FCatStats CatStats[static_cast<int32>(EProfilingCategory::MAX)];

	struct FExclTimer
	{
		FString Name;
		double PerFrameExclMs;
		double PerFrameInclMs;
		int32 Count;
		EProfilingCategory Category;
	};
	TArray<FExclTimer> TopExclusive;

	if (TimingProvider)
	{
		TraceServices::FCreateAggregationParams AggParams;
		AggParams.IntervalStart = 0.0;
		AggParams.IntervalEnd = CurrentSession->GetDurationSeconds();
		AggParams.CpuThreadFilter = [](uint32) { return true; };
		AggParams.SortBy = TraceServices::FCreateAggregationParams::ESortBy::TotalInclusiveTime;
		AggParams.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::Descending;
		AggParams.TableEntryLimit = 0; // All timers for accurate category totals

		TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggTable =
			TimingProvider->CreateAggregation(AggParams);

		if (AggTable)
		{
			TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>* Reader =
				AggTable->CreateReader();

			while (Reader->IsValid())
			{
				const TraceServices::FTimingProfilerAggregatedStats* Row = Reader->GetCurrentRow();
				if (Row && Row->Timer && Row->Timer->Name)
				{
					FString TimerName(Row->Timer->Name);

					// Per-frame average = total across trace / frame count
					double PerFrameExclMs = SafeDouble(
						(Row->TotalExclusiveTime / FMath::Max(1.0, static_cast<double>(GameFrameCount))) * 1000.0);
					double PerFrameInclMs = SafeDouble(
						(Row->TotalInclusiveTime / FMath::Max(1.0, static_cast<double>(GameFrameCount))) * 1000.0);

					// Categorize and accumulate
					EProfilingCategory Cat = CategorizeTimerName(TimerName);
					int32 CatIdx = static_cast<int32>(Cat);

					CatStats[CatIdx].TotalExclMs += PerFrameExclMs;
					CatStats[CatIdx].TimerCount++;

					if (PerFrameExclMs > CatStats[CatIdx].TopTimerExclMs)
					{
						CatStats[CatIdx].TopTimerExclMs = PerFrameExclMs;
						CatStats[CatIdx].TopTimer = TimerName;
					}

					// Collect all non-negligible timers for exclusive sorting
					if (PerFrameExclMs > 0.01)
					{
						TopExclusive.Add({
							TimerName,
							PerFrameExclMs,
							PerFrameInclMs,
							static_cast<int32>(Row->InstanceCount),
							Cat
						});
					}
				}
				Reader->NextRow();
			}

			delete Reader;
			delete AggTable;

			// Sort by exclusive time and keep top 10
			TopExclusive.Sort([](const FExclTimer& A, const FExclTimer& B)
			{
				return A.PerFrameExclMs > B.PerFrameExclMs;
			});
			if (TopExclusive.Num() > 10)
			{
				TopExclusive.SetNum(10);
			}
		}
	}

	// ── Step 4: Build sorted categories ──────────────────────

	struct FSortedCat
	{
		EProfilingCategory Category;
		double ExclMs;
		FString TopTimer;
		double TopMs;
	};

	TArray<FSortedCat> SortedCats;
	double TotalCatExclMs = 0.0;

	for (int32 i = 0; i < static_cast<int32>(EProfilingCategory::MAX); ++i)
	{
		if (CatStats[i].TotalExclMs > 0.01)
		{
			SortedCats.Add({
				static_cast<EProfilingCategory>(i),
				CatStats[i].TotalExclMs,
				CatStats[i].TopTimer,
				CatStats[i].TopTimerExclMs
			});
			TotalCatExclMs += CatStats[i].TotalExclMs;
		}
	}

	SortedCats.Sort([](const FSortedCat& A, const FSortedCat& B)
	{
		return A.ExclMs > B.ExclMs;
	});

	// ── Step 5: Generate findings ────────────────────────────

	TArray<TSharedPtr<FJsonValue>> FindingsArray;

	auto AddFinding = [&](const TCHAR* Severity, const FString& Message)
	{
		TSharedPtr<FJsonObject> F = MakeShared<FJsonObject>();
		F->SetStringField(TEXT("sev"), Severity);
		F->SetStringField(TEXT("msg"), Message);
		FindingsArray.Add(MakeShared<FJsonValueObject>(F));
	};

	// Budget finding
	if (OnBudgetPct < 25.0)
	{
		AddFinding(TEXT("critical"),
			FString::Printf(TEXT("Only %.0f%% of frames meet %.0f FPS target. Median %.1fms (%.0f FPS). p95=%.1fms."),
				OnBudgetPct, TargetFps, MedianMs, 1000.0 / MedianMs, P95Ms));
	}
	else if (OnBudgetPct < 50.0)
	{
		AddFinding(TEXT("high"),
			FString::Printf(TEXT("%.0f%% of frames miss %.0f FPS target. Median %.1fms (%.0f FPS)."),
				100.0 - OnBudgetPct, TargetFps, MedianMs, 1000.0 / MedianMs));
	}
	else if (OnBudgetPct < 75.0)
	{
		AddFinding(TEXT("medium"),
			FString::Printf(TEXT("%.0f%% of frames miss %.0f FPS. Median %.1fms."),
				100.0 - OnBudgetPct, TargetFps, MedianMs));
	}
	else
	{
		AddFinding(TEXT("ok"),
			FString::Printf(TEXT("%.0f%% on budget at %.0f FPS. Median %.1fms."),
				OnBudgetPct, TargetFps, MedianMs));
	}

	// GPU vs CPU finding
	if (bHasGpu && GpuAvgMs > 0.1)
	{
		if (GpuAvgMs > AvgMs * 1.2)
		{
			AddFinding(TEXT("high"),
				FString::Printf(TEXT("GPU-bound: GPU %.1fms avg vs CPU %.1fms avg. Optimize rendering."),
					GpuAvgMs, AvgMs));
		}
		else if (AvgMs > GpuAvgMs * 1.2)
		{
			AddFinding(TEXT("info"),
				FString::Printf(TEXT("CPU-bound: CPU %.1fms avg vs GPU %.1fms avg. GPU has headroom."),
					AvgMs, GpuAvgMs));
		}
		else
		{
			AddFinding(TEXT("high"),
				FString::Printf(TEXT("Both CPU (%.1fms) and GPU (%.1fms) near budget. Optimize both."),
					AvgMs, GpuAvgMs));
		}
	}

	// Spike findings
	if (StartupSpikes > 3)
	{
		AddFinding(TEXT("medium"),
			FString::Printf(TEXT("Startup hitches: %d frames >%.0fms in first 20 frames (max %.0fms). Likely shader compilation / asset loading."),
				StartupSpikes, TargetMs * 4.0, MaxMs));
	}

	int32 NonStartupSpikes = TotalSpikes - StartupSpikes;
	if (NonStartupSpikes > 5)
	{
		AddFinding(TEXT("high"),
			FString::Printf(TEXT("Runtime hitches: %d frames >%.0fms outside startup. Check GC, streaming, or periodic operations."),
				NonStartupSpikes, TargetMs * 4.0));
	}

	// Category findings with recommendations
	for (const FSortedCat& Cat : SortedCats)
	{
		double Pct = TotalCatExclMs > 0.0 ? (Cat.ExclMs / TotalCatExclMs) * 100.0 : 0.0;
		if (Pct < 5.0)
		{
			continue;
		}

		const TCHAR* Severity = Pct > 40.0 ? TEXT("high") : (Pct > 20.0 ? TEXT("medium") : TEXT("info"));
		const TCHAR* Recommendation = GetCategoryRecommendation(Cat.Category);

		FString Msg = FString::Printf(TEXT("%s: %.0f%% of CPU (%.1fms/frame). Top: %s (%.1fms)."),
			GetCategoryName(Cat.Category), Pct, Cat.ExclMs, *Cat.TopTimer, Cat.TopMs);

		if (FCString::Strlen(Recommendation) > 0)
		{
			Msg += FString::Printf(TEXT(" Tip: %s"), Recommendation);
		}

		AddFinding(Severity, Msg);
	}

	// ── Step 6: Build verdict ────────────────────────────────

	FString BoundType;
	if (bHasGpu && GpuAvgMs > 0.1)
	{
		if (GpuAvgMs > AvgMs * 1.2)
		{
			BoundType = TEXT("GPU-bound");
		}
		else if (AvgMs > GpuAvgMs * 1.2)
		{
			BoundType = TEXT("CPU-bound");
		}
		else
		{
			BoundType = TEXT("CPU+GPU bound");
		}
	}
	else
	{
		BoundType = TEXT("CPU-bound (no GPU data)");
	}

	FString Verdict = FString::Printf(TEXT("%s. %.0f FPS median, %.0f%% on budget."),
		*BoundType, 1000.0 / MedianMs, OnBudgetPct);

	if (TotalSpikes > 0)
	{
		Verdict += FString::Printf(TEXT(" %d spike frames (max %.0fms)."), TotalSpikes, MaxMs);
	}

	// ── Step 7: Build result ─────────────────────────────────

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);

	// Overview
	TSharedPtr<FJsonObject> Overview = MakeShared<FJsonObject>();
	Overview->SetNumberField(TEXT("frames"), N);
	Overview->SetNumberField(TEXT("duration_s"), SafeDouble(CurrentSession->GetDurationSeconds()));
	Overview->SetNumberField(TEXT("avg_fps"), SafeDouble(AvgMs > 0.0 ? 1000.0 / AvgMs : 0.0));
	Overview->SetNumberField(TEXT("avg_ms"), SafeDouble(AvgMs));
	Overview->SetNumberField(TEXT("median_ms"), SafeDouble(MedianMs));
	Overview->SetNumberField(TEXT("p95_ms"), SafeDouble(P95Ms));
	Overview->SetNumberField(TEXT("p99_ms"), SafeDouble(P99Ms));
	Overview->SetNumberField(TEXT("target_fps"), TargetFps);
	Overview->SetNumberField(TEXT("on_budget_pct"), SafeDouble(OnBudgetPct));
	if (bHasGpu)
	{
		Overview->SetNumberField(TEXT("gpu_avg_ms"), SafeDouble(GpuAvgMs));
		Overview->SetStringField(TEXT("gpu_queue"), GpuQueueName);
	}
	Result->SetObjectField(TEXT("overview"), Overview);

	Result->SetStringField(TEXT("verdict"), Verdict);
	Result->SetArrayField(TEXT("findings"), FindingsArray);

	// Categories (top 5)
	TArray<TSharedPtr<FJsonValue>> CatsArray;
	int32 CatLimit = FMath::Min(SortedCats.Num(), 5);
	for (int32 i = 0; i < CatLimit; ++i)
	{
		double Pct = TotalCatExclMs > 0.0 ? (SortedCats[i].ExclMs / TotalCatExclMs) * 100.0 : 0.0;
		TSharedPtr<FJsonObject> CatObj = MakeShared<FJsonObject>();
		CatObj->SetStringField(TEXT("name"), GetCategoryName(SortedCats[i].Category));
		CatObj->SetNumberField(TEXT("excl_ms"), SafeDouble(SortedCats[i].ExclMs));
		CatObj->SetNumberField(TEXT("pct"), SafeDouble(Pct));
		CatObj->SetStringField(TEXT("top_timer"), SortedCats[i].TopTimer);
		CatsArray.Add(MakeShared<FJsonValueObject>(CatObj));
	}
	Result->SetArrayField(TEXT("categories"), CatsArray);

	// Top exclusive timers
	TArray<TSharedPtr<FJsonValue>> ExclArray;
	for (const FExclTimer& T : TopExclusive)
	{
		TSharedPtr<FJsonObject> TimerObj = MakeShared<FJsonObject>();
		TimerObj->SetStringField(TEXT("name"), T.Name);
		TimerObj->SetNumberField(TEXT("excl_ms"), SafeDouble(T.PerFrameExclMs));
		TimerObj->SetNumberField(TEXT("incl_ms"), SafeDouble(T.PerFrameInclMs));
		TimerObj->SetNumberField(TEXT("count"), T.Count);
		TimerObj->SetStringField(TEXT("cat"), GetCategoryName(T.Category));
		ExclArray.Add(MakeShared<FJsonValueObject>(TimerObj));
	}
	Result->SetArrayField(TEXT("top_exclusive"), ExclArray);

	return Result;
}

// ────────────────────────────────────────────────────────────
// flame — Top timers by exclusive (self) time
// Shows per-frame average exclusive ms + category label.
// ────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> FEpicUnrealMCPProfilingCommands::HandleGetFlame(
	const TSharedPtr<FJsonObject>& Params)
{
	TraceServices::FAnalysisSessionReadScope ReadScope(*CurrentSession);

	int32 MaxResults = 30;
	if (Params->HasField(TEXT("count")))
	{
		MaxResults = FMath::Clamp(
			static_cast<int32>(Params->GetNumberField(TEXT("count"))), 1, 200);
	}

	FString NameFilter;
	Params->TryGetStringField(TEXT("filter"), NameFilter);

	const TraceServices::ITimingProfilerProvider* TimingProvider =
		TraceServices::ReadTimingProfilerProvider(*CurrentSession);
	if (!TimingProvider)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Timing profiler not available"));
	}

	const TraceServices::IFrameProvider& FrameProvider =
		TraceServices::ReadFrameProvider(*CurrentSession);
	const uint64 GameFrameCount = FrameProvider.GetFrameCount(ETraceFrameType::TraceFrameType_Game);
	double FrameCountDbl = FMath::Max(1.0, static_cast<double>(GameFrameCount));

	double IntervalStart = 0.0;
	double IntervalEnd = CurrentSession->GetDurationSeconds();
	ParseTimeRange(Params, IntervalEnd, IntervalStart, IntervalEnd);

	TraceServices::FCreateAggregationParams AggParams;
	AggParams.IntervalStart = IntervalStart;
	AggParams.IntervalEnd = IntervalEnd;
	AggParams.CpuThreadFilter = [](uint32) { return true; };
	// API only supports sorting by TotalInclusiveTime, so we fetch all and re-sort by exclusive
	AggParams.SortBy = TraceServices::FCreateAggregationParams::ESortBy::TotalInclusiveTime;
	AggParams.SortOrder = TraceServices::FCreateAggregationParams::ESortOrder::Descending;
	AggParams.TableEntryLimit = 0; // Get all so we can re-sort by exclusive

	TraceServices::ITable<TraceServices::FTimingProfilerAggregatedStats>* AggTable =
		TimingProvider->CreateAggregation(AggParams);

	if (!AggTable)
	{
		return FEpicUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create aggregation"));
	}

	// Collect all timers first, then sort by exclusive time
	struct FFlameEntry
	{
		FString Name;
		double PerFrameExclMs;
		double PerFrameInclMs;
		int32 Count;
	};

	TArray<FFlameEntry> AllEntries;
	TraceServices::ITableReader<TraceServices::FTimingProfilerAggregatedStats>* Reader =
		AggTable->CreateReader();

	while (Reader->IsValid())
	{
		const TraceServices::FTimingProfilerAggregatedStats* Row = Reader->GetCurrentRow();
		if (Row && Row->Timer && Row->Timer->Name)
		{
			FString TimerName(Row->Timer->Name);

			if (!NameFilter.IsEmpty() && !TimerName.Contains(NameFilter))
			{
				Reader->NextRow();
				continue;
			}

			double PerFrameExclMs = SafeDouble((Row->TotalExclusiveTime / FrameCountDbl) * 1000.0);
			double PerFrameInclMs = SafeDouble((Row->TotalInclusiveTime / FrameCountDbl) * 1000.0);

			if (PerFrameExclMs > 0.001 || !NameFilter.IsEmpty())
			{
				AllEntries.Add({
					TimerName,
					PerFrameExclMs,
					PerFrameInclMs,
					static_cast<int32>(Row->InstanceCount)
				});
			}
		}
		Reader->NextRow();
	}

	delete Reader;
	delete AggTable;

	// Sort by exclusive time descending
	AllEntries.Sort([](const FFlameEntry& A, const FFlameEntry& B)
	{
		return A.PerFrameExclMs > B.PerFrameExclMs;
	});

	if (AllEntries.Num() > MaxResults)
	{
		AllEntries.SetNum(MaxResults);
	}

	TArray<TSharedPtr<FJsonValue>> TimersArray;
	for (const FFlameEntry& Entry : AllEntries)
	{
		TSharedPtr<FJsonObject> TimerObj = MakeShared<FJsonObject>();
		TimerObj->SetStringField(TEXT("name"), Entry.Name);
		TimerObj->SetNumberField(TEXT("excl_ms"), SafeDouble(Entry.PerFrameExclMs));
		TimerObj->SetNumberField(TEXT("incl_ms"), SafeDouble(Entry.PerFrameInclMs));
		TimerObj->SetNumberField(TEXT("self_pct"),
			SafeDouble(Entry.PerFrameInclMs > 0.0 ? (Entry.PerFrameExclMs / Entry.PerFrameInclMs) * 100.0 : 100.0));
		TimerObj->SetNumberField(TEXT("count"), Entry.Count);
		TimerObj->SetStringField(TEXT("cat"),
			GetCategoryName(CategorizeTimerName(Entry.Name)));

		TimersArray.Add(MakeShared<FJsonValueObject>(TimerObj));
	}

	int32 Count = TimersArray.Num();

	TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(TEXT("success"), true);
	Result->SetNumberField(TEXT("timer_count"), Count);
	Result->SetNumberField(TEXT("frame_count"), static_cast<double>(GameFrameCount));
	Result->SetStringField(TEXT("note"), TEXT("excl_ms/incl_ms are per-frame averages. self_pct = excl/incl ratio."));
	Result->SetArrayField(TEXT("timers"), TimersArray);

	return Result;
}
