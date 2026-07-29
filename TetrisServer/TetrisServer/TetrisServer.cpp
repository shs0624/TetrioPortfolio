#include "Includes.h"
#include "Protocol.h"
#include "NetServer.h"
#include "TetrisServer.h"

int main()
{
    TetrisServer* pServer = new TetrisServer();
    pServer->InitTetrisServer(INADDR_ANY, SERVERPORT, true, 10000);

	char ch;
    while (1)
    {
		// 컨트롤?
		ch = _getch();
		if (ch == 'Q' || ch == 'q')
		{
			pServer->QuitServer();
			break;
		}
		if (ch == 'P' || ch == 'p')
		{
			ProfileDataOutText("ProfileData.txt");
		}
    }

	return 0;
}

// 필요할 때 초기화 해서 사용할 수 있는 함수
cpp_redis::client& TetrisServer::GetTLSRedisClient()
{
	thread_local cpp_redis::client client;
	thread_local bool connected = false;

	if (!connected) {
		client.connect();
		connected = true;
	}

	return client;
}

void TetrisServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	(**cPacket) >> type;

	// enum에 따라 다른 메세지 처리
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_TETRISLOGIN_REQ_LOGIN:
		MessageProc_Login(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_CHAT_MESSAGE:
		MessageProc_ChatMessage(sessionID, cPacket);
	}
}

void TetrisServer::OnRelease(ULONGLONG sessionID)
{
	// 세션 Release
	AcquireSRWLockExclusive(&_UserMapLock);
	auto itUser = _UserMap.find(sessionID);
	if (itUser != _UserMap.end())
	{
		st_USER* pUser = (*itUser).second;

		// @@TODO : 유저가 어떤 서버에 속해있는지 확인 후 제거
		switch (pUser->enServerState)
		{
		case en_SERVER_CHAT:
			AcquireSRWLockExclusive(&_ChatDataLock);
			auto itChat = _ChatUserIndexMap.find(sessionID);
			if (itChat != _ChatUserIndexMap.end())
			{
				swap(_ChatUserVec.back(), _ChatUserVec[(*itChat).second]);
				_ChatUserVec.pop_back();
			}

			_ChatUserIndexMap.erase(sessionID);
			ReleaseSRWLockExclusive(&_ChatDataLock);
			break;
		}

		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		AcquireSRWLockExclusive(&_AccountNumUserMapLock);
		_AccountNumUserMap.erase(pUser->AccountNum);
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

		_UserPool->Free(pUser);

		_pLog._dwUserCount--;
		_pLog._dwPlayerPoolUse--;
	}
	else
		ReleaseSRWLockExclusive(&_UserMapLock);

	AcquireSRWLockExclusive(&_SessionMapLock);
	auto itSession = _SessionMap.find(sessionID);
	if (itSession != _SessionMap.end())
	{
		st_SESSION* pSession = (*itSession).second;
		_SessionMap.erase(sessionID);
		_SessionPool->Free(pSession);

		ReleaseSRWLockExclusive(&_SessionMapLock);

		_pLog._dwSessionCount--;
	}
	else
		ReleaseSRWLockExclusive(&_SessionMapLock);
}