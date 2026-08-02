#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::MessageProc_Login(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	INT64 accountNum;
	BYTE status = FALSE;
	WCHAR Nickname[20];
	CHAR tempSessionKey[64];

	(**cPacket) >> accountNum;

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
		ReleaseSRWLockExclusive(&_UserMapLock);

		// 이미 로그인 한 세션이니까, 메세지 취소하고 디스커넥트;
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);
		return;
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

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

	AcquireSRWLockExclusive(&_UserMapLock);
	_UserMap[userPtr->ulSessionID] = userPtr;
	ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_AccountNumUserMapLock);
	_AccountNumUserMap[userPtr->AccountNum] = userPtr;
	ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

	// 로그인 성공 RES 보내기
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESLogin(cPacket, status);
	SendPacket_UniCast(sessionID, cPacket);
	
	// 채팅 서버로의 입장
	InterlockedExchange((LONG*)&(userPtr->enServerState), en_SERVER_CHAT);

	// 채팅 벡터 내의 유저들 모두에게 채팅 메세지 전달
	// 하나의 패킷을 여러 유저에게 보내는 방식
	RefCountPointer chatEnterPacket = RefCountPointer::MakeSharedPtr();
	(*chatEnterPacket)->Clear(sizeof(st_NetHeader));
	mpACKChatEnter(chatEnterPacket, accountNum, Nickname);
	MakePacketHeader(chatEnterPacket);

	AcquireSRWLockExclusive(&_ChatDataLock);
	for (int i = 0; i < _ChatUserVec.size(); i++)
	{
		chatEnterPacket.IncRefCount();
		SendPacket_UniCast(_ChatUserVec[i]->ulSessionID, chatEnterPacket, false);
	}

	int idx = _ChatUserVec.size();
	_ChatUserVec.push_back(userPtr);
	_ChatUserIndexMap[userPtr->AccountNum] = idx;
	ReleaseSRWLockExclusive(&_ChatDataLock);

	_pLog._dwLoginMessageTPS++;
}

void TetrisServer::MessageProc_ChatMessage(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	static const int _MaxMessageLen = 100;

	INT64 accountNum;

	// 최대 제한은 100글자로 두자.
	WORD messageLen;
	WCHAR message[128];

	(**cPacket) >> messageLen;
	messageLen = min(messageLen, _MaxMessageLen);

	int copyLen = (*cPacket)->GetData((char*)message, sizeof(WCHAR) * messageLen);
	if (copyLen != sizeof(WCHAR) * messageLen)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);
		return;
	}

	// 채팅 보낸 유저 정보 찾기 -> 채팅을 보내고 서버를 이동하진 않았을거라 가정
	WCHAR nickname[20];

	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);
		return;
	}

	st_USER* userPtr = (*it).second;
	wcsncpy_s(nickname, userPtr->NickName, _TRUNCATE);
	accountNum = userPtr->AccountNum;
	ReleaseSRWLockShared(&_UserMapLock);

	// 채팅 벡터 내의 유저들 모두에게 채팅 메세지 전달
	// 하나의 패킷을 여러 유저에게 보내는 방식
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESChatMessage(cPacket, accountNum, nickname, messageLen, message);
	MakePacketHeader(cPacket);

	AcquireSRWLockExclusive(&_ChatDataLock);
	// 자기 자신도 포함해서 RES를 보낼 것
	for (int i = 0; i < _ChatUserVec.size(); i++)
	{
		cPacket.IncRefCount();
		SendPacket_UniCast(_ChatUserVec[i]->ulSessionID, cPacket, false);
	}
	ReleaseSRWLockExclusive(&_ChatDataLock);

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;

	_pLog._dwChatMessageTPS++;
}

void TetrisServer::MessageProc_MatchingReq(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	BYTE status = TRUE;

	// 매칭 큐에 넣기
	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);
		return;
	}

	st_USER* userPtr = (*it).second;

	// 매칭 서버로의 입장
	InterlockedExchange((LONG*)&(userPtr->enServerState), en_SERVER_MATCHING);
	ReleaseSRWLockShared(&_UserMapLock);

	pMatchManager->Enqueue(userPtr);

	// 매칭 요청에 대한 응답
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESMatching(cPacket, status);

	SendPacket_UniCast(sessionID, cPacket);
}