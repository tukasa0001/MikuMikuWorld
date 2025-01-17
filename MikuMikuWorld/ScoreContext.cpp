#include "ScoreContext.h"
#include "Constants.h"
#include "IO.h"
#include "Utilities.h"
#include "UI.h"

#undef min
#undef max

using json = nlohmann::json;
using namespace IO;

namespace MikuMikuWorld
{
	void ScoreContext::setStep(HoldStepType type)
	{
		if (!selectedNotes.size())
			return;

		bool edit = false;
		Score prev = score;
		for (int id : selectedNotes)
		{
			const Note& note = score.notes.at(id);
			if (note.getType() != NoteType::HoldMid)
				continue;

			HoldNote& hold = score.holdNotes.at(note.parentID);
			int pos = findHoldStep(hold, id);
			if (pos != -1)
			{
				if (type == HoldStepType::HoldStepTypeCount)
				{
					cycleStepType(hold.steps[pos]);
					edit = true;
				}
				else
				{
					// don't record history if the type did not change
					edit |= hold.steps[pos].type != type;
					hold.steps[pos].type = type;
				}
			}
		}

		if (edit)
			history.pushHistory("Change step type", prev, score);
	}

	void ScoreContext::setFlick(FlickType flick)
	{
		if (!selectedNotes.size())
			return;

		bool edit = false;
		Score prev = score;
		for (int id : selectedNotes)
		{
			Note& note = score.notes.at(id);
			if (!note.hasEase())
			{
				if (flick == FlickType::FlickTypeCount)
				{
					cycleFlick(note);
					edit = true;
				}
				else
				{
					edit |= note.flick != flick;
					note.flick = flick;
				}
			}
		}

		if (edit)
			history.pushHistory("Change flick", prev, score);
	}

	void ScoreContext::setEase(EaseType ease)
	{
		if (!selectedNotes.size())
			return;

		bool edit = false;
		Score prev = score;
		for (int id : selectedNotes)
		{
			Note& note = score.notes.at(id);
			if (note.getType() == NoteType::Hold)
			{
				if (ease == EaseType::EaseTypeCount)
				{
					cycleStepEase(score.holdNotes.at(note.ID).start);
					edit = true;
				}
				else
				{
					edit |= score.holdNotes.at(note.ID).start.ease != ease;
					score.holdNotes.at(note.ID).start.ease = ease;
				}
			}
			else if (note.getType() == NoteType::HoldMid)
			{
				HoldNote& hold = score.holdNotes.at(note.parentID);
				int pos = findHoldStep(hold, id);
				if (pos != -1)
				{
					if (ease == EaseType::EaseTypeCount)
					{
						cycleStepEase(hold.steps[pos]);
						edit = true;
					}
					else
					{
						// don't record history if the type did not change
						edit |= hold.steps[pos].ease != ease;
						hold.steps[pos].ease = ease;
					}
				}
			}
		}

		if (edit)
			history.pushHistory("Change ease", prev, score);
	}

	void ScoreContext::toggleCriticals()
	{
		if (!selectedNotes.size())
			return;

		Score prev = score;
		std::unordered_set<int> critHolds;
		for (int id : selectedNotes)
		{
			Note& note = score.notes.at(id);
			if (note.getType() == NoteType::Tap)
			{
				note.critical ^= true;
			}
			else if (note.getType() == NoteType::HoldEnd && note.isFlick())
			{
				// if the start is critical the entire hold must be critical
				note.critical = score.notes.at(note.parentID).critical ? true : !note.critical;
			}
			else
			{
				critHolds.insert(note.getType() == NoteType::Hold ? note.ID : note.parentID);
			}
		}

		for (auto& hold : critHolds)
		{
			// flip critical state
			HoldNote& note = score.holdNotes.at(hold);
			bool critical = !score.notes.at(note.start.ID).critical;

			// again if the hold start is critical, every note in the hold must be critical
			score.notes.at(note.start.ID).critical = critical;
			score.notes.at(note.end).critical = critical;
			for (auto& step : note.steps)
				score.notes.at(step.ID).critical = critical;
		}

		history.pushHistory("Change note", prev, score);
	}

	void ScoreContext::deleteSelection()
	{
		if (!selectedNotes.size())
			return;

		Score prev = score;
		for (auto& id : selectedNotes)
		{
			auto notePos = score.notes.find(id);
			if (notePos == score.notes.end())
				continue;

			Note& note = notePos->second;
			if (note.getType() != NoteType::Hold && note.getType() != NoteType::HoldEnd)
			{
				if (note.getType() == NoteType::HoldMid)
				{
					// find hold step and remove it from the steps data container
					if (score.holdNotes.find(note.parentID) != score.holdNotes.end())
					{
						for (auto it = score.holdNotes.at(note.parentID).steps.begin(); it != score.holdNotes.at(note.parentID).steps.end(); ++it)
							if (it->ID == id) { score.holdNotes.at(note.parentID).steps.erase(it); break; }
					}
				}
				score.notes.erase(id);
			}
			else
			{
				const HoldNote& hold = score.holdNotes.at(note.getType() == NoteType::Hold ? note.ID : note.parentID);
				score.notes.erase(hold.start.ID);
				score.notes.erase(hold.end);

				// hold steps cannot exist without a hold
				for (const auto& step : hold.steps)
					score.notes.erase(step.ID);

				score.holdNotes.erase(hold.start.ID);
			}
		}

		selectedNotes.clear();
		pushHistory("Delete notes", prev, score);
	}

