#pragma once

#include "CoreMinimal.h"
#include "Json.h"
#include "TraceServices/Model/TimingProfiler.h"
#include <cmath>

// Shared log category for all profiling command files
DECLARE_LOG_CATEGORY_EXTERN(LogMCPProfiling, Log, All);

// ────────────────────────────────────────────────────────────
// Numeric safety
// ────────────────────────────────────────────────────────────

// JSON doesn't support inf/nan - clamp to fallback for safety
inline double SafeDouble(double Value, double Fallback = 0.0)
{
	return std::isfinite(Value) ? Value : Fallback;
}

// ────────────────────────────────────────────────────────────
// Butterfly serialization
// ────────────────────────────────────────────────────────────

// Recursively serialize a butterfly node tree to JSON
inline TSharedPtr<FJsonObject> SerializeButterflyNode(
	const TraceServices::FTimingProfilerButterflyNode& Node,
	int32 MaxDepth,
	int32 CurrentDepth = 0)
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();

	if (Node.Timer && Node.Timer->Name)
	{
		Obj->SetStringField(TEXT("name"), FString(Node.Timer->Name));
	}
	else
	{
		Obj->SetStringField(TEXT("name"), TEXT("<unknown>"));
	}

	Obj->SetNumberField(TEXT("count"), static_cast<double>(Node.Count));
	Obj->SetNumberField(TEXT("inclusive_ms"), SafeDouble(Node.InclusiveTime * 1000.0));
	Obj->SetNumberField(TEXT("exclusive_ms"), SafeDouble(Node.ExclusiveTime * 1000.0));

	if (CurrentDepth < MaxDepth && Node.Children.Num() > 0)
	{
		TArray<const TraceServices::FTimingProfilerButterflyNode*> SortedChildren;
		for (const auto* Child : Node.Children)
		{
			if (Child)
			{
				SortedChildren.Add(Child);
			}
		}
		SortedChildren.Sort([](const TraceServices::FTimingProfilerButterflyNode& A,
			const TraceServices::FTimingProfilerButterflyNode& B)
		{
			return A.InclusiveTime > B.InclusiveTime;
		});

		TArray<TSharedPtr<FJsonValue>> ChildrenArray;
		int32 ChildLimit = FMath::Min(SortedChildren.Num(), 20);
		for (int32 i = 0; i < ChildLimit; ++i)
		{
			ChildrenArray.Add(MakeShared<FJsonValueObject>(
				SerializeButterflyNode(*SortedChildren[i], MaxDepth, CurrentDepth + 1)));
		}
		Obj->SetArrayField(TEXT("children"), ChildrenArray);
	}

	return Obj;
}

// ────────────────────────────────────────────────────────────
// Category bucketing for smart analysis queries
// ────────────────────────────────────────────────────────────

enum class EProfilingCategory : uint8
{
	Animation,
	Slate,
	Network,
	Physics,
	Rendering,
	Gameplay,
	Audio,
	Loading,
	GarbageCollection,
	Other,

	MAX
};

inline const TCHAR* GetCategoryName(EProfilingCategory Cat)
{
	switch (Cat)
	{
	case EProfilingCategory::Animation:         return TEXT("Animation");
	case EProfilingCategory::Slate:             return TEXT("Slate");
	case EProfilingCategory::Network:           return TEXT("Network");
	case EProfilingCategory::Physics:           return TEXT("Physics");
	case EProfilingCategory::Rendering:         return TEXT("Rendering");
	case EProfilingCategory::Gameplay:          return TEXT("Gameplay");
	case EProfilingCategory::Audio:             return TEXT("Audio");
	case EProfilingCategory::Loading:           return TEXT("Loading");
	case EProfilingCategory::GarbageCollection: return TEXT("GarbageCollection");
	default:                                    return TEXT("Other");
	}
}

