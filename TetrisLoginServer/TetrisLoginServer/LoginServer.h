#pragma once
#include "DBWriter.h"
#define dfSECTOR_MAX_Y 50
#define dfSECTOR_MAX_X 50
#define dfSLEEPTIME 1000
#define dfTIMEOUT_SESSION 10000

#define dfGAMESERVER_IP L"127.0.0.1"
#define dfCHATSERVER_PUBLICIP L"106.245.38.102"
#define dfGAMESERVER_PORT 11004

#define dfCHATSERVER_PORT 20204

// 세션
struct st_SESSION
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;
	INT64 AccountNum;

	// 내가 임의로 설정해서 넘길거임.
	WCHAR ID[20];
	WCHAR NickName[20];
	char SessionKey[64];


	// 타임아웃용 시간
	DWORD dwLastRecvTime;
};

class LoginServer : CNetServer
{
public:
	LoginServer()
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
	// time 측정을 위한 Update
	void TimeCheck(DWORD sleepTime);

	cpp_redis::client& GetTLSRedisClient();

	static unsigned int WINAPI TimerThread(LPVOID arg);

	void mpLoginRES(RefCountPointer& cPacket, INT64 accountNum, BYTE status, WCHAR* ID, WCHAR* Nickname, WCHAR* gameIP, USHORT gamePort, WCHAR* chatIP, USHORT chatPort);

	procademy::CMemoryPool_LockFree<st_SESSION>* _SessionPool;

	SHS::DBWriterManager* _DBWriterManager;

	HANDLE _hQuitEvent;
	HANDLE _hTimeoutEvent;
	HANDLE _hMessageQueueEvent;

	HANDLE _TimerThreadHandle;
	unsigned int _TimerThreadID;

	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
	SRWLOCK _SessionMapLock;
};