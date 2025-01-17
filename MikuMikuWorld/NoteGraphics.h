#pragma once

namespace MikuMikuWorld
{
	constexpr float flickArrowWidths[] =
	{
		0.95f, 1.25f, 1.8f, 2.3f, 2.6f, 3.2f
	};

	constexpr float flickArrowHeights[] =
	{
		1, 1.05f, 1.2f, 1.4f, 1.5f, 1.6f
	};

	enum class ZIndex : int32_t
	{
		HoldPath,
		Note,
		FlickArrow
	};

	struct NoteTextures
	{
		int notes;
		int holdPath;
		int criticalHoldPath;
	};

	extern NoteTextures noteTextures;
}