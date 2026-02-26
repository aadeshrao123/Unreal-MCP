#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "TraceServices/Model/AnalysisSession.h"

namespace TraceServices
{
	class IAnalysisService;
}

/**
 * Handler for Performance Profiling MCP commands.
 *
 * Two workflows:
 *   1. Record: performance_start_trace -> (do stuff) -> performance_stop_trace
 *      Stop automatically loads the recorded trace for analysis.
 *   2. Load existing: performance_analyze_insight(query="load", trace_path="...")
 *
 * Then query with performance_analyze_insight(query="summary"|"bottlenecks"|...).
 *
 * Smart queries (bottlenecks, hotpath, compare) do analysis in C++ and return
 * compact results instead of dumping raw data.
 */
class UNREALMCPBRIDGE_API FEpicUnrealMCPProfilingCommands
{
public:
	FEpicUnrealMCPProfilingCommands();
	~FEpicUnrealMCPProfilingCommands();

	TSharedPtr<FJsonObject> HandleCommand(
		const FString& CommandType,
		const TSharedPtr<FJsonObject>& Params);

private:
	// ── Recording ─────────────────────────────────────────────

	/** Start recording a live trace via FTraceAuxiliary. */
	TSharedPtr<FJsonObject> HandleStartTrace(const TSharedPtr<FJsonObject>& Params);

	/** Stop recording and auto-load the trace for analysis. */
	TSharedPtr<FJsonObject> HandleStopTrace(const TSharedPtr<FJsonObject>& Params);

	// ── Analysis Queries (EpicUnrealMCPProfilingAnalysis.cpp) ─

	/** Unified query dispatcher (summary, worst_frames, frame_details, etc.). */
	TSharedPtr<FJsonObject> HandleAnalyzeInsight(const TSharedPtr<FJsonObject>& Params);

	/** Load an existing .utrace file for analysis. */
	TSharedPtr<FJsonObject> HandleLoadTrace(const TSharedPtr<FJsonObject>& Params);

	/** High-level summary (duration, frame count, avg/min/max frame time). */
	TSharedPtr<FJsonObject> HandleGetSummary(const TSharedPtr<FJsonObject>& Params);

	/** Find the N slowest frames. */
	TSharedPtr<FJsonObject> HandleGetWorstFrames(const TSharedPtr<FJsonObject>& Params);

	/** Hierarchical per-thread timing for a single frame. */
	TSharedPtr<FJsonObject> HandleGetFrameDetails(const TSharedPtr<FJsonObject>& Params);

	/** Aggregated timer stats across a time range. */
	TSharedPtr<FJsonObject> HandleGetTimerStats(const TSharedPtr<FJsonObject>& Params);

	/** List all threads in the trace. */
	TSharedPtr<FJsonObject> HandleGetThreads(const TSharedPtr<FJsonObject>& Params);

	/** List counters with optional value sampling. */
	TSharedPtr<FJsonObject> HandleGetCounters(const TSharedPtr<FJsonObject>& Params);

	/** Butterfly (callers/callees) analysis for a specific timer. */
	TSharedPtr<FJsonObject> HandleGetButterfly(const TSharedPtr<FJsonObject>& Params);

	// ── Smart Analysis Queries (EpicUnrealMCPProfilingAnalysis.cpp) ─

	/** Auto-categorize a frame's timing into buckets (Animation, Slate, Network, etc.)
	 *  Returns compact summary: category totals + top event per category.
	 *  ~200 bytes output vs ~5-15KB from frame_details. */
	TSharedPtr<FJsonObject> HandleGetBottlenecks(const TSharedPtr<FJsonObject>& Params);

	/** Drill into a specific category or event within a frame.
	 *  Returns sorted child events for targeted investigation. */
	TSharedPtr<FJsonObject> HandleGetHotpath(const TSharedPtr<FJsonObject>& Params);

	/** Compare a frame against trace-wide median to find outlier events.
	 *  Returns only events that deviate significantly from normal. */
	TSharedPtr<FJsonObject> HandleGetCompare(const TSharedPtr<FJsonObject>& Params);

	/** Auto-detect worst frames AND categorize them in one call.
	 *  Combines worst_frames + bottlenecks — each spike frame gets top 3 categories. */
	TSharedPtr<FJsonObject> HandleGetSpikes(const TSharedPtr<FJsonObject>& Params);

	/** Find a specific timer across all frames in the trace.
	 *  Returns min/avg/max/p95/p99 stats + worst frames list. */
	TSharedPtr<FJsonObject> HandleGetSearch(const TSharedPtr<FJsonObject>& Params);

	/** Frame time distribution histogram for pattern detection.
	 *  Returns bucket counts + budget summary (on-budget / over 2x / over 4x). */
	TSharedPtr<FJsonObject> HandleGetHistogram(const TSharedPtr<FJsonObject>& Params);

	// ── Provider Queries (EpicUnrealMCPProfilingProviders.cpp) ─

	/** Network profiling - game instances, connections, packet overview. */
	TSharedPtr<FJsonObject> HandleGetNetStats(const TSharedPtr<FJsonObject>& Params);

	/** Asset loading analysis - packages, exports, requests. */
	TSharedPtr<FJsonObject> HandleGetLoading(const TSharedPtr<FJsonObject>& Params);

	/** Log messages from the trace. */
	TSharedPtr<FJsonObject> HandleGetLogs(const TSharedPtr<FJsonObject>& Params);

	/** LLM memory tag tracking. */
	TSharedPtr<FJsonObject> HandleGetMemory(const TSharedPtr<FJsonObject>& Params);

	/** Timing regions by category. */
	TSharedPtr<FJsonObject> HandleGetRegions(const TSharedPtr<FJsonObject>& Params);

	/** Bookmark events from the trace. */
	TSharedPtr<FJsonObject> HandleGetBookmarks(const TSharedPtr<FJsonObject>& Params);

	// ── Helpers ───────────────────────────────────────────────

	TSharedPtr<FJsonObject> EnsureSession() const;
	TSharedPtr<FJsonObject> EnsureAnalysisService();

	// ── State ─────────────────────────────────────────────────

	TSharedPtr<TraceServices::IAnalysisService> AnalysisService;
	TSharedPtr<const TraceServices::IAnalysisSession> CurrentSession;
	FString LoadedTracePath;

	/** Path of the .utrace file being recorded (empty when not recording). */
	FString RecordingTracePath;
	bool bIsRecording = false;
};
