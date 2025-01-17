#pragma once
#include <string>

namespace IO
{
	enum class FileType
	{
		ScoreFile,
		MMWSFile,
		SUSFile,
		AudioFile,
		ImageFile
	};

	class FileDialog
	{
	private:
		static const wchar_t* getDialogTitle(FileType type);
		static const wchar_t* getDialogFilters(FileType type);

	public:
		static bool openFile(std::string& name, FileType type);
		static bool saveFile(std::string& name, FileType type);
		static const wchar_t* getExtensionFromType(FileType type);
	};
}