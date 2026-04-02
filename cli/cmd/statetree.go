package cmd

func init() {
	ensureGroup("statetree", "StateTree AI")
	registerCommands(statetreeCommands)
}

var statetreeCommands = []CommandSpec{
	// -- Read/Query --
	{
		Name:    "get_statetree_info",
		Group:   "statetree",
		Short:   "Get StateTree overview (schema, counts)",
		Long:    "Returns a compact overview of the StateTree: schema class, state count, evaluator count, global task count, parameter count, and color count.",
		Example: `ue-cli get_statetree_info --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
		},
	},
	{
		Name:  "get_statetree_states",
		Group: "statetree",
		Short: "Get state hierarchy tree",
		Long: "Returns the full state hierarchy as a JSON tree. Each state includes name, type, ID, " +
			"index path, task/transition counts, and a children array. Use max_depth to limit recursion " +
			"and name_filter for substring matching on state names.",
		Example: `ue-cli get_statetree_states --asset-path /Game/AI/ST_HollowCrawler
ue-cli get_statetree_states --asset-path /Game/AI/ST_HollowCrawler --max-depth 1
ue-cli get_statetree_states --asset-path /Game/AI/ST_HollowCrawler --name-filter "Attack"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "max_depth", Type: "int", Default: "-1", Help: "Max recursion depth (-1=unlimited, 0=roots only)"},
			{Name: "name_filter", Type: "string", Help: "Case-insensitive substring filter on state names"},
		},
	},
	{
		Name:  "get_statetree_state",
		Group: "statetree",
		Short: "Get detailed info for a single state",
		Long: "Returns full details for one state: all properties, tasks with their properties, " +
			"enter conditions, transitions, and considerations. Identify by index path, name, or GUID.",
		Example: `ue-cli get_statetree_state --asset-path /Game/AI/ST_HollowCrawler --state "0/0"
ue-cli get_statetree_state --asset-path /Game/AI/ST_HollowCrawler --state-name Patrol
ue-cli get_statetree_state --asset-path /Game/AI/ST_HollowCrawler --state-guid "XXXXXXXX..."`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "state", Type: "string", Help: "State index path (e.g. '0/1/2')"},
			{Name: "state_name", Type: "string", Help: "State name (case-insensitive)"},
			{Name: "state_guid", Type: "string", Help: "State GUID string"},
		},
	},
	{
		Name:    "get_statetree_evaluators",
		Group:   "statetree",
		Short:   "Get global evaluators with properties",
		Example: `ue-cli get_statetree_evaluators --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "filter", Type: "string", Help: "Substring filter on evaluator class name"},
		},
	},
	{
		Name:    "get_statetree_global_tasks",
		Group:   "statetree",
		Short:   "Get global tasks with properties",
		Example: `ue-cli get_statetree_global_tasks --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "filter", Type: "string", Help: "Substring filter on task class name"},
		},
	},
	{
		Name:    "get_statetree_parameters",
		Group:   "statetree",
		Short:   "Get tree-level parameters",
		Example: `ue-cli get_statetree_parameters --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
		},
	},
	{
		Name:  "get_statetree_node",
		Group: "statetree",
		Short: "Get detailed info for a single node by GUID",
		Long:  "Returns full details for a task, evaluator, or condition including all properties, instance data, and context (which state it belongs to).",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "node_guid", Type: "string", Required: true, Help: "GUID of the node"},
		},
	},
	{
		Name:  "get_statetree_bindings",
		Group: "statetree",
		Short: "Get property bindings with full path details",
		Long:  "Returns all property bindings with source/target node names, struct IDs, and property paths. Optionally filter to bindings involving a specific node.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "node_guid", Type: "string", Help: "Filter to bindings involving this node"},
		},
	},
	{
		Name:  "get_statetree_full_info",
		Group: "statetree",
		Short: "Get complete StateTree data in one call",
		Long:  "Returns complete StateTree data with verbosity control. 'summary'=counts only, 'standard'=all states/tasks/conditions, 'full'=everything+bindings inline. Use 'sections' to filter: states,evaluators,global_tasks,parameters,bindings.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "verbosity", Type: "string", Help: "summary, standard, or full (default: standard)"},
			{Name: "sections", Type: "string", Help: "Comma-separated section filter (empty=all)"},
		},
	},

	{
		Name:  "search_statetree_nodes",
		Group: "statetree",
		Short: "Search for nodes by class name or category",
		Long:  "Searches entire StateTree for tasks, evaluators, conditions, or considerations matching a class filter. Returns each match with its location context.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "class_filter", Type: "string", Help: "Substring filter on node class name"},
			{Name: "category", Type: "string", Help: "Filter by category: task, evaluator, condition, consideration, global_task"},
		},
	},

	// -- Create/Add --
	{
		Name:    "create_statetree",
		Group:   "statetree",
		Short:   "Create a new StateTree asset",
		Long:    "Creates a new StateTree data asset with the specified schema. Available schemas: MassStateTreeSchema, StateTreeComponentSchema, StateTreeAIComponentSchema, CameraDirectorStateTreeSchema.",
		Example: `ue-cli create_statetree --asset-path /Game/AI/ST_NewTree --schema-class MassStateTreeSchema`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path for the new asset"},
			{Name: "schema_class", Type: "string", Required: true, Help: "Schema class name"},
		},
	},
	{
		Name:  "add_statetree_state",
		Group: "statetree",
		Short: "Add a state to the tree",
		Long:  "Adds a new state. Omit parent params for a root-level state. state_type: State, Group, Linked, LinkedAsset, Subtree.",
		Example: `ue-cli add_statetree_state --asset-path /Game/AI/ST_Tree --name Patrol
ue-cli add_statetree_state --asset-path /Game/AI/ST_Tree --name Attack --state-name Root --state-type State`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "name", Type: "string", Required: true, Help: "Name for the new state"},
			{Name: "state", Type: "string", Help: "Parent state index path"},
			{Name: "state_name", Type: "string", Help: "Parent state name"},
			{Name: "state_guid", Type: "string", Help: "Parent state GUID"},
			{Name: "state_type", Type: "string", Default: "State", Help: "State/Group/Linked/LinkedAsset/Subtree"},
			{Name: "selection_behavior", Type: "string", Help: "TrySelectChildrenInOrder, TrySelectChildrenAtRandom, etc."},
			{Name: "position", Type: "int", Default: "-1", Help: "Insert position (-1=append)"},
		},
	},
	{
		Name:    "add_statetree_task",
		Group:   "statetree",
		Short:   "Add a task to a state",
		Long:    "Adds a task node to a state's task list. Supports engine and custom task classes.",
		Example: `ue-cli add_statetree_task --asset-path /Game/AI/ST_Tree --state-name Patrol --task-class MassEnemyNestPatrolTask --properties '{"CheckInterval":3.0}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "task_class", Type: "string", Required: true, Help: "Task struct name or full path"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
			{Name: "properties", Type: "json", Help: "JSON object for task node properties"},
			{Name: "instance_properties", Type: "json", Help: "JSON object for task instance data properties"},
		},
	},
	{
		Name:    "add_statetree_evaluator",
		Group:   "statetree",
		Short:   "Add a global evaluator",
		Example: `ue-cli add_statetree_evaluator --asset-path /Game/AI/ST_Tree --evaluator-class MassEnemyHasTargetEvaluator`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "evaluator_class", Type: "string", Required: true, Help: "Evaluator struct name or full path"},
			{Name: "properties", Type: "json", Help: "JSON object for evaluator properties"},
			{Name: "instance_properties", Type: "json", Help: "JSON object for instance data properties"},
		},
	},
	{
		Name:    "add_statetree_global_task",
		Group:   "statetree",
		Short:   "Add a global task",
		Example: `ue-cli add_statetree_global_task --asset-path /Game/AI/ST_Tree --task-class MassEnemyTargetLocationEvaluator`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "task_class", Type: "string", Required: true, Help: "Task struct name"},
			{Name: "properties", Type: "json", Help: "JSON object for task properties"},
			{Name: "instance_properties", Type: "json", Help: "JSON object for instance data properties"},
		},
	},
	{
		Name:    "add_statetree_condition",
		Group:   "statetree",
		Short:   "Add a condition to state enter or transition",
		Long:    "Adds a condition node to either a state's enter conditions or a transition's conditions.",
		Example: `ue-cli add_statetree_condition --asset-path /Game/AI/ST_Tree --state-name Patrol --condition-class StateTreeCompareBoolCondition --target enter_conditions --properties '{"bInvert":false}'`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "condition_class", Type: "string", Required: true, Help: "Condition struct name"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
			{Name: "target", Type: "string", Default: "enter_conditions", Help: "enter_conditions or transition"},
			{Name: "transition_index", Type: "int", Help: "Required when target=transition"},
			{Name: "operand", Type: "string", Default: "And", Help: "And or Or"},
			{Name: "properties", Type: "json", Help: "JSON object for condition properties"},
			{Name: "instance_properties", Type: "json", Help: "JSON object for instance data properties"},
		},
	},
	{
		Name:    "add_statetree_transition",
		Group:   "statetree",
		Short:   "Add a transition to a state",
		Long:    "Adds a transition with trigger, target state, priority, and optional delay.",
		Example: `ue-cli add_statetree_transition --asset-path /Game/AI/ST_Tree --state-name Patrol --trigger OnTick --target-state-name WaveMarch --priority Normal`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "trigger", Type: "string", Required: true, Help: "OnStateCompleted/OnStateFailed/OnStateSucceeded/OnTick/OnEvent"},
			{Name: "state", Type: "string", Help: "Source state index path"},
			{Name: "state_name", Type: "string", Help: "Source state name"},
			{Name: "state_guid", Type: "string", Help: "Source state GUID"},
			{Name: "target_state", Type: "string", Help: "Target: index path or TreeSucceeded/TreeFailed/TreeStopped/NextSibling"},
			{Name: "target_state_name", Type: "string", Help: "Target state name"},
			{Name: "target_state_guid", Type: "string", Help: "Target state GUID"},
			{Name: "priority", Type: "string", Default: "Normal", Help: "Low/Normal/Medium/High/Critical"},
			{Name: "delay", Type: "float", Help: "Delay in seconds"},
			{Name: "delay_variance", Type: "float", Help: "Random variance on delay"},
			{Name: "event_tag", Type: "string", Help: "Gameplay tag for OnEvent trigger"},
		},
	},
	{
		Name:    "add_statetree_parameter",
		Group:   "statetree",
		Short:   "Add a tree-level parameter",
		Example: `ue-cli add_statetree_parameter --asset-path /Game/AI/ST_Tree --name MaxHealth --type float`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "name", Type: "string", Required: true, Help: "Parameter name"},
			{Name: "type", Type: "string", Required: true, Help: "Property type (bool/int32/float/FVector/FName/FString/etc.)"},
		},
	},

	// -- Modify --
	{
		Name:  "set_statetree_state_property",
		Group: "statetree",
		Short: "Set a property on a state",
		Long:  "Modifies a state property: Name, Type, SelectionBehavior, Enabled, Tag, Weight, etc.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property to set"},
			{Name: "value", Type: "string", Required: true, Help: "New value"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
		},
	},
	{
		Name:  "set_statetree_node_property",
		Group: "statetree",
		Short: "Set a property on a task/evaluator/condition",
		Long:  "Modifies a property on a node identified by GUID. Target 'node' for the struct itself, 'instance' for instance data.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "node_guid", Type: "string", Required: true, Help: "GUID of the node"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property to set"},
			{Name: "property_value", Type: "json", Required: true, Help: "New value"},
			{Name: "target", Type: "string", Default: "node", Help: "node or instance"},
		},
	},
	{
		Name:  "set_statetree_transition_property",
		Group: "statetree",
		Short: "Set a property on a transition",
		Long:  "Modifies Trigger, Priority, TargetState, DelayDuration, Enabled, etc.",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "transition_index", Type: "int", Required: true, Help: "Transition index"},
			{Name: "property_name", Type: "string", Required: true, Help: "Property to set"},
			{Name: "value", Type: "string", Required: true, Help: "New value"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
		},
	},
	{
		Name:    "set_statetree_schema",
		Group:   "statetree",
		Short:   "Change the schema of a StateTree",
		Example: `ue-cli set_statetree_schema --asset-path /Game/AI/ST_Tree --schema-class MassStateTreeSchema`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "schema_class", Type: "string", Required: true, Help: "Schema class name"},
		},
	},
	{
		Name:  "add_statetree_binding",
		Group: "statetree",
		Short: "Add a property binding between nodes",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "source_node_guid", Type: "string", Required: true, Help: "Source node GUID"},
			{Name: "source_property", Type: "string", Required: true, Help: "Source property path"},
			{Name: "target_node_guid", Type: "string", Required: true, Help: "Target node GUID"},
			{Name: "target_property", Type: "string", Required: true, Help: "Target property path"},
		},
	},
	{
		Name:    "set_statetree_color",
		Group:   "statetree",
		Short:   "Add or update a theme color",
		Example: `ue-cli set_statetree_color --asset-path /Game/AI/ST_Tree --name "Attack" --color "#FF0000"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "name", Type: "string", Required: true, Help: "Color display name"},
			{Name: "color", Type: "string", Required: true, Help: "Hex '#RRGGBB' or 'R,G,B,A' floats"},
		},
	},

	// -- Remove --
	{
		Name:  "remove_statetree_state",
		Group: "statetree",
		Short: "Remove a state and its children",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
		},
	},
	{
		Name:  "remove_statetree_node",
		Group: "statetree",
		Short: "Remove a task/evaluator/condition by GUID",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "node_guid", Type: "string", Required: true, Help: "GUID of the node to remove"},
		},
	},
	{
		Name:  "remove_statetree_transition",
		Group: "statetree",
		Short: "Remove a transition from a state",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "transition_index", Type: "int", Required: true, Help: "Index of the transition"},
			{Name: "state", Type: "string", Help: "State index path"},
			{Name: "state_name", Type: "string", Help: "State name"},
			{Name: "state_guid", Type: "string", Help: "State GUID"},
		},
	},
	{
		Name:  "remove_statetree_binding",
		Group: "statetree",
		Short: "Remove a property binding",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "binding_index", Type: "int", Required: true, Help: "Index of the binding to remove"},
		},
	},
	{
		Name:  "remove_statetree_parameter",
		Group: "statetree",
		Short: "Remove a tree-level parameter",
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "name", Type: "string", Required: true, Help: "Parameter name to remove"},
		},
	},

	// -- Compile/Discovery --
	{
		Name:    "compile_statetree",
		Group:   "statetree",
		Short:   "Compile a StateTree and return errors",
		Example: `ue-cli compile_statetree --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
		},
	},
	{
		Name:  "list_statetree_node_types",
		Group: "statetree",
		Short: "List available task/evaluator/condition types",
		Long:  "Lists all available node types for the StateTree's schema. Use category to filter by task, evaluator, or condition.",
		Example: `ue-cli list_statetree_node_types --asset-path /Game/AI/ST_Tree --category task
ue-cli list_statetree_node_types --asset-path /Game/AI/ST_Tree --category evaluator --filter "Mass"`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
			{Name: "category", Type: "string", Required: true, Help: "task, evaluator, or condition"},
			{Name: "filter", Type: "string", Help: "Substring filter on class name"},
		},
	},
	{
		Name:  "get_statetree_transition_targets",
		Group: "statetree",
		Short: "List valid transition targets for a StateTree",
		Long: "Returns all states (name, GUID, index_path, type) and meta-transition targets " +
			"(None, TreeSucceeded, TreeFailed, TreeStopped, NextSibling, NextState, NextSelectableState). " +
			"Use this to discover valid values for target_state when adding or modifying transitions.",
		Example: `ue-cli get_statetree_transition_targets --asset-path /Game/AI/ST_HollowCrawler`,
		Params: []ParamSpec{
			{Name: "asset_path", Type: "string", Required: true, Help: "Content path to the StateTree asset"},
		},
	},
	{
		Name:  "list_statetree_enum_values",
		Group: "statetree",
		Short: "List valid enum values for StateTree properties",
		Long: "Returns all valid values for a StateTree enum category. Use before creating states, " +
			"transitions, or conditions to discover valid values. " +
			"Categories: trigger, priority, state_type, selection_behavior, operand.",
		Example: `ue-cli list_statetree_enum_values --category trigger
ue-cli list_statetree_enum_values --category priority`,
		Params: []ParamSpec{
			{Name: "category", Type: "string", Required: true, Help: "trigger/priority/state_type/selection_behavior/operand"},
		},
	},
}
