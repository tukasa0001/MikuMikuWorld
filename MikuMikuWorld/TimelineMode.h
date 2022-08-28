#pragma once

namespace MikuMikuWorld
{
	enum class TimelineMode
	{
		Select,
		InsertTap,
		InsertLong,
		InsertLongMid,
		InsertFlick,
		MakeCritical,
		InsertBPM,
		InsertTimeSign,
		InsertSkill,
		TimelineToolMax
	};

	enum class ScrollMode
	{
		None,
		Page,
		Smooth,
		ScrollModeMax
	};

	constexpr const char* timelineModes[]
	{
		"Select",
		"Tap",
		"Hold",
		"Hold Step",
		"Flick",
		"Critical",
		"BPM",
		"Time Signature",
		"Skill"
	};

	constexpr const char* scrollModes[]
	{
		"None",
		"Page",
		"Smooth"
	};

	constexpr int divisions[]
	{
		4, 8, 12, 16, 24, 32, 48, 64, 96, 128, 192, -1
	};
}