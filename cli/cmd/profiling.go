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
		LargeOp: true,
		Params: []ParamSpec{
			{Name: "file_path", Type: "string", Help: "Output file path (empty = auto)"},
			{Name: "channels", Type: "string", Default: "default", Help: "Trace channels (cpu,gpu,frame,log,bookmark,screenshot,region,net,loadtime,memtag,memalloc)"},
		},
	},
	{
		Name:  "performance_stop_trace",
		Group: "profiling",
		Short: "Stop recording and optionally auto-load for analysis",
		Params: []ParamSpec{
			{Name: "auto_load", Type: "bool", Default: true, Help: "Auto-load trace for analysis"},
		},
	},
	{
		Name:    "performance_analyze_insight",
		Group:   "profiling",
		Short:   "Analyze a performance trace (diagnose, spikes, flame, hotpath, etc.)",
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
