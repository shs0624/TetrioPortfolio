#pragma once
#include "Includes.h"
#include "NetServer.h"
#include "LogManager.h"

DWORD _threadID = 0;
DWORD _logID = 0;

bool _bServerEnabled = true;

// 빌드에러 방지를 위한 정의
thread_local stChatLog CNetServer::_pLog;

// thread-safe 락프리 스택
int CNetServer::FindUsableSessionIndex()
{
	ULONGLONG idx = -1;
	if (_emptyIndexStack->pop(&idx))
		return idx;
	else
		return -1;
}

void CNetServer::FindSession(ULONGLONG sessionID, st_NetSession** pSession)
{
	ULONGLONG idx = sessionID >> 48;
	if (sessionID == _sessionArr[idx].ulSessionID)
	{
		*pSession = &_sessionArr[idx];
	}
	else
	{
		*pSession = NULL;
	}

	return;
}

bool CNetServer::StartNetServer(ULONG ip, LONG port, int workerCount, int concurrentThreads, bool bNagleEnabled, int maxConnection)
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	// socket();
	_ListenSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (_ListenSocket == INVALID_SOCKET)
		err_quit("socket()");

	LINGER lingerOpt = { 0,1 };
	int lingerRet = setsockopt(_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(LINGER));
	if (lingerRet == SOCKET_ERROR)
		err_quit("Linger()");

	int optval = 0;
	retval = setsockopt(_ListenSocket, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
		err_quit("SO_SNDBUF()");

	// bind()
	SOCKADDR_IN serveraddr;
	ZeroMemory(&serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	serveraddr.sin_port = htons(SERVERPORT);
	retval = ::bind(_ListenSocket, (SOCKADDR*)&serveraddr, sizeof(serveraddr));
	if (retval == SOCKET_ERROR)
		err_quit("bind()");

	// listen()
	retval = listen(_ListenSocket, SOMAXCONN_HINT(65535));
	if (retval == SOCKET_ERROR)
		err_quit("listen()");

	if (!Init(maxConnection))
		return false;

	printf("\n[TCP 서버] 시작\n");
}

unsigned int WINAPI CNetServer::AcceptThread(LPVOID arg)
{
	// static 선언해서 함수 호출을 위한 포인터
	CNetServer* thisPtr = (CNetServer*)arg;

	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;

	// Accept스레드 로그 등록
	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		if (!_bServerEnabled)
		{
			break;
		}

		//accept()
		if (!(thisPtr->AcceptProc(thisPtr)))
		{
			continue;
		}
	}

	return 0;
}

bool CNetServer::AcceptProc(CNetServer* thisPtr)
{
	// 데이터 통신에 사용할 변수
	SOCKET client_sock;
	SOCKADDR_IN clientaddr;
	char ipbuffer[50];

	//accept()
	int addrlen = sizeof(clientaddr);
	client_sock = accept(_ListenSocket, (SOCKADDR*)&clientaddr, &addrlen);
	if (client_sock == INVALID_SOCKET)
	{
		err_display("accept()");
		return false;
	}
	_pLog._dwAcceptTPS++;
	_pLog._dwAcceptTotal++;

	ULONGLONG idx;
	// 비동기 입출력 시작
	{
		int sessionCount = InterlockedIncrement((LONG*)&_iSessionCount);
		if (sessionCount > _imaxConnection)
		{
			// 연결끊기 후 sessionCount 롤백
			closesocket(client_sock);
			InterlockedDecrement((LONG*)&_iSessionCount);
			_pLog._lDisconnectMaxSession++;
			return false;
		}

		idx = FindUsableSessionIndex();
		if (idx == -1)
		{
			DebugBreak();
			return false;
		}
	}

	st_NetSession* ptr = &_sessionArr[idx];

	// 다른 곳에서 Send후 Dec로 해제되는 걸 막기위해 먼저 Inc
	ptr->stIORefCount.ulIOCount = 0;
	if (InterlockedIncrement(&ptr->stIORefCount.ulIOCount) != 1)
		DebugBreak();

	while (ptr->sendBuf->Size() > 0)
	{
		RefCountPointer cPacket;
		ptr->sendBuf->Dequeue(cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}
	ptr->recvBuf->ClearBuffer();

	// 수정이 필요함
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(ptr->sendOverlapped), sizeof(OVERLAPPED));
	ULONGLONG id = (_threadID++) & 0x0000ffffffffffff;
	ULONGLONG ulIdx = (idx << 48);
	ptr->dwSendCount = 0;
	ptr->clientAddr = clientaddr;
	ptr->ulSessionID = (ulIdx | id);
	ptr->stIORefCount.ulReleaseCheck = 0;
	ptr->bSendFlag = false;
	ptr->bCanceled = false;
	ptr->bUseFlag = true;
	ptr->sock = client_sock;

	InterlockedIncrement((LONG*)&_iAcceptTPS);
	if (!OnAccept(ptr->ulSessionID, clientaddr))
		return false;

	// 소켓을 IOCP에 등록
	CreateIoCompletionPort((HANDLE)client_sock, _NetIOCPHandle, (ULONG_PTR)ptr, 0);

	if (!SetWSARecv(ptr))
	{
		if (InterlockedDecrement((DWORD*)&(ptr->stIORefCount.ulIOCount)) == 0)
		{
			// 연결 끊기
			thisPtr->ReleaseSession(ptr->ulSessionID);
			return false;
		}
	}

	if (InterlockedDecrement((DWORD*)&(ptr->stIORefCount.ulIOCount)) == 0)
	{
		thisPtr->ReleaseSession(ptr->ulSessionID);
		return false;
	}

	return true;
}

