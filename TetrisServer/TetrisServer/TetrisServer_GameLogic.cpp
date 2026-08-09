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
	unsigned int idx = (_InterlockedIncrement(&pGameServer->_GameSessionThreadCount) - 1);

	for (int i = 0; i < pGameServer->_MaxGameSessionPerThread; i++)
	{
		// 순회하며 초기화
		st_GAMESESSION* pGameSession;
		pGameSession = &(pGameServer->_GameSessionArr[idx][i]);

		pGameSession->_State = en_GAMESTATE_UNUSED;
		InitializeSRWLock(&pGameSession->_GameSessionLock);
	}

	while (1)
	{
		for (int i = 0; i < pGameServer->_MaxGameSessionPerThread; i++)
		{
			st_GAMESESSION* pGameSession;
			pGameSession = &(pGameServer->_GameSessionArr[idx][i]);

			if (pGameSession->_State == en_GAMESTATE_UNUSED)
				continue;

			// 순회하며 작업 -> 구현 예정
		}

		// 프레임 관리용 Sleep 넣을 예정
		if (!pGameServer->SleepCheck())
			return 0;
	}
}

bool TetrisServer::SetGameSession(st_USER* pUser1, st_USER* pUser2)
{
	// 세션 순회하며 배정할 스레드 탐색
	int targetIdx = -1;
	LONG minCount = 1001;
	for (int i = 0; i < 10; i++)
	{
		if (_GameSessionActiveCountArr[i] < minCount)
		{
			minCount = _GameSessionActiveCountArr[i];
			targetIdx = i;
		}
	}

	if (targetIdx == -1)
		return false;

	// 순회하며 빈 세션 찾기...
	int targetSessionIdx = -1;
	for (int i = 0; i < _MaxGameSessionPerThread; i++)
	{
		if (InterlockedCompareExchange((LONG*)&_GameSessionArr[targetIdx][i]._State,
			en_GAMESTATE_WAIT_READY, en_GAMESTATE_UNUSED) == en_GAMESTATE_UNUSED)
		{
			InterlockedIncrement(&_GameSessionActiveCountArr[targetIdx]);
			targetSessionIdx = i;
			break;
		}
	}

	if (targetSessionIdx == -1)
		return false;

	// 게임 세션에 있는거 초기화 후 bUsing 인터락변경
	for (int i = 0; i < 2; i++)
	{
		memset(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._GameBoard, 0, sizeof(_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[0]._GameBoard));
		memset(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._NextBlockArr, 0, sizeof(_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[0]._NextBlockArr));
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._GarbageLine = 0;
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._HoldingBlock = -1;

		// @@TODO: 테트로미노 초기화 추가 예정
	}

	pUser1->pGameSession = &_GameSessionArr[targetIdx][targetSessionIdx];
	pUser2->pGameSession = &_GameSessionArr[targetIdx][targetSessionIdx];

	_GameSessionArr[targetIdx][targetSessionIdx]._SessionIDArr[0] = pUser1->ulSessionID;
	_GameSessionArr[targetIdx][targetSessionIdx]._SessionIDArr[1] = pUser2->ulSessionID;

	pUser1->byGameSessionIndex = 0;
	pUser2->byGameSessionIndex = 1;

	return true;
}

void TetrisServer::StartCountDown(st_GAMESESSION* pGameSession)
{
	// 둘 다 준비 완료 -> 카운트다운 시작 처리
	RefCountPointer startCountPacket = RefCountPointer::MakeSharedPtr();
	(*startCountPacket)->Clear(sizeof(st_NetHeader));

	mpACKCountDown(startCountPacket, _wCountDown);
	MakePacketHeader(startCountPacket);

	startCountPacket.IncRefCount();
	SendPacket_UniCast(pGameSession->_SessionIDArr[0], startCountPacket, false);
	SendPacket_UniCast(pGameSession->_SessionIDArr[1], startCountPacket, false);
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