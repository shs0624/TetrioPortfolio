#include "Includes.h"
#include "LogManager.h"
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
	pGameServer->RegisterNetServerLog(arg);

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

	// @@TODO : 드랍 간격 구체적으로 설정하기
	_GameSessionArr[targetIdx][targetSessionIdx]._dwDropTick = 1000;

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
	_pLog._dwPacketPoolUse++;

	RefCountPointer matchingSuccessPacket2 = RefCountPointer::MakeSharedPtr();
	(*matchingSuccessPacket2)->Clear(sizeof(st_NetHeader));
	mpRESMatchingSuccess(matchingSuccessPacket2, pUser2->AccountNum, pUser1->AccountNum, pUser1->NickName);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pUser1->ulSessionID, matchingSuccessPacket1))
	{
		DebugBreak();
		return false;
	}

	_pLog._dwMatchingSuccessMessageTotal++;

	if (!SendPacket_UniCast(pUser2->ulSessionID, matchingSuccessPacket2))
	{
		DebugBreak();
		return false;
	}

	_pLog._dwMatchingSuccessMessageTotal++;

	return true;
}

void TetrisServer::EndGameSession(st_GAMESESSION* pGameSession, int winnerIdx, int loserIdx)
{
	if (winnerIdx != -1)
	{
		ULONGLONG winnerSessionID = pGameSession->_SessionIDArr[winnerIdx];

		// 승리 패킷
		RefCountPointer winResultPacket = RefCountPointer::MakeSharedPtr();
		(*winResultPacket)->Clear(sizeof(st_NetHeader));
		mpACKGameResult(winResultPacket, true);
		_pLog._dwPacketPoolUse++;

		SendPacket_UniCast(winnerSessionID, winResultPacket);
	}
	
	if (loserIdx != -1)
	{
		ULONGLONG loserSessionID = pGameSession->_SessionIDArr[loserIdx];

		// 패배 패킷
		RefCountPointer loseResultPacket = RefCountPointer::MakeSharedPtr();
		(*loseResultPacket)->Clear(sizeof(st_NetHeader));
		mpACKGameResult(loseResultPacket, false);
		_pLog._dwPacketPoolUse++;

		SendPacket_UniCast(loserSessionID, loseResultPacket);
	}

	// 세션 사용 중지 표시
	pGameSession->_State = en_GAMESTATE_UNUSED;
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

	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
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
				pGameInfo->_DropY -= 1;

				// 여기서 보드 업데이트 메세지도 보내니까, 블록 생성 함수 호출
				CreateBlock(pGameSession, i);
			}
			else
			{
				// 블록 업데이트
				RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
				(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
				mpACKBlockUpdate(blockUpdatePacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
				_pLog._dwPacketPoolUse++;

				if (!SendPacket_UniCast(pGameSession->_SessionIDArr[i], blockUpdatePacket))
				{
					EndGameSession(pGameSession, 1 - i, -1);
					ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
					return;
				}

				// 상대방 업데이트
				RefCountPointer opponetPacket = RefCountPointer::MakeSharedPtr();
				(*opponetPacket)->Clear(sizeof(st_NetHeader));
				mpACKBlockUpdate(opponetPacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
				_pLog._dwPacketPoolUse++;

				if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - i], opponetPacket))
				{
					EndGameSession(pGameSession, i, -1);
					ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
					return;
				}
			}

			pGameInfo->_dwLastDropTime = nowTime;
		}
	}
	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
}

// 예정 블록 5개를 Bag 배열에 복사해서 넣어주기
void TetrisServer::GetNextBlockArr(st_GameInfo* pGameInfo, enTetBlock* pBagArr)
{
	int head = pGameInfo->_BagHead;
	int tail = pGameInfo->_BagTail;

	for (int i = 0; i < 5; i++)
	{
		pBagArr[i] = pGameInfo->_NextBlockArr[(head + i) % _iBagMaxSize];
	}

	return;
}

// 예정 블록 한개를 빼고, 예정 블록 5개를 Bag 배열에 복사해서 넣어주기
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
	if (dropY >= _iMaxY)
		return false;

	// 충돌 체크
	for (int x = 0; x < BLOCK_ARR_LENGTH; x++)
	{
		for (int y = 0; y < BLOCK_ARR_LENGTH; y++)
		{
			if (ShapeTable[block][dropRotate][y][x] == 0)
				continue;

			int nx = dropX + x;
			int ny = dropY + y;

			if (nx < 0 || nx >= _iMaxX || ny >= _iMaxY)
				return false;

			// 위 쪽은 허용
			if (ny < 0)
				continue;

			if (pGameInfo->_GameBoard[ny][nx] != 0)
			{
				return false;
			}
		}
	}

	return true;
}