unsigned int WINAPI CNetServer::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CNetServer* thisPtr = (CNetServer*)arg;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	while (1)
	{
		DWORD cbTransferred = 0;
		st_NetSession* ptr = NULL;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(thisPtr->_NetIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		// Release 작업 진행
		if (pOverlapped == &(thisPtr->_ReleaseOverlapped))
		{
			thisPtr->ReleaseSession(ptr->ulSessionID);
			continue;
		}

		if (ptr->stIORefCount.ulReleaseCheck == TRUE)
		{
			continue;
		}

		if (ptr->bCanceled)
		{
			if (!thisPtr->DecrementIOCount(ptr))
				continue;

			continue;
		}

		if (retval == 0 || cbTransferred == 0)
		{
			thisPtr->DecrementIOCount(ptr);

			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			// RecvProc_Net은 IOCount를 증가시키지 않는다.
			thisPtr->RecvProc_Net(ptr, cbTransferred);

			// SetWSARecv는 증가시키니까 실패를 반환하면 Decrease
			if (!thisPtr->SetWSARecv(ptr))
			{
				if (!thisPtr->DecrementIOCount(ptr))
					continue;
			}
		}
		else
		{
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				if (!ptr->cPacketArr[i].DecRefCount())
					_pLog._dwPacketPoolUse--;
			}
			ptr->dwSendCount = 0;

			int size = ptr->sendBuf->Size();
			if (size > 0)
			{
				if (!thisPtr->SetWSASend(ptr))
				{
					InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
					if (!thisPtr->DecrementIOCount(ptr))
						continue;
				}
			}
			else
			{
				if (ptr->sendBuf->Empty())
				{
					if (InterlockedExchange((DWORD*)&(ptr->bSendFlag), FALSE) == TRUE)
					{
						if (ptr->sendBuf->Size() > 0)
						{
							if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
							{
								if (!thisPtr->SetWSASend(ptr))
								{
									InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

									if (!thisPtr->DecrementIOCount(ptr))
										continue;
								}
							}
						}
					}
				}
				else
				{
					if (!thisPtr->SetWSASend(ptr))
					{
						InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

						if (!thisPtr->DecrementIOCount(ptr))
							continue;
					}
				}
			}
		}

		// 완료 통지에 대한 IO차감
		if (!thisPtr->DecrementIOCount(ptr))
			continue;
	}
}

unsigned int WINAPI CNetServer::TimerThread(LPVOID arg)
{
	const DWORD _sSleepTime = 1000;

	CNetServer* thisPtr = (CNetServer*)arg;

	LogController::GetInstance()->RegisterLogStruct(&_pLog);

	HANDLE hHandleArr[2] = { thisPtr->_hQuitEvent, thisPtr->_hTimeoutEvent };

	DWORD ret = 0;
	while (1)
	{
		thisPtr->TimeCheck(_sSleepTime);

		ret = WaitForMultipleObjects(2, hHandleArr, FALSE, _sSleepTime);
		if (ret == WAIT_OBJECT_0)
		{
			return 0;
		}
	}
}

