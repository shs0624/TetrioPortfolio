#pragma once

enum en_SERVER
{
	None,
	en_SERVER_CHAT,
	en_SERVER_MATCHING,
	en_SERVER_GAME
};

enum en_GAMESESSION_STATE : LONG
{
	en_GAMESTATE_UNUSED,      // 사용 안하는 상태
	en_GAMESTATE_WAIT_READY,  // 매칭됨 -> 준비완료 패킷 대기중
	en_GAMESTATE_COUNTING,    // 준비 완료 -> 카운트다운 패킷 보냄
	en_GAMESTATE_PLAYING,     // 실제 게임 틱 진행중
};

// 게임에서 쓰는 블록
enum TetBlock 
{
	None = 0,
	IBlock,
	OBlock,
	TBlock,
	SBlock,
	ZBlock,
	JBlock,
	LBlock
};

// 유저별 게임 상황 구조체
struct st_GameInfo
{
	// [세로][가로]
	BYTE _GameBoard[20][10];
	// 공격받아 쌓인 라인
	BYTE _GarbageLine;

	// 테트로미노 예고
	TetBlock _NextBlockArr[5];
	// 홀딩한 블록
	TetBlock _HoldingBlock;
	// 현재 낙하중인 블록 정보
	TetBlock _DropBlock;
};

// 게임이 진행되는 세션 (방)
struct st_GAMESESSION
{
	SRWLOCK _GameSessionLock;
	en_GAMESESSION_STATE _State;	// LONG
	LONG _lReady;					// 둘 다 Ready인지 한번에 체크하기 위한 비트체크 -> idx를 활용해서 그 비트 변경
	LONG startTime;					// 카운트 다운이 끝나는 시간

	// 몇 번 인덱스인지는 유저가 들고있고, 자기 정보는 idx, 상대 정보는 1-idx	
	st_GameInfo _GameInfoArr[2];
	ULONGLONG _SessionIDArr[2];
};

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
	WCHAR SessionKey[64];

	// 게임 관련 정보 -> 방 정보, 인덱스
	st_GAMESESSION* pGameSession;
	BYTE byGameSessionIndex;

	// 속한 서버 구분용
	en_SERVER enServerState;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bBatched;

	// 공격 메세지 체크용 카운터
	DWORD dwMessageAlertCount;
	DWORD dwDisconnectAlertCount;
};