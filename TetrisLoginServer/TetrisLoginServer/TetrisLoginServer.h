#pragma once
#pragma once
#include "DBWriter.h"
#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50

class TetrisLoginServer : CNetServer
{
public:
	TetrisLoginServer()
	{

	}

	void InitLoginServer(bool bNagleEnabled, int maxConnection);

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

	void mpDupcheckRES(RefCountPointer& cPacket, BYTE status);
	void mpRegisterRES(RefCountPointer& cPacket, BYTE status);
	void mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* gameIP, USHORT gamePort, const WCHAR* sessionKey);

	SHS::DBWriterManager* _DBWriterManager;

	std::wstring _wGameServerIP;
	int _iGameServerPort;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;
};