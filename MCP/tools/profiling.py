"""Performance profiling tools — record and analyze .utrace traces."""

from _bridge import mcp
from _tcp_bridge import _call


@mcp.tool()
def performance_start_trace(
    file_path: str = "",
    channels: str = "default",
) -> str:
    """Start recording a performance trace from the running Unreal Editor.

    Records CPU, GPU, frame timing, and other profiling data to a .utrace file.
    Call performance_stop_trace when done to stop recording and auto-load for analysis.

    Args:
        file_path: Where to save the .utrace file. Empty = auto-generate in Saved/Profiling/
        channels: Comma-separated trace channels or preset name.
            Presets: "default" (cpu,gpu,frame,log,bookmark,screenshot,region)
            Individual: cpu, gpu, frame, log, bookmark, screenshot, region, memtag, memalloc
            For network profiling add: net
            For asset loading add: loadtime
            For memory tracking add: memtag
            Example: "default,net,loadtime,memtag" for full profiling
    """
    params = {}
    if file_path:
        params["file_path"] = file_path
    if channels != "default":
        params["channels"] = channels

    return _call("performance_start_trace", params)


@mcp.tool()
def performance_stop_trace(auto_load: bool = True) -> str:
    """Stop the active trace recording and optionally load it for analysis.

    After stopping, the trace is automatically loaded so you can immediately
    query it with performance_analyze_insight.

    Args:
        auto_load: If True (default), auto-load the recorded trace for analysis
    """
    params = {}
    if not auto_load:
        params["auto_load"] = False

    return _call("performance_stop_trace", params)


