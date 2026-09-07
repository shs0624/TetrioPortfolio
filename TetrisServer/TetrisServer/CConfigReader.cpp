#include "CConfigReader.h"
#include <fstream>
#include <algorithm>
#include <cctype>

// 스페이스, 탭, 줄바꿈으로 문자열 안에서 단어 자르기.
std::string CConfigReader::Trim(const std::string& s)
{
	size_t begin = s.find_first_not_of(" \t\r\n");
	if (begin == std::string::npos)
		return "";

	size_t end = s.find_last_not_of(" \t\r\n");
	return s.substr(begin, end - begin + 1);
}

// 따옴표가 있으면 제거하는 함수
std::string CConfigReader::StripQuotes(const std::string& s)
{
	if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
		return s.substr(1, s.size() - 2);

	return s;
}

bool CConfigReader::Load(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (!file.is_open())
		return false;

	_values.clear();

	std::string line;
	while (std::getline(file, line))
	{
		std::string trimmed = Trim(line);
		if (trimmed.empty())
			continue;

		// 주석 줄 무시
		if (trimmed[0] == '#' || trimmed[0] == ';')
			continue;

		// '=' 또는 ':' 둘 다 구분자로 허용
		size_t sep = trimmed.find_first_of("=:");
		if (sep == std::string::npos)
			continue; // 구분자가 없는 줄은 그냥 무시

		std::string key   = Trim(trimmed.substr(0, sep));
		std::string value = StripQuotes(Trim(trimmed.substr(sep + 1)));

		if (!key.empty())
			_values[key] = value;
	}

	return true;
}

bool CConfigReader::Has(const std::string& key) const
{
	return _values.find(key) != _values.end();
}

std::string CConfigReader::GetString(const std::string& key, const std::string& defaultValue) const
{
	auto it = _values.find(key);
	return (it != _values.end()) ? it->second : defaultValue;
}

int CConfigReader::GetInt(const std::string& key, int defaultValue) const
{
	auto it = _values.find(key);
	if (it == _values.end())
		return defaultValue;

	try
	{
		return std::stoi(it->second);
	}
	catch (...)
	{
		return defaultValue;
	}
}

bool CConfigReader::GetBool(const std::string& key, bool defaultValue) const
{
	auto it = _values.find(key);
	if (it == _values.end())
		return defaultValue;

	std::string v = it->second;
	std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return (char)std::tolower(c); });

	if (v == "1" || v == "true"  || v == "yes") return true;
	if (v == "0" || v == "false" || v == "no")  return false;

	return defaultValue;
}
