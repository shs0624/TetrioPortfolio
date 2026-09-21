#pragma once

#ifndef _CPACKET_
#define _CPACKET_
#include <Windows.h>
#include <new>
#include "CFreeList.h"
#include "CFreeList_LockFree.h"
#include "TLSMemoryPool.h"
#define CPACKET_LOGSIZE 10000
#define PROTOCOL_MAX_SIZE 500

//#define MALLOC_ON_CALL
//#define LOG_CPACKET
//#define DEBUG_CPACKET

enum en_PACKET
{
	eBUFFER_DFAULT = 24
};

class CPacket
{
public:
#pragma warning(disable:26495)
	CPacket()
	{
		_iBufferSize = PROTOCOL_MAX_SIZE;
		_head = 0;
		_tail = 0;
		_iDataSize = 0;
		_iHeaderSize = 0;
		_iBuffer = (char*)malloc(_iBufferSize);
		if (_iBuffer == nullptr)
		{
			DebugBreak();
			return;
		}
	}
#pragma warning(default:26495)}
	//CPacket(int iBufferSize);

#ifndef MALLOC_ON_CALL
	virtual ~CPacket()
	{
#ifdef LOG_CPACEKT
		int idx = InterlockedIncrement(&_iLogFreeIdx) % CPACKET_LOGSIZE;
		_freeLog[idx] = (LPVOID)this;
#endif
	}
#endif

#ifdef MALLOC_ON_CALL
	~CPacket()
	{
#ifdef LOG_CPACEKT
		int idx = InterlockedIncrement(&_iLogFreeIdx) % CPACKET_LOGSIZE;
		_freeLog[idx] = (LPVOID)this;
#endif

		free(_iBuffer);
		_iBuffer = nullptr;
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void inline Clear(void)
	{
		_head = 0;
		_tail = 0;
		_iDataSize = 0;
		_iHeaderSize = 0;
	}

	void inline Clear(int iHeaderSize)
	{
		_head = iHeaderSize;
		_tail = iHeaderSize;
		_iDataSize = 0;
		_iHeaderSize = iHeaderSize;
	}


	//////////////////////////////////////////////////////////////////////////
	// 버퍼 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)패킷 버퍼 사이즈 얻기.
	//////////////////////////////////////////////////////////////////////////
	int	GetBufferSize(void) { return _iBufferSize; }
	//////////////////////////////////////////////////////////////////////////
	// 현재 사용중인 사이즈 얻기.
	//
	// Parameters: 없음.
	// Return: (int)사용중인 데이타 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int		GetDataSize(void) { return _iDataSize; }

	unsigned char GetCheckSum()
	{
		unsigned char* payloadPtr = (unsigned char*)_iBuffer + _iHeaderSize;
		unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

		unsigned long sum = 0;
		while (payloadPtr != tailPtr)
		{
			sum += (unsigned char)*payloadPtr;
			payloadPtr++;
		}

		unsigned char checkSum = (unsigned char)(sum % 256);
		return checkSum;
	}

	void SetCheckSum()
	{
		unsigned char* payloadPtr = (unsigned char*)_iBuffer + _iHeaderSize;
		unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

		unsigned long sum = 0;
		while (payloadPtr != tailPtr)
		{
			//sum += (unsigned char)*(_iBuffer + _iHeaderSize) + 1;
			sum += (unsigned char)*payloadPtr;
			payloadPtr++;
		}

		unsigned char checkSum = (unsigned char)(sum % 256);
		*(GetCheckSumPtr()) = checkSum;
	}


	//////////////////////////////////////////////////////////////////////////
	// 이 직렬화 버퍼에 저장된 값을 인코딩
	//
	// Parameters: 고정키, 랜덤키.
	// Return: 없음
	//////////////////////////////////////////////////////////////////////////
	void Encode(unsigned char K, unsigned char RK)
	{
		SetCheckSum();

		unsigned char* cursorPtr = (unsigned char*)GetCheckSumPtr();
		unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

		unsigned char E = 0;
		unsigned char P = 0;

		int cnt = 1;
		while (cursorPtr != tailPtr)
		{
			unsigned char D = *cursorPtr;

			P = D ^ (P + RK + cnt);
			E = P ^ (E + K + cnt);

			*cursorPtr = E;

			cursorPtr++;
			cnt++;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	// 이 직렬화 버퍼에 저장된 값을 디코딩후 체크섬까지 체크 후 결과 반환
	//
	// Parameters: 고정키, 랜덤키.
	// Return: (bool) 체크섬 일치 여부
	//////////////////////////////////////////////////////////////////////////
	bool Decode(unsigned char K, unsigned char RK)
	{
		unsigned char* cursorPtr = (unsigned char*)GetCheckSumPtr();
		unsigned char* tailPtr = (unsigned char*)_iBuffer + _tail;

		unsigned char D = 0;
		unsigned char P = 0;
		unsigned char prevE = 0;
		unsigned char prevP = 0;

		int cnt = 1;
		while (cursorPtr != tailPtr)
		{
			unsigned char E = *cursorPtr;

			P = E ^ (prevE + K + cnt);
			D = P ^ (prevP + RK + cnt);

			prevP = P;
			prevE = E;

			*cursorPtr = D;
			cursorPtr++;
			cnt++;
		}

		unsigned char checkSum = GetCheckSum();
		if (checkSum != (unsigned char)*GetCheckSumPtr())
			return false;

		return true;
	}

	// 미리 생성해놓는 경우 사용할 Initialize
#ifndef MALLOC_ON_CALL
	void Initialize(int iHeaderSize)
	{
		_head = iHeaderSize;
		_tail = iHeaderSize;
		_iDataSize = 0;
		_iHeaderSize = iHeaderSize;

#ifdef LOG_CPACEKT
		int idx = InterlockedIncrement(&_iLogAllocIdx) % CPACKET_LOGSIZE;
		_allocLog[idx] = (LPVOID)this;
#endif
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터 얻기.
	//
	// Parameters: 없음.
	// Return: (char *)버퍼 포인터.
	//////////////////////////////////////////////////////////////////////////
	char* GetBufferPtr(void) { return _iBuffer; }

	char* GetTailPtr(void) { return _iBuffer + _tail; }

	char* GetHeadPtr(void) { return _iBuffer + _head; }

	char* GetCheckSumPtr(void) { return _iBuffer + _iHeaderSize - sizeof(unsigned char); }

	char* GetPayloadPtr(void) { return _iBuffer + _iHeaderSize; }

	//////////////////////////////////////////////////////////////////////////
	// 버퍼 Pos 이동. (음수이동은 안됨)
	// GetBufferPtr 함수를 이용하여 외부에서 강제로 버퍼 내용을 수정할 경우 사용. 
	//
	// Parameters: (int) 이동 사이즈.
	// Return: (int) 이동된 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	MoveWritePos(int iSize)
	{
		int moveSize = (_tail + iSize > _iBufferSize) ? ((_tail + iSize) - _iBufferSize) : iSize;
		_iDataSize += moveSize;
		_tail += moveSize;
		return moveSize;
	}
	int	MoveReadPos(int iSize)
	{
		int moveSize = (iSize > _iDataSize) ? _iDataSize : iSize;
		_head += moveSize;
		_iDataSize -= moveSize;
		return moveSize;
	}

	void inline PushHeader(char* header, int headerSize)
	{
		_head -= headerSize;

		memcpy(_iBuffer + _head, header, headerSize);

		_iDataSize += headerSize;
	}

	CPacket& operator = (CPacket& clSrcPacket)
	{
		/*this->_iBufferSize = clSrcPacket._iBufferSize;
		this->_iDataSize = clSrcPacket._iDataSize;
		this->_head = clSrcPacket._head;
		this->_tail = clSrcPacket._tail;
		memcpy(this->_iBuffer, clSrcPacket._iBuffer, clSrcPacket._iDataSize);*/
		memcpy(this, &clSrcPacket, sizeof(CPacket));

		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 넣기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator << (const unsigned char byValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(unsigned char) > _iBufferSize)
			return *this;
#endif

		* (unsigned char*)(_iBuffer + _tail) = byValue;
		_tail += sizeof(unsigned char);
		_iDataSize += sizeof(unsigned char);
		return *this;
	}
	CPacket& operator << (const char chValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(char) > _iBufferSize)
			return *this;
#endif 

		* (char*)(_iBuffer + _tail) = chValue;
		_tail += sizeof(char);
		_iDataSize += sizeof(char);
		return *this;
	}

	CPacket& operator << (const short shValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(short) > _iBufferSize)
			return *this;
#endif 

		* (short*)(_iBuffer + _tail) = shValue;
		_tail += sizeof(short);
		_iDataSize += sizeof(short);
		return *this;
	}
	CPacket& operator << (const unsigned short wValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(unsigned short) > _iBufferSize)
			return *this;
#endif 

		* (unsigned short*)(_iBuffer + _tail) = wValue;
		_tail += sizeof(unsigned short);
		_iDataSize += sizeof(unsigned short);
		return *this;
	}

	/*CPacket& operator << (const WORD dwValue)
	{
		if (_tail + sizeof(WORD) > _iBufferSize)
			return *this;

		*(int*)(_iBuffer + _tail) = dwValue;
		_tail += sizeof(WORD);
		_iDataSize += sizeof(WORD);
		return *this;
	}*/

	CPacket& operator << (const DWORD dwValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(DWORD) > _iBufferSize)
			return *this;
#endif

		* (int*)(_iBuffer + _tail) = dwValue;
		_tail += sizeof(DWORD);
		_iDataSize += sizeof(DWORD);
		return *this;
	}

