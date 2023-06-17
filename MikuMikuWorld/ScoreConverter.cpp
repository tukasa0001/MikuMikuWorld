#include "ScoreConverter.h"
#include "SUS.h"
#include "Score.h"
#include <stdexcept>
#include "Constants.h"
#include <unordered_set>
#include <algorithm>

namespace MikuMikuWorld
{
	std::string ScoreConverter::noteKey(const SUSNote& note)
	{
		return std::to_string(note.tick) + "-" + std::to_string(note.lane);
	}

	std::pair<int, int> ScoreConverter::barLengthToFraction(float length, float fractionDenom)
	{
		int factor = 1;
		for (int i = 2; i < 10; ++i)
		{
			if (fmodf(factor * length, 1) == 0)
				return std::pair<int, int>(factor * length, pow(2, i));

			factor *= 2;
		}

		return std::pair<int, int>(4, 4);
	}

	Score ScoreConverter::susToScore(const SUS& sus)
	{
		ScoreMetadata metadata
		{
			sus.metadata.data.at("title"),
			sus.metadata.data.at("artist"),
			sus.metadata.data.at("designer"),
			"",
			"",
			sus.metadata.waveOffset * 1000 // seconds -> milliseconds
		};

		std::unordered_map<std::string, FlickType> flicks;
		std::unordered_set<std::string> criticals;
		std::unordered_set<std::string> stepIgnore;
		std::unordered_set<std::string> easeIns;
		std::unordered_set<std::string> easeOuts;
		std::unordered_set<std::string> slideKeys;

		for (const auto& slide : sus.slides)
		{
			for (const auto& note : slide)
			{
				switch (note.type)
				{
				case 1:
				case 2:
				case 3:
				case 5:
					slideKeys.insert(noteKey(note));
				}
			}
		}

		for (const auto& dir : sus.directionals)
		{
			const std::string key = noteKey(dir);
			switch (dir.type)
			{
			case 1:
				flicks.insert_or_assign(key, FlickType::Default);
				break;
			case 3:
				flicks.insert_or_assign(key, FlickType::Left);
				break;
			case 4:
				flicks.insert_or_assign(key, FlickType::Right);
				break;
			case 2:
				easeIns.insert(key);
				break;
			case 5:
			case 6:
				easeOuts.insert(key);
				break;
			default:
				break;
			}
		}

		for (const auto& tap : sus.taps)
		{
			const std::string key = noteKey(tap);
			switch (tap.type)
			{
			case 2:
				criticals.insert(key);
				break;
			case 3:
				stepIgnore.insert(key);
				break;
			default:
				break;
			}
		}

		std::unordered_set<std::string> tapKeys;
		tapKeys.reserve(sus.taps.size());

		std::unordered_map<int, Note> notes;
		notes.reserve(sus.taps.size());

		std::unordered_map<int, HoldNote> holds;
		holds.reserve(sus.slides.size());

		std::vector<SkillTrigger> skills;
		Fever fever{ -1, -1 };

		for (const auto& note : sus.taps)
		{
			if (note.lane == 0 && note.width == 1 && note.type == 4)
			{
				skills.push_back(SkillTrigger{ nextSkillID++, note.tick });
			}
			else if (note.lane == 15 && note.width == 1)
			{
				if (note.type == 1)
					fever.startTick = note.tick;
				else if (note.type == 2)
					fever.endTick = note.tick;
			}

			if (note.lane - 2 < MIN_LANE || note.lane - 2 > MAX_LANE)
				continue;

			const std::string key = noteKey(note);
			if (slideKeys.find(key) != slideKeys.end() || (note.type != 1 && note.type != 2))
				continue;

			if (tapKeys.find(key) != tapKeys.end())
				continue;

			tapKeys.insert(key);
			Note n(NoteType::Tap);
			n.tick = note.tick;
			n.lane = note.lane - 2;
			n.width = note.width;
			n.critical = note.type == 2;
			n.flick = flicks.find(key) != flicks.end() ? flicks[key] : FlickType::None;
			n.ID = nextID++;

			notes[n.ID] = n;
		}

		for (const auto& slide : sus.slides)
		{
			const std::string key = noteKey(slide[0]);

			auto start = std::find_if(slide.begin(), slide.end(),
				[](const SUSNote& a) { return a.type == 1 || a.type == 2; });

			if (start == slide.end())
				continue;

			bool critical = criticals.find(key) != criticals.end();

			HoldNote hold;
			int startID = nextID++;
			hold.steps.reserve(slide.size() - 2);

			for (const auto& note : slide)
			{
				const std::string key = noteKey(note);

				EaseType ease = EaseType::Linear;
				if (easeIns.find(key) != easeIns.end())
					ease = EaseType::EaseIn;
				else if (easeOuts.find(key) != easeOuts.end())
					ease = EaseType::EaseOut;

				switch (note.type)
				{
					// start
				case 1:
				{
					Note n(NoteType::Hold);
					n.tick = note.tick;
					n.lane = note.lane - 2;
					n.width = note.width;
					n.critical = critical;
					n.ID = startID;

					notes[n.ID] = n;
					hold.start = HoldStep{ n.ID, HoldStepType::Normal, ease };
				}
				break;
				// end
				case 2:
				{
					Note n(NoteType::HoldEnd);
					n.tick = note.tick;
					n.lane = note.lane - 2;
					n.width = note.width;
					n.critical = (critical ? true : (criticals.find(key) != criticals.end()));
					n.ID = nextID++;
					n.parentID = startID;
					n.flick = flicks.find(key) != flicks.end() ? flicks[key] : FlickType::None;

					notes[n.ID] = n;
					hold.end = n.ID;
				}
				break;
				// mid
				case 3:
				case 5:
				{

					Note n(NoteType::HoldMid);
					n.tick = note.tick;
					n.lane = note.lane - 2;
					n.width = note.width;
					n.critical = critical;
					n.ID = nextID++;
					n.parentID = startID;

					HoldStepType type = note.type == 3 ? HoldStepType::Normal : HoldStepType::Hidden;
					if (stepIgnore.find(key) != stepIgnore.end())
						type = HoldStepType::Skip;

					notes[n.ID] = n;
					hold.steps.push_back(HoldStep{ n.ID, type, ease });
				}
				break;
				default:
					break;
				}
			}

			if (hold.start.ID == 0 || hold.end == 0)
				throw std::runtime_error("Invalid hold note.");

			holds[startID] = hold;
			if (notes.at(hold.end).critical && !notes.at(hold.end).isFlick())
			{
				notes.at(hold.start.ID).critical = true;
				for (const auto& step : hold.steps)
					notes.at(step.ID).critical = true;
			}
		}

		std::vector<Tempo> tempos;
		tempos.reserve(sus.bpms.size());
		for (const auto& tempo : sus.bpms)
			tempos.push_back(Tempo(tempo.tick, tempo.bpm));

		std::map<int, TimeSignature> timeSignatures;
		for (const auto& sign : sus.barlengths)
		{
			auto fraction = barLengthToFraction(sign.length, 4.0f);
			timeSignatures.insert(std::pair<int, TimeSignature>(sign.bar,
				TimeSignature{ sign.bar, fraction.first, fraction.second }));
		}

		std::vector<HiSpeedChange> hiSpeedChanges;
		hiSpeedChanges.reserve(sus.hiSpeeds.size());
		for (const auto& speed : sus.hiSpeeds)
			hiSpeedChanges.push_back({ speed.tick, speed.speed });

		Score score;
		score.metadata = metadata;
		score.notes = notes;
		score.holdNotes = holds;
		score.tempoChanges = tempos;
		score.timeSignatures = timeSignatures;
		score.hiSpeedChanges = hiSpeedChanges;
		score.skills = skills;
		score.fever = fever;

		return score;
	}

