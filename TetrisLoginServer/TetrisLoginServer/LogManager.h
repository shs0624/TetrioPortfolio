#pragma once
#include "Includes.h"
#include "CPUUsage.h"
#define dfLOG_MAX 10000
//#define MONITORING_ON

#ifdef MONITORING_ON
	#include "LanClient.h"
	#include "PDHMonitor.h"
	#include "MonitorProtocol.h"
	#include "MonitorClient.h"
#endif

struct stChatLog
{
	LONG _dwRecvMessageTPS;
	LONG _dwSendMessageTPS;

	LONG _dwAcceptTotal;
	LONG _dwAcceptTPS;
	LONG _dwDBSelectTPS;

	LONG _dwSessionCount;

	LONG _dwPacketPoolUse;
	LONG _dwPlayerPoolUse;

	LONG _dwTimeoutSessionTotal;
	LONG _dwTimeoutUserTotal;
	LONG _lDisconnectInvalidAccountNum;
	LONG _lDisconnectMaxSession;
};

class LogController
{
public:
	static LogController* GetInstance()
	{
		static LogController logC;
		return &logC;
	}

	// 외부의 스레드 저장소를
	void RegisterLogStruct(stChatLog* pLog)
	{
		int idx = InterlockedIncrement(&_dwLogArrIdx);
		_LogStructArr[idx] = pLog;
	}

	// 이걸로 로그 구조체를 할당해줌(TLS). 받는 스레드는 이 주소를 저장하고 사용
	stChatLog* AllocLogStruct()
	{
		stChatLog* ptr = (stChatLog*)TlsGetValue(_dwTlsIdx);
		if (ptr == NULL)
		{
			ptr = (stChatLog*)malloc(sizeof(stChatLog));
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
			_stPrintLog._dwSessionCount += _LogStructArr[i]->_dwSessionCount;
			_stPrintLog._dwAcceptTotal += _LogStructArr[i]->_dwAcceptTotal;
			_stPrintLog._dwAcceptTPS += _LogStructArr[i]->_dwAcceptTPS;
			_stPrintLog._dwDBSelectTPS += _LogStructArr[i]->_dwDBSelectTPS;
			_stPrintLog._dwRecvMessageTPS += _LogStructArr[i]->_dwRecvMessageTPS;
			_stPrintLog._dwSendMessageTPS += _LogStructArr[i]->_dwSendMessageTPS;
			_stPrintLog._dwTimeoutSessionTotal += _LogStructArr[i]->_dwTimeoutSessionTotal;
			_stPrintLog._dwTimeoutUserTotal += _LogStructArr[i]->_dwTimeoutUserTotal;
			_stPrintLog._dwPacketPoolUse += _LogStructArr[i]->_dwPacketPoolUse;
			_stPrintLog._dwPlayerPoolUse += _LogStructArr[i]->_dwPlayerPoolUse;
			_stPrintLog._lDisconnectInvalidAccountNum += _LogStructArr[i]->_lDisconnectInvalidAccountNum;
			_stPrintLog._lDisconnectMaxSession += _LogStructArr[i]->_lDisconnectMaxSession;
		}
	}

	// ReadLog가 선행된 후 내 지역변수 값을 출력
	void PrintLog()
	{
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Session Count :", _stPrintLog._dwSessionCount);
		printf("%-25s%5d\n", "Accept  Total :", _stPrintLog._dwAcceptTotal);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Accept TPS : ", _stPrintLog._dwAcceptTPS);
		printf("%-25s%5d\n", "RecvPacket TPS : ", _stPrintLog._dwRecvMessageTPS);
		printf("%-25s%5d\n", "SendPacket TPS : ", _stPrintLog._dwSendMessageTPS);
		printf("%-25s%5d\n", "DB SELECT TPS : ", _stPrintLog._dwDBSelectTPS);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "Timeout_Session :", _stPrintLog._dwTimeoutSessionTotal);
		printf("%-25s%5d\n", "Timeout_User   :", _stPrintLog._dwTimeoutUserTotal);
		printf("%-25s%5d\n", "Disconnect_InvalidAccountNum   :", _stPrintLog._lDisconnectInvalidAccountNum);
		printf("%-25s%5d\n", "Disconnect_MaxSession   :", _stPrintLog._lDisconnectMaxSession);
		printf("==============================================================================\n");
		printf("%-25s%5d\n", "PacketPool Use :", _stPrintLog._dwPacketPoolUse);
		printf("%-25s%5d\n", "UserPool Use   :", _stPrintLog._dwPlayerPoolUse);
		printf("==============================================================================\n\n\n");
		printf("\n\n\n\n\n\n\n\n\n");
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
			_LogStructArr[i]->_dwDBSelectTPS = 0;
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

	stChatLog _stPrintLog;

	// 몇 번 TLS 주소에 구조체가 저장되어 있는가
	DWORD _dwTlsIdx;
	DWORD _dwLogArrIdx;
	stChatLog* _LogStructArr[500];

	HANDLE _hLogUpdateEvent;
	HANDLE _htpsThreadHandle;
	unsigned int _tpsThreadID;

	DWORD _iLogCount;
};