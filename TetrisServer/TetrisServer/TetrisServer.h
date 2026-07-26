#pragma once

// 로그인 하지 않은 세션
struct st_SESSION
{
	ULONGLONG ulSessionID;
	SOCKADDR_IN ClientAddr;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
};

// 로그인 한 유저
struct st_USER
{
	ULONGLONG ulSessionID;
	INT64 AccountNum;
	SOCKADDR_IN ClientAddr;

	WCHAR ID[20];
	WCHAR NickName[20];
	char SessionKey[64];

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bBatched;

	// 공격 메세지 체크용 카운터
	DWORD dwMessageAlertCount;
	DWORD dwDisconnectAlertCount;
};

class TetrisServer : CNetServer
{
public:
	TetrisServer()
	{

	}

	void InitTetrisServer(ULONG ip, LONG port, bool bNagleEnabled, int maxConnection)
	{
		try
		{
			StartNetServer(ip, port, bNagleEnabled, maxConnection, _FixedKey, _ProgramKey);
		}
		catch (const std::exception& e)
		{
			std::cerr << "[TetrisServer] 초기화 실패: " << e.what() << std::endl;
			throw;
		}

		InitializeSRWLock(&_UserMapLock);
		InitializeSRWLock(&_SessionMapLock);
	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	void MessageProc_Login(ULONGLONG sessionID, ULONGLONG accountNum, RefCountPointer& cPacket);

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	cpp_redis::client& GetTLSRedisClient();

	void mpRESLogin(RefCountPointer& cPacket, BYTE status);

	procademy::CMemoryPool_LockFree<st_USER>* _UserPool;
	procademy::CMemoryPool_LockFree<st_SESSION>* _SessionPool;

	// AccountNum, 유저 구조체
	unordered_map<ULONGLONG, st_USER*> _AccountNumUserMap;
	SRWLOCK _AccountNumUserMapLock;

	// SessionID, 유저 구조체
	unordered_map<ULONGLONG, st_USER*> _UserMap;
	SRWLOCK _UserMapLock;

	// SessionID, 세션 구조체
	unordered_map<ULONGLONG, st_SESSION*> _SessionMap;
	SRWLOCK _SessionMapLock;

	const char _FixedKey = 0x32;
	const char _ProgramKey = 0x77;
};