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
			pGameServer->GameUpdate(pGameSession);
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

	// 순회하며 빈 세션 찾기
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

	// 두 게임 세션 상태 초기화
	for (int i = 0; i < 2; i++)
	{
		memset(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._GameBoard, 0, sizeof(_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[0]._GameBoard));
		memset(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._NextBlockArr, 0, sizeof(_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[0]._NextBlockArr));
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._GarbageLine = 0;
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._HoldingBlock = NoneBlock;
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._DropBlock = NoneBlock;
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._BagHead = 0;
		_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._BagTail = _iBagMaxSize - 1;

		GenerateBag(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._NextBlockArr[0]);
		GenerateBag(&_GameSessionArr[targetIdx][targetSessionIdx]._GameInfoArr[i]._NextBlockArr[7]);
	}

	pUser1->pGameSession = &_GameSessionArr[targetIdx][targetSessionIdx];
	pUser2->pGameSession = &_GameSessionArr[targetIdx][targetSessionIdx];

	_GameSessionArr[targetIdx][targetSessionIdx]._SessionIDArr[0] = pUser1->ulSessionID;
	_GameSessionArr[targetIdx][targetSessionIdx]._SessionIDArr[1] = pUser2->ulSessionID;

	pUser1->byGameSessionIndex = 0;
	pUser2->byGameSessionIndex = 1;

	// 매칭 성공 응답 보내기
	RefCountPointer matchingSuccessPacket1 = RefCountPointer::MakeSharedPtr();
	(*matchingSuccessPacket1)->Clear(sizeof(st_NetHeader));
	mpRESMatchingSuccess(matchingSuccessPacket1, pUser1->AccountNum, pUser2->AccountNum, pUser2->NickName);
	MakePacketHeader(matchingSuccessPacket1);

	RefCountPointer matchingSuccessPacket2 = RefCountPointer::MakeSharedPtr();
	(*matchingSuccessPacket2)->Clear(sizeof(st_NetHeader));
	mpRESMatchingSuccess(matchingSuccessPacket2, pUser2->AccountNum, pUser1->AccountNum, pUser1->NickName);
	MakePacketHeader(matchingSuccessPacket2);

	if (!SendPacket_UniCast(pUser1->ulSessionID, matchingSuccessPacket1, false))
	{
		DebugBreak();
		return false;
	}

	_pLog._dwMatchingSuccessMessageTPS++;
	_pLog._dwMatchingSuccessMessageTotal++;

	if (!SendPacket_UniCast(pUser2->ulSessionID, matchingSuccessPacket2, false))
	{
		DebugBreak();
		return false;
	}

	_pLog._dwMatchingSuccessMessageTPS++;
	_pLog._dwMatchingSuccessMessageTotal++;

	LeaveChat(pUser1->ulSessionID);
	LeaveChat(pUser2->ulSessionID);

	return true;
}

void TetrisServer::GameUpdate(st_GAMESESSION* pGameSession)
{
	switch (pGameSession->_State)
	{
	case en_GAMESTATE_COUNTING:
		CheckCountDown(pGameSession);
		break;
	case en_GAMESTATE_PLAYING:
		UpdatePlay(pGameSession);
		break;
	}
}

void TetrisServer::UpdatePlay(st_GAMESESSION* pGameSession)
{
	// 드랍 시간을 체크하고, 시간이 지났으면 드랍중인 블록 한 칸 내리기
	DWORD nowTime = timeGetTime();

	for (int i = 0; i < 2; i++)
	{
		st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[i]);
		if ((nowTime - pGameInfo->_dwLastDropTime) > pGameSession->_dwDropTick)
		{
			// 한 칸 내리기
			pGameInfo->_DropY += 1;
			
			// 내렸을 때 충돌 체크
			if (!CollisionCheck(pGameInfo, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY))
			{
				// 충돌했음. 보드 업데이트
				for (int x = 0; x < BLOCK_ARR_LENGTH; x++)
				{
					int nx = pGameInfo->_DropX;
					for (int y = 0; y < BLOCK_ARR_LENGTH; y++)
					{
						if (ShapeTable[pGameInfo->_DropBlock][pGameInfo->_DropRotate][y][x] == 0)
							continue;

						pGameInfo->_GameBoard[pGameInfo->_DropY + y][nx] = pGameInfo->_DropBlock;
					}
				}

				// 여기서 보드 업데이트 메세지도 보내니까, 블록 생성 함수 호출
				CreateBlock(pGameSession, i);
			}
			else
			{
				// 블록 업데이트
				RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
				(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
				mpACKBlockUpdate(blockUpdatePacket, pGameInfo->_DropBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY);
				MakePacketHeader(blockUpdatePacket);

				if (!SendPacket_UniCast(pGameSession->_SessionIDArr[i], blockUpdatePacket, false))
				{
					// @@ TODO : 연결 끊김 처리
					DebugBreak();
					return;
				}

				// @@TODO : 로그찍기
			}
		}

		pGameInfo->_dwLastDropTime = nowTime;
	}
}

