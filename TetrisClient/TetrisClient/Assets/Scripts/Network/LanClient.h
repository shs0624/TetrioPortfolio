#pragma once
#define PROTOCOL_MAX_SIZE 500
#define MAX_PACKET_BATCH 300
#define MONITORSERVERPORT	20221
#define SERVERADDR "127.0.0.1"
#define PROTOCOL_SIZE 10
#define PROTOCOL_NUMSIZE 8
#define FIXED_KEY 0x32
#define PROGRAM_KEY 0x77

#pragma pack(push, 1)
struct st_LanHeader
{
	unsigned char FixedKey;
	short shLen;
	unsigned char RandKey;
	unsigned char CheckSum;
};
#pragma pack(pop)

//#pragma pack(1)
struct st_Session
{
	OVERLAPPED sendOverlapped;
	OVERLAPPED recvOverlapped;
	SOCKET sock;
	LockFreeQueue<RefCountPointer>* sendBuf;
	CRingBuffer* recvBuf;
	RefCountPointer cPacketArr[MAX_PACKET_BATCH];

	DWORD dwSendCount;
	alignas(4) DWORD dwIOCount;
	BOOL bReleaseFlag;
	BOOL bSendFlag;
	BOOL bCanceled;
	BOOL bDeleted;
};
//#pragma pack(pop)

class CLanClient
{
public:
	bool StartLanClient();

	bool Connect();
	bool Disconnect();

	bool SendPacket_UniCast(RefCountPointer& cPacket, bool pushHeader = true);

	virtual bool OnRecv(RefCountPointer& cPacket) = 0;

	//virtual bool OnSend() = 0;

	virtual bool OnConnect() = 0;

	/*virtual bool OnConnectionRequest(ULONG ip, LONG port) = 0;

	virtual bool OnAccept(ULONGLONG sessionID) = 0;

	virtual void OnRelease(ULONGLONG SessionID) = 0;

	virtual void OnError(int errorcode, WCHAR* message) = 0;*/
protected:
	OVERLAPPED _ReleaseOverlapped;

	st_Session* _mySession;

	SOCKADDR_IN _serverAddr;
	HANDLE _LanIOCPHandle;
	HANDLE _LanIOCPWorkerThreadHandleArr[2];
	unsigned int _LanIOCPWorkerThreadID[2];

	// 초기화 함수
	bool Init();

	// IOCP 함수
	static unsigned int WINAPI IOCPWorkerThread(LPVOID arg);

	bool DecrementIOCount();

	void PostRelease();

	// 메세지 처리를 위한 함수
	bool SetWSARecv();
	bool SetWSASend();
	bool RecvProc_Lan(DWORD cbTransferred);
};