void CNetServer::TimeCheck(const DWORD sleepTime)
{
	for (int i = 0; i < _imaxConnection; i++)
	{
		st_NetSession* pSession = _sessionArr + i;

		if (!pSession->bUseFlag)
			continue;

		DWORD timeDiff = timeGetTime() - pSession->dwLastRecvTime;
		if (timeDiff >= dfTIMEOUT_SESSION)
		{
			Disconnect(pSession->ulSessionID);
			_pLog._dwTimeoutSessionTotal++;
			continue;
		}
	}
}

void CNetServer::InitializeSessions(ULONG maxConnection)
{
	_emptyIndexStack = new LockFreeStack<ULONGLONG>();

	_sessionArr = (st_NetSession*)malloc(sizeof(st_NetSession) * maxConnection);
	_iSessionCount = 0;

	for (ULONGLONG i = 0; i < maxConnection; i++)
	{
		_sessionArr[i].stIORefCount.ulReleaseCheck = 0;
		_sessionArr[i].sendBuf = new LockFreeQueue<RefCountPointer>();
		_sessionArr[i].recvBuf = new CRingBuffer(3000);

		_emptyIndexStack->push(i);
	}
}

bool CNetServer::Init(int maxConnection)
{
	_imaxConnection = maxConnection;
	InitializeSessions(maxConnection);

	//CPU 개수 확인
	SYSTEM_INFO si;
	GetSystemInfo(&si);

	_hQuitEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	_hTimeoutEvent = CreateEvent(NULL, FALSE, TRUE, NULL);

	int concurrentThread = si.dwNumberOfProcessors - 2;
	if (concurrentThread <= 0)
		concurrentThread = si.dwNumberOfProcessors - 1;

	_NetIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, concurrentThread);
	if (_NetIOCPHandle == NULL) return false;

	_acceptThreadHandle = (HANDLE)_beginthreadex(NULL, 0, AcceptThread, this, 0, &_acceptThreadID);
	if (_acceptThreadHandle == NULL)
		return false;

	_TimerThreadHandle = (HANDLE)_beginthreadex(NULL, 0, TimerThread, this, 0, &_TimerThreadID);

	//IOCP_THREADCOUNT
	for (int i = 0; i < IOCP_THREADCOUNT; i++)
	{
		_NetIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, &_NetIOCPWorkerThreadID[i]);
		if (_NetIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}
}

bool CNetServer::RecvProc_Net(st_NetSession* ptr, DWORD cbTransferred)
{
	st_NetHeader netHeader;
	ptr->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 ChatServer에 전달
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(sizeof(st_NetHeader));
		_pLog._dwPacketPoolUse++;

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			// 수신 버퍼가 가득한지 체크
			int leftSize = ptr->recvBuf->GetFreeSize();
			if (leftSize < sizeof(st_NetHeader))
			{
				Disconnect(ptr->ulSessionID);
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;

				return false;
			}

			int useSize = ptr->recvBuf->GetUseSize();
			if (useSize < sizeof(st_NetHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			int peekRet = ptr->recvBuf->Peek((char*)(*csPacket)->GetBufferPtr(), sizeof(st_NetHeader));
			if (peekRet != sizeof(st_NetHeader))
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			len = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			//if (len < 0 || len > PROTOCOL_MAX_SIZE) {
			if (len < 0) {
				Disconnect(ptr->ulSessionID);
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;

				return false;
			}

			RK = ((st_NetHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_NetHeader) + len)
			{
				if (!csPacket.DecRefCount())
					_pLog._dwPacketPoolUse--;
				break;
			}

			ptr->recvBuf->MoveFront(sizeof(st_NetHeader));
			ptr->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);

			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(FIXED_KEY, RK))
		{
			Disconnect(ptr->ulSessionID);
			if (!csPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;

			return false;
		}

		// netHeader만큼 이동시키고, OnRecv
		OnRecv(ptr->ulSessionID, csPacket);
		ptr->dwLastRecvTime = timeGetTime();
		_pLog._dwRecvMessageTPS++;
	}

	return true;
}

bool CNetServer::DecrementIOCount(st_NetSession* ptr)
{
	LONG result = InterlockedDecrement((LONG*)&(ptr->stIORefCount.ulIOCount));
	if (result == 0)
	{
		PostRelease(ptr);
		return false;
	}

	return true;
}

