#pragma once
#include<Windows.h>
#include <iostream>
#include <queue>
#include <utility>
#include <unordered_map>
using namespace std;
#define LOGARR_MAX 10000
//#define LOG_LOCKFREESTACK

enum workType
{
	PUSH,
	POP
};

template <typename T>
class LockFreeStack
{
	struct st_NODE
	{
		T value;
		st_NODE* Next;
	};

public:
	LockFreeStack()
	{
		_StartNode = new st_NODE;
		_TopNode = _StartNode;
	}

	void push(T data)
	{
		st_NODE* newNodePtr = new st_NODE;
		newNodePtr->value = data;

		ULONGLONG localIdx = InterlockedIncrement(&_IdxValue);
		st_NODE* newNode = (st_NODE*)((ULONGLONG)newNodePtr | ((ULONGLONG)localIdx << 47));

		while (1)
		{
			st_NODE* oldTop = _TopNode;
			newNodePtr->Next = oldTop;

			// 현재 TopNode가 oldTop과 같다면 TopNode를 newNode로 변경
			// 반환은 연산 전 TopNode에 저장된 값을 반환하므로, oldTop이면 TopNode에 변경이 없었다는 뜻.
			if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
			{
				// 로그 남기기용
#ifdef LOG_LOCKFREESTACK
				unsigned long idx = InterlockedIncrement(&_logIdx) - 1;
				idx = idx % LOGARR_MAX;
				_workArr[idx] = { PUSH, newNode };
#endif

				InterlockedIncrement(&cnt);
				return;
			}
		}
	}

	bool pop(T* output)
	{
		while (1)
		{
			st_NODE* oldTop = _TopNode;
			st_NODE* topPtr = (st_NODE*)(0x00007fffffffffff & (ULONGLONG)oldTop);
			st_NODE* newNode = topPtr->Next;

			if (topPtr == _StartNode)
				return false;

			// 현재 TopNode가 oldTop과 같다면 TopNode를 newNode로 변경
			// 반환은 연산 전 TopNode에 저장된 값을 반환하므로, oldTop이면 TopNode에 변경이 없었다는 뜻.
			if (InterlockedCompareExchange64((__int64*)&_TopNode, (__int64)newNode, (__int64)oldTop) == (__int64)oldTop)
			{
				// 로그 남기기용
#ifdef LOG_LOCKFREESTACK
				unsigned long idx = _InterlockedIncrement(&_logIdx) - 1;
				idx = idx % LOGARR_MAX;
				_workArr[idx] = { POP, oldTop };
#endif

				* output = topPtr->value;
				//*deletePtr = topPtr;
				delete topPtr;

				InterlockedDecrement(&cnt);
				return true;
			}
		}
	}

	bool Log(int num)
	{
		if (num < _logIdx)
		{
			switch (_workArr[num].first)
			{
			case PUSH:
				printf("PUSH : %p\n", _workArr[num].second);
				break;
			case POP:
				printf("POP : %p\n", _workArr[num].second);
				break;
			}
			return true;
		}
		return false;
	}
private:
	DWORD cnt = 0;
	DWORD trycnt = 0;
	unsigned long _logIdx = 0;
	ULONGLONG _IdxValue = 0;

	//void* _workArr[10000000];
	pair<workType, void*> _workArr[LOGARR_MAX];
	queue<pair<workType, void*>> _workQ;
	//queue<void*> _workQ;
	//unordered_map<void*, int> _nodeMap;
	st_NODE* _TopNode;
	st_NODE* _StartNode;
};