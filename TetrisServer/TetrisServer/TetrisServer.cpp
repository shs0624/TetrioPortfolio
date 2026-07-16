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

	INT64 AccountNo;
	(**cPacket) >> AccountNo;

	// enum에 따라 다른 메세지 처리
	switch ((en_PACKET_TYPE)type)
	{
	case en_PACKET_CS_TETRIS_REQ_LOGIN:
		MessageProc_Login(sessionID, AccountNo, cPacket);
		break;
	}
}