#include "ProcademyProfiler.h"
#include <iostream>

#define STRUCT_ARR_MAX 100
#define THREAD_ARR_MAX 101

CHAR _Line[200] = "--------------------------------------------------------------------------------------------------\n";
CHAR _Header[200] = "               Name |          Average |             Min |              Max |       Call |\n";

// 측정을 위한 자료구조 / 측정 결과를 담는 자료구조 두 개가 필요함.
// 측정은 TLS를 이용하면 된다. 하지만 결과는 굳이 TLS가 아니어도 되지않나.
// 그렇다면 측정은 TLS에서 얻어오고, 거기서 측정을 시작하기 (map)
struct Profile_Struct
{
	DWORD _ThreadID;
	bool _IsUsing = false;
	bool _IsCounting = false;
	CHAR _Tag[64];
	LARGE_INTEGER _StartTime;
	__int64 _TotalTime;
	__int64 _MinTime;
	__int64 _MaxTime;
	__int64 _CallCount;
};

struct ProfilePerThread
{
	DWORD _ThreadID;
	vector<Profile_Struct*> _ProfileVec;
	bool _IsUsing = false;
};

//void ProfileBegin(const CHAR* tagName);
//
//void ProfileEnd(const CHAR* tagName);
//
//void ProfileDataOutText(const CHAR* szFileName);
//
//void ProfileReset(void);

class PrivateProfiler
{
public:
	PrivateProfiler()
	{
		QueryPerformanceFrequency(&_Freq);
		InitializeSRWLock(&_Lock);
		_TlsIdx = TlsAlloc();
		_ThreadTlsIdx = TlsAlloc();
		_ThreadIdx = 0;
	}

	bool FindProfile(int* idx, const CHAR* tag, Profile_Struct* profileArr)
	{
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == profileArr[i]._IsUsing)
			{
				continue;
			}

			if (!strcmp(profileArr[i]._Tag, tag))
			{
				*idx = i;
				return true;
			}
		}

		return false;
	}

	DWORD AddProfile(const CHAR* tag, Profile_Struct* pStruct)
	{
		// 스레드 정보 저장 배열에 새로 할당
		DWORD setValue = InterlockedIncrement(&_ThreadIdx);
		TlsSetValue(_ThreadTlsIdx, (LPVOID)setValue);
		_ThreadProfileArr[setValue]._ThreadID = GetCurrentThreadId();

		return setValue;
	}

	bool BeginCount(LARGE_INTEGER starTime, Profile_Struct* pProfile)
	{
		if (true == pProfile->_IsCounting)
		{
			return false;
		}

		pProfile->_StartTime = starTime;
		pProfile->_IsCounting = true;
		return true;
	}

	bool EndCount(LARGE_INTEGER endTime, Profile_Struct* pProfile)
	{
		if (false == pProfile->_IsCounting)
		{
			return false;
		}

		__int64 time = endTime.QuadPart - pProfile->_StartTime.QuadPart;
		pProfile->_TotalTime += time;
		pProfile->_MaxTime = max(pProfile->_MaxTime, time);
		pProfile->_MinTime = min(pProfile->_MinTime, time);
		pProfile->_CallCount++;
		pProfile->_IsCounting = false;

		// 스레드의 배열 인덱스를 TLS에서 얻어옴
		DWORD threadidx = (DWORD)TlsGetValue(_ThreadTlsIdx);
		if (threadidx == 0)
		{
			// 결산 배열에 등록안된거니까 등록해야함.
			threadidx = AddProfile(pProfile->_Tag, pProfile);
		}

		SaveCount(threadidx, pProfile);

		return true;
	}

	void SaveCount(DWORD index, Profile_Struct* pProfile)
	{
		AcquireSRWLockExclusive(&_Lock);

		ProfilePerThread* pThreadProfile = &_ThreadProfileArr[index];

		DWORD idx = -1;
		auto it = pThreadProfile->_ProfileVec.begin();
		for (; it != pThreadProfile->_ProfileVec.end(); it++)
		{
			if (!strcmp((*it)->_Tag, pProfile->_Tag))
			{
				break;
			}
		}

		// 전체 계산 벡터에 없는 경우
		if (it == pThreadProfile->_ProfileVec.end())
		{
			pThreadProfile->_ProfileVec.push_back(pProfile);
		}
		// 있는 경우, 갱신(메모리 통복사)
		else
		{
			memcpy_s((*it), sizeof(Profile_Struct), pProfile, sizeof(Profile_Struct));
		}

		ReleaseSRWLockExclusive(&_Lock);
	}

	void WriteFile(FILE* file)
	{
		CHAR context[200];

		AcquireSRWLockExclusive(&_Lock);
		for (int threadIdx = 1; threadIdx <= _ThreadIdx; threadIdx++)
		{
			sprintf_s(context, 200, "Thread ID : %d\n", _ThreadProfileArr[threadIdx]._ThreadID);
			fwrite(&context, strlen(context), 1, file);
			fwrite(_Line, strlen(_Line), 1, file);

			ProfilePerThread* pThreadProfile = &_ThreadProfileArr[threadIdx];
			for (auto it = pThreadProfile->_ProfileVec.begin(); it != pThreadProfile->_ProfileVec.end(); it++)
			{
				Profile_Struct* ptr = (*it);

				long long callCount = (ptr->_CallCount > 2) ? ptr->_CallCount - 2 : ptr->_CallCount;
				double average = ptr->_TotalTime - (ptr->_MaxTime + ptr->_MinTime);
				average = (((average / callCount)) * _Freq.QuadPart) * (1 / 1000000.0f);
				double min = (double)((ptr->_MinTime) * _Freq.QuadPart) * (1 / 1000000.0f);
				double max = (double)((ptr->_MaxTime) * _Freq.QuadPart) * (1 / 1000000.0f);
				sprintf_s(context, 200, "%20s | %.4f㎲ | %.4f㎲ | %.4f㎲ | %lld\n",
					ptr->_Tag, average, min, max, ptr->_CallCount);
				fwrite(&context, strlen(context), 1, file);
			}
			fwrite(_Line, strlen(_Line), 1, file);
		}
		ReleaseSRWLockExclusive(&_Lock);
	}

	// 아직 안됨 ㅇㅇ;
	void ResetProfiles()
	{
		AcquireSRWLockExclusive(&_Lock);
		for (int threadIdx = 1; threadIdx <= _ThreadIdx; threadIdx++)
		{
			ProfilePerThread* pThreadProfile = &_ThreadProfileArr[threadIdx];
			for (auto it = pThreadProfile->_ProfileVec.begin(); it != pThreadProfile->_ProfileVec.end(); it++)
			{
				Profile_Struct* ptr = (*it);

				ptr->_TotalTime = 0;
				ptr->_MaxTime = 0;
				ptr->_MinTime = LLONG_MAX;
				ptr->_CallCount = 0;
			}
		}
		ReleaseSRWLockExclusive(&_Lock);
	}

	// ThreadProfileArr 저장된 갯수
	DWORD _ThreadIdx = 0;
	// 해당 스레드의 측정정보를 담는 unordered_map의 주소를 담을 TLS 인덱스
	DWORD _TlsIdx = -1;
	// 해당 스레드가 결과를 담는 배열의 몇 번 인덱스에 있는지 얻어오는 TLS 인덱스
	DWORD _ThreadTlsIdx;