	void ScoreContext::flipSelection()
	{
		Score prev = score;
		for (int id : selectedNotes)
		{
			Note& note = score.notes.at(id);
			note.lane = MAX_LANE - note.lane - note.width + 1;

			if (note.flick == FlickType::Left)
				note.flick = FlickType::Right;
			else if (note.flick == FlickType::Right)
				note.flick = FlickType::Left;
		}

		pushHistory("Flip notes", prev, score);
	}

	void ScoreContext::cutSelection()
	{
		copySelection();
		deleteSelection();
	}

	void ScoreContext::copySelection()
	{
		if (!selectedNotes.size())
			return;

		int minTick = INT_MAX;
		for (int id : selectedNotes)
		{
			const auto& it = score.notes.find(id);
			if (it == score.notes.end())
				continue;

			minTick = std::min(minTick, it->second.tick);
		}

		json data = jsonIO::noteSelectionToJson(score, selectedNotes, minTick);

		std::string clipboard = "MikuMikuWorld clipboard\n";
		clipboard.append(data.dump());

		ImGui::SetClipboardText(clipboard.c_str());
	}

	void ScoreContext::cancelPaste()
	{
		pasteData.pasting = false;
	}

	void ScoreContext::doPasteData(const json& data, bool flip)
	{
		int baseId = 0;
		pasteData.notes.clear();
		pasteData.holds.clear();

		if (jsonIO::arrayHasData(data, "notes"))
		{
			for (const auto& entry : data["notes"])
			{
				Note note = jsonIO::jsonToNote(entry, NoteType::Tap);
				note.ID = baseId++;

				pasteData.notes[note.ID] = note;
			}
		}

		if (jsonIO::arrayHasData(data, "holds"))
		{
			for (const auto& entry : data["holds"])
			{
				Note start = jsonIO::jsonToNote(entry["start"], NoteType::Hold);
				start.ID = baseId++;
				pasteData.notes[start.ID] = start;

				Note end = jsonIO::jsonToNote(entry["end"], NoteType::HoldEnd);
				end.ID = baseId++;
				end.parentID = start.ID;
				pasteData.notes[end.ID] = end;

				std::string startEase = entry["start"]["ease"];
				HoldNote hold;
				hold.start = { start.ID, HoldStepType::Normal, (EaseType)findArrayItem(startEase.c_str(), easeTypes, TXT_ARR_SZ(easeTypes)) };
				hold.end = end.ID;

				if (startEase == "none")
					hold.start.ease = EaseType::Linear;

				if (jsonIO::keyExists(entry, "steps"))
				{
					hold.steps.reserve(entry["steps"].size());
					for (const auto& step : entry["steps"])
					{
						Note mid = jsonIO::jsonToNote(step, NoteType::HoldMid);
						mid.critical = start.critical;
						mid.ID = baseId++;
						mid.parentID = start.ID;
						pasteData.notes[mid.ID] = mid;

						std::string midType = step["type"];
						std::string midEase = step["ease"];

						int stepTypeIndex = findArrayItem(midType.c_str(), stepTypes, TXT_ARR_SZ(stepTypes));
						int easeTypeIndex = findArrayItem(midEase.c_str(), easeTypes, TXT_ARR_SZ(easeTypes));

						// maintain compatibility with old step type names
						if (stepTypeIndex == -1)
						{
							stepTypeIndex = 0;
							if (midType == "invisible") stepTypeIndex = 1;
							if (midType == "ignored") stepTypeIndex = 2;
						}

						// maintain compatibility with old ease type names
						if (easeTypeIndex == -1)
						{
							easeTypeIndex = 0;
							if (midEase == "in") easeTypeIndex = 1;
							if (midEase == "out") easeTypeIndex = 2;
						}

						hold.steps.push_back({ mid.ID, (HoldStepType)stepTypeIndex, (EaseType)easeTypeIndex });
					}
				}

				pasteData.holds[hold.start.ID] = hold;
			}
		}

		if (flip)
		{
			for (auto& [_, note] : pasteData.notes)
			{
				note.lane = MAX_LANE - note.lane - note.width + 1;

				if (note.flick == FlickType::Left)
					note.flick = FlickType::Right;
				else if (note.flick == FlickType::Right)
					note.flick = FlickType::Left;
			}
		}

		// find the lane in which the cursor is in the middle of pasted notes
		int left = MAX_LANE;
		int right = MIN_LANE;
		int leftmostLane = MAX_LANE;
		int rightmostLane = MIN_LANE;
		for (const auto& [_, note] : pasteData.notes)
		{
			leftmostLane = std::min(leftmostLane, note.lane);
			rightmostLane = std::max(rightmostLane, note.lane + note.width - 1);
			left = std::min(left, note.lane + note.width);
			right = std::max(right, note.lane);
		}

		pasteData.minLaneOffset = MIN_LANE - leftmostLane;
		pasteData.maxLaneOffset = MAX_LANE - rightmostLane;
		pasteData.midLane = (left + right) / 2;
		pasteData.pasting = pasteData.notes.size() > 0;
	}

