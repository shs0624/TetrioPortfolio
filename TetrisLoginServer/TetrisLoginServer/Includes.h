#pragma once
#pragma comment(lib,"ws2_32")
#pragma comment(lib,"winmm.lib")

#include <cpp_redis/cpp_redis>
#include <tacopie/tacopie>
#pragma comment(lib, "cpp_redis.lib")
#pragma comment(lib, "tacopie.lib")

#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include <string>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <queue>
#include <conio.h>

#include "CFreeList_LockFree.h"
#include "ProcademyProfiler.h"
#include "CSerializationBuffer.h"
#include "RefCountPointer.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "LockFreeStack_Re.h"

#include "CCrashDump.h"
#include "DebugLog.h"

#define IOCP_THREADCOUNT 30
#define LOGCOUNT 10000