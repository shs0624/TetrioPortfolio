//#ifndef __GODDAMNBUG_ONLINE_PROTOCOL__
//#define __GODDAMNBUG_ONLINE_PROTOCOL__
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

enum en_PACKET_TYPE
{
	////////////////////////////////////////////////////////
	//q
	//	Client & Server Protocol
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------------
	//  회원가입 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Passwd[20]			// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_REGISTER,

	//------------------------------------------------------------
	//	회원가입 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_RES_REGISTER,

	//------------------------------------------------------------
	//  중복체크 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Nickname[20]		// null 포함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_DUPCHECK_ID,
	en_PACKET_CS_TETRIS_REQ_DUPCHECK_NICKNAME,

	//------------------------------------------------------------
	//  중복체크 응답 - ID, Nick 공용
	//
	//	{
	//		WORD	Type
	//		
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_RES_DUPCHECK,

	//------------------------------------------------------------
	//  로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WCHAR	ID[20]				// null 포함
	//		WCHAR	Passwd[20]			// null 포함
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
	//		INT64	AccountNo
	//		WCHAR	GameIP[16];
	//		USHORT	GamePort;
	// 
	//		WCHAR	SessionKey[64]
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_RES_LOGIN,

	//------------------------------------------------------------
	// 매칭 신청 요청
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		WORD	SectorX
	//		WORD	SectorY
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_REQ_MATCHING,

	//------------------------------------------------------------
	// 매칭 신청 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status				// 0:실패	1:성공
	//		INT64	AccountNo
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_TETRIS_RES_MATCHING,

	//------------------------------------------------------------
	// 매칭 성공 응답
	//
	//	{
	//		WORD	Type
	//
	//		INT64	AccountNo
	//		INT64	OpAccountNo
	//		WCHAR	OpNickname[20]	
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SC_TETRIS_MATCHING_SUCCESS,


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


	////////////////////////////////////////////////////////
	//
	//   MonitorServer & MoniterTool Protocol / 응답을 받지 않음.
	//
	////////////////////////////////////////////////////////

	//------------------------------------------------------
	// Monitor Server  Protocol
	//------------------------------------------------------
	en_PACKET_SS_MONITOR = 20000,
	//------------------------------------------------------
	// Server -> Monitor Protocol
	//------------------------------------------------------
	//------------------------------------------------------------
	// LoginServer, GameServer , ChatServer  가 모니터링 서버에 로그인 함
	//
	// 
	//	{
	//		WORD	Type
	//
	//		int		ServerNo		//  각 서버마다 고유 번호를 부여하여 사용
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_LOGIN,

	//------------------------------------------------------------
	// 서버가 모니터링서버로 데이터 전송
	// 각 서버는 자신이 모니터링중인 수치를 1초마다 모니터링 서버로 전송.
	//
	// 서버의 다운 및 기타 이유로 모니터링 데이터가 전달되지 못할떄를 대비하여 TimeStamp 를 전달한다.
	// 이는 모니터링 클라이언트에서 계산,비교 사용한다.
	// 
	//	{
	//		WORD	Type
	//
	//		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
	//		int		DataValue				// 해당 데이터 수치.
	//		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
	//										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
	//										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_SS_MONITOR_DATA_UPDATE,


	en_PACKET_CS_MONITOR = 25000,
	//------------------------------------------------------
	// Monitor -> Monitor Tool Protocol  (Client <-> Server 프로토콜)
	//------------------------------------------------------
	//------------------------------------------------------------
	// 모니터링 클라이언트(툴) 이 모니터링 서버로 로그인 요청
	//
	//	{
	//		WORD	Type
	//
	//		char	LoginSessionKey[32]		// 로그인 인증 키. (이는 모니터링 서버에 고정값으로 보유)
	//										// 각 모니터링 툴은 같은 키를 가지고 들어와야 함
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_MONITOR_TOOL_REQ_LOGIN,

	//------------------------------------------------------------
	// 모니터링 클라이언트(툴) 모니터링 서버로 로그인 응답
	//
	//	{
	//		WORD	Type
	//
	//		BYTE	Status					// 로그인 결과 0 / 1 / 2 ... 하단 Define
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_MONITOR_TOOL_RES_LOGIN,

	//------------------------------------------------------------
	// 모니터링 서버가 모니터링 클라이언트(툴) 에게 모니터링 데이터 전송
	// 
	// 통합 모니터링 방식을 사용 중이므로, 모니터링 서버는 모든 모니터링 클라이언트에게
	// 수집되는 모든 데이터를 바로 전송시켜 준다.
	// 
	//
	// 데이터를 절약하기 위해서는 초단위로 모든 데이터를 묶어서 30~40개의 모니터링 데이터를 하나의 패킷으로 만드는게
	// 좋으나  여러가지 생각할 문제가 많으므로 그냥 각각의 모니터링 데이터를 개별적으로 전송처리 한다.
	//
	//	{
	//		WORD	Type
	//		
	//		BYTE	ServerNo				// 서버 No
	//		BYTE	DataType				// 모니터링 데이터 Type 하단 Define 됨.
	//		int		DataValue				// 해당 데이터 수치.
	//		int		TimeStamp				// 해당 데이터를 얻은 시간 TIMESTAMP  (time() 함수)
	//										// 본래 time 함수는 time_t 타입변수이나 64bit 로 낭비스러우니
	//										// int 로 캐스팅하여 전송. 그래서 2038년 까지만 사용가능
	//	}
	//
	//------------------------------------------------------------
	en_PACKET_CS_MONITOR_TOOL_DATA_UPDATE,
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
	dfTETRIS_REGISTER_ERR_NICKNAME= 4,			// 닉네임 오류
	dfTETRIS_REGISTER_ERR_DUPLICATED = 5,
};