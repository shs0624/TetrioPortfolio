#include "CTextLogger.h"
#include <chrono>
#include <ctime>

CTextLogger::CTextLogger(const std::string& filePath) : _filePath(filePath)
{

}

CTextLogger::~CTextLogger()
{
	Close();
}

bool CTextLogger::OpenIfNeeded()
{
	if (_file != nullptr)
		return true;

	// a: 파일이 있으면 끝에 이어쓰고, 없으면 새로 만든다.
	if (fopen_s(&_file, _filePath.c_str(), "a") != 0)
	{
		_file = nullptr;
		return false;
	}

	return true;
}

bool CTextLogger::Write(const std::string& message)
{
	auto now = std::chrono::system_clock::now();
	std::time_t seconds = std::chrono::system_clock::to_time_t(now);
	int millis = (int)(std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000);

	std::tm local;
	localtime_s(&local, &seconds);

	char prefix[32];
	snprintf(prefix, sizeof(prefix), "[%04d-%02d-%02d %02d:%02d:%02d.%03d] ",
		local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
		local.tm_hour, local.tm_min, local.tm_sec, millis);

	std::string line = prefix + message + "\n";

	std::lock_guard<std::mutex> guard(_lock);
	if (!OpenIfNeeded())
		return false;

	size_t written = fwrite(line.data(), 1, line.size(), _file);
	fflush(_file);

	return written == line.size();
}

void CTextLogger::Close()
{
	std::lock_guard<std::mutex> guard(_lock);
	if (_file != nullptr)
	{
		fclose(_file);
		_file = nullptr;
	}
}
