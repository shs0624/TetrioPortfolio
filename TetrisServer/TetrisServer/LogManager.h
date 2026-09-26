#pragma once
//#include "PDHMonitor.h"
#include "Includes.h"
#include "CTextLogger.h"
//#include "CPUUsage.h"
//#include "LanClient.h"
//#include "MonitorProtocol.h"
//#include "MonitorClient.h"
#define dfLOG_MAX 10000
//#define MONITORING_ON

struct stServerLog
{
	LONG _dwRecvMessageTPS;
	LONG _dwSendMessageTPS;

	LONG _dwAcceptTotal;
	LONG _dwAcceptTPS;

	LONG _dwSessionCount;
	LONG _dwChatUserCount;
	LONG _dwGameUserCount;

	LONG _dwPacketPoolUse;
	LONG _dwPlayerPoolUse;

	LONG _dwChatEnterMessageTPS;
	LONG _dwChatEnterMessageTotal;

	LONG _dwChatLeaveMessageTPS;
	LONG _dwChatLeaveMessageTotal;

	LONG _dwLoginMessageTotal;
	LONG _dwMatchingSuccessMessageTotal;

	LONG _dwDuplicatedLoginTotal;
	LONG _dwDecodeDisconnectTotal;
	LONG _dwNotCorrectAccountNumTotal;
	LONG _dwRedisCertificationFailTotal;
	LONG _lDisconnectExcessiveMessageTotal;

	LONG _lDisconnectInvalidAccountNum;
	LONG _lDisconnectLenOverMax;
	LONG _lDisconnectOutOfMoveRange;
	LONG _lDisconnectMaxSession;

	LONG _dwTimeoutSessionTotal;
	LONG _dwTimeoutUserTotal;
};

class LogController
{
public:
	static LogController* GetInstance()
	{
		static LogController logC;
		return &logC;
	}

	void WriteLog(const std::string& message)
	{
		_pCTextLogger->Write(message);
		_pCTextLogger->Close();
	}

	// 외부의 스레드 저장소를
	void RegisterLogStruct(stServerLog* pLog)
	{
		int idx = InterlockedIncrement(&_dwLogArrIdx);
		_LogStructArr[idx] = pLog;
	}

	// 이걸로 로그 구조체를 할당해줌(TLS). 받는 스레드는 이 주소를 저장하고 사용
	stServerLog* AllocLogStruct()
	{
		stServerLog* ptr = (stServerLog*)TlsGetValue(_dwTlsIdx);
		if (ptr == NULL)
		{
			ptr = (stServerLog*)malloc(sizeof(stServerLog));
			int idx = InterlockedIncrement(&_dwLogArrIdx);
			_LogStructArr[idx] = ptr;
		}


		return ptr;
	}

