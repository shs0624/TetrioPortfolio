#pragma once
#include "Includes.h"
#include "Util.h"
#include "NetServer.h"
#include "DBConnector.h"
#include "DBWriter.h"
#include "TetrisLoginServer.h"
#include "Protocol.h"
#include "CFreeList_LockFree.h"
#include "LogManager.h"

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBTLSConnector::_JobPool(2000, 5, 20);

TLSMemoryPoolManager<CDBPoolStruct>
SHS::DBWriterManager::_JobPool(2000, 5, 20);

void TetrisLoginServer::InitLoginServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
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
cpp_redis::client& TetrisLoginServer::GetTLSRedisClient()
{
	thread_local cpp_redis::client client;
	thread_local bool connected = false;

	if (!connected) {
		client.connect();
		connected = true;
	}

	return client;
}

bool TetrisLoginServer::OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr)
{
	_pLog._dwSessionCount++;

	return true;
}

void TetrisLoginServer::OnRelease(ULONGLONG sessionID)
{
	// 세션 Release -> 성공한 경우에만 탈거임
	_pLog._dwSessionCount--;
}

void TetrisLoginServer::OnRecv(ULONGLONG sessionID, RefCountPointer& cPacket)
{
	// 무조건 로그인 요청만 들어옴.
	WORD type;
	(**cPacket) >> type;

	switch (type)
	{
	case en_PACKET_CS_TETRIS_REQ_REGISTER:
		break;
	case en_PACKET_CS_TETRIS_REQ_LOGIN:
		MessageProc_Login(cPacket, sessionID);
		break;
	default:
		Disconnect(sessionID);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		break;
	}
}

void TetrisLoginServer::MessageProc_Login(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	WCHAR gameServerIP[16];
	WCHAR chatServerIP[16];
	WCHAR clientAddr[16];

	BYTE status = 1;

	INT64 AccountNo;
	(**cPacket) >> AccountNo;

	char ID[20];
	(*cPacket)->GetData(ID, sizeof(ID));

	char Passwd[20];
	(*cPacket)->GetData(Passwd, sizeof(Passwd));

	// @@TODO: DB에 전송할 때 여기에 넣기
	SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

	LPVOID pAddr = pDBConnector->AllocJobAddress();
	CDBLogin* pCDBLogin = new(pAddr)CDBLogin;
	pCDBLogin->_AccountNum = AccountNo;
	strcpy_s(pCDBLogin->_ID, 20, ID);
	strcpy_s(pCDBLogin->_Passwd, 20, Passwd);

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
		status = dfTETRIS_LOGIN_ERR_ID;
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpLoginRES(cPacket, AccountNo, status, NULL, NULL, NULL);
		SendPacket_UniCast(sessionID, cPacket);

		Disconnect(sessionID);
		return;
	}

	// 얻어온 결과 꺼내서 Passwd 비교
	std::wstring wsessionKey = GenerateSessionKey();

	std::string sessionKey = WstrToStr(wsessionKey);
	std::string passwd = pDBConnector->GetString(_enPasswd);
	std::string nickname(pDBConnector->GetString(_enNickname));

	pDBConnector->FreeQueryResult();
	_pLog._dwDBSelectTPS++;

	if (passwd.compare(Passwd) != 0)
	{
		status = dfTETRIS_LOGIN_ERR_PASSWD;
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpLoginRES(cPacket, AccountNo, status, NULL, NULL, NULL);
		SendPacket_UniCast(sessionID, cPacket);

		Disconnect(sessionID);
		return;
	}

	// Redis에 넣기.
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	_redisClient.hset(std::to_string(AccountNo), "SessionKey", sessionKey);
	_redisClient.hset(std::to_string(AccountNo), "Nickname", nickname);
	_redisClient.sync_commit();

	// 패킷 전송 준비
	(*cPacket)->Clear(sizeof(st_NetHeader));

	// IP 주소 관련 수정 필요
	wcsncpy_s(gameServerIP, _countof(gameServerIP), dfGAMESERVER_IP, sizeof(WCHAR) * 16);
	wcsncpy_s(chatServerIP, _countof(chatServerIP), dfCHATSERVER_PUBLICIP, sizeof(WCHAR) * 16);

	status = dfTETRIS_LOGIN_OK;

	mpLoginRES(cPacket, AccountNo, status, gameServerIP, (USHORT)dfGAMESERVER_PORT,
		wsessionKey.c_str());

	SendPacket_UniCast(sessionID, cPacket);
}

std::wstring TetrisLoginServer::GenerateSessionKey()
{
	static const std::wstring alphabet = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const size_t KEY_LEN = 64;
	BYTE randomBytes[KEY_LEN];

	std::wstring sessionKey;

	int len = alphabet.size();
	for (int i = 0; i < KEY_LEN; i++)
	{
		int randNum = (rand() % len);
		sessionKey.push_back(alphabet[randNum]);
	}
	
	return sessionKey;
}

void TetrisLoginServer::mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* gameIP, USHORT gamePort, const WCHAR* sessionKey)
{
	(**cPacket) << (WORD)en_PACKET_CS_TETRIS_RES_LOGIN;
	(**cPacket) << status;
	(**cPacket) << accountNum;
	

	(*cPacket)->PutData((char*)gameIP, sizeof(WCHAR) * 16);
	(**cPacket) << gamePort;
	(*cPacket)->PutData((char*)sessionKey, sizeof(WCHAR) * 64);
}

void TetrisLoginServer::OnError(int errorcode, WCHAR* message)
{

}
