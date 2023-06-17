#include "JsonIO.h"

using namespace nlohmann;

namespace jsonIO
{
	mmw::Note jsonToNote(const json& data, mmw::NoteType type)
	{
		if (data.find("damage") != data.end() && data["damage"]) {
			type = mmw::NoteType::Damage;
		}

		mmw::Note note(type);

		if (data.find("tick") != data.end())
			note.tick = data["tick"];

		if (data.find("lane") != data.end())
			note.lane = data["lane"];

		if (data.find("width") != data.end())
			note.width = data["width"];
		else
			note.width = 3;

		if (note.getType() != mmw::NoteType::HoldMid && data.find("critical") != data.end())
			note.critical = data["critical"];

		if (data.find("trace") != data.end())
			note.trace = data["trace"];

		if (!note.hasEase())
		{
			std::string flickString = data["flick"];
			std::transform(flickString.begin(), flickString.end(), flickString.begin(), ::tolower);

			if (flickString == "up" || flickString == "default")
				note.flick = mmw::FlickType::Default;
			else if (flickString == "left")
				note.flick = mmw::FlickType::Left;
			else if (flickString == "right")
				note.flick = mmw::FlickType::Right;

			// default -> FlickType::None
		}

		return note;
	}

	static json noteToJson(const mmw::Note& note)
	{
		json data;
		data["tick"] = note.tick;
		data["lane"] = note.lane;
		data["width"] = note.width;
		data["trace"] = note.trace;
		data["damage"] = note.getType() == mmw::NoteType::Damage;

		if (note.getType() != mmw::NoteType::HoldMid)
			data["critical"] = note.critical;

		if (!note.hasEase())
			data["flick"] = mmw::flickTypes[(int)note.flick];

		return data;
	}

	json noteSelectionToJson(const mmw::Score& score, const std::unordered_set<int>& selection, int baseTick)
	{
		json data, notes, holds;
		std::unordered_set<int> selectedNotes;
		std::unordered_set<int> selectedHolds;

		for (int id : selection)
		{
			if (score.notes.find(id) == score.notes.end())
				continue;

			const mmw::Note& note = score.notes.at(id);
			switch (note.getType())
			{
			case mmw::NoteType::Tap:
			case mmw::NoteType::Damage: selectedNotes.insert(note.ID); break;
			case mmw::NoteType::Hold: selectedHolds.insert(note.ID); break;
			case mmw::NoteType::HoldMid:
			case mmw::NoteType::HoldEnd:
				selectedHolds.insert(note.parentID);
				break;

			default: break;
			}
		}

		for (int id : selectedNotes)
		{
			const mmw::Note& note = score.notes.at(id);
			json data = noteToJson(note);
			data["tick"] = note.tick - baseTick;
			
			notes.push_back(data);
		}

		for (int id : selectedHolds)
		{
			const mmw::HoldNote& hold = score.holdNotes.at(id);
			const mmw::Note& start = score.notes.at(hold.start.ID);
			const mmw::Note& end = score.notes.at(hold.end);

			json holdData, stepsArray;

			json holdStart = noteToJson(start);
			holdStart["tick"] = start.tick - baseTick;
			holdStart["ease"] = mmw::easeTypes[(int)hold.start.ease];

			for (auto& step : hold.steps)
			{
				const mmw::Note& mid = score.notes.at(step.ID);
				json stepData = noteToJson(mid);
				stepData["tick"] = mid.tick - baseTick;
				stepData["type"] = mmw::stepTypes[(int)step.type];
				stepData["ease"] = mmw::easeTypes[(int)step.ease];

				stepsArray.push_back(stepData);
			}

			json holdEnd = noteToJson(end);
			holdEnd["tick"] = end.tick - baseTick;

			holdData["start"] = holdStart;
			holdData["steps"] = stepsArray;
			holdData["end"] = holdEnd;
			holds.push_back(holdData);
		}

		data["notes"] = notes;
		data["holds"] = holds;
		return data;
	}
}