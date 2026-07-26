#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "TetrisServer.h"

void TetrisServer::MessageProc_Login(ULONGLONG sessionID, ULONGLONG accountNum, RefCountPointer& cPacket)
{
	BYTE status = FALSE;
	WCHAR Nickname[20];
	CHAR tempSessionKey[64];

	(*cPacket)->GetData((char*)tempSessionKey, sizeof(tempSessionKey));

	// Redis 검증
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	cpp_redis::reply reply_SessionKey;
	cpp_redis::reply reply_Nickname;

	// future가 error가 나올 수 있어 try catch 시도
	try {
		// future_error 가능
		auto fut_key = _redisClient.hget(std::to_string(accountNum), "SessionKey");
		auto fut_nick = _redisClient.hget(std::to_string(accountNum), "Nickname");
		_redisClient.sync_commit();

		reply_SessionKey = fut_key.get();
		reply_Nickname = fut_nick.get();
	}
	catch (const std::exception& e) {
		// Redis 통신 실패 처리
		_redisClient.disconnect();

		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	if (!reply_SessionKey.is_string() || !reply_Nickname.is_string())
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}

	// 보낸 세션키와 레디스에 꺼낸 세션키 비교
	if (reply_SessionKey.as_string().compare(0, 64, tempSessionKey, 64) != 0)
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);
		return;
	}
	
	// 닉네임도 Redis에서
	int result = MultiByteToWideChar(CP_UTF8, 0, reply_Nickname.as_string().c_str(), -1,
		Nickname, _countof(Nickname));
	
	if (result <= 0)
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status);
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
	memcpy_s(userPtr->SessionKey, sizeof(userPtr->SessionKey), tempSessionKey, sizeof(tempSessionKey));
	//wcsncpy_s(userPtr->ID, tempID, sizeof(WCHAR) * 20);
	wcsncpy_s(userPtr->NickName, Nickname, _TRUNCATE);
	
}