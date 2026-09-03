#pragma once

class MatchingManager
{
public:
	using OnMatchingCallback = void(*)(void* context, st_USER* pUser1, st_USER* pUser2);
	using OnInitCallback = void(*)(void* context);

	void InitMatchingManager(LPVOID context, OnMatchingCallback callback, OnInitCallback initCallback)
	{
		_Context = context;
		_OnMatchFound = callback;
		_OnInit = initCallback;

		_hMatchEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_hMatchingThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MatchingThread, this, 0, &_hMatchingThreadID);
	}

	void Enqueue(st_USER* pUser)
	{
		AcquireSRWLockExclusive(&_QueueLock);
		_MatchingList.push_back(pUser);
		ReleaseSRWLockExclusive(&_QueueLock);

		SetEvent(_hMatchEvent);
	}

	bool Dequeue(st_USER* pUser)
	{
		AcquireSRWLockExclusive(&_QueueLock);
		auto it = find(_MatchingList.begin(), _MatchingList.end(), pUser);
		if (it != _MatchingList.end())
		{
			_MatchingList.erase(it);
			ReleaseSRWLockExclusive(&_QueueLock);

			return true;
		}
		ReleaseSRWLockExclusive(&_QueueLock);

		return false;
	}
private:
	void Notify(st_USER* pUser1, st_USER* pUser2)
	{
		if (_OnMatchFound != nullptr)
			_OnMatchFound(_Context, pUser1, pUser2);
	}

	void MatchFind();

	OnMatchingCallback _OnMatchFound = nullptr;
	OnInitCallback _OnInit = nullptr;
	void* _Context = nullptr;

	// 성공하면 콜백으로 알려주기
	list<st_USER*> _MatchingList;
	//queue<st_USER*> _MatchingQ;
	SRWLOCK _QueueLock;

	HANDLE _hMatchEvent = NULL;
	HANDLE _hMatchingThreadHandle = NULL;
	unsigned int _hMatchingThreadID;

	stServerLog _pLog;

	static unsigned int WINAPI MatchingThread(LPVOID arg);
};
