#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "DBConnector.h"
#include "DBWriter.h"
#include "LoginServer.h"
#include "CommonProtocol_Login.h"
#include "CFreeList_LockFree.h"
#include "LogManager.h"

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBTLSConnector::_JobPool(2000, 5, 20);

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBWriterManager::_JobPool(2000, 5, 20);

void LoginServer::InitLoginServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
{
	mysql_library_init(0, NULL, NULL);

	SYSTEM_INFO si;
	GetSystemInfo(&si);

	int workCount = (int)si.dwNumberOfProcessors * 2;
	int concurrentCount = ((int)si.dwNumberOfProcessors / 2) - 1;
	StartNetServer(ip, port, workCount, concurrentCount, true, maxConnection);

	_DBWriterManager = new SHS::DBWriterManager();
	_DBWriterManager->InitDBWriterManager(workCount);

	_TimerThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, this, 0, &_TimerThreadID);
}

// 필요할 때 초기화 해서 사용할 수 있는 함수
cpp_redis::client& LoginServer::GetTLSRedisClient()
{
	thread_local cpp_redis::client client;
	thread_local bool connected = false;

	if (!connected) {
		client.connect();
		connected = true;
	}

	return client;
}

bool LoginServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	_pLog._dwSessionCount++;

	return true;
}

void LoginServer::OnRelease(ULONGLONG sessionID)
{
	// 세션 Release -> 성공한 경우에만 탈거임
	_pLog._dwSessionCount--;
}

void LoginServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	WCHAR gameServerIP[16];
	WCHAR chatServerIP[16];
	WCHAR clientAddr[16];

	BYTE status = 1;

	// 무조건 로그인 요청만 들어옴.
	WORD type;
	(**cPacket) >> type;

	INT64 AccountNo;
	(**cPacket) >> AccountNo;

	char sessionKey[64];
	(*cPacket)->GetData(sessionKey, sizeof(sessionKey));

	// @@TODO: DB에 전송할 때 여기에 넣기
	SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

	LPVOID pAddr = pDBConnector->AllocJobAddress();
	CDBLogin* pCDBLogin = new(pAddr)CDBLogin;
	pCDBLogin->_AccountNum = AccountNo;
	strcpy_s(pCDBLogin->_SessionKey, 64, sessionKey);

	pDBConnector->SendQuery_SELECT((IDBJob*)pCDBLogin);
	if (!pDBConnector->StoreQueryResult())
	{
		// 그냥 에러난거니까 디버그 브레이크 걸릴예정
		Disconnect(sessionID);
		return;
	}

	// SELECT 결과가 없다
	if (!pDBConnector->FetchQueryResult())
	{
		status = dfLOGIN_STATUS_ACCOUNT_MISS;
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpLoginRES(cPacket, AccountNo, status, NULL, NULL, NULL, NULL, NULL, NULL);
		SendPacket_UniCast(sessionID, cPacket);

		Disconnect(sessionID);
		return;
	}

	pDBConnector->FreeQueryResult();
	_pLog._dwDBSelectTPS++;

	// Redis에 넣기.
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	_redisClient.setex(std::to_string(AccountNo), 15, sessionKey);
	_redisClient.sync_commit();

	// 패킷 전송 준비
	(*cPacket)->Clear(sizeof(st_NetHeader));

	WCHAR ID[20];
	WCHAR Nickname[20];

	std::wstring wsID = L"ID_" + std::to_wstring(AccountNo);
	wcsncpy_s(ID, wsID.c_str(), sizeof(WCHAR) * 20);

	std::wstring wsNick = L"NICK_" + std::to_wstring(AccountNo);
	wcsncpy_s(Nickname, wsNick.c_str(), sizeof(WCHAR) * 20);

	if (wcscmp(clientAddr, L"127.0.0.1") == 0)
	{
		wcsncpy_s(chatServerIP, _countof(chatServerIP), L"127.0.0.1", sizeof(WCHAR) * 16);
	}
	else if (wcscmp(clientAddr, L"10.0.1.2") == 0)
	{
		wcsncpy_s(chatServerIP, _countof(chatServerIP), L"10.0.1.1", sizeof(WCHAR) * 16);
	}
	else if (wcscmp(clientAddr, L"10.0.2.2") == 0)
	{
		wcsncpy_s(chatServerIP, _countof(chatServerIP), L"10.0.2.1", sizeof(WCHAR) * 16);
	}
	else
	{
		// 외부에서 온 접속이니 외부 IP 
		wcsncpy_s(chatServerIP, _countof(chatServerIP), dfCHATSERVER_PUBLICIP, sizeof(WCHAR) * 16);
	}

	wcsncpy_s(gameServerIP, _countof(gameServerIP), dfGAMESERVER_IP, sizeof(WCHAR) * 16);

	status = dfMONITOR_TOOL_LOGIN_OK;

	mpLoginRES(cPacket, AccountNo, status, ID, Nickname,
		gameServerIP, (USHORT)dfGAMESERVER_PORT, chatServerIP, (USHORT)dfCHATSERVER_PORT);

	SendPacket_UniCast(sessionID, cPacket);
}

void LoginServer::mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* ID, WCHAR* Nickname, WCHAR* gameIP, USHORT gamePort, WCHAR* chatIP, USHORT chatPort)
{
	(**cPacket) << (WORD)en_PACKET_CS_LOGIN_RES_LOGIN;
	(**cPacket) << accountNum;
	(**cPacket) << status;

	(*cPacket)->PutData((char*)ID, sizeof(WCHAR) * 20);
	(*cPacket)->PutData((char*)Nickname, sizeof(WCHAR) * 20);

	(*cPacket)->PutData((char*)gameIP, sizeof(WCHAR) * 16);
	(**cPacket) << gamePort;
	(*cPacket)->PutData((char*)chatIP, sizeof(WCHAR) * 16);
	(**cPacket) << chatPort;
}

void LoginServer::OnError(int errorcode, WCHAR* message)
{

}
