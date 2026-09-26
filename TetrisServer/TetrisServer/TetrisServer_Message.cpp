#include "Includes.h"
#include "LogManager.h"
#include "Util.h"
#include "Protocol.h"
#include "GameHeader.h"
#include "NetServer.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"

void TetrisServer::MessageProc_Login(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	INT64 accountNum;
	BYTE status = FALSE;
	WCHAR Nickname[20];
	WCHAR tempSessionKey[64];

	(**cPacket) >> accountNum;

	(*cPacket)->GetData((char*)tempSessionKey, sizeof(tempSessionKey));
	std::wstring wSessionKey(tempSessionKey, 64);

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

	std::string sessionKey = WstrToStr(wSessionKey);
	// 보낸 세션키와 레디스에 꺼낸 세션키 비교
	if (reply_SessionKey.as_string().compare(0, 64, sessionKey) != 0)
	{
		// 검증 실패
		(*cPacket)->Clear(sizeof(st_NetHeader));
		mpRESLogin(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);

		_pLog._dwRedisCertificationFailTotal++;
		Disconnect(sessionID);

		LogController::GetInstance()->WriteLog("Disconnect_SessionKey Redis Cert Fail - " + to_string(accountNum));

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

		LogController::GetInstance()->WriteLog("Disconnect_Nickname Redis Cert Fail - " + to_string(accountNum));

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

		LogController::GetInstance()->WriteLog("Disconnect_SessionIDUserMap Duplicated - " + to_string(sessionID));
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

		LogController::GetInstance()->WriteLog("Disconnect_AccountNumUserMap Duplicated - " + to_string(accountNum));
	}
	else
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

	st_USER* userPtr = _UserPool->Alloc();

	_pLog._dwPlayerPoolUse++;

	userPtr->ulSessionID = sessionID;
	userPtr->AccountNum = accountNum;
	userPtr->dwLastRecvTime = timeGetTime();
	userPtr->bBatched = FALSE;
	userPtr->pGameSession = NULL;
	userPtr->enServerState = None;
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
	
	LogController::GetInstance()->WriteLog("LoginSuccess - Nickname : " + WstrToStr(userPtr->NickName) + " | AccountNum : " + to_string(userPtr->AccountNum));

	EnterChat(userPtr);

	_pLog._dwLoginMessageTotal++;
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
	if (userPtr->enServerState != en_SERVER_CHAT && userPtr->enServerState != en_SERVER_MATCHING)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(sessionID);

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}

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
}

void TetrisServer::MessageProc_MatchingReq(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	BYTE status = TRUE;

	AcquireSRWLockShared(&_UserMapLock);
	auto it = _UserMap.find(sessionID);
	if (it == _UserMap.end())
	{
		ReleaseSRWLockShared(&_UserMapLock);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		Disconnect(sessionID);

		LogController::GetInstance()->WriteLog("Disconnect_MatchingREQ_InvalidUser - " + to_string(sessionID));

		return;
	}

	st_USER* userPtr = (*it).second;
	if (userPtr->enServerState != en_SERVER_CHAT)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(sessionID);

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		LogController::GetInstance()->WriteLog("Disconnect_MatchingREQ_Not Chat State- " + to_string(userPtr->AccountNum));

		return;
	}

	// 매칭 서버로의 입장
	InterlockedExchange((LONG*)&(userPtr->enServerState), en_SERVER_MATCHING);
	ReleaseSRWLockShared(&_UserMapLock);

	pMatchManager->Enqueue(userPtr);

	// 매칭 요청에 대한 응답
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESMatching(cPacket, status);

	SendPacket_UniCast(sessionID, cPacket);
}