	CPacket& operator << (const int iValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(int) > _iBufferSize)
			return *this;
#endif

		* (int*)(_iBuffer + _tail) = iValue;
		_tail += sizeof(int);
		_iDataSize += sizeof(int);
		return *this;
	}
	CPacket& operator << (const long lValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(long) > _iBufferSize)
			return *this;
#endif

		* (long*)(_iBuffer + _tail) = lValue;
		_tail += sizeof(long);
		_iDataSize += sizeof(long);
		return *this;
	}
	CPacket& operator << (const float fValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(float) > _iBufferSize)
			return *this;
#endif

		* ((float*)(_iBuffer + _tail)) = fValue;
		_tail += sizeof(float);
		_iDataSize += sizeof(float);
		return *this;
	}

	CPacket& operator << (const __int64 iValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(__int64) > _iBufferSize)
			return *this;
#endif

		* (__int64*)(_iBuffer + _tail) = iValue;
		_tail += sizeof(__int64);
		_iDataSize += sizeof(__int64);
		return *this;
	}
	CPacket& operator << (const double dValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(double) > _iBufferSize)
			return *this;
#endif

		* ((double*)(_iBuffer + _tail)) = dValue;
		_tail += sizeof(double);
		_iDataSize += sizeof(double);
		return *this;
	}

	CPacket& operator << (const ULONGLONG dValue)
	{
#ifdef DEBUG_CPACKET
		if (_tail + sizeof(ULONGLONG) > _iBufferSize)
			return *this;
#endif

		* ((ULONGLONG*)(_iBuffer + _tail)) = dValue;
		_tail += sizeof(ULONGLONG);
		_iDataSize += sizeof(ULONGLONG);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator >> (BYTE& byValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(BYTE))
			return *this;
#endif

		byValue = *((BYTE*)(_iBuffer + _head));

		_iDataSize -= sizeof(BYTE);
		_head += sizeof(BYTE);
		return *this;
	}
	CPacket& operator >> (char& chValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(char))
			return *this;
