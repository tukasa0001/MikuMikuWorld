#pragma once
#include <string>
#include <vector>

namespace IO
{
	class File
	{
	private:
		FILE* stream;

	public:
		File(const std::wstring& filename, const wchar_t* mode);
		File(const std::string& filename, const char* mode);
		File();

		static std::string getFilename(const std::string& filename);
		static std::string getFileExtension(const std::string& filename);
		static std::string getFilenameWithoutExtension(const std::string& filename);
		static std::string getFilepath(const std::string& filename);
		static std::string fixPath(const std::string& path);
		static bool exists(const std::string& path);

		void open(const std::wstring& filename, const wchar_t* mode);
		void open(const std::string& filename, const char* mode);
		void close();
		void flush();

		std::string readLine() const;
		std::vector<std::string> readAllLines() const;
		std::string readAllText() const;
		void write(const std::string str);
		void writeLine(const std::string line);
		void writeAllLines(const std::vector<std::string>& lines);
		bool isEndofFile() const;
	};
}
