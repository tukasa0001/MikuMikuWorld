#pragma once
#include <unordered_map>
#include <vector>
#include <string>

namespace MikuMikuWorld
{
	struct SUSNote
	{
		int tick;
		int lane;
		int width;
		int type;
	};

	struct BPM
	{
		int tick;
		float bpm;
	};

	struct Bar
	{
		int measure;
		int ticksPerMeasure;
		int ticks;
	};

	struct BarLength
	{
		int bar;
		float length;
	};

	struct HiSpeed
	{
		int tick;
		float speed;
	};

	struct SUSMetadata
	{
		std::unordered_map<std::string, std::string> data;
		std::vector<std::string> requests;
		float waveOffset;
		float movieOffset;
		float baseBPM;

		SUSMetadata() :
			waveOffset{ 0 }, movieOffset{ 0 }, baseBPM{ 0 } {}
	};

	struct SUS
	{
		SUSMetadata metadata;
		std::vector<SUSNote> taps;
		std::vector<SUSNote> directionals;
		std::vector<std::vector<SUSNote>> slides;
		std::vector<BPM> bpms;
		std::vector<BarLength> barlengths;
		std::vector<HiSpeed> hiSpeeds;
	};
}