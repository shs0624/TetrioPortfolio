#include "Includes.h"
#include "LogManager.h"
#include "Protocol.h"
#include "NetServer.h"
#include "GameHeader.h"
#include "UserSession.h"
#include "MatchingManager.h"
#include "TetrisServer.h"
#include "CConfigReader.h"

procademy::CCrashDump cCrashDump;

int main()
{
	srand(time(NULL));

	CConfigReader config;
	if (!config.Load("config.txt"))
	{
		std::cerr << "config.txt를 열 수 없습니다." << std::endl;
		return 1;
	}

	int gameServerPort = config.GetInt("GameServerPort");

    TetrisServer* pServer = new TetrisServer();
    pServer->InitTetrisServer(INADDR_ANY, gameServerPort, true, 10000);

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

bool TetrisServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	_pLog._dwSessionCount++;
	return true;
}

void TetrisServer::OnError(int errorcode, WCHAR* message)
{

}

void TetrisServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WORD type;
	(**cPacket) >> type;

	// enum에 따라 다른 메세지 처리
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_TETRIS_REQ_LOGIN:
		MessageProc_Login(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_CHAT_MESSAGE:
		MessageProc_ChatMessage(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_MATCHING:
		MessageProc_MatchingReq(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_GAME_READY:
		MessageProc_GameReadyReq(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_ACK_GAME_USERINPUT:
		MessageProc_GameInput(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_GAME_RETURNCHAT:
		MessageProc_ReturnChat(sessionID, cPacket);
		break;
	case en_PACKET_CS_TETRIS_REQ_MATCHING_CANCEL:
		MessageProc_MatchingCancelReq(sessionID, cPacket);
		break;
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
			LeaveChat(sessionID);
			break;
		case en_SERVER_MATCHING:
			pMatchManager->Dequeue(pUser);
			break;
		case en_SERVER_GAME:
			// 카운트 다운 전 상태라면 조치가 필요함
			if (pUser->pGameSession->_State == en_GAMESTATE_WAIT_READY)
				EndGameSession(pUser->pGameSession, 1 - (pUser->byGameSessionIndex), -1);
			break;
		}

		_UserMap.erase(sessionID);
		ReleaseSRWLockExclusive(&_UserMapLock);

		AcquireSRWLockExclusive(&_AccountNumUserMapLock);
		auto itAccountNum = _AccountNumUserMap.find(pUser->AccountNum);
		if (itAccountNum != _AccountNumUserMap.end())
		{
			if((*itAccountNum).second->ulSessionID == pUser->ulSessionID)
				_AccountNumUserMap.erase(pUser->AccountNum);
		}
		ReleaseSRWLockExclusive(&_AccountNumUserMapLock);

		_UserPool->Free(pUser);

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

// context를 통해 어떤 객체인지 전달해서 사용 (static 함수)
void TetrisServer::OnMatchFound(LPVOID context, st_USER* pUser1, st_USER* pUser2)
{
	TetrisServer* pServer = (TetrisServer*)context;

	pServer->LeaveChat(pUser1->ulSessionID);
	pServer->LeaveChat(pUser2->ulSessionID);

	// 게임 방 생성 후 매칭 성공 패킷 전송까지
	if (!pServer->SetGameSession(pUser1, pUser2))
	{
		// @@TODO: 방 생성이 실패했는데 -> 연결끊김 / 세션 꽉참
	}
}

void TetrisServer::RegisterNetServerLog(LPVOID context)
{
	TetrisServer* pServer = (TetrisServer*)context;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);
}