	// 내가 할당한 주소를 쭉 훑으며 내 지역변수를 변경
	void ReadLog()
	{
		memset(&_stPrintLog, 0, sizeof(_stPrintLog));
		for (int i = 1; i <= _dwLogArrIdx; i++)
		{
			_stPrintLog._dwChatUserCount += _LogStructArr[i]->_dwChatUserCount;
			_stPrintLog._dwGameUserCount += _LogStructArr[i]->_dwGameUserCount;
			_stPrintLog._dwSessionCount += _LogStructArr[i]->_dwSessionCount;
			_stPrintLog._dwAcceptTotal += _LogStructArr[i]->_dwAcceptTotal;
			_stPrintLog._dwAcceptTPS += _LogStructArr[i]->_dwAcceptTPS;
			_stPrintLog._dwRecvMessageTPS += _LogStructArr[i]->_dwRecvMessageTPS;
			_stPrintLog._dwSendMessageTPS += _LogStructArr[i]->_dwSendMessageTPS;
			_stPrintLog._dwChatEnterMessageTPS += _LogStructArr[i]->_dwChatEnterMessageTPS;
			_stPrintLog._dwChatLeaveMessageTPS += _LogStructArr[i]->_dwChatLeaveMessageTPS;
			_stPrintLog._dwLoginMessageTotal += _LogStructArr[i]->_dwLoginMessageTotal;
			_stPrintLog._dwChatEnterMessageTotal += _LogStructArr[i]->_dwChatEnterMessageTotal;
			_stPrintLog._dwChatLeaveMessageTotal += _LogStructArr[i]->_dwChatLeaveMessageTotal;
			_stPrintLog._dwMatchingSuccessMessageTotal += _LogStructArr[i]->_dwMatchingSuccessMessageTotal;
			_stPrintLog._dwDuplicatedLoginTotal += _LogStructArr[i]->_dwDuplicatedLoginTotal;
			_stPrintLog._dwDecodeDisconnectTotal += _LogStructArr[i]->_dwDecodeDisconnectTotal;
			_stPrintLog._dwNotCorrectAccountNumTotal += _LogStructArr[i]->_dwNotCorrectAccountNumTotal;
			_stPrintLog._dwRedisCertificationFailTotal += _LogStructArr[i]->_dwRedisCertificationFailTotal;
			_stPrintLog._lDisconnectExcessiveMessageTotal += _LogStructArr[i]->_lDisconnectExcessiveMessageTotal;
			_stPrintLog._lDisconnectInvalidAccountNum += _LogStructArr[i]->_lDisconnectInvalidAccountNum;
			_stPrintLog._lDisconnectLenOverMax += _LogStructArr[i]->_lDisconnectLenOverMax;
			_stPrintLog._lDisconnectOutOfMoveRange += _LogStructArr[i]->_lDisconnectOutOfMoveRange;
			_stPrintLog._lDisconnectMaxSession += _LogStructArr[i]->_lDisconnectMaxSession;
			_stPrintLog._dwTimeoutSessionTotal += _LogStructArr[i]->_dwTimeoutSessionTotal;
			_stPrintLog._dwTimeoutUserTotal += _LogStructArr[i]->_dwTimeoutUserTotal;
			_stPrintLog._dwPacketPoolUse += _LogStructArr[i]->_dwPacketPoolUse;
			_stPrintLog._dwPlayerPoolUse += _LogStructArr[i]->_dwPlayerPoolUse;
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Session Count :", _stPrintLog._dwSessionCount);
		printf("%-25s%5d\n", "Chat User Count :", _stPrintLog._dwChatUserCount);
		printf("%-25s%5d\n", "Game User Count :", _stPrintLog._dwGameUserCount);
		printf("%-25s%5d\n", "Accept  Total :", _stPrintLog._dwAcceptTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Accept TPS : ", _stPrintLog._dwAcceptTPS);
		printf("%-25s%5d\n", "RecvPacket TPS : ", _stPrintLog._dwRecvMessageTPS);
		printf("%-25s%5d\n", "SendPacket TPS : ", _stPrintLog._dwSendMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Contents - Chat Enter  TPS :", _stPrintLog._dwChatEnterMessageTPS);
		printf("%-25s%5d\n", "Contents - Chat Leave  TPS :", _stPrintLog._dwChatLeaveMessageTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Contents - Login Total :", _stPrintLog._dwLoginMessageTotal);
		printf("%-25s%5d\n", "Contents - Chat Enter  Total :", _stPrintLog._dwChatEnterMessageTotal);
		printf("%-25s%5d\n", "Contents - Chat Leave  Total :", _stPrintLog._dwChatLeaveMessageTotal);
		printf("%-25s%5d\n", "Contents - Matching Success Total :", _stPrintLog._dwMatchingSuccessMessageTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Duplicated Login Total :", _stPrintLog._dwDuplicatedLoginTotal);
		printf("%-25s%5d\n", "Decode Disconnect Total :", _stPrintLog._dwDecodeDisconnectTotal);
		printf("%-25s%5d\n", "Not Correct AccountNum Total :", _stPrintLog._dwNotCorrectAccountNumTotal);
		printf("%-25s%5d\n", "Redis Certification Fail Total :", _stPrintLog._dwRedisCertificationFailTotal);
		printf("%-25s%5d\n", "Disconnect MaxSession Total :", _stPrintLog._lDisconnectMaxSession);
		//printf("%-25s%5d\n", "Disconnect Excessive Message Total :", _stPrintLog._lDisconnectExcessiveMessageTotal);
		//printf("%-25s%5d\n", "Disconnect InvalidAccountNum Total :", _stPrintLog._lDisconnectInvalidAccountNum);
		//printf("%-25s%5d\n", "Disconnect LenOverMax Total :", _stPrintLog._lDisconnectLenOverMax);
		//printf("%-25s%5d\n", "Disconnect OutOfMoveRange Total :", _stPrintLog._lDisconnectOutOfMoveRange);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Timeout_Session :", _stPrintLog._dwTimeoutSessionTotal);
		printf("%-25s%5d\n", "Timeout_User   :", _stPrintLog._dwTimeoutUserTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "PacketPool Use :", _stPrintLog._dwPacketPoolUse);
		printf("%-25s%5d\n", "UserPool Use   :", _stPrintLog._dwPlayerPoolUse);
		printf("==============================================================================\n\n\n");
	}

	static LogController _LogController;
private:
	LogController()
	{
		_dwTlsIdx = TlsAlloc();

#ifdef MONITORING_ON
		_pPDHMonitor = new PDHMonitor();
		_pCpuUsage = new CCpuUsage();

		_pMonitorClient = new MonitorClient();
		_pMonitorClient->InitMonitorClient();
#endif

		_pCTextLogger = new CTextLogger("TetrisServerLog.txt");

		_hLogUpdateEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		_htpsThreadHandle = (HANDLE)_beginthreadex(NULL, 0, LogingThread, this, 0, &_tpsThreadID);
		if (_htpsThreadHandle == NULL)
			DebugBreak();
	};

	~LogController() {};

	void ResetTPS()
	{
		for (int i = 1; i <= _dwLogArrIdx; i++)
		{
			_LogStructArr[i]->_dwAcceptTPS = 0;

			_LogStructArr[i]->_dwChatEnterMessageTPS = 0;
			_LogStructArr[i]->_dwChatLeaveMessageTPS = 0;

			_LogStructArr[i]->_dwRecvMessageTPS = 0;
			_LogStructArr[i]->_dwSendMessageTPS = 0;
		}
	}

#ifdef MONITORING_ON
	void SendChatMonitorPacket()
	{
		// 1초마다 갱신
		_pCpuUsage->UpdateCpuTime();
		_pPDHMonitor->QueryUpdate();

		// PDH로 서버 CPU, MEM 얻기
		int timeStamp = (int)time(NULL);
		int cpuUsage = _pCpuUsage->ProcessTotal();//_pPDHMonitor->GetCPUUsage();
		int memoryMB = _pPDHMonitor->GetPrivateMemory() / 1000000;

		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_RUN, true, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_CPU, cpuUsage, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SERVER_MEM, memoryMB, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_UPDATE_TPS, _stPrintLog._dwRecvMessageTPS, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_SESSION, _stPrintLog._dwSessionCount, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PLAYER, _stPrintLog._dwUserCount, timeStamp);
		_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_PACKET_POOL, _stPrintLog._dwPacketPoolUse, timeStamp);
		//_pMonitorClient->SendMonitorData(dfMONITOR_DATA_TYPE_CHAT_UPDATEMSG_POOL, _stPrintLog., timeStamp);
		
	}
#endif

	static unsigned int WINAPI LogingThread(LPVOID arg)
	{
		LogController* thisPtr = (LogController*)arg;

		while (1)
		{
			thisPtr->ReadLog();

#ifdef MONITORING_ON
			// 채팅서버 로그를 보내자.
			thisPtr->SendChatMonitorPacket();
#endif

			thisPtr->ResetTPS();

			thisPtr->PrintLog();

			WaitForSingleObject(thisPtr->_hLogUpdateEvent, 1000);
		}

		return 0;
	}

#ifdef MONITORING_ON
	CCpuUsage* _pCpuUsage;
	PDHMonitor* _pPDHMonitor;
	MonitorClient* _pMonitorClient;
#endif


	CTextLogger* _pCTextLogger;

	stServerLog _stPrintLog;

	// 몇 번 TLS 주소에 구조체가 저장되어 있는가
	DWORD _dwTlsIdx;
	DWORD _dwLogArrIdx;
	stServerLog* _LogStructArr[500];

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	DWORD _iLogCount;
};