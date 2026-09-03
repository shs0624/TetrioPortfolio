#include "Includes.h"
#include "LogManager.h"
#include "UserSession.h"
#include "MatchingManager.h"

unsigned int WINAPI MatchingManager::MatchingThread(LPVOID arg)
{
	// 30fps
	const DWORD dwTick = 33;
	MatchingManager* pMatchManager = (MatchingManager*)arg;

	pMatchManager->_OnInit(&pMatchManager->_Context);

	while (1)
	{
		bool bLoop = false;
		AcquireSRWLockShared(&pMatchManager->_QueueLock);
		bLoop = !pMatchManager->_MatchingList.empty();
		ReleaseSRWLockShared(&pMatchManager->_QueueLock);

		DWORD waitMS = bLoop ? dwTick : INFINITE;
		WaitForSingleObject(pMatchManager->_hMatchEvent, waitMS);

		pMatchManager->MatchFind();
	}
}

void MatchingManager::MatchFind()
{
	if (_MatchingList.size() < 2)
		return;

	AcquireSRWLockExclusive(&_QueueLock);
	st_USER* pUser1 = _MatchingList.front();
	_MatchingList.pop_front();

	st_USER* pUser2 = _MatchingList.front();
	_MatchingList.pop_front();
	ReleaseSRWLockExclusive(&_QueueLock);

	Notify(pUser1, pUser2);
}