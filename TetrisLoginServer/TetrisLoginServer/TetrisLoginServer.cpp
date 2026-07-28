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
	case en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_ID:
	case en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_NICKNAME:
		MessageProc_Dupcheck(cPacket, type, sessionID);
		break;
	case en_PACKET_CS_TETRISLOGIN_REQ_REGISTER:
		MessageProc_Register(cPacket, sessionID);
		break;
	case en_PACKET_CS_TETRISLOGIN_REQ_LOGIN:
		MessageProc_Login(cPacket, sessionID);
		break;
	default:
		Disconnect(sessionID);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
		break;
	}
}

void TetrisLoginServer::MessageProc_Register(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	BYTE status = 1;
	INT64 AccountNum = -1;

	char ID[20];
	(*cPacket)->GetData(ID, sizeof(ID));

	char Passwd[20];
	(*cPacket)->GetData(Passwd, sizeof(Passwd));

	char Nickname[20];
	(*cPacket)->GetData(Nickname, sizeof(Nickname));

	bool idPass = false;
	bool nickPass = false;

	// Redis에 id, nickname 검색 -> 중복 가입을 통과했는지 체크
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	_redisClient.get("signup:id:" + std::string(ID),
		[&](cpp_redis::reply& reply) {
			idPass = !reply.is_null();
		});

	_redisClient.get("signup:nickname:" + std::string(Nickname),
		[&](cpp_redis::reply& reply) {
			nickPass = !reply.is_null();
		});

	_redisClient.sync_commit();

	if (!idPass || !nickPass)
	{
		status = dfTETRIS_REGISTER_ERR_DUPLICATED;
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpRegisterRES(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);
		return;
	}

	// 가입하기 -> INSERT
	SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

	LPVOID pAddr = pDBConnector->AllocJobAddress();
	CDBRegister_Insert* pCDBRegister = new(pAddr)CDBRegister_Insert;
	strcpy_s(pCDBRegister->_Nickname, 20, Nickname);
	strcpy_s(pCDBRegister->_ID, 20, ID);
	strcpy_s(pCDBRegister->_Passwd, 20, Passwd);

	int result = pDBConnector->SendQuery_INSERT((IDBJob*)pCDBRegister);
	if (result != true)
	{
		if (result == 1062)
		{
			status = dfTETRIS_REGISTER_ERR_DUPLICATED;
			// 실패 패킷 전송 준비
			(*cPacket)->Clear(sizeof(st_NetHeader));

			mpRegisterRES(cPacket, status);
			SendPacket_UniCast(sessionID, cPacket);
			return;
		}

		Disconnect(sessionID);
		return;
	}

	_redisClient.del({ "signup:id:" + std::string(ID) });
	_redisClient.del({ "signup:nickname:" + std::string(Nickname) });
	_redisClient.sync_commit();

	status = dfTETRIS_REGISTER_OK;
	(*cPacket)->Clear(sizeof(st_NetHeader));

	mpRegisterRES(cPacket, status);
	SendPacket_UniCast(sessionID, cPacket);
	return;
}

