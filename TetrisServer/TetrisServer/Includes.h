#pragma once
#pragma comment(lib,"ws2_32")
#pragma comment(lib,"winmm.lib")
#include <iostream>
#include <process.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <crtdbg.h>
#include <minidumpapiset.h>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <queue>
#include <stack>
#include <conio.h>

#include "CFreeList_LockFree.h"
#include "ProcademyProfiler.h"
#include "CSerializationBuffer.h"
#include "RefCountPointer.h"
#include "CRingBuffer.h"
#include "LockFreeQueue.h"
#include "LockFreeStack_Re.h"

#include "CCrashDump.h"

#define IOCP_THREADCOUNT 20
#define LOGCOUNT 10000