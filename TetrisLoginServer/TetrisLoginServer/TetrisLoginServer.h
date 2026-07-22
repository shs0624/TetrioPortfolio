#pragma once
#pragma once
#include "DBWriter.h"
#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50

#define dfGAMESERVER_IP L"127.0.0.1"
#define dfCHATSERVER_PUBLICIP L"106.245.38.102"
#define dfGAMESERVER_PORT 11004

#define dfCHATSERVER_PORT 20204

class TetrisLoginServer : CNetServer
{
public:
	TetrisLoginServer()
	{

	}

	void InitLoginServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection);

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	cpp_redis::client& GetTLSRedisClient();

	std::wstring GenerateSessionKey();

	void MessageProc_Login(RefCountPointer& cPacket, ULONGLONG sessionID);
	void MessageProc_Dupcheck(RefCountPointer& cPacket, WORD type, ULONGLONG sessionID);
	void MessageProc_Register(RefCountPointer& cPacket, ULONGLONG sessionID);

	void mpDupcheckRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status);
	void mpRegisterRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status);
	void mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* gameIP, USHORT gamePort, const WCHAR* sessionKey);

	SHS::DBWriterManager* _DBWriterManager;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;
};