	SUS ScoreConverter::scoreToSus(const Score& score)
	{
		std::unordered_map<FlickType, int> flickToType;
		flickToType[FlickType::Default] = 1;
		flickToType[FlickType::Left] = 3;
		flickToType[FlickType::Right] = 4;

		std::vector<SUSNote> taps, directionals;
		std::vector<std::vector<SUSNote>> slides;
		std::vector<BPM> bpms;
		std::vector<BarLength> barlengths;
		std::vector<HiSpeed> hiSpeeds;

		for (const auto& [id, note] : score.notes)
		{
			if (note.getType() == NoteType::Tap)
			{
				int noteTypeId = note.critical ? 2 : 1;
				if (note.trace) noteTypeId = 3;
				taps.push_back(SUSNote{ note.tick, note.lane + 2, note.width, noteTypeId });
				if (note.isFlick())
					directionals.push_back(SUSNote{ note.tick, note.lane + 2, note.width, flickToType[note.flick] });
				if(note.trace && note.critical)
					taps.push_back(SUSNote{ note.tick, note.lane + 2, note.width, 2 });
			}
			else if (note.getType() == NoteType::Damage)
			{
				taps.push_back(SUSNote{ note.tick, note.lane + 2, note.width, 4});
			}
		}

		for (const auto& [id, hold] : score.holdNotes)
		{
			std::vector<SUSNote> slide;
			slide.reserve(hold.steps.size() + 2);

			const Note& start = score.notes.at(hold.start.ID);

			slide.push_back(SUSNote{ start.tick, start.lane + 2, start.width, 1 });
			EaseType ease = hold.start.ease;
			if (ease != EaseType::Linear)
			{
				taps.push_back(SUSNote{ start.tick, start.lane + 2, start.width, 1 });
				directionals.push_back(SUSNote{ start.tick, start.lane + 2, start.width, ease == EaseType::EaseIn ? 2 : 6 });
			}

			if (start.critical)
				taps.push_back(SUSNote{ start.tick, start.lane + 2, start.width, 2 });

			for (const auto& step : hold.steps)
			{
				const Note& midNote = score.notes.at(step.ID);

				slide.push_back(SUSNote{ midNote.tick, midNote.lane + 2, midNote.width, step.type == HoldStepType::Hidden ? 5 : 3 });
				if (step.type == HoldStepType::Skip)
					taps.push_back(SUSNote{ midNote.tick, midNote.lane + 2, midNote.width, 3 });
				else if (step.ease != EaseType::Linear)
				{
					taps.push_back(SUSNote{ midNote.tick, midNote.lane + 2, midNote.width, 1 });
					directionals.push_back(SUSNote{ midNote.tick, midNote.lane + 2, midNote.width, step.ease == EaseType::EaseIn ? 2 : 6 });
				}
			}

			const Note& end = score.notes.at(hold.end);

			slide.push_back(SUSNote{ end.tick, end.lane + 2, end.width, 2 });
			if (end.isFlick())
			{
				directionals.push_back(SUSNote{ end.tick, end.lane + 2, end.width, flickToType[end.flick] });
				if (end.critical)
					taps.push_back(SUSNote{ end.tick, end.lane + 2, end.width, 2 });
			}

			slides.push_back(slide);
		}

		for (const auto& skill : score.skills)
			taps.push_back(SUSNote{ skill.tick, 0, 1, 4 });

		if (score.fever.startTick != -1)
			taps.push_back(SUSNote{ score.fever.startTick, 15, 1, 1 });

		if (score.fever.endTick != -1)
			taps.push_back(SUSNote{ score.fever.endTick, 15, 1, 2 });

		for (const auto& tempo : score.tempoChanges)
			bpms.push_back(BPM{ tempo.tick, tempo.bpm });

		if (!bpms.size())
			bpms.push_back(BPM{ 0, 120 });

		for (const auto& [measure, ts] : score.timeSignatures)
			barlengths.push_back(BarLength{ ts.measure, ((float)ts.numerator / (float)ts.denominator) * 4 });

		hiSpeeds.reserve(score.hiSpeedChanges.size());
		for (const auto& hiSpeed : score.hiSpeedChanges)
			hiSpeeds.push_back(HiSpeed{ hiSpeed.tick, hiSpeed.speed });

		SUSMetadata metadata;
		metadata.data["title"] = score.metadata.title;
		metadata.data["artist"] = score.metadata.artist;
		metadata.data["designer"] = score.metadata.author;
		metadata.requests.push_back("ticks_per_beat 480");

		// milliseconds -> seconds
		metadata.waveOffset = score.metadata.musicOffset / 1000.0f;

		return SUS{ metadata, taps, directionals, slides, bpms, barlengths, hiSpeeds };
	}
}