// 락 처리 시점에 호출. 반환값이 0이 아니면 그 비트가 지워진 행.
DWORD TetrisServer::LineClear(st_GameInfo* pGameInfo, int dropY)
{
	DWORD clearedMask = 0;

	// 지워지는 라인 체크
	for (int r = 0; r < BLOCK_ARR_LENGTH; r++) 
	{
		int boardRow = dropY + r;
		if (boardRow < 0 || boardRow >= 20)
			continue;

		bool full = true;
		for (int c = 0; c < 10; c++)
		{
			if (pGameInfo->_GameBoard[boardRow][c] == 0)
			{
				full = false;
				break;   
			}
		}

		if (full)
			clearedMask |= (1 << boardRow);
	}

	if (clearedMask == 0)
		return 0;

	// 빈 라인 채우기
	int idx = (dropY + BLOCK_ARR_LENGTH - 1) >= _iMaxY ? _iMaxY - 1 : (dropY + BLOCK_ARR_LENGTH - 1);
	for (int i = idx; i >= 0; i--)
	{
		// i번째 줄이 소멸됐으면
		if (clearedMask & (1 << i))
			continue;

		memcpy(pGameInfo->_GameBoard[idx--], pGameInfo->_GameBoard[i], sizeof(BYTE) * _iMaxX);
	}

	for (; idx >= 0; idx--)
	{
		memset(pGameInfo->_GameBoard[idx], 0, sizeof(BYTE) * _iMaxX);
	}

	return clearedMask;
}

void TetrisServer::Attack(st_GAMESESSION* pGameSession, int sessionIndex, DWORD clearBit)
{
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	st_GameInfo* opGameInfo = &(pGameSession->_GameInfoArr[1 - sessionIndex]);

	// 몇 줄 지워졌는지 비트 연산으로 체크
	int clearedLines = 0;
	for (DWORD m = clearBit; m; m >>= 1)
		clearedLines += (m & 1);

	// 공격 체크
	int attackLine = 0;
	if (clearedLines > 0)
	{
		pGameInfo->_Combo++;

		if (clearedLines == 1)
			attackLine = 0;
		else
			attackLine = 1 << (clearedLines - 2);

		if (pGameInfo->_Combo >= 2 && pGameInfo->_Combo < 4)
			attackLine += 1;
		else if (pGameInfo->_Combo >= 4 && pGameInfo->_Combo < 6)
			attackLine += 2;
		else if (pGameInfo->_Combo >= 6 && pGameInfo->_Combo < 8)
			attackLine += 3;
		else if (pGameInfo->_Combo >= 8 && pGameInfo->_Combo < 11)
			attackLine += 4;
		else if (pGameInfo->_Combo >= 11)
			attackLine += 5;

		// 상쇄
		if (pGameInfo->_GarbageLine >= attackLine)
		{
			pGameInfo->_GarbageLine -= attackLine;
		}
		else
		{
			attackLine -= pGameInfo->_GarbageLine;
			pGameInfo->_GarbageLine = 0;
		}
		
		// 상쇄 후 공격이 남았으면 공격
		if (attackLine > 0)
			opGameInfo->_GarbageLine += attackLine;

		// 상대 데미지 알림 전송
		RefCountPointer damagePacket = RefCountPointer::MakeSharedPtr();
		(*damagePacket)->Clear(sizeof(st_NetHeader));
		mpACKDamage(damagePacket, opGameInfo->_GarbageLine);
		_pLog._dwPacketPoolUse++;

		if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], damagePacket))
		{
			EndGameSession(pGameSession, sessionIndex, -1);
			return;
		}
	}
	else
	{
		pGameInfo->_Combo = 0;
	}
}

