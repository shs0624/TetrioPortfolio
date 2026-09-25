#include "Includes.h"
#include "LogManager.h"
#include "Protocol.h"
#include "NetServer.h"
#include "GameHeader.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::MoveLeft(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	ULONGLONG sessionID = pGameSession->_SessionIDArr[sessionIndex];

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	int newX = pGameInfo->_DropX - 1;

	// 충돌하면 취소
	if (!CollisionCheck(pGameInfo, pGameInfo->_DropBlock, pGameInfo->_DropRotate, newX, pGameInfo->_DropY))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	pGameInfo->_DropX -= 1;

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
	
	if (!SendPacket_UniCast(sessionID, cPacket))
	{
		EndGameSession(pGameSession, 1 - sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	// 상대편에게도 전송
	RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
	{
		EndGameSession(pGameSession, sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
}

void TetrisServer::MoveRight(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	ULONGLONG sessionID = pGameSession->_SessionIDArr[sessionIndex];

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	int newX = pGameInfo->_DropX + 1;

	// 충돌하면 취소
	if (!CollisionCheck(pGameInfo, pGameInfo->_DropBlock, pGameInfo->_DropRotate, newX, pGameInfo->_DropY))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	pGameInfo->_DropX += 1;

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);

	if (!SendPacket_UniCast(sessionID, cPacket))
	{
		EndGameSession(pGameSession, 1 - sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	// 상대편에게도 전송
	RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
	{
		EndGameSession(pGameSession, sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
}

void TetrisServer::SoftDrop(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	ULONGLONG sessionID = pGameSession->_SessionIDArr[sessionIndex];

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	int newY = pGameInfo->_DropY + 1;

	// 충돌하면 고정 -> 취소가 아니다.
	if (!CollisionCheck(pGameInfo, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, newY))
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		CreateBlock(pGameSession, sessionIndex);

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	pGameInfo->_DropY += 1;

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);

	if (!SendPacket_UniCast(sessionID, cPacket))
	{
		EndGameSession(pGameSession, 1 - sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	// 상대편에게도 전송
	RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
	(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
	mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
	_pLog._dwPacketPoolUse++;

	if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
	{
		EndGameSession(pGameSession, sessionIndex, -1);
		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
}

// 하드 드랍할 수 있는 가장 낮은 Y값 반환
int TetrisServer::GetHardDropY(st_GameInfo* pGameInfo)
{
	int retY = _iMaxY - 1;
	for (int x = 0; x < BLOCK_ARR_LENGTH; x++)
	{
		//현재 블록에서 이 x위치에 블록이 있는지
		int blockY = -1;
		for (int y = BLOCK_ARR_LENGTH - 1; y >= 0; y--)
		{
			if (ShapeTable[pGameInfo->_DropBlock][pGameInfo->_DropRotate][y][x] != 0)
			{
				blockY = y;
				break;
			}
		}

		if (blockY == -1)
			continue;

		int nx = pGameInfo->_DropX + x;
		int ny = pGameInfo->_DropY + blockY;
		
		int landY = _iMaxY - 1;
		for (int i = ny + 1; i < _iMaxY; i++)
		{
			if (pGameInfo->_GameBoard[i][nx] != 0)
			{
				landY = i - 1;
				break;
			}
		}

		landY = landY - blockY;

		retY = min(landY, retY);
	}

	return retY;
}

void TetrisServer::HardDrop(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	int hardDropY = GetHardDropY(pGameInfo);

	pGameInfo->_DropY = hardDropY;

	CreateBlock(pGameSession, sessionIndex);
	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);

	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
}

const st_KickOffset* TetrisServer::GetKicks(enTetBlock block, int fromRotate, bool clockwise)
{
	int idx = KickLookup[fromRotate][clockwise ? 0 : 1];

	if (block == enIBlock)
		return KicksI[idx];
	if (block == enOBlock || block == NoneBlock || block == enGarbageBlock)
		return KicksNone;

	return KicksJLSTZ[idx];
}

void TetrisServer::Rotate(st_GAMESESSION* pGameSession, int sessionIndex, bool clockwise, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	ULONGLONG sessionID = pGameSession->_SessionIDArr[sessionIndex];
	int dropRotate = pGameInfo->_DropRotate;
	int targetRotate = 0;

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	if (clockwise)
		targetRotate = (dropRotate + 1) % BLOCK_ARR_LENGTH;
	else
		targetRotate = (dropRotate + BLOCK_ARR_LENGTH - 1) % BLOCK_ARR_LENGTH;

	const st_KickOffset* kicks = GetKicks(pGameInfo->_DropBlock, dropRotate, clockwise);

	// 0은 offset없이 회전하는 경우
	for (int i = 0; i < 5; i++)
	{
		int nx = pGameInfo->_DropX + kicks[i].x;
		int ny = pGameInfo->_DropY + kicks[i].y;

		if (nx < 0 || ny < 0)
			continue;

		// 충돌하면 패스
		if (!CollisionCheck(pGameInfo, pGameInfo->_DropBlock, targetRotate, nx, ny))
			continue;

		pGameInfo->_DropRotate = targetRotate;
		pGameInfo->_DropX = nx;
		pGameInfo->_DropY = ny;

		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
			return;
		}

		// 상대편에게도 전송
		RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
		(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
		_pLog._dwPacketPoolUse++;

		if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
		{
			EndGameSession(pGameSession, sessionIndex, -1);
			ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
			return;
		}

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);

		return;
	}

	ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);

	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
}

void TetrisServer::Hold(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket)
{
	AcquireSRWLockExclusive(&pGameSession->_GameSessionLock);
	st_GameInfo* pGameInfo = &(pGameSession->_GameInfoArr[sessionIndex]);
	ULONGLONG sessionID = pGameSession->_SessionIDArr[sessionIndex];

	if (pGameSession->_State != en_GAMESTATE_PLAYING)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
		return;
	}

	// Hold에 블록이 있다면, 현재 블록과 교체
	if (pGameInfo->_HoldingBlock != NoneBlock)
	{
		// Drop블록과 Hold블록 교체
		enTetBlock newblock = pGameInfo->_HoldingBlock;
		pGameInfo->_HoldingBlock = pGameInfo->_DropBlock;
		pGameInfo->_DropBlock = newblock;
		pGameInfo->_DropRotate = 0;
		pGameInfo->_DropX = (_iMaxX / 2) - (_iShapeXSize / 2);
		pGameInfo->_DropY = 0;

		// 보드 갱신 패킷에 담을 NextBlock큐 정보 
		enTetBlock nextBlockBag[5];
		GetNextBlockArr(pGameInfo, nextBlockBag);

		// 블록 업데이트 패킷 전송
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
		
		// 상대편에게도 전송
		RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
		(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
		_pLog._dwPacketPoolUse++;

		if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
		{
			EndGameSession(pGameSession, sessionIndex, -1);
			ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
			return;
		}

		// 현재 보드 상태를 전송 (생성예정 큐 보내기 위함)
		RefCountPointer boardUpdatePacket = RefCountPointer::MakeSharedPtr();
		(*boardUpdatePacket)->Clear(sizeof(st_NetHeader));
		mpACKBoardUpdate(boardUpdatePacket, pGameInfo->_HoldingBlock, nextBlockBag, (BYTE*)(pGameSession->_GameInfoArr[sessionIndex]._GameBoard),
			(BYTE*)(pGameSession->_GameInfoArr[1 - sessionIndex]._GameBoard));
		_pLog._dwPacketPoolUse++;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			return;
		}

		if (!SendPacket_UniCast(sessionID, boardUpdatePacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			return;
		}
	}
	else
	{
		// 들고 있던 블록 hold에 넣기
		pGameInfo->_HoldingBlock = pGameInfo->_DropBlock;

		// 다음 블록 꺼내기
		enTetBlock nextBlockBag[5];
		pGameInfo->_DropBlock = GetNextBlockType(pGameInfo, nextBlockBag);
		pGameInfo->_DropRotate = 0;
		pGameInfo->_DropX = (_iMaxX / 2) - (_iShapeXSize / 2);
		pGameInfo->_DropY = 0;

		// 블록 업데이트 패킷 전송
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(cPacket, true, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);

		// 상대편에게도 전송
		RefCountPointer blockUpdatePacket = RefCountPointer::MakeSharedPtr();
		(*blockUpdatePacket)->Clear(sizeof(st_NetHeader));
		mpACKBlockUpdate(blockUpdatePacket, false, pGameInfo->_DropBlock, pGameInfo->_DropRotate, pGameInfo->_DropX, pGameInfo->_DropY);
		_pLog._dwPacketPoolUse++;

		if (!SendPacket_UniCast(pGameSession->_SessionIDArr[1 - sessionIndex], blockUpdatePacket))
		{
			EndGameSession(pGameSession, sessionIndex, -1);
			ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);
			return;
		}

		// 현재 보드 상태를 전송 (생성예정 큐 보내기 위함)
		RefCountPointer boardUpdatePacket = RefCountPointer::MakeSharedPtr();
		(*boardUpdatePacket)->Clear(sizeof(st_NetHeader));
		mpACKBoardUpdate(boardUpdatePacket, pGameInfo->_HoldingBlock, nextBlockBag, (BYTE*)(pGameSession->_GameInfoArr[sessionIndex]._GameBoard),
			(BYTE*)(pGameSession->_GameInfoArr[1 - sessionIndex]._GameBoard));
		_pLog._dwPacketPoolUse++;

		ReleaseSRWLockExclusive(&pGameSession->_GameSessionLock);

		if (!SendPacket_UniCast(sessionID, cPacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			return;
		}

		if (!SendPacket_UniCast(sessionID, boardUpdatePacket))
		{
			EndGameSession(pGameSession, 1 - sessionIndex, -1);
			return;
		}
	}
}