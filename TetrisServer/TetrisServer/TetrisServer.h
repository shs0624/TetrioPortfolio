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
			StartNetServer(ip, port, bNagleEnabled, maxConnection, _ProgramKey, _FixedKey);

			InitializeSRWLock(&_UserMapLock);
			InitializeSRWLock(&_SessionMapLock);

			_UserPool = new procademy::CMemoryPool_LockFree<st_USER>(maxConnection, false, false);
			_SessionPool = new procademy::CMemoryPool_LockFree<st_SESSION>(maxConnection, false, false);

			_hQuitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
			
			pMatchManager = new MatchingManager();
			pMatchManager->InitMatchingManager(this, &TetrisServer::OnMatchFound);

			for (int i = 0; i < 10; i++)
			{
				_GameTickThreadHandleArr[i] = 
					(HANDLE)_beginthreadex(NULL, 0, GameTickThread, this, 0, &_GameTickThreadIDArr[i]);
			}
		}
		catch (const std::exception& e)
		{
			std::cerr << "[TetrisServer] 초기화 실패: " << e.what() << std::endl;
			throw;
		}

	}

	void QuitServer() override
	{
		// 세션 전체 삭제.. 그런작업
		CNetServer::QuitServer();
	}

	void MessageProc_Login(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_ChatMessage(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_MatchingReq(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_GameReadyReq(ULONGLONG sessionID, RefCountPointer& cPacket);
	void MessageProc_GameInput(ULONGLONG sessionID, RefCountPointer& cPacket);

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
	void mpRESMatchingSuccess(RefCountPointer& cPacket, INT64 accountNum, INT64 opAccountNum, WCHAR* opNickname);
	void mpRESGameReady(RefCountPointer& cPacket, BYTE status);
	void mpACKCountDown(RefCountPointer& cPacket, WORD count);

	void mpACKBoardUpdate(RefCountPointer& cPacket, enTetBlock holdingBlock, enTetBlock* NextBlockBag, BYTE* pMyBoard, BYTE* pOpBoard, BYTE clearCount = 0, BYTE clearY = 0);
	void mpACKBlockUpdate(RefCountPointer& cPacket, BYTE blockType, BYTE rotate, signed char x, signed char y);
	void mpACKDamage(RefCountPointer& cPacket, BYTE damageCount);

	// 함수 포인터에 전달하기 위해 static
	static void OnMatchFound(LPVOID context, st_USER* pUser1, st_USER* pUser2);
	bool SetGameSession(st_USER* pUser1, st_USER* pUser2);
	void GameUpdate(st_GAMESESSION* pGameSession);
	void UpdatePlay(st_GAMESESSION* pGameSession);
	void UpdateBoard(st_GAMESESSION* pGameSession, int sessionIndex);
	void CreateBlock(st_GAMESESSION* pGameSession, int sessionIndex);
	void GetNextBlockArr(st_GameInfo* pGameInfo, enTetBlock* pBagArr);
	enTetBlock GetNextBlockType(st_GameInfo* pGameInfo, enTetBlock* pBagArr);
	void GenerateBag(enTetBlock* pBag);
	void Attack(st_GAMESESSION* pGameSession, int sessionIndex, DWORD clearBit);
	void Damage(st_GAMESESSION* pGameSession, int sessionIndex);

	void MoveLeft(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket);
	void MoveRight(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket);
	void SoftDrop(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket);
	void HardDrop(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket);
	int GetHardDropY(st_GameInfo* pGameInfo);
	void Rotate(st_GAMESESSION* pGameSession, int sessionIndex, bool clockwise, RefCountPointer& cPacket);
	void Hold(st_GAMESESSION* pGameSession, int sessionIndex, RefCountPointer& cPacket);

	const st_KickOffset* GetKicks(enTetBlock block, int fromRotate, bool clockwise);

	DWORD LineClear(st_GameInfo* pGameInfo, int dropY);
	bool CollisionCheck(st_GameInfo* pGameInfo, enTetBlock block, int dropRotate, int dropX, int dropY);

	void StartCountDown(st_GAMESESSION* pGameSession);
	void CheckCountDown(st_GAMESESSION* pGameSession);
	
	void EnterChat(st_USER* userPtr);
	void LeaveChat(ULONGLONG sessionID);

	// 게임 틱 스레드 관리 -> 동시 게임 5000명 -> 스레드당 500개 관리
	HANDLE _GameTickThreadHandleArr[10];
	unsigned int _GameTickThreadIDArr[10];
	
	// 스레드 당 최대 관리 세션 수, 틱 프레임 타임
	const int _MaxGameSessionPerThread = 500;
	const DWORD _dwFrameTime = 33;
	const WORD _wCountDown = 4;

	LONG _GameSessionThreadCount = 0;
	LONG _GameSessionActiveCountArr[10] = { 0 };
	st_GAMESESSION _GameSessionArr[10][500];
	static unsigned int WINAPI GameTickThread(LPVOID arg);
	bool inline SleepCheck();

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

	// 종료 체크용 핸들
	HANDLE _hQuitEvent;

	const int _iMaxX = 10;
	const int _iMaxY = 20;
	const int _iBagMaxSize = 14;

	const char _FixedKey = 0x32;
	const char _ProgramKey = 0x77;
};