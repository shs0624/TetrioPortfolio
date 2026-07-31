#pragma once

enum en_SERVER
{
	None,
	en_SERVER_CHAT,
	en_SERVER_MATCHING,
	en_SERVER_GAME
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

	// 속한 서버 구분용
	en_SERVER enServerState;

	// 타임아웃용 시간
	DWORD dwLastRecvTime;
	bool bBatched;

	// 공격 메세지 체크용 카운터
	DWORD dwMessageAlertCount;
	DWORD dwDisconnectAlertCount;
};