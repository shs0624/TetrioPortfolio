#pragma once

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

			pMatchManager = new MatchingManager();
			pMatchManager->InitMatchingManager(this, &TetrisServer::OnMatchFound);
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

	void MessageProc_Login(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_ChatMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_MatchingReq(ULONGLONG sessionID, RefCountPointer& cPacket);

	//virtual bool OnConnectionRequest(ULONG ip, LONG port);
	virtual bool OnAccept(ULONGLONG sessionID, SOCKADDR_IN clientAddr);
	virtual void OnRelease(ULONGLONG sessionID);
	virtual void OnRecv(ULONGLONG sessionID, RefCountPointer& cpacket);
	virtual void OnError(int errorcode, WCHAR* message);
private:
	MatchingManager* pMatchManager;

	cpp_redis::client& GetTLSRedisClient();

	void mpRESLogin(RefCountPointer& cPacket, BYTE status);
	void mpRESChatMessage(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname, WORD messageLen, WCHAR* message);
	void mpACKChatEnter(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname);
	void mpACKChatExit(RefCountPointer& cPacket, INT64 accountNum, WCHAR* nickname);

	void mpRESMatching(RefCountPointer& cPacket, BYTE status);

	// 함수 포인터에 전달하기 위해 static
	static void OnMatchFound(LPVOID context, st_USER* pUser1, st_USER* pUser2);

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

	// 벡터(세션 ID, 인덱스) 와 map으로 동시에 저장 -> 벡터는 swap, pop_back으로 삭제 O(1) 만들기.
	vector<st_USER*> _ChatUserVec;
	unordered_map<ULONGLONG, int> _ChatUserIndexMap;
	SRWLOCK _ChatDataLock;

	const char _FixedKey = 0x32;
	const char _ProgramKey = 0x77;
};