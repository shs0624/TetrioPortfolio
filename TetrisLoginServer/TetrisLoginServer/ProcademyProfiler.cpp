#include "ProcademyProfiler.h"
#include <iostream>
#define STRUCT_ARR_MAX 50
#define THREAD_ARR_MAX 21

CHAR _Line[200] = "--------------------------------------------------------------------------------------------------\n";
CHAR _Header[200] = "               Name |          Average |             Min |              Max |       Call |\n";


// TLS에 애초에 구조체를 저장?
// TLS에서 받은 인덱스는 저장해야함. 테이블에 매핑해서 따라가게 해야함.
// TLS에서 인덱스 얻어서?
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
	Profile_Struct* _ProfileArr;
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

	void AddProfile(int* idx, LARGE_INTEGER starTime, const CHAR* tag, Profile_Struct* profileArr)
	{
		*idx = -1;
		for (int i = 0; i < STRUCT_ARR_MAX; i++)
		{
			if (false == profileArr[i]._IsUsing)
			{
				*idx = i;
				break;
			}
		}

		if (*idx == -1)
		{
			throw -1;
		}

		// 스레드의 배열 인덱스를 TLS에 저장
		DWORD _tidx = (DWORD)TlsGetValue(_ThreadTlsIdx);
		if (_tidx == 0)
		{
			DWORD setValue = InterlockedIncrement(&_ThreadIdx);
			TlsSetValue(_ThreadTlsIdx, (LPVOID)setValue);
			_ThreadProfileArr[setValue]._ThreadID = GetCurrentThreadId();
			_ThreadProfileArr[setValue]._ProfileArr = (Profile_Struct*)malloc(sizeof(Profile_Struct) * STRUCT_ARR_MAX);
		}

		profileArr[*idx]._IsUsing = true;
		profileArr[*idx]._StartTime = starTime;
		profileArr[*idx]._MinTime = LLONG_MAX;
		profileArr[*idx]._TotalTime = 0;
		profileArr[*idx]._CallCount = 0;
		strcpy_s(profileArr[*idx]._Tag, 64, tag);
	}

	bool BeginCount(int idx, LARGE_INTEGER starTime, Profile_Struct* profileArr)
	{
		if (true == profileArr[idx]._IsCounting)
		{
			return false;
		}

		profileArr[idx]._StartTime = starTime;
		profileArr[idx]._IsCounting = true;
		return true;
	}

	bool EndCount(int idx, LARGE_INTEGER endTime, Profile_Struct* profileArr)
	{
		if (false == profileArr[idx]._IsCounting)
		{
			return false;
		}

		__int64 time = endTime.QuadPart - profileArr[idx]._StartTime.QuadPart;
		profileArr[idx]._TotalTime += time;
		profileArr[idx]._MaxTime = max(profileArr[idx]._MaxTime, time);
		profileArr[idx]._MinTime = min(profileArr[idx]._MinTime, time);
		profileArr[idx]._CallCount++;
		profileArr[idx]._IsCounting = false;

		// 스레드의 배열 인덱스를 TLS에서 얻어옴
		DWORD threadidx = (DWORD)TlsGetValue(_ThreadTlsIdx);
		if (threadidx == 0)
		{
			DebugBreak();
		}

		//_ThreadProfileArr[threadidx]._ProfileArr[idx];
		AcquireSRWLockShared(&_Lock);
		memcpy_s(&(_ThreadProfileArr[threadidx]._ProfileArr[idx]), sizeof(Profile_Struct), &profileArr[idx], sizeof(Profile_Struct));
		ReleaseSRWLockShared(&_Lock);

		return true;
	}

	void WriteFile(FILE* file)
	{
		CHAR context[200];

		AcquireSRWLockExclusive(&_Lock);
		for (int threadIdx = 1; threadIdx <= _ThreadIdx; threadIdx++)
		{
			sprintf_s(context, 200, "Thread ID : %d\n",_ThreadProfileArr[threadIdx]._ThreadID);
			fwrite(&context, strlen(context), 1, file);
			fwrite(_Line, strlen(_Line), 1, file);
			for (int i = 1; i < STRUCT_ARR_MAX; i++)
			{
				Profile_Struct* ptr = &_ThreadProfileArr[threadIdx]._ProfileArr[i];
				if (false == ptr->_IsUsing)
				{
					break;
				}

				double average = ptr->_TotalTime - (ptr->_MaxTime + ptr->_MinTime);
				average = ((average / (ptr->_CallCount - 2))) * (1000000.0f / (float)_Freq.QuadPart);
				double min = (double)((double)ptr->_MinTime) * (1000000.0f / (float)_Freq.QuadPart);
				double max = (double)((double)ptr->_MaxTime) * (1000000.0f / (float)_Freq.QuadPart);
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
			for (int i = 0; i < STRUCT_ARR_MAX; i++)
			{
				Profile_Struct* ptr = &_ThreadProfileArr[threadIdx]._ProfileArr[i];
				if (false == ptr->_IsUsing)
				{
					break;
				}

				ptr->_TotalTime = 0;
				ptr->_MaxTime = 0;
				ptr->_MinTime = LLONG_MAX;
				ptr->_CallCount = 0;
			}
		}
		ReleaseSRWLockExclusive(&_Lock);
	}

	// ThreadProfileArr 저장된 갯수
	DWORD _ThreadIdx;
	// 해당 스레드의 ProfileStruct 배열이 몇 번 TLS 인덱스에 저장되어 있는가
	DWORD _TlsIdx = -1;
	// ThreadProfileArr에 해당 스레드가 몇 번 인덱스에 저장되어 있는가
	DWORD _ThreadTlsIdx;
private:
	Profile_Struct _ProfileArr[STRUCT_ARR_MAX];
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

	Profile_Struct* ptr = (Profile_Struct*)TlsGetValue(_Profiler._TlsIdx);
	if (ptr == NULL)
	{
		ptr = (Profile_Struct*)malloc(sizeof(Profile_Struct) * STRUCT_ARR_MAX);
		TlsSetValue(_Profiler._TlsIdx, ptr);
	}

	QueryPerformanceCounter(&startTime);
	if (false == _Profiler.FindProfile(&idx, tagName, ptr))
	{
		//구조체 추가
		_Profiler.AddProfile(&idx, startTime, tagName, ptr);
	}

	// Begin-Begin 구조인지 확인
	if (false == _Profiler.BeginCount(idx, startTime, ptr))
	{
		// Begin-Begin구조면 크래쉬
		throw 1;
	}
}

void ProfileEnd(const CHAR* tagName)
{
	int idx;
	LARGE_INTEGER endTime;

	Profile_Struct* ptr = (Profile_Struct*)TlsGetValue(_Profiler._TlsIdx);
	if (ptr == NULL)
	{
		ptr = (Profile_Struct*)malloc(sizeof(Profile_Struct) * STRUCT_ARR_MAX);
	}

	QueryPerformanceCounter(&endTime);
	if (false == _Profiler.FindProfile(&idx, tagName, ptr))
	{
		// 없는 태그를 End했음. 이걸 알려야 할까?
		DebugBreak();
		return;
	}

	if (false == _Profiler.EndCount(idx, endTime, ptr))
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