#endif

		chValue = *((char*)(_iBuffer + _head));

		_iDataSize -= sizeof(char);
		_head += sizeof(char);
		return *this;
	}

	CPacket& operator >> (short& shValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(short))
			return *this;
#endif

		shValue = *((short*)(_iBuffer + _head));

		_iDataSize -= sizeof(short);
		_head += sizeof(short);
		return *this;
	}
	CPacket& operator >> (WORD& wValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(WORD))
			return *this;
#endif

		wValue = *((WORD*)(_iBuffer + _head));

		_iDataSize -= sizeof(WORD);
		_head += sizeof(WORD);
		return *this;
	}

	CPacket& operator >> (int& iValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(int))
			return *this;
#endif

		iValue = *((int*)(_iBuffer + _head));

		_iDataSize -= sizeof(int);
		_head += sizeof(int);
		return *this;
	}
	CPacket& operator >> (DWORD& dwValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(DWORD))
			return *this;
#endif

		dwValue = *((DWORD*)(_iBuffer + _head));

		_iDataSize -= sizeof(DWORD);
		_head += sizeof(DWORD);
		return *this;
	}
	CPacket& operator >> (float& fValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(float))
			return *this;
#endif

		fValue = *((float*)(_iBuffer + _head));

		_iDataSize -= sizeof(float);
		_head += sizeof(float);
		return *this;
	}

	CPacket& operator >> (__int64& iValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(__int64))
			return *this;
