#pragma once
#include <Windows.h>
#include <unordered_map>
using namespace std;

#define PROFILE
#ifdef PROFILE
#define PRO_BEGIN(TagName) ProfileBegin(TagName)
#define PRO_END(TagName) ProfileEnd(TagName)
#elif
#define PRO_BEGIN(TagName)  
#define PRO_END(TagName)  
#endif

class Profiler
{
public:
	Profiler(const char* tag);
	~Profiler();
private:
	const char* tag;

	//void ProfileBegin(const CHAR* tagName);

	//void ProfileEnd(const CHAR* tagName);

	//void ProfileDataOutText(const CHAR* szFileName);

	//void ProfileReset(void);
};

void ProfileBegin(const CHAR* tagName);

void ProfileEnd(const CHAR* tagName);

void ProfileDataOutText(const CHAR* szFileName);

void ProfileReset(void);