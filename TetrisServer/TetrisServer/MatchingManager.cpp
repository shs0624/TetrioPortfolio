#include "Includes.h"
#include "UserSession.h"
#include "MatchingManager.h"

unsigned int WINAPI MatchingManager::MatchingThread(LPVOID arg)
{
	// 30fps
	const DWORD dwTick = 33;
	MatchingManager* pMatchManager = (MatchingManager*)arg;

	while (1)
	{
		bool bLoop = false;
		AcquireSRWLockShared(&pMatchManager->_QueueLock);
		bLoop = !pMatchManager->_MatchingQ.empty();
		ReleaseSRWLockShared(&pMatchManager->_QueueLock);

		DWORD waitMS = bLoop ? dwTick : INFINITE;
		WaitForSingleObject(pMatchManager->_hMatchEvent, waitMS);

		pMatchManager->MatchFind();
	}
}

void MatchingManager::MatchFind()
{
	if (_MatchingQ.size() < 2)
		return;

	AcquireSRWLockExclusive(&_QueueLock);
	st_USER* pUser1 = _MatchingQ.front();
	_MatchingQ.pop();

	st_USER* pUser2 = _MatchingQ.front();
	_MatchingQ.pop();
	ReleaseSRWLockExclusive(&_QueueLock);

	Notify(pUser1, pUser2);
}