#endif

		iValue = *((__int64*)(_iBuffer + _head));

		_iDataSize -= sizeof(__int64);
		_head += sizeof(__int64);
		return *this;
	}
	CPacket& operator >> (double& dValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(double))
			return *this;
#endif

		dValue = *((double*)(_iBuffer + _head));

		_iDataSize -= sizeof(double);
		_head += sizeof(double);
		return *this;
	}

	CPacket& operator >> (ULONGLONG& dValue)
	{
#ifdef DEBUG_CPACKET
		if (_iDataSize < sizeof(ULONGLONG))
			return *this;
#endif

		dValue = *((ULONGLONG*)(_iBuffer + _head));

		_iDataSize -= sizeof(ULONGLONG);
		_head += sizeof(ULONGLONG);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 데이타 얻기.
	//
	// Parameters: (char *)Dest 포인터. (int)Size.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	GetData(char* chpDest, int iSize)
	{
		int getSize = (iSize > _iDataSize) ? _iDataSize : iSize;
		memcpy(chpDest, _iBuffer + _head, getSize);

		_iDataSize -= getSize;
		_head += getSize;
		return getSize;
	}

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	PutData(char* chpDest, int iSize)
	{
		int putSize = (_tail + iSize > _iBufferSize) ? ((_tail + iSize) - _iBufferSize) : iSize;
		memcpy(_iBuffer + _tail, chpDest, putSize);

		_iDataSize += putSize;
		_tail += putSize;
		return putSize;
	}

#ifdef LOG_CPACKET
	static DWORD _iLogFreeIdx;
	static LPVOID _freeLog[CPACKET_LOGSIZE];
	static DWORD _iLogAllocIdx;
	static LPVOID _allocLog[CPACKET_LOGSIZE];
#endif
	static SHS::CMemoryPool<CPacket> _CPacketPool;
	//static procademy::CMemoryPool_LockFree<CPacket> _CPacketPool;
	//static TLSMemoryPoolManager<CPacket> _CPacketPool;
	friend class TLSMemoryPoolManager<CPacket>;
protected:
	bool _isUsing;
	int _iBufferSize;
	// 현재 버퍼에 사용중인 사이즈
	int _iDataSize;
	int _iHeaderSize;

	int _head;
	int _tail;
	char* _iBuffer;
};

#endif