void TetrisLoginServer::MessageProc_Dupcheck(RefCountPointer& cPacket, WORD type, ULONGLONG sessionID)
{
	BYTE status = 1;

	char ID[20];
	(*cPacket)->GetData(ID, sizeof(ID));

	char Nickname[20];
	(*cPacket)->GetData(Nickname, sizeof(Nickname));

	if (type == en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_ID)
	{
		bool idPass = false;

		// ID 중복 체크 -> SELECT
		SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

		LPVOID pAddr = pDBConnector->AllocJobAddress();
		CDBRegister_Check_ID* pCDDupCheck = new(pAddr)CDBRegister_Check_ID;
		strcpy_s(pCDDupCheck->_ID, 20, ID);

		pDBConnector->SendQuery_SELECT((IDBJob*)pCDDupCheck);
		if (!pDBConnector->StoreQueryResult())
		{
			// 그냥 에러난거니까 디버그 브레이크 걸릴예정
			Disconnect(sessionID);
			return;
		}

		// SELECT 결과가 있다 -> 중복됨
		if (pDBConnector->FetchQueryResult())
		{
			idPass = false;
		}
		else
		{
			// Redis에 id 검색 -> 누군가 중복 가입중인지 체크
			cpp_redis::client& _redisClient = GetTLSRedisClient();
			_redisClient.set_advanced("signup:id:" + std::string(ID), "1", true, 60, false, 0, true, false,
				[&](cpp_redis::reply& reply) {
					idPass = !reply.is_null();
				});

			_redisClient.sync_commit();
		}		

		if (!idPass)
		{
			status = dfTETRIS_REGISTER_ERR_DUPLICATED;
			// 실패 패킷 전송 준비
			(*cPacket)->Clear(sizeof(st_NetHeader));

			mpDupcheckRES(cPacket, status);
			SendPacket_UniCast(sessionID, cPacket);
			return;
		}

		// 중복체크 성공
		status = true;
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpDupcheckRES(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);
		return;
	}
	else if (type == en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_NICKNAME)
	{
		bool nickPass = false;

		// 닉네임 중복 체크 -> SELECT
		SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

		LPVOID pAddr = pDBConnector->AllocJobAddress();
		CDBRegister_Check_Nickname* pCDDupCheck = new(pAddr)CDBRegister_Check_Nickname;
		strcpy_s(pCDDupCheck->_Nickname, 20, Nickname);

		pDBConnector->SendQuery_SELECT((IDBJob*)pCDDupCheck);
		if (!pDBConnector->StoreQueryResult())
		{
			// 그냥 에러난거니까 디버그 브레이크 걸릴예정
			Disconnect(sessionID);
			return;
		}

		// SELECT 결과가 있다 -> 중복됨
		if (pDBConnector->FetchQueryResult())
		{
			nickPass = false;
		}
		else
		{
			// Redis에 닉네임 검색 -> 누군가 중복 가입중인지 체크
			cpp_redis::client& _redisClient = GetTLSRedisClient();
			_redisClient.set_advanced("signup:nickname:" + std::string(Nickname), "1", true, 60, false, 0, true, false,
				[&](cpp_redis::reply& reply) {
					nickPass = !reply.is_null();
				});

			_redisClient.sync_commit();
		}

		if (!nickPass)
		{
			status = dfTETRIS_REGISTER_ERR_DUPLICATED;
			// 실패 패킷 전송 준비
			(*cPacket)->Clear(sizeof(st_NetHeader));

			mpDupcheckRES(cPacket, status);
			SendPacket_UniCast(sessionID, cPacket);
			return;
		}

		// 중복체크 성공
		status = true;
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpDupcheckRES(cPacket, status);
		SendPacket_UniCast(sessionID, cPacket);
		return;
	}
}


void TetrisLoginServer::MessageProc_Login(RefCountPointer& cPacket, ULONGLONG sessionID)
{
	WCHAR gameServerIP[16];
	WCHAR chatServerIP[16];
	WCHAR clientAddr[16];

	BYTE status = 1;
	INT64 AccountNum = 0;

	char ID[20];
	(*cPacket)->GetData(ID, sizeof(ID));

	char Passwd[20];
	(*cPacket)->GetData(Passwd, sizeof(Passwd));

	// @@TODO: DB에 전송할 때 여기에 넣기
	SHS::DBTLSConnector* pDBConnector = SHS::DBTLSConnector::GetDBConnectorTLS();

	LPVOID pAddr = pDBConnector->AllocJobAddress();
	CDBLogin* pCDBLogin = new(pAddr)CDBLogin;
	strcpy_s(pCDBLogin->_ID, 20, ID);

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

		mpLoginRES(cPacket, AccountNum, status, NULL, NULL, NULL);
		SendPacket_UniCast(sessionID, cPacket);
		return;
	}

	// 얻어온 결과 꺼내서 Passwd 비교
	std::wstring wsessionKey = GenerateSessionKey();

	std::string sessionKey = WstrToStr(wsessionKey);

	AccountNum = pDBConnector->GetInt64(_enAccountNum);
	std::string passwd = pDBConnector->GetString(_enPasswd);
	std::string nickname(pDBConnector->GetString(_enNickname));

	pDBConnector->FreeQueryResult();
	_pLog._dwDBSelectTPS++;

	if (passwd.compare(Passwd) != 0)
	{
		status = dfTETRIS_LOGIN_ERR_PASSWD;
		// 실패 패킷 전송 준비
		(*cPacket)->Clear(sizeof(st_NetHeader));

		mpLoginRES(cPacket, AccountNum, status, NULL, NULL, NULL);
		SendPacket_UniCast(sessionID, cPacket);
		return;
	}

	// Redis에 넣기.
	cpp_redis::client& _redisClient = GetTLSRedisClient();
	_redisClient.hset(std::to_string(AccountNum), "SessionKey", sessionKey);
	_redisClient.hset(std::to_string(AccountNum), "Nickname", nickname);

	_redisClient.expire(std::to_string(AccountNum), 60);
	_redisClient.sync_commit();

	// 패킷 전송 준비
	(*cPacket)->Clear(sizeof(st_NetHeader));

	// IP 주소 관련 수정 필요
	wcsncpy_s(gameServerIP, _countof(gameServerIP), dfGAMESERVER_IP, sizeof(WCHAR) * 16);
	wcsncpy_s(chatServerIP, _countof(chatServerIP), dfCHATSERVER_PUBLICIP, sizeof(WCHAR) * 16);

	status = dfTETRIS_LOGIN_OK;

	mpLoginRES(cPacket, AccountNum, status, gameServerIP, (USHORT)dfGAMESERVER_PORT,
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

void TetrisLoginServer::OnError(int errorcode, WCHAR* message)
{

}
