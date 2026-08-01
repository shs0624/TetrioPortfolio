#pragma once

class MatchingManager
{
public:
	using OnMatchingCallback = void(*)(void* context, st_USER* pUser1, st_USER* pUser2);

	void InitMatchingManager(LPVOID context, OnMatchingCallback callback)
	{
		_Context = context;
		_OnMatchFound = callback;

		_hMatchEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_hMatchingThreadHandle = (HANDLE)_beginthreadex(NULL, 0, MatchingThread, this, 0, &_hMatchingThreadID);
	}

	void Enqueue(st_USER* pUser)
	{
		AcquireSRWLockExclusive(&_QueueLock);
		_MatchingQ.push(pUser);
		ReleaseSRWLockExclusive(&_QueueLock);

		SetEvent(_hMatchEvent);
	}
private:
	void Notify(st_USER* pUser1, st_USER* pUser2)
	{
		if (_OnMatchFound != nullptr)
			_OnMatchFound(_Context, pUser1, pUser2);
	}

	void MatchFind();

	OnMatchingCallback _OnMatchFound = nullptr;
	void* _Context = nullptr;

	// 성공하면 콜백으로 알려주기
	queue<st_USER*> _MatchingQ;
	SRWLOCK _QueueLock;

	HANDLE _hMatchEvent = NULL;
	HANDLE _hMatchingThreadHandle = NULL;
	unsigned int _hMatchingThreadID;

	static unsigned int WINAPI MatchingThread(LPVOID arg);
};