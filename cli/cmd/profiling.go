package cmd

func init() {
	ensureGroup("profiling", "Performance Profiling")
	registerCommands(profilingCommands)
}

var profilingCommands = []CommandSpec{
	{
		Name:    "performance_start_trace",
		Group:   "profiling",
		Short:   "Start recording a live .utrace from the running editor",
		Long:    "Begins recording a Unreal Insights .utrace file from the running editor. The trace captures per-frame CPU/GPU timing, log messages, bookmarks, and more depending on which channels are enabled. Leave file_path empty for auto-generated paths. Use the 'default' channel preset for general profiling, or add 'net' for network, 'loadtime' for asset loading, 'memalloc' for memory allocation tracking. Stop recording with performance_stop_trace.",
		Example: `ue-cli performance_start_trace
ue-cli performance_start_trace --channels "default,net"
ue-cli performance_start_trace --file-path "C:/Traces/my_trace.utrace" --channels "cpu,gpu,frame,memalloc"`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "file_path", Type: "string", Help: "Output file path (empty = auto)"},
			{Name: "channels", Type: "string", Default: "default", Help: "Trace channels (cpu,gpu,frame,log,bookmark,screenshot,region,net,loadtime,memtag,memalloc)"},
		},
	},
	{
		Name:    "performance_stop_trace",
		Group:   "profiling",
		Short:   "Stop recording and optionally auto-load for analysis",
		Long:    "Stops the active trace recording. When auto_load is true (default), the trace is immediately loaded for analysis so you can run performance_analyze_insight queries against it without a separate load step.",
		Example: `ue-cli performance_stop_trace
ue-cli performance_stop_trace --auto-load=false`,
		Params: []ParamSpec{
			{Name: "auto_load", Type: "bool", Default: true, Help: "Auto-load trace for analysis"},
		},
	},
	{
		Name:    "performance_analyze_insight",
		Group:   "profiling",
		Short:   "Analyze a performance trace (diagnose, spikes, flame, hotpath, etc.)",
		Long:    `Runs analysis queries against a loaded .utrace file. The recommended workflow is: (1) performance_start_trace to record, (2) performance_stop_trace to stop and auto-load, (3) performance_analyze_insight with query="diagnose" for a full automatic report, then drill down with "spikes", "flame", "hotpath", or "search" as needed. Use query="load" with trace_path to analyze a previously saved trace. The "diagnose" query returns a one-call verdict with severity findings, category breakdown, and top bottlenecks. The "flame" query shows timers by exclusive (self) time to find actual CPU-consuming code. The "search" query finds a specific timer across all frames with min/avg/max/p95/p99 stats.`,
		Example: `ue-cli performance_analyze_insight --query diagnose
ue-cli performance_analyze_insight --query load --trace-path "C:/Traces/my_trace.utrace"
ue-cli performance_analyze_insight --query spikes --count 5 --target-fps 60
ue-cli performance_analyze_insight --query flame --count 10 --thread GameThread
ue-cli performance_analyze_insight --query hotpath --frame-index 42 --category Animation
ue-cli performance_analyze_insight --query search --filter "ConveyorProcessor" --count 10
ue-cli performance_analyze_insight --query butterfly --timer-name "MassProcessor" --mode both
ue-cli performance_analyze_insight --query frame_details --frame-index 100 --max-depth 5 --min-duration-ms 0.5`,
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "query", Type: "string", Required: true, Help: "Query type: diagnose, spikes, flame, bottlenecks, hotpath, compare, search, histogram, load, summary, worst_frames, frame_details, timer_stats, butterfly, threads, counters, net_stats, loading, logs, memory, regions, bookmarks, session, modules, file_io, tasks, context_switches, allocations, stack_samples, screenshots"},
			{Name: "trace_path", Type: "string", Help: "Path to .utrace file (for 'load' query)"},
			{Name: "frame_index", Type: "int", Default: -1, Help: "Frame index"},
			{Name: "count", Type: "int", Default: 20, Help: "Result count"},
			{Name: "threshold_ms", Type: "float", Default: 0.0, Help: "Time threshold in ms"},
			{Name: "max_depth", Type: "int", Default: 3, Help: "Max depth"},
			{Name: "min_duration_ms", Type: "float", Default: 0.1, Help: "Min duration filter"},
			{Name: "thread_name", Type: "string", Help: "Thread name filter"},
			{Name: "thread", Type: "string", Help: "Thread filter (alias)"},
			{Name: "filter", Type: "string", Help: "Name filter"},
			{Name: "start_time", Type: "float", Default: -1.0, Help: "Start time in seconds"},
			{Name: "end_time", Type: "float", Default: -1.0, Help: "End time in seconds"},
			{Name: "include_values", Type: "bool", Default: false, Help: "Include counter values"},
			{Name: "max_samples", Type: "int", Default: 100, Help: "Max samples"},
			{Name: "timer_name", Type: "string", Help: "Timer name (for butterfly query)"},
			{Name: "mode", Type: "string", Default: "both", Help: "Mode (callers/callees/both)"},
			{Name: "connection_index", Type: "int", Default: -1, Help: "Network connection index"},
			{Name: "verbosity", Type: "string", Help: "Log verbosity filter"},
			{Name: "tracker", Type: "int", Default: -1, Help: "Memory tracker index"},
			{Name: "target_fps", Type: "float", Default: 0.0, Help: "Target FPS for analysis"},
			{Name: "category", Type: "string", Help: "Category filter"},
			{Name: "event_name", Type: "string", Help: "Event name filter"},
			{Name: "min_deviation_pct", Type: "float", Default: 0.0, Help: "Min deviation percentage"},
			{Name: "bucket_size_ms", Type: "float", Default: 0.0, Help: "Histogram bucket size"},
		},
	},
}