	void ScoreContext::confirmPaste()
	{
		Score prev = score;

		// update IDs and copy notes
		for (auto& [_, note] : pasteData.notes)
		{
			note.ID += nextID;
			if (note.parentID != -1)
				note.parentID += nextID;

			note.lane += pasteData.offsetLane;
			note.tick += pasteData.offsetTicks;
			score.notes[note.ID] = note;
		}

		for (auto& [_, hold] : pasteData.holds)
		{
			hold.start.ID += nextID;
			hold.end += nextID;
			for (auto& step : hold.steps)
				step.ID += nextID;

			score.holdNotes[hold.start.ID] = hold;
		}
		
		// select newly pasted notes
		selectedNotes.clear();
		std::transform(pasteData.notes.begin(), pasteData.notes.end(),
			std::inserter(selectedNotes, selectedNotes.end()),
			[this](const auto& it) { return it.second.ID; });

		nextID += pasteData.notes.size();
		pasteData.pasting = false;
		pushHistory("Paste notes", prev, score);
	}

	void ScoreContext::paste(bool flip)
	{
		std::string clipboardData = ImGui::GetClipboardText();
		if (!startsWith(clipboardData, "MikuMikuWorld clipboard\n"))
			return;

		clipboardData = clipboardData.substr(24);
		json data = json::parse(clipboardData);
		if (jsonIO::arrayHasData(data, "notes") || jsonIO::arrayHasData(data, "holds"))
			doPasteData(data, flip);
	}

	void ScoreContext::shrinkSelection(Direction direction)
	{
		if (selectedNotes.size() < 2)
			return;

		Score prev = score;

		std::vector<int> sortedSelection(selectedNotes.begin(), selectedNotes.end());
		std::sort(sortedSelection.begin(), sortedSelection.end(), [this](int a, int b) 
		{
			const Note& n1 = score.notes.at(a);
			const Note& n2 = score.notes.at(b);
			return n1.tick == n2.tick ? n1.lane < n2.lane : n1.tick < n2.tick;
		});

		int factor = 1; // tick increment/decrement amount
		if (direction == Direction::Up)
		{
			// start from the last note
			std::reverse(sortedSelection.begin(), sortedSelection.end());
			factor = -1;
		}

		int firstTick = score.notes.at(*sortedSelection.begin()).tick;
		for (int i = 0; i < sortedSelection.size(); ++i)
			score.notes[sortedSelection[i]].tick = firstTick + (i * factor);

		const std::unordered_set<int> holds = getHoldsFromSelection();
		for (const auto& hold : holds)
			sortHoldSteps(score, score.holdNotes.at(hold));

		pushHistory("Shrink notes", prev, score);
	}

	void ScoreContext::undo()
	{
		if (history.hasUndo())
		{
			score = history.undo();
			clearSelection();

			UI::setWindowTitle((workingData.filename.size() ? File::getFilename(workingData.filename) : windowUntitled) + "*");
			upToDate = false;

			scoreStats.calculateStats(score);
		}
	}

	void ScoreContext::redo()
	{
		if (history.hasRedo())
		{
			score = history.redo();
			clearSelection();

			UI::setWindowTitle((workingData.filename.size() ? File::getFilename(workingData.filename) : windowUntitled) + "*");
			upToDate = false;

			scoreStats.calculateStats(score);
		}
	}

	void ScoreContext::pushHistory(std::string description, const Score& prev, const Score& curr)
	{
		history.pushHistory(description, prev, curr);

		UI::setWindowTitle((workingData.filename.size() ? File::getFilename(workingData.filename) : windowUntitled) + "*");
		scoreStats.calculateStats(score);

		upToDate = false;
	}

	bool ScoreContext::selectionHasEase() const
	{
		return std::find_if(selectedNotes.begin(), selectedNotes.end(), 
			[this](const int id) { return score.notes.at(id).hasEase(); }) != selectedNotes.end();
	}

	bool ScoreContext::selectionHasStep() const
	{
		return std::find_if(selectedNotes.begin(), selectedNotes.end(),
			[this](const int id) { return score.notes.at(id).getType() == NoteType::HoldMid; }) != selectedNotes.end();
	}

	bool ScoreContext::selectionHasFlickable() const
	{
		return std::find_if(selectedNotes.begin(), selectedNotes.end(),
			[this](const int id) { return !score.notes.at(id).hasEase(); }) != selectedNotes.end();
	}
}