// 예정 블록 5개를 Bag 배열에 복사해서 넣어주기
enTetBlock TetrisServer::GetNextBlockType(st_GameInfo* pGameInfo, enTetBlock* pBagArr)
{
	int head = pGameInfo->_BagHead;
	int tail = pGameInfo->_BagTail;

	enTetBlock nextBlock = pGameInfo->_NextBlockArr[head];
	head += 1;

	if (head >= _iBagMaxSize)
		head = 0;

	if (head == (_iBagMaxSize / 2))
	{
		GenerateBag(&pGameInfo->_NextBlockArr[0]);

		tail = (_iBagMaxSize / 2) - 1;
	}
	else if (head == 0)
	{
		GenerateBag(&pGameInfo->_NextBlockArr[_iBagMaxSize / 2]);

		tail = _iBagMaxSize - 1;
	}

	for (int i = 0; i < 5; i++)
	{
		pBagArr[i] = pGameInfo->_NextBlockArr[(head + i) % _iBagMaxSize];
	}

	pGameInfo->_BagHead = head;
	pGameInfo->_BagTail = tail;

	return nextBlock;
}

// 7개의 블록을 랜덤으로 섞는 함수
void TetrisServer::GenerateBag(enTetBlock* pBag)
{
	pBag[0] = enIBlock;
	pBag[1] = enOBlock;
	pBag[2] = enTBlock;
	pBag[3] = enSBlock;
	pBag[4] = enZBlock;
	pBag[5] = enJBlock;
	pBag[6] = enLBlock;

	// 6 5 4 3 2 1 에서 뽑고, 뽑힌 건 맨 끝에 배치해서 섞이지 않게 설정
	for (int i = 6; i > 0; i--)
	{
		int j = rand() % (i + 1);       
		enTetBlock temp = pBag[i];
		pBag[i] = pBag[j];
		pBag[j] = temp;
	}
}

// 충돌하면 false, 정상이면 true
bool TetrisServer::CollisionCheck(st_GameInfo* pGameInfo, enTetBlock block, int dropRotate, int dropX, int dropY)
{
	if (dropY >= 20)
		return false;

	// 충돌 체크
	for (int x = 0; x < BLOCK_ARR_LENGTH; x++)
	{
		for (int y = 0; y < BLOCK_ARR_LENGTH; y++)
		{
			if (ShapeTable[block][dropRotate][y][x] == 0)
				continue;

			if (pGameInfo->_GameBoard[dropY][dropX] != 0)
			{
				return false;
			}
		}
	}

	return true;
}

void TetrisServer::CreateBlock(st_GAMESESSION* pGameSession, int sessionIndex)
{
	//pGameSession->_GameInfoArr[sessionIndex];
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);

	int midX = (10 / 2) - (_iShapeXSize / 2);

	enTetBlock nextBlockBag[5];

	pGameInfo->_DropBlock = GetNextBlockType(pGameInfo, nextBlockBag);
	pGameInfo->_DropRotate = 0;
	pGameInfo->_DropX = midX;
	pGameInfo->_DropY = 0;

	// @@TODO : 값 상수로 변경하기

	enTetBlock nextBlock = pGameInfo->_DropBlock;

	// 현재 보드 상태를 전송
	RefCountPointer boardUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*boardUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBoardUpdate(boardUpdatePacket, nextBlockBag, (BYTE*)(pGameSession->_GameInfoArr[sessionIndex]._GameBoard),
		(BYTE*)(pGameSession->_GameInfoArr[1 - sessionIndex]._GameBoard));
	MakePacketHeader(boardUpdatePacket);

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[sessionIndex], boardUpdatePacket, false))
	{
		// @@ TODO : 연결 끊김 처리
		DebugBreak();
		return;
	}

	// @@TODO : 로그찍기

	if (!CollisionCheck(pGameInfo, nextBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY))
	{
		// @@TODO : 게임오버 -> 패배
		return;
	}

	// 스폰
	RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(blockUpdatePacket, nextBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY);
	MakePacketHeader(blockUpdatePacket);

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[sessionIndex], blockUpdatePacket, false))
	{
		// @@ TODO : 연결 끊김 처리
		DebugBreak();
		return;
	}

	// @@TODO : 로그찍기
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

	// 게임 세션 카운팅으로 수정
	InterlockedExchange((LONG*)&pGameSession->_State, en_GAMESTATE_COUNTING);
	pGameSession->startTime = timeGetTime() + _wCountDown;
}

void TetrisServer::CheckCountDown(st_GAMESESSION* pGameSession)
{
	LONG nowTime = timeGetTime();

	if (pGameSession->startTime < nowTime)
	{
		// 카운트다운 끝
		InterlockedExchange((LONG*)&pGameSession->_State, en_GAMESTATE_PLAYING);
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