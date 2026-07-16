#ifndef _PROCADEMY_LIB_CRASH_DUMP_
#define _PROCADEMY_LIB_CRASH_DUMP_
#pragma comment(lib,"Dbghelp.lib")
#include <Windows.h>
#include <DbgHelp.h>
#include <iostream>
#include <minidumpapiset.h>

namespace procademy
{
	class CCrashDump
	{
	public:
		CCrashDump()
		{
			_DumpCount = 0;

			_invalid_parameter_handler oldHandler, newHandler;
			newHandler = myInvalidParameterHandler;

			oldHandler = _set_invalid_parameter_handler(newHandler); // crt함수에 null포인터등을 넣었을 때
			_CrtSetReportMode(_CRT_WARN, 0);
			_CrtSetReportMode(_CRT_ASSERT, 0); 
			_CrtSetReportMode(_CRT_ERROR, 0);

			_CrtSetReportHook(_custom_Report_hook);

			// pure virtual function called 에러 핸들러를 우회
			_set_purecall_handler(myPurecallHandler);

			SetHandlerDump();
		}

		static void Crash(void)
		{
			int* p = nullptr;
			*p = 0;
		}

		static LONG WINAPI MyExceptionFilter(__in PEXCEPTION_POINTERS pExceptionPointer)
		{
			int iWorkingMemory = 0;
			SYSTEMTIME stNowTime;

			long DumpCount = InterlockedIncrement(&_DumpCount);

			WCHAR fileName[MAX_PATH];
			GetLocalTime(&stNowTime);
			wsprintf(fileName, L"Dump_%d%02d%02d_%02d.%02d_%d_%d.dmp",
				stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond, DumpCount);
			wprintf(L"\n\n\n!!! Crash Error !!! %d.%d.%d / %d:%d:%d\n",
				stNowTime.wYear, stNowTime.wMonth, stNowTime.wDay, stNowTime.wHour, stNowTime.wMinute, stNowTime.wSecond);
			wprintf(L"Now Save Dump File...\n");

			HANDLE hDumpFile = ::CreateFile(fileName,
				GENERIC_WRITE,
				0,
				NULL,
				CREATE_ALWAYS,
				FILE_ATTRIBUTE_NORMAL, NULL);

			if (hDumpFile != INVALID_HANDLE_VALUE)
			{
				_MINIDUMP_EXCEPTION_INFORMATION MinidumpExceptionInformation;

				MinidumpExceptionInformation.ThreadId = GetCurrentThreadId();
				MinidumpExceptionInformation.ExceptionPointers = pExceptionPointer;
				MinidumpExceptionInformation.ClientPointers = TRUE;

				MiniDumpWriteDump(GetCurrentProcess(),
					GetCurrentProcessId(),
					hDumpFile,
					MiniDumpWithFullMemory,
					&MinidumpExceptionInformation,
					NULL,
					NULL);

				CloseHandle(hDumpFile);

				wprintf(L"CrashDump Save Finish!\n");
			}

			return EXCEPTION_EXECUTE_HANDLER;
		}

		static void SetHandlerDump()
		{
			SetUnhandledExceptionFilter(MyExceptionFilter);
		}

		static void myInvalidParameterHandler(const wchar_t* expression, const wchar_t* function, const wchar_t* file, unsigned int line, uintptr_t pReserverd)
		{
			Crash();
		}

		static int _custom_Report_hook(int ireposttype, char* message, int* returnvalue)
		{
			Crash();
			return true;
		}

		static void myPurecallHandler(void)
		{
			Crash();
		}

		static long _DumpCount;
	};

	inline long CCrashDump::_DumpCount = 0;
}

#endif