// Categorize a timer name into a profiling bucket.
// Checked in priority order: specific categories first, generic last.
inline EProfilingCategory CategorizeTimerName(const FString& TimerName)
{
	// Animation (check before Gameplay since "Tick" is too broad)
	if (TimerName.Contains(TEXT("Anim"))
		|| TimerName.Contains(TEXT("Skeletal"))
		|| TimerName.Contains(TEXT("CompletionEvents"))
		|| TimerName.Contains(TEXT("Mutable"))
		|| TimerName.Contains(TEXT("Montage"))
		|| TimerName.Contains(TEXT("Pose")))
	{
		return EProfilingCategory::Animation;
	}

	// Slate / UI
	if (TimerName.Contains(TEXT("Slate"))
		|| TimerName.Contains(TEXT("Widget"))
		|| TimerName.Contains(TEXT("HUD"))
		|| TimerName.Contains(TEXT("UMG"))
		|| TimerName.Contains(TEXT("SWindow"))
		|| TimerName.Contains(TEXT("DrawWindow")))
	{
		return EProfilingCategory::Slate;
	}

	// Network
	if (TimerName.Contains(TEXT("Net"))
		|| TimerName.Contains(TEXT("Replication"))
		|| TimerName.Contains(TEXT("RPC"))
		|| TimerName.Contains(TEXT("Broadcast"))
		|| TimerName.Contains(TEXT("Bunch")))
	{
		return EProfilingCategory::Network;
	}

	// Physics
	if (TimerName.Contains(TEXT("Physics"))
		|| TimerName.Contains(TEXT("Chaos"))
		|| TimerName.Contains(TEXT("Collision"))
		|| TimerName.Contains(TEXT("Simulate"))
		|| TimerName.Contains(TEXT("PhysX")))
	{
		return EProfilingCategory::Physics;
	}

	// Rendering
	if (TimerName.Contains(TEXT("Render"))
		|| TimerName.Contains(TEXT("Draw"))
		|| TimerName.Contains(TEXT("RHI"))
		|| TimerName.Contains(TEXT("Scene"))
		|| TimerName.Contains(TEXT("Niagara"))
		|| TimerName.Contains(TEXT("Particle"))
		|| TimerName.Contains(TEXT("ISM"))
		|| TimerName.Contains(TEXT("EndOfFrame")))
	{
		return EProfilingCategory::Rendering;
	}

	// Audio
	if (TimerName.Contains(TEXT("Audio"))
		|| TimerName.Contains(TEXT("Sound")))
	{
		return EProfilingCategory::Audio;
	}

	// Loading
	if (TimerName.Contains(TEXT("Load"))
		|| TimerName.Contains(TEXT("AsyncLoad"))
		|| TimerName.Contains(TEXT("Stream")))
	{
		return EProfilingCategory::Loading;
	}

	// Garbage Collection
	if (TimerName.Contains(TEXT("GC"))
		|| TimerName.Contains(TEXT("Garbage"))
		|| TimerName.Contains(TEXT("Cluster"))
		|| TimerName.Contains(TEXT("Destruct")))
	{
		return EProfilingCategory::GarbageCollection;
	}

	// Gameplay (broad catch-all for game logic)
	if (TimerName.Contains(TEXT("Tick"))
		|| TimerName.Contains(TEXT("Component"))
		|| TimerName.Contains(TEXT("Actor"))
		|| TimerName.Contains(TEXT("World"))
		|| TimerName.Contains(TEXT("Gameplay"))
		|| TimerName.Contains(TEXT("Blueprint"))
		|| TimerName.Contains(TEXT("NavMesh"))
		|| TimerName.Contains(TEXT("AI")))
	{
		return EProfilingCategory::Gameplay;
	}

	return EProfilingCategory::Other;
}

// ────────────────────────────────────────────────────────────
// Category-specific optimization recommendations
// ────────────────────────────────────────────────────────────

inline const TCHAR* GetCategoryRecommendation(EProfilingCategory Cat)
{
	switch (Cat)
	{
	case EProfilingCategory::Animation:
		return TEXT("Reduce skeletal meshes, enable URO, simplify AnimBPs, use LODs");
	case EProfilingCategory::Slate:
		return TEXT("Reduce widget count, use invalidation boxes, cache widget refs");
	case EProfilingCategory::Network:
		return TEXT("Reduce replicated properties, batch RPCs, use net relevancy");
	case EProfilingCategory::Physics:
		return TEXT("Simplify collision, reduce physics bodies, use async queries");
	case EProfilingCategory::Rendering:
		return TEXT("Reduce draw calls, use Nanite/instancing, optimize materials");
	case EProfilingCategory::Gameplay:
		return TEXT("Reduce tick frequency, use timers, optimize BP event graphs");
	case EProfilingCategory::Audio:
		return TEXT("Reduce concurrent sounds, use concurrency, pool AudioComponents");
	case EProfilingCategory::Loading:
		return TEXT("Use async loading, stream assets, pre-cache on level load");
	case EProfilingCategory::GarbageCollection:
		return TEXT("Reduce UObject churn, use pools, extend GC interval");
	default:
		return TEXT("");
	}
}

// ────────────────────────────────────────────────────────────
// Common time-range parameter parsing
// ────────────────────────────────────────────────────────────

inline void ParseTimeRange(
	const TSharedPtr<FJsonObject>& Params,
	double TraceDuration,
	double& OutStart,
	double& OutEnd)
{
	OutStart = 0.0;
	OutEnd = TraceDuration;

	if (Params->HasField(TEXT("start_time")))
	{
		double Val = Params->GetNumberField(TEXT("start_time"));
		if (Val >= 0.0)
		{
			OutStart = Val;
		}
	}
	if (Params->HasField(TEXT("end_time")))
	{
		double Val = Params->GetNumberField(TEXT("end_time"));
		if (Val >= 0.0)
		{
			OutEnd = Val;
		}
	}
}