@mcp.tool()
def performance_analyze_insight(
    query: str,
    trace_path: str = "",
    frame_index: int = -1,
    count: int = 20,
    threshold_ms: float = 0.0,
    max_depth: int = 3,
    min_duration_ms: float = 0.1,
    thread_name: str = "",
    thread: str = "",
    filter: str = "",
    start_time: float = -1.0,
    end_time: float = -1.0,
    include_values: bool = False,
    max_samples: int = 100,
    timer_name: str = "",
    mode: str = "both",
    connection_index: int = -1,
    verbosity: str = "",
    tracker: int = -1,
    target_fps: float = 0.0,
    category: str = "",
    event_name: str = "",
    min_deviation_pct: float = 0.0,
    bucket_size_ms: float = 0.0,
) -> str:
    """Query profiling data from a loaded .utrace trace.

    RECOMMENDED WORKFLOW (compact output, saves context):
      1. "summary"      — Get trace overview (frame count, avg/p95/p99 frame times)
      2. "histogram"    — See frame time distribution (periodic hitches vs sustained?)
      3. "spikes"       — Auto worst frames + category breakdown in ONE call
      4. "bottlenecks"  — Drill into a specific frame's categories
      5. "hotpath"      — Drill into a category or event's children
      6. "search"       — Find a timer across all frames (stats + worst frames)
      7. "compare"      — Compare frame vs trace median, show outlier events

    Use performance_start_trace/stop_trace to record and auto-load a trace,
    or use query="load" to load an existing .utrace file.

    Args:
        query: Type of analysis. One of:

            SMART QUERIES (compact output, do analysis in C++):
            - "bottlenecks"   — Auto-categorize frame into Animation/Slate/Network/etc
                                with total time per category and top event. VERY compact.
            - "hotpath"       — Drill into a category or event's children, sorted by time
            - "compare"       — Compare frame vs trace median, show outlier events only
            - "spikes"        — Auto worst frames + category breakdown (combines worst_frames
                                + bottlenecks). Shows top 3 categories per spike frame.
            - "search"        — Find a timer across ALL frames. Returns min/avg/max/p95/p99
                                stats + worst frames list. Use filter= for timer name.
            - "histogram"     — Frame time distribution. Shows bucket counts + budget
                                summary (on-budget / slightly over / 2x / 4x over).

            STANDARD QUERIES:
            - "load"          — Load an existing .utrace file (requires trace_path)
            - "summary"       — Overview: duration, frame count, avg/min/max/p95 frame time
            - "worst_frames"  — Find the slowest frames with top events
            - "frame_details" — Per-thread timing breakdown for one frame (use bottlenecks first!)
            - "timer_stats"   — Aggregated stats for timers (top N by inclusive time)
            - "butterfly"     — Callers/callees for a specific timer (requires timer_name)
            - "threads"       — List all threads in the trace
            - "counters"      — List counters (GPU memory, draw calls, etc.)

            PROVIDER QUERIES:
            - "net_stats"     — Network profiling (needs 'net' channel)
            - "loading"       — Asset loading analysis (needs 'loadtime' channel)
            - "logs"          — Log messages (needs 'log' channel)
            - "memory"        — LLM memory tags (needs 'memtag' channel)
            - "regions"       — Timing regions (needs 'region' channel)
            - "bookmarks"     — Bookmark events (needs 'bookmark' channel)

        trace_path: (load) Absolute path to a .utrace file
        frame_index: (bottlenecks, hotpath, compare, frame_details) Which frame (0-based)
        target_fps: (bottlenecks, spikes, histogram) Target FPS for budget (default: 60)
        category: (hotpath) Category to drill into: Animation, Slate, Network, Physics,
                  Rendering, Gameplay, Audio, Loading, GarbageCollection, Other
        event_name: (hotpath) Specific event name to show children of
        min_deviation_pct: (compare) Min deviation % to report (default: 50)
        thread: (bottlenecks, hotpath, compare, spikes, search) Thread (default: GameThread)
        count: (worst_frames, spikes, search, timer_stats, hotpath) Max results
        threshold_ms: (worst_frames, spikes) Only show frames slower than this
        max_depth: (frame_details, butterfly, hotpath) Max call stack depth
        min_duration_ms: (frame_details) Hide events shorter than this
        thread_name: (frame_details) Filter to threads containing this string
        filter: (search, timer_stats, counters, logs, memory, regions, bookmarks) Name filter
        start_time: Start of time range in seconds (-1 = trace start)
        end_time: End of time range in seconds (-1 = trace end)
        include_values: (counters) Include sampled values
        max_samples: (counters) Max value samples per counter
        timer_name: (butterfly) Function/scope name to analyze
        mode: (butterfly) "callers", "callees", or "both"
        connection_index: (net_stats) Connection index for packet details
        verbosity: (logs) Filter: Fatal, Error, Warning, Display, Log, Verbose
        tracker: (memory) Memory tracker ID
        bucket_size_ms: (histogram) Custom bucket width in ms (0 = auto based on target_fps)
    """
    params = {"query": query}

    if trace_path:
        params["trace_path"] = trace_path
    if frame_index >= 0:
        params["frame_index"] = frame_index
    if count != 20:
        params["count"] = count
    if threshold_ms > 0.0:
        params["threshold_ms"] = threshold_ms
    if max_depth != 3:
        params["max_depth"] = max_depth
    if min_duration_ms != 0.1:
        params["min_duration_ms"] = min_duration_ms
    if thread_name:
        params["thread_name"] = thread_name
    if thread:
        params["thread"] = thread
    if filter:
        params["filter"] = filter
    if start_time >= 0.0:
        params["start_time"] = start_time
    if end_time >= 0.0:
        params["end_time"] = end_time
    if include_values:
        params["include_values"] = True
    if max_samples != 100:
        params["max_samples"] = max_samples
    if timer_name:
        params["timer_name"] = timer_name
    if mode != "both":
        params["mode"] = mode
    if connection_index >= 0:
        params["connection_index"] = connection_index
    if verbosity:
        params["verbosity"] = verbosity
    if tracker >= 0:
        params["tracker"] = tracker
    if target_fps > 0.0:
        params["target_fps"] = target_fps
    if category:
        params["category"] = category
    if event_name:
        params["event_name"] = event_name
    if min_deviation_pct > 0.0:
        params["min_deviation_pct"] = min_deviation_pct
    if bucket_size_ms > 0.0:
        params["bucket_size_ms"] = bucket_size_ms

    return _call("performance_analyze_insight", params)