void TetrisServer::MessageProc_MatchingCancelReq(ULONGLONG sessionID, RefCountPointer& cPacket)
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

		LogController::GetInstance()->WriteLog("Disconnect_MatchCancelREQ_InvalidUser - " + to_string(sessionID));

		return;
	}

	st_USER* userPtr = (*it).second;
	if (userPtr->enServerState != en_SERVER_MATCHING)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(sessionID);

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		LogController::GetInstance()->WriteLog("Disconnect_NotMatchingState - " + to_string(userPtr->AccountNum));

		return;
	}

	ReleaseSRWLockShared(&_UserMapLock);

	if (!pMatchManager->Dequeue(userPtr))
		status = false;
	else
		InterlockedExchange((LONG*)&(userPtr->enServerState), en_SERVER_CHAT);

	// 매칭 취소 요청에 대한 응답
	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESMatchingCancel(cPacket, status);

	SendPacket_UniCast(sessionID, cPacket);
}

void TetrisServer::MessageProc_GameReadyReq(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	BYTE status = TRUE;

	// 유저 찾기
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
	ReleaseSRWLockShared(&_UserMapLock);

	// 매칭 상태가 아니라면 실패
	(*cPacket)->Clear(sizeof(st_NetHeader));
	if (userPtr->enServerState != en_SERVER_MATCHING)
	{
		status = FALSE;
		mpRESGameReady(cPacket, status);

		SendPacket_UniCast(sessionID, cPacket);
		return;
	}

	InterlockedExchange((LONG*)&userPtr->enServerState, en_SERVER_GAME);

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESGameReady(cPacket, status);

	SendPacket_UniCast(sessionID, cPacket);

	// 0번이면 0b01, 1번이면 0b10
	LONG myBit = 1 << userPtr->byGameSessionIndex;  
	LONG prevMask = InterlockedOr((LONG*)&userPtr->pGameSession->_lReady, myBit);

	// 둘 다 준비 완료된 상태면 
	if ((prevMask | myBit) == 0b11)
	{
		if (InterlockedCompareExchange((LONG*)&userPtr->pGameSession->_State,
			en_GAMESTATE_COUNTING, en_GAMESTATE_WAIT_READY) == en_GAMESTATE_WAIT_READY)
		{
			StartCountDown(userPtr->pGameSession);
		}
	}
}

void TetrisServer::MessageProc_GameInput(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	// 유저 찾기
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
	if (userPtr->enServerState != en_SERVER_GAME)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(sessionID);

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}
	ReleaseSRWLockShared(&_UserMapLock);

	en_INPUT_TYPE inputType;
	(**cPacket) >> (DWORD&)inputType;

	switch (inputType)
	{
	case en_INPUT_LEFT:
		MoveLeft(userPtr->pGameSession, userPtr->byGameSessionIndex, cPacket);
		break;
	case en_INPUT_RIGHT:
		MoveRight(userPtr->pGameSession, userPtr->byGameSessionIndex, cPacket);
		break;
	case en_INPUT_SOFTDROP:
		SoftDrop(userPtr->pGameSession, userPtr->byGameSessionIndex, cPacket);
		break;
	case en_INPUT_HARDDROP:
		HardDrop(userPtr->pGameSession, userPtr->byGameSessionIndex, cPacket);
		break;
	case en_INPUT_ROTATE_CLOCKWISE:
		Rotate(userPtr->pGameSession, userPtr->byGameSessionIndex, true, cPacket);
		break;
	case en_INPUT_ROTATE_COUNTERCLOCKWISE:
		Rotate(userPtr->pGameSession, userPtr->byGameSessionIndex, false, cPacket);
		break;
	case en_INPUT_HOLD:
		Hold(userPtr->pGameSession, userPtr->byGameSessionIndex, cPacket);
		break;
	}
}

void TetrisServer::MessageProc_ReturnChat(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	BYTE status = true;

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
	if (userPtr->enServerState != en_SERVER_GAME)
	{
		ReleaseSRWLockShared(&_UserMapLock);
		Disconnect(sessionID);

		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		return;
	}
	ReleaseSRWLockShared(&_UserMapLock);

	_pLog._dwGameUserCount--;

	EnterChat(userPtr);

	(*cPacket)->Clear(sizeof(st_NetHeader));
	mpRESReturnChat(cPacket, status);

	SendPacket_UniCast(sessionID, cPacket);
}