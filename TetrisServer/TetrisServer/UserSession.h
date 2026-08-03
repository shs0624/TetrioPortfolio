#pragma once

enum en_SERVER
{
	None,
	en_SERVER_CHAT,
	en_SERVER_MATCHING,
	en_SERVER_GAME
};

// 게임에서 쓰는 블록
struct st_Stacker
{
	BYTE type;
};

// 유저별 게임 상황 구조체
struct st_GameInfo
{
	// [세로][가로]
	BYTE _GameBoard[20][10];
	// 홀딩한 블록 정보 없으면 -1
	BYTE _HoldingBlock;
	// 테트로미노 예고
	BYTE _NextBlockArr[5];
	// 공격받아 쌓인 라인
	BYTE _GarbageLine;

	// 현재 낙하중인 블록 정보 -> 추가 예정
};

// 게임이 진행되는 세션 (방)
struct st_GAMESESSION
{
	SRWLOCK _GameSessionLock;
	bool _bUsing;

	// 몇 번 인덱스인지는 유저가 들고있고, 자기 정보는 idx, 상대 정보는 1-idx	
	st_GameInfo _GameInfoArr[2];
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
	char SessionKey[64];

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