private:
	ProfilePerThread _ThreadProfileArr[THREAD_ARR_MAX];
	SRWLOCK _Lock;
	LARGE_INTEGER _Freq;
	int _iCnt = 0;
};

PrivateProfiler _Profiler;

Profiler::Profiler(const char* tag)
{
	PRO_BEGIN(tag);
	this->tag = tag;
}

Profiler::~Profiler()
{
	PRO_END(tag);
}

void ProfileBegin(const CHAR* tagName)
{
	int idx;
	LARGE_INTEGER startTime;

	std::unordered_map<char*, Profile_Struct*>* pProfileMap;
	pProfileMap = (std::unordered_map<char*, Profile_Struct*>*)TlsGetValue(_Profiler._TlsIdx);
	if (pProfileMap == NULL)
	{
		pProfileMap = new std::unordered_map<char*, Profile_Struct*>();
		TlsSetValue(_Profiler._TlsIdx, pProfileMap);
	}

	Profile_Struct* pStruct = NULL;
	QueryPerformanceCounter(&startTime);

	auto it = pProfileMap->find((char*)tagName);
	if (it == pProfileMap->end())
	{
		pStruct = (Profile_Struct*)malloc(sizeof(Profile_Struct));
		memset(pStruct, 0, sizeof(Profile_Struct));
		pStruct->_MinTime = INT64_MAX;

		int len = strlen(tagName);
		memcpy_s(pStruct->_Tag, len, tagName, len);
		//strcpy_s(pStruct->_Tag, len, tagName);
		pProfileMap->insert({ (char*)tagName, pStruct });
	}
	else
		pStruct = (*it).second;

	// Begin-Begin 구조인지 확인
	if (false == _Profiler.BeginCount(startTime, pStruct))
	{
		// Begin-Begin구조면 크래쉬
		throw 1;
	}
}

void ProfileEnd(const CHAR* tagName)
{
	int idx;
	LARGE_INTEGER endTime;

	std::unordered_map<char*, Profile_Struct*>* pProfileMap;
	pProfileMap = (std::unordered_map<char*, Profile_Struct*>*)TlsGetValue(_Profiler._TlsIdx);
	if (pProfileMap == NULL)
	{
		//pProfileMap = new std::unordered_map<char*, Profile_Struct*>();
		//TlsSetValue(_Profiler._TlsIdx, pProfileMap);
		DebugBreak();
		return;
	}

	Profile_Struct* pStruct = NULL;
	QueryPerformanceCounter(&endTime);

	auto it = pProfileMap->find((char*)tagName);
	pStruct = (*it).second;

	if (false == _Profiler.EndCount(endTime, pStruct))
	{
		// End-End 구조면 크래쉬
		throw 1;
	}
}

void ProfileDataOutText(const CHAR* szFileName)
{
	FILE* fptr;
	fopen_s(&fptr, szFileName, "wb");
	if (fptr == nullptr)
	{
		throw 0;
	}

	fwrite(_Line, strlen(_Line), 1, fptr);
	fwrite(_Header, strlen(_Header), 1, fptr);
	fwrite(_Line, strlen(_Line), 1, fptr);
	_Profiler.WriteFile(fptr);
	fwrite(_Line, strlen(_Line), 1, fptr);

	fclose(fptr);
}

void ProfileReset(void)
{
	_Profiler.ResetProfiles();
}