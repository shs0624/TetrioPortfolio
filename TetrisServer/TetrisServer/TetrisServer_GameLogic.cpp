#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "GameHeader.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

unsigned int WINAPI TetrisServer::GameTickThread(LPVOID arg)
{
	// 30fps
	TetrisServer* pGameServer = (TetrisServer*)arg;

	// 사용하는 게임 세션 배열 인덱스
	unsigned int idx = (_InterlockedIncrement(&pGameServer->_GameSessionCount) - 1);

	for (int i = 0; i < pGameServer->_MaxGameSessionPerThread; i++)
	{
		// 순회하며 초기화
		st_GAMESESSION* pGameSession;
		pGameSession = &(pGameServer->_GameSessionArr[idx][i]);

		pGameSession->_bUsing = false;
		InitializeSRWLock(&pGameSession->_GameSessionLock);
	}

	while (1)
	{
		for (int i = 0; i < pGameServer->_MaxGameSessionPerThread; i++)
		{
			st_GAMESESSION* pGameSession;
			pGameSession = &(pGameServer->_GameSessionArr[idx][i]);

			if (!pGameSession->_bUsing)
				continue;

			// 순회하며 작업 -> 구현 예정
		}

		// 프레임 관리용 Sleep 넣을 예정
		if (!pGameServer->SleepCheck())
			return 0;
	}
}

bool TetrisServer::SleepCheck()
{
	thread_local static ULONGLONG _ulNextFrameTick = GetTickCount64() + _dwFrameTime;
	const ULONGLONG now = GetTickCount64();

	// 초과: 바로 다음 루프로 진행 (Sleep 없음)
	if (now >= _ulNextFrameTick)
	{
		// 여러 프레임 초과분을 한 번에 보정.
		ULONGLONG late = now - _ulNextFrameTick;
		ULONGLONG skip = late / _dwFrameTime + 1;
		_ulNextFrameTick += skip * _dwFrameTime;

		// 종료 이벤트만 즉시 확인
		return (WaitForSingleObject(_hQuitEvent, 0) != WAIT_OBJECT_0);
	}

	// 남은 시간: 그만큼만 대기
	DWORD waitMs = (_ulNextFrameTick - now);
	int ret = WaitForSingleObject(_hQuitEvent, waitMs);
	if (ret == WAIT_OBJECT_0)
		return false;

	_ulNextFrameTick += _dwFrameTime;
	return true;
}