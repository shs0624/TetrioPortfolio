#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "TetrisServer.h"

void TetrisServer::MessageProc_Login(ULONGLONG sessionID, ULONGLONG accountNum, RefCountPointer& cPacket)
{
	BYTE status = FALSE;
	WCHAR tempID[20];
	WCHAR tempPasswd[20];
	CHAR tempSessionKey[64];

	(*cPacket)->GetData((char*)tempID, sizeof(tempID));
	(*cPacket)->GetData((char*)tempPasswd, sizeof(tempPasswd));
	(*cPacket)->GetData((char*)tempSessionKey, sizeof(tempSessionKey));

	// Redis 검증
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	cpp_redis::reply reply;

	// future가 error가 나올 수 있어 try catch 시도
	try {
		// future_error 가능
		auto fut = _redisClient.get(std::to_string(accountNum));
		_redisClient.sync_commit();
		reply = fut.get();
	}
	catch (const std::exception& e) {
		// Redis 통신 실패 처리
		_redisClient.disconnect();

		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	if (!reply.is_string())
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status, accountNum);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	status = TRUE;
	// 세션 -> 유저로 변경

	AcquireSRWLockExclusive(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it != _UserMap.end())
	{
		// 이미 로그인 한 세션이니까, 메세지 취소하고 디스커넥트
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);
		return;
	}
	else
		ReleaseSRWLockShared(&_UserMapLock);

	AcquireSRWLockExclusive(&_AccountNumUserMapLock);
	it = _AccountNumUserMap.find(accountNum);
	if (it != _AccountNumUserMap.end())
	{
		// 기존에 있던 것만 쳐내겠다.
		ULONGLONG _aliveSessionID = (*it).second->ulSessionID;
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

		_pLog._dwDuplicatedLoginTotal++;
		Disconnect(_aliveSessionID);
	}
	else
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

	st_USER* userPtr = _UserPool->Alloc();

	_pLog._dwPlayerPoolUse++;

	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bBatched = FALSE;
	wcsncpy_s(userPtr->ID, tempID, sizeof(WCHAR) * 20);
	// 닉네임도 Redis에서
	//wcsncpy_s(userPtr->NickName, tempN, sizeof(WCHAR) * 20);
	memcpy_s(userPtr->SessionKey, sizeof(userPtr->SessionKey), tempSessionKey, sizeof(tempSessionKey));
}