#pragma once
#define CHATSERVERNUM 10
#define LOGINSERVERNUM 11
#define GAMESERVERNUM 12

enum en_WORK_TYPE
{
	en_WORK_PACKET = 0,
	en_WORK_ACCEPT,
	en_WORK_RELEASE
};

enum en_INPUT_TYPE
{
	en_INPUT_None = 0,
	en_INPUT_LEFT,
	en_INPUT_RIGHT,
	en_INPUT_SOFTDROP,
	en_INPUT_HARDDROP,
	en_INPUT_ROTATE_CLOCKWISE,
	en_INPUT_ROTATE_COUNTERCLOCKWISE,
	en_INPUT_HOLD
};

enum en_PACKET_TYPE
{
	////////////////////////////////////////////////////////
	//
	//	Client & Server Protocol
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------------
	//  회원가입 요청
	//
	//	{
	//		WORD	Type
	//
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Passwd[20]			// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRISLOGIN_REQ_REGISTER,

	//------------------------------------------------------------
	//	회원가입 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRISLOGIN_RES_REGISTER,

	//------------------------------------------------------------
	//  중복체크 요청
	//
	//	{
	//		WORD	Type
	//
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_ID,
	en_PACKET_CS_TETRISLOGIN_REQ_DUPCHECK_NICKNAME,

	//------------------------------------------------------------
	//  중복체크 응답 - ID, Nick 공용
	//
	//	{
	//		WORD	Type
	//		
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRISLOGIN_RES_DUPCHECK,

	//------------------------------------------------------------
	//  로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Passwd[20]			// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRISLOGIN_REQ_LOGIN,

	//------------------------------------------------------------
	//	로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNum
	//		WCHAR	GameIP[16];
	//		USHORT	GamePort;
	// 
	//		WCHAR	SessionKey[64]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRISLOGIN_RES_LOGIN,

	//------------------------------------------------------------------------------
	// 게임 서버
	//------------------------------------------------------------------------------
	
	//------------------------------------------------------------
	//  게임서버 로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//		WCHAR	SessionKey[64]				// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_LOGIN,
	
	//------------------------------------------------------------
	//	로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_LOGIN,

	//------------------------------------------------------------
	//  채팅 서버 입장
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//		WCHAR	Nickname[20]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_CHAT_ENTER,

	//------------------------------------------------------------
	//  채팅 서버 퇴장
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//		WCHAR	Nickname[20]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_CHAT_EXIT,

	//------------------------------------------------------------
	//	채팅 서버 메세지 보내기
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//		WORD	MessageLen
	//		WCHAR	Message[Len / 2]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_CHAT_MESSAGE,

	//------------------------------------------------------------
	//	매칭 요청
	//
	//	{
	//		WORD	Type
	// 
	//		INT64	AccountNum
	//		WCHAR	Nickname[20]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_MATCHING,

	//------------------------------------------------------------
	// 매칭 요청 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_MATCHING,

	//------------------------------------------------------------
	//	매칭 취소 요청
	//
	//	{
	//		WORD	Type
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_MATCHING_CANCEL,

	//------------------------------------------------------------
	// 매칭 취소 요청 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_MATCHING_CANCEL,

	//------------------------------------------------------------
	// 매칭 성공 응답
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//		INT64	OpAccountNum
	//		WCHAR	OpNickname[20]	
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_MATCHING_SUCCESS,

	//------------------------------------------------------------
	//  게임 시작 준비 완료
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNum
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_GAME_READY,

	//------------------------------------------------------------
	//  게임 시작 준비 완료 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_GAME_READY,

	//------------------------------------------------------------
	//  카운트 다운 시작 (S->C)
	//
	//	{
	//		WORD	Type
	//
	//		WORD	Count
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_COUNTDOWN,

	//------------------------------------------------------------
	//  게임 보드 업데이트 패킷
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	HoldingBlock;
	//		WORD	NextBlockBag[5];
	//		BYTE	MyBoard[20][10];
	//		BYTE	OpponentBoard[20][10];
	// 
	//		// 지워지는 줄 비트연산 필요
	//		BYTE	ClearLineByte; 
	//		BYTE	ClearY;
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_GAME_BOARDUPDATE,
	//------------------------------------------------------------
	//  게임 드랍 블록 업데이트 패킷
	//
	//	{
	//		WORD		Type
	//
	//		BYTE		IsSelf // 내 블록이면 1(true)
	//		BYTE		BlockType
	//		BYTE		Rotate
	//		signed char	X
	//		signed char	Y
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_GAME_BLOCKUPDATE,

	//------------------------------------------------------------
	//  게임 데미지 알림 패킷
	//
	//	{
	//		WORD	Type
	// 
	//		BYTE	DamageCount
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_GAME_DAMAGE,

	//------------------------------------------------------------
	//  유저 인풋 패킷
	//
	//	{
	//		WORD	Type
	// 
	//		DWORD	InputType
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_ACK_GAME_USERINPUT,

	//------------------------------------------------------------
	//  게임 결과 패킷
	//
	//	{
	//		WORD	Type
	// 
	//		BYTE	GameResultFlag
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_ACK_GAME_RESULT,

	//------------------------------------------------------------
	//  로비 복귀 요청 패킷 (클라 -> 서버)
	//
	//	{
	//		WORD	Type
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_GAME_RETURNCHAT,

	//------------------------------------------------------------
	//  로비 복귀 요청 패킷 응답 (서버 -> 클라)
	//
	//	{
	//		WORD	Type
	// 
	//		BYTE	Status				// 0:실패	1:성공
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_RES_GAME_RETURNCHAT,

	//------------------------------------------------------------
	// 하트비트
	//
	//	{
	//		WORD		Type
	//	}
	//
	//
	// 클라이언트는 이를 30초마다 보내줌.
	// 서버는 40초 이상동안 메시지 수신이 없는 클라이언트를 강제로 끊어줘야 함.
	//------------------------------------------------------------	
	en_PACKET_CS_CHAT_REQ_HEARTBEAT,
};

//#endif

enum en_PACKETTYPE_TETRIS_RES_LOGIN
{
	dfTETRIS_LOGIN_OK = 1,		// 로그인 성공
	dfTETRIS_LOGIN_ERR_NOSERVER = 2,		// 서버이름 오류 (매칭미스)
	dfTETRIS_LOGIN_ERR_ID = 3,				// ID 오류
	dfTETRIS_LOGIN_ERR_PASSWD = 4,			// 패스워드 오류
	dfTETRIS_LOGIN_ERR_SESSIONKEY = 5,		// 로그인 세션키 오류
};

enum en_PACKETTYPE_TETRIS_RES_REGISTER
{
	dfTETRIS_REGISTER_OK = 1,					// 가입 성공
	dfTETRIS_REGISTER_ERR_NOSERVER = 2,		// 서버이름 오류 (매칭미스)
	dfTETRIS_REGISTER_ERR_ID = 3,				// ID 오류
	dfTETRIS_REGISTER_ERR_NICKNAME = 4,			// 닉네임 오류
	dfTETRIS_REGISTER_ERR_DUPLICATED = 5,
};