bool CNetServer::Disconnect(ULONGLONG sessionID)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
		return false;

	InterlockedIncrement(&ptr->stIORefCount.ulIOCount);
	if (sessionID != ptr->ulSessionID)
	{
		DecrementIOCount(ptr);
		return false;
	}

	if (ptr->bCanceled)
	{
		DecrementIOCount(ptr);
		return false;
	}

	ptr->bCanceled = true;

	CancelIoEx((HANDLE)ptr->sock, NULL);

	DecrementIOCount(ptr);

	return true;
}

bool CNetServer::GetClientAddr(ULONGLONG sessionID, WCHAR* buffer, int len)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
	{
		return false;
	}

	InterlockedIncrement(&ptr->stIORefCount.ulIOCount);
	if (ptr->stIORefCount.ulReleaseCheck == 1)
	{
		DecrementIOCount(ptr);
		return false;
	}

	if (sessionID != ptr->ulSessionID)
	{
		DecrementIOCount(ptr);
		return false;
	}

	if (InetNtop(AF_INET, &ptr->clientAddr.sin_addr, buffer, len)) {
		DecrementIOCount(ptr);
		return true;
	}

	DecrementIOCount(ptr);
	return false;

}

void CNetServer::PostRelease(st_NetSession* ptr)
{
	// 일부러 -1이 되게 Post
	//InterlockedIncrement(&ptr->dwIOCount);
	PostQueuedCompletionStatus(_NetIOCPHandle, 0, (ULONG_PTR)ptr, &_ReleaseOverlapped);
}

bool CNetServer::SendPacket_UniCast(ULONGLONG sessionID, RefCountPointer& cPacket, bool pushHeader)
{
	st_NetSession* ptr;
	FindSession(sessionID, &ptr);
	if (ptr == NULL)
	{
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	InterlockedIncrement(&ptr->stIORefCount.ulIOCount);
	if (ptr->stIORefCount.ulReleaseCheck == 1)
	{
		DecrementIOCount(ptr);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	if (sessionID != ptr->ulSessionID)
	{
		DecrementIOCount(ptr);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	if (ptr->bCanceled)
	{
		DecrementIOCount(ptr);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;

		return false;
	}

	if (pushHeader)
	{
		short shSize = (*cPacket)->GetDataSize();

		st_NetHeader netHeader;
		netHeader.FixedKey = PROGRAM_KEY;
		netHeader.RandKey = (unsigned char)rand() % 256;
		netHeader.shLen = shSize;

		(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
		(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);
	}

	//cPacket.IncRefCount();
	ptr->sendBuf->Enqueue(cPacket);

	if (InterlockedExchange((LONG*)&(ptr->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend(ptr))
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			if (ptr->bCanceled)
				CancelIoEx((HANDLE)ptr->sock, NULL);

			DecrementIOCount(ptr);
			DecrementIOCount(ptr);
			return false;
		}

		if (ptr->bCanceled)
		{
			InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
			CancelIoEx((HANDLE)ptr->sock, NULL);

			DecrementIOCount(ptr);
			if (!cPacket.DecRefCount())
				_pLog._dwPacketPoolUse--;

			return false;
		}
	}

	_pLog._dwSendMessageTPS++;
	DecrementIOCount(ptr);
	return true;
}

bool CNetServer::SendPacket_MultiCast(ULONGLONG* sessionIDArr, WORD count, RefCountPointer& cPacket)
{
	// 메세지를 먼저 생성, 인코딩하기
	st_NetHeader netHeader;
	netHeader.FixedKey = PROGRAM_KEY;
	netHeader.RandKey = (unsigned char)rand() % 256;
	netHeader.shLen = (*cPacket)->GetDataSize();

	(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_NetHeader));
	(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);

	// 그 후, 여러 세션에 하나의 메세지를 전송
	for (int i = 0; i < count; i++)
	{
		cPacket.IncRefCount();
		SendPacket_UniCast(sessionIDArr[i], cPacket, false);
	}

	// 자신 포함해서 다 보냈으니 1을 줄여야 짝이 맞는다.
	if (!cPacket.DecRefCount())
		_pLog._dwPacketPoolUse--;
	return true;
}

bool CNetServer::SetWSARecv(st_NetSession* ptr)
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;

	InterlockedIncrement((DWORD*)&(ptr->stIORefCount.ulIOCount));
	WSABUF recvWsa[MAX_PACKET_BATCH];
	ZeroMemory(&(ptr->recvOverlapped), sizeof(OVERLAPPED));

	if (ptr->bCanceled)
		return false;

	if (ptr->recvBuf->DirectEnqueueSize() < ptr->recvBuf->GetFreeSize())
	{
		// 두개로 나눠 받아야 함
		recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
		recvWsa[0].len = ptr->recvBuf->DirectEnqueueSize();

		recvWsa[1].buf = ptr->recvBuf->GetArrPtr();
		recvWsa[1].len = ptr->recvBuf->GetFreeSize() - ptr->recvBuf->DirectEnqueueSize();

		recvRet = WSARecv(ptr->sock, recvWsa, 2, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
	}
	else
	{
		recvWsa[0].buf = ptr->recvBuf->GetRearBufferPtr();
		recvWsa[0].len = ptr->recvBuf->GetFreeSize();

		recvRet = WSARecv(ptr->sock, &recvWsa[0], 1, &recvbytes, &flags, &(ptr->recvOverlapped), NULL);
	}

	if (recvRet == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			return false;
		}
	}

	if (ptr->bCanceled)
	{
		CancelIoEx((HANDLE)ptr->sock, NULL);;
		return false;
	}

	return true;
}

bool CNetServer::SetWSASend(st_NetSession* ptr)
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(ptr->stIORefCount.ulIOCount));
	WSABUF sendWsa[MAX_PACKET_BATCH];

	if (ptr->bCanceled)
		return false;

	RefCountPointer cpacket;
	int loopCnt = ptr->sendBuf->Size();
	if (loopCnt >= MAX_PACKET_BATCH)
		DebugBreak();

	for (int i = 0; i < loopCnt; i++)
	{
		(ptr->sendBuf->Dequeue(cpacket));
		ptr->cPacketArr[i] = cpacket;

		sendWsa[i].buf = (*cpacket)->GetBufferPtr();
		sendWsa[i].len = (*cpacket)->GetDataSize();
		sendCount++;
	}

	if (sendCount == 0)
	{
		return false;
	}

	ptr->dwSendCount = sendCount;
	retval = WSASend(ptr->sock, sendWsa, sendCount, &sendbytes,
		0, &(ptr->sendOverlapped), NULL);

	if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			// 전송이 실패했으니, 여기서 다시 제거
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				if (!ptr->cPacketArr[i].DecRefCount())
					_pLog._dwPacketPoolUse--;
			}
			ptr->dwSendCount = 0;
			return false;
		}
	}

	return true;
}

