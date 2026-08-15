#pragma once
#include <string>

std::string WstrToStr(const std::wstring& source)
{
	return std::string().assign(source.begin(), source.end());
}

std::wstring StrToWstr(const std::string& source)
{
	return std::wstring().assign(source.begin(), source.end());
}