void TetrisServer::Damage(st_GAMESESSION* pGameSession, int sessionIndex)
{
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	int garbageLine = pGameInfo->_GarbageLine;
	if (pGameInfo->_GarbageLine > 0)
	{
		if (garbageLine >= _iMaxY)
		{
			// 게임 오버
			EndGameSession(pGameSession, 1 - sessionIndex, sessionIndex);
			return;
		}

		// garbageLine만큼 위로 올리기
		for (int i = 0; i < _iMaxY - garbageLine; i++)
		{
			// i + garbageLine에 있는 줄을 위로 올리는 방식
			memcpy(pGameInfo->_GameBoard[i], pGameInfo->_GameBoard[i + garbageLine], sizeof(BYTE) * _iMaxX);
		}

		// 데미지 라인 채우기
		for (int i = _iMaxY - garbageLine; i < _iMaxY; i++)
		{
			int randX = rand() % _iMaxX;
			for (int j = 0; j < _iMaxX; j++)
				pGameInfo->_GameBoard[i][j] = (j == randX) ? 0 : enTetBlock::enGarbageBlock;
		}

		pGameInfo->_GarbageLine = 0;

		// 현재 데미지 알림 전송
		RefCountPointer damagePacket = RefCountPointer::MakeSharedPtr();
		(*damagePacket)->Clear(sizeof(st_NetHeader));
		mpACKDamage(damagePacket, pGameInfo->_GarbageLine);
		_pLog._dwPacketPoolUse++;

		if (!SendPacket_UniCast(pGameSession->_SessionIDArr[sessionIndex], damagePacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			return;
		}
	}
}

void TetrisServer::UpdateBoard(st_GAMESESSION* pGameSession, int sessionIndex)
{
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	st_GameInfo* opGameInfo = &(pGameSession->_GameInfoArr[1 - sessionIndex]);

	for (int x = 0; x < BLOCK_ARR_LENGTH; x++)
	{
		int nx = pGameInfo->_DropX + x;
		for (int y = 0; y < BLOCK_ARR_LENGTH; y++)
		{
			if (ShapeTable[pGameInfo->_DropBlock][pGameInfo->_DropRotate][y][x] == 0)
				continue;

			pGameInfo->_GameBoard[pGameInfo->_DropY + y][nx] = pGameInfo->_DropBlock;
		}
	}

	// 블록이 놓인 위치부터 +3까지 체크. 리턴값은 패킷으로 보낼 ClearLineByte로 지워지는 줄을 담은것.
	DWORD clearBit = LineClear(pGameInfo, pGameInfo->_DropY);

	// 공격/상쇄 체크
	Attack(pGameSession, sessionIndex, clearBit);

	// 받은 데미지가 있다면 그만큼 라인 생성
	Damage(pGameSession, sessionIndex);
	
	int midX = (_iMaxX / 2) - (_iShapeXSize / 2);

	enTetBlock nextBlockBag[5];

	pGameInfo->_DropBlock = GetNextBlockType(pGameInfo, nextBlockBag);
	pGameInfo->_DropRotate = 0;
	pGameInfo->_DropX = midX;
	pGameInfo->_DropY = 0;

	enTetBlock nextBlock = pGameInfo->_DropBlock;

	// 현재 보드 상태를 전송
	RefCountPointer boardUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*boardUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBoardUpdate(boardUpdatePacket, pGameInfo->_HoldingBlock, nextBlockBag, (BYTE*)(pGameInfo->_GameBoard),
		(BYTE*)(opGameInfo->_GameBoard));
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[sessionIndex], boardUpdatePacket))
	{
		EndGameSession(pGameSession, 1 - sessionIndex, -1);
		return;
	}

	// 상대에게 보드 전달
	RefCountPointer opponentPacket = RefCountPointer::MakeSharedPtr();
	(*opponentPacket)->Clear(sizeof(st_NetHeader));
	mpACKBoardUpdate(opponentPacket, opGameInfo->_HoldingBlock, nextBlockBag, (BYTE*)(opGameInfo->_GameBoard),
		(BYTE*)(pGameInfo->_GameBoard));
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], opponentPacket))
	{
		EndGameSession(pGameSession, sessionIndex, -1);
		return;
	}
}

void TetrisServer::CreateBlock(st_GAMESESSION* pGameSession, int sessionIndex)
{
	//pGameSession->_GameInfoArr[sessionIndex];
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);

	UpdateBoard(pGameSession, sessionIndex);

	enTetBlock nextBlock = pGameInfo->_DropBlock;

	if (!CollisionCheck(pGameInfo, nextBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY))
	{
		// 게임 오버
		EndGameSession(pGameSession, 1 - sessionIndex, sessionIndex);
		return;
	}

	// 스폰
	RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(blockUpdatePacket, true, nextBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[sessionIndex], blockUpdatePacket))
	{
		EndGameSession(pGameSession, 1 - sessionIndex, -1);
		return;
	}

	// 스폰
	RefCountPointer opponentBlockPacket = RefCountPointer::MakeSharedPtr();
	(*opponentBlockPacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(opponentBlockPacket, false, nextBlock, 0, pGameInfo->_DropX, pGameInfo->_DropY);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], opponentBlockPacket))
	{
		EndGameSession(pGameSession, sessionIndex, -1);
		return;
	}
}

void TetrisServer::StartCountDown(st_GAMESESSION* pGameSession)
{
	// 둘 다 준비 완료 -> 카운트다운 시작 처리
	RefCountPointer startCountPacket = RefCountPointer::MakeSharedPtr();
	(*startCountPacket)->Clear(sizeof(st_NetHeader));
	_pLog._dwPacketPoolUse++;

	mpACKCountDown(startCountPacket, _wCountDown - 1);
	MakePacketHeader(startCountPacket);

	startCountPacket.IncRefCount();
	SendPacket_UniCast(pGameSession->_SessionIDArr[0], startCountPacket, false);
	SendPacket_UniCast(pGameSession->_SessionIDArr[1], startCountPacket, false);

	// 게임 세션 카운팅으로 수정
	InterlockedExchange((LONG*)&pGameSession->_State, en_GAMESTATE_COUNTING);
	pGameSession->startTime = timeGetTime() + (_wCountDown * 1000);
}

void TetrisServer::CheckCountDown(st_GAMESESSION* pGameSession)
{
	LONG nowTime = timeGetTime();

	if (pGameSession->startTime < nowTime)
	{
		CreateBlock(pGameSession, 0);
		CreateBlock(pGameSession, 1);

		pGameSession->_GameInfoArr[0]._dwLastDropTime = timeGetTime();
		pGameSession->_GameInfoArr[1]._dwLastDropTime = timeGetTime();

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