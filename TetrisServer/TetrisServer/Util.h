#pragma once
#include <string>

std::string inline WstrToStr(const std::wstring& source)
{
	return std::string().assign(source.begin(), source.end());
}

std::wstring inline StrToWstr(const std::string& source)
{
	return std::wstring().assign(source.begin(), source.end());
}