#pragma comment(lib,"ws2_32")
#include "Includes.h"
#include "LogManager.h"
#include "LanClient.h"
#define LOGCOUNT 10000

// 처음 1회 초기화 함수
bool CLanClient::StartLanClient()
{
	

	int retval;

	// 그냥 IOCP 만들자. concurrent = 2

	// 굳이 쓸 이유 없을듯?
	//LINGER lingerOpt = { 0,1 };
	//int lingerRet = setsockopt(_ListenSocket, SOL_SOCKET, SO_LINGER, (char*)&lingerOpt, sizeof(LINGER));
	//if (lingerRet == SOCKET_ERROR)
	//	err_quit("Linger()");
	return true;
}

bool CLanClient::Init()
{
	ZeroMemory(&_serverAddr, sizeof(_serverAddr));
	_serverAddr.sin_family = AF_INET;
	if (inet_pton(AF_INET, SERVERADDR, &_serverAddr.sin_addr.S_un.S_addr) != 1)
	{
		DebugBreak();
	}
	_serverAddr.sin_port = htons(MONITORSERVERPORT);

	_LanIOCPHandle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 2);
	if (_LanIOCPHandle == NULL) return false;

	//IOCP_THREADCOUNT
	for (int i = 0; i < 2; i++)
	{
		_LanIOCPWorkerThreadHandleArr[i] = (HANDLE)_beginthreadex(NULL, 0, IOCPWorkerThread, this, 0, &_LanIOCPWorkerThreadID[i]);
		if (_LanIOCPWorkerThreadHandleArr[i] == NULL)
			return false;
	}

	return true;
}

unsigned int WINAPI CLanClient::IOCPWorkerThread(LPVOID arg)
{
	char tempBuffer[PROTOCOL_SIZE + 1];
	int retval;
	CLanClient* thisPtr = (CLanClient*)arg;

	while (1)
	{
		DWORD cbTransferred = 0;
		st_Session* ptr = thisPtr->_mySession;
		OVERLAPPED* pOverlapped = NULL;

		retval = GetQueuedCompletionStatus(thisPtr->_LanIOCPHandle, &cbTransferred, (PULONG_PTR)&ptr, (LPOVERLAPPED*)&pOverlapped, INFINITE);

		if (retval == 0 && ptr == NULL && pOverlapped == NULL)
		{
			// 종료
			return 0;
		}

		if (thisPtr->_mySession != ptr)
		{
			continue;
		}

		InterlockedIncrement(&thisPtr->_mySession->dwIOCount);	
		if (retval == 0 || cbTransferred == 0)
		{
			thisPtr->DecrementIOCount();
			thisPtr->DecrementIOCount();

			continue;
		}

		if (pOverlapped == &ptr->recvOverlapped)
		{
			if (!thisPtr->RecvProc_Lan(cbTransferred))
			{
				if (!thisPtr->DecrementIOCount())
					continue;
			}

			if (!thisPtr->SetWSARecv())
			{
				if (!thisPtr->DecrementIOCount())
					continue;
			}
		}
		else
		{
			int cnt = ptr->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				if (ptr->cPacketArr[i].DecRefCount())
					DebugBreak();
			}
			ptr->dwSendCount = 0;

			int size = ptr->sendBuf->Size();
			if (size > 0)
			{
				if (!thisPtr->SetWSASend())
				{
					InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);
					if (!thisPtr->DecrementIOCount())
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
								if (!thisPtr->SetWSASend())
								{
									InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

									if (!thisPtr->DecrementIOCount())
										continue;
								}
							}
						}
					}
				}
				else
				{
					if (!thisPtr->SetWSASend())
					{
						InterlockedExchange((LONG*)&(ptr->bSendFlag), FALSE);

						if (!thisPtr->DecrementIOCount())
							continue;
					}
				}
			}
		}

		// 완료 통지에 대한 IO차감
		if (!thisPtr->DecrementIOCount())
			continue;
		// 여긴 세션 참조에 대한 IO차감
		if (!thisPtr->DecrementIOCount())
			continue;
	}
}

bool CLanClient::DecrementIOCount()
{
	LONG result = InterlockedDecrement((LONG*)(& _mySession->dwIOCount));
	if (result == 0)
	{
		PostRelease();
		return false;
	}

	return true;
}

void CLanClient::PostRelease()
{
	// 일부러 -1이 되게 Post
	//InterlockedIncrement(&ptr->dwIOCount);
	PostQueuedCompletionStatus(_LanIOCPHandle, 0, (ULONG_PTR)_mySession, &_ReleaseOverlapped);
}

// @@TODO : 이거에서 false는 연결 끊김 뿐
// 얘는 빼서 받은 메세지를 상속받은 쪽에 OnRecv로 전달만하자.
bool CLanClient::RecvProc_Lan(DWORD cbTransferred)
{
	st_LanHeader netHeader;
	_mySession->recvBuf->MoveRear(cbTransferred);

	// 받은 데이터 ChatServer에 전달
	while (1)
	{
		RefCountPointer csPacket = RefCountPointer::MakeSharedPtr();
		(*csPacket)->Initialize(sizeof(st_LanHeader));

		short len;
		unsigned char RK;

		// csPacket 초기화 후 ptr->recvBuf에서 Dequeue
		{
			int useSize = _mySession->recvBuf->GetUseSize();
			if (useSize < sizeof(st_LanHeader))
			{
				csPacket.DecRefCount();
				break;
			}

			int peekRet = _mySession->recvBuf->Peek((char*)(*csPacket)->GetBufferPtr(), sizeof(st_LanHeader));
			if (peekRet != sizeof(st_LanHeader))
			{
				csPacket.DecRefCount();
				break;
			}

			len = ((st_LanHeader*)((*csPacket)->GetBufferPtr()))->shLen;
			if (len < 0 || len > PROTOCOL_MAX_SIZE) {
				Disconnect();
				return false;
			}

			RK = ((st_LanHeader*)((*csPacket)->GetBufferPtr()))->RandKey;
			if (useSize < sizeof(st_LanHeader) + len)
			{
				csPacket.DecRefCount();
				break;
			}

			_mySession->recvBuf->MoveFront(sizeof(st_LanHeader));
			_mySession->recvBuf->Dequeue((*csPacket)->GetTailPtr(), len);

			(*csPacket)->MoveWritePos(len);
		}

		// 디코딩, 체크섬 검사
		if (!(*csPacket)->Decode(FIXED_KEY, RK))
		{
			Disconnect();
			return false;
		}

		// netHeader만큼 이동시키고, OnRecv
		OnRecv(csPacket);
	}

	return true;
}