void CNetServer::ReleaseSession(ULONGLONG ulSessionID)
{
	st_NetSession* ptr;
	FindSession(ulSessionID, &ptr);
	if (ptr == NULL)
		return;

	// dwIOCount가 0이면서 Release가 False(0)이면 Release를 1로 변경
	st_IORefCheck Target = { 0, 0 };
	st_IORefCheck Fix = { 0, 1 };

	long long result = _InterlockedCompareExchange64((long long*)&ptr->stIORefCount,
		(*(long long*)&Fix), (*(long long*)&Target));
	st_IORefCheck stResult = *((st_IORefCheck*)&result);
	if (stResult.ulIOCount != Target.ulIOCount || stResult.ulReleaseCheck != Target.ulReleaseCheck)
		return;

	ULONGLONG idx = (ulSessionID) >> 48;
	OnRelease(ptr->ulSessionID);

	ptr->recvBuf->ClearBuffer();
	while (ptr->sendBuf->Size() > 0)
	{
		RefCountPointer cPacket;
		ptr->sendBuf->Dequeue(cPacket);
		if (!cPacket.DecRefCount())
			_pLog._dwPacketPoolUse--;
	}

	int cnt = ptr->dwSendCount;
	for (int i = 0; i < cnt; i++)
	{
		if (!ptr->cPacketArr[i].DecRefCount())
			_pLog._dwPacketPoolUse--;
	}

	ptr->bUseFlag = false;
	ptr->dwSendCount = 0;
	closesocket(ptr->sock);

	_emptyIndexStack->push(idx);

	InterlockedDecrement((LONG*)&_iSessionCount);
}

void CNetServer::QuitServer()
{

}