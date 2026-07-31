#pragma once

class MatchingManager
{
public:
	using OnMatchingCallback = void(*)(void* context, st_USER* pUser1, st_USER* pUser2);

	void InitMatchingManager(LPVOID context, OnMatchingCallback callback)
	{

	}

	void Enqueue(st_USER* pUser)
	{

	}
private:
	void Notify(st_USER* pUser1, st_USER* pUser2)
	{
		if (_OnMatchFound != nullptr)
			_OnMatchFound(_Context, pUser1, pUser2);
	}

	OnMatchingCallback _OnMatchFound = nullptr;
	void* _Context = nullptr;

	// 성공하면 콜백으로 알려주기
	queue<st_USER*> _MatchingQ;

	static unsigned int WINAPI MatchingThread(LPVOID arg);
};