bool CLanClient::Connect()
{
	int retval;

	// 윈속 초기화
	WSADATA wsa;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return 1;

	_mySession = new st_Session;

	// connect전에 초기화
	ZeroMemory(&(_mySession->recvOverlapped), sizeof(OVERLAPPED));
	ZeroMemory(&(_mySession->sendOverlapped), sizeof(OVERLAPPED));
	
	_mySession->bCanceled = false;
	_mySession->bSendFlag = false;
	_mySession->sendBuf = new LockFreeQueue<RefCountPointer>();
	_mySession->recvBuf = new CRingBuffer(15000);
	_mySession->dwSendCount = 0;

	// socket();
	_mySession->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (_mySession->sock == INVALID_SOCKET)
	{
		DebugBreak();
		err_quit("SOCKET()");
		return false;
	}

	int optval = 0;
	retval = setsockopt(_mySession->sock, SOL_SOCKET, SO_SNDBUF, (char*)&optval, sizeof(optval));
	if (retval == SOCKET_ERROR)
	{
		err_quit("SO_SNDBUF()");
		return false;
	}

	// 실패가 아니라, connect 실패 띄워야 함
	int connectRet = connect(_mySession->sock, (SOCKADDR*)&_serverAddr, sizeof(_serverAddr));
	if (connectRet == SOCKET_ERROR)
		return false;

	CreateIoCompletionPort((HANDLE)_mySession->sock, _LanIOCPHandle, (ULONG_PTR)_mySession, 0);

	SetWSARecv();

	OnConnect();

	return true;
}

// 그냥 closesocket?
bool CLanClient::Disconnect()
{
	closesocket(_mySession->sock);

	delete(_mySession);

	return true;
}

// 완성된 패킷이 들어온다는 가정
bool CLanClient::SendPacket_UniCast(RefCountPointer& cPacket, bool pushHeader)
{
	InterlockedIncrement(&_mySession->dwIOCount);

	if (pushHeader)
	{
		short shSize = (*cPacket)->GetDataSize();

		st_LanHeader netHeader;
		netHeader.FixedKey = PROGRAM_KEY;
		netHeader.RandKey = (unsigned char)rand() % 256;
		netHeader.shLen = shSize;

		(*cPacket)->PushHeader((char*)&netHeader, sizeof(st_LanHeader));
		(*cPacket)->Encode(FIXED_KEY, netHeader.RandKey);
	}

	_mySession->sendBuf->Enqueue(cPacket);

	if (InterlockedExchange((LONG*)&(_mySession->bSendFlag), TRUE) != TRUE)
	{
		if (!SetWSASend())
		{
			InterlockedExchange((LONG*)&(_mySession->bSendFlag), FALSE);

			DecrementIOCount();
			DecrementIOCount();

			return false;
		}
	}

	DecrementIOCount();
	return true;
}

bool CLanClient::SetWSARecv()
{
	// WSARecv
	int recvRet, recvCount = 0;
	DWORD flags = 0, recvbytes = 0;
	WSABUF recvWsa[200];
	st_Session* ptr = _mySession;

	ZeroMemory(&(_mySession->recvOverlapped), sizeof(OVERLAPPED));
	//ZeroMemory(&(_mySession->sendOverlapped), sizeof(OVERLAPPED));

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

	return true;
}

bool CLanClient::SetWSASend()
{
	int retval, sendCount = 0;
	DWORD sendbytes;

	InterlockedIncrement((DWORD*)&(_mySession->dwIOCount));
	WSABUF sendWsa[MAX_PACKET_BATCH];

	if (_mySession->bCanceled)
		return false;

	RefCountPointer cpacket;
	int loopCnt = _mySession->sendBuf->Size();
	if (loopCnt >= MAX_PACKET_BATCH)
		DebugBreak();

	for (int i = 0; i < loopCnt; i++)
	{
		(_mySession->sendBuf->Dequeue(cpacket));
		_mySession->cPacketArr[i] = cpacket;

		sendWsa[i].buf = (*cpacket)->GetBufferPtr();
		sendWsa[i].len = (*cpacket)->GetDataSize();
		sendCount++;
	}

	if (sendCount == 0)
	{
		return false;
	}

	_mySession->dwSendCount = sendCount;
	retval = WSASend(_mySession->sock, sendWsa, sendCount, &sendbytes,
		0, &(_mySession->sendOverlapped), NULL);


	if (retval == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err != WSA_IO_PENDING)
		{
			// 전송이 실패했으니, 여기서 다시 제거
			int cnt = _mySession->dwSendCount;
			for (int i = 0; i < cnt; i++)
			{
				_mySession->cPacketArr[i].DecRefCount();
			}
			_mySession->dwSendCount = 0;
			return false;
		}
	}

	return true;
}