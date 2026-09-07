#pragma once
#include <string>
#include <unordered_map>

// 간단한 Key,Value 형식의 설정 파일을 읽어들이는 범용 유틸리티.
class CConfigReader
{
public:
	// config 파일을 읽어 파싱한다. 파일을 열 수 없으면 false를 반환하고 기존 값도 비운다.
	// 같은 인스턴스로 다시 Load()하면 이전에 읽은 값은 전부 지워지고 새로 채워진다.
	bool Load(const std::string& filePath);

	// key가 존재하면 true.
	bool Has(const std::string& key) const;

	// key가 없으면 defaultValue를 그대로 반환한다(예외를 던지지 않는다).
	std::string GetString(const std::string& key, const std::string& defaultValue = "") const;
	int         GetInt(const std::string& key, int defaultValue = 0) const;
	bool        GetBool(const std::string& key, bool defaultValue = false) const;

private:
	static std::string Trim(const std::string& s);
	static std::string StripQuotes(const std::string& s);

	std::unordered_map<std::string, std::string> _values;
};
