#include "CSerializationBuffer.h"

//TLSMemoryPoolManager<CPacket> CPacket::_CPacketPool(1000, 5, 10, false, true);
//procademy::CMemoryPool_LockFree<CPacket> CPacket::_CPacketPool(50000, false, true);
procademy::CMemoryPool<CPacket> CPacket::_CPacketPool(50000, false, true);

#ifdef LOG_CPACKET
DWORD CPacket::_iLogFreeIdx;
LPVOID CPacket::_freeLog[CPACKET_LOGSIZE];
DWORD CPacket::_iLogAllocIdx;
LPVOID CPacket::_allocLog[CPACKET_LOGSIZE];
#endif

// 직렬화버퍼 초기화. 동적으로 사용을 원하면 사용. 헤더를 넣었다면 헤더 사이즈도 설정
#ifdef MALLOC_ON_CALL
void CPacket::Initialize(int iBufferSize, int iHeaderSize = 0)
{
	_iBufferSize = iBufferSize;
	_head = iHeaderSize;
	_tail = iHeaderSize;
	_iDataSize = 0;
	_iHeaderSize = iHeaderSize;
	_iBuffer = (char*)malloc(_iBufferSize);
	if (_iBuffer == nullptr)
	{
		DebugBreak();
		return;
	}

	if (_isUsing == TRUE)
		DebugBreak();
	_isUsing = TRUE;
#ifdef LOG_CPACEKT
	int idx = InterlockedIncrement(&_iLogAllocIdx) % CPACKET_LOGSIZE;
	_allocLog[idx] = (LPVOID)this;
#endif
}
#endif