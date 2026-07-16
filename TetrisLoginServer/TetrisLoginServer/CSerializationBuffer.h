#pragma once

#ifndef _CPACKET_
#define _CPACKET_
#include <Windows.h>
#include <new>
#include "TLSMemoryPool.h"
#define CPACKET_LOGSIZE 10000
#define PROTOCOL_MAX_SIZE 500

//#define MALLOC_ON_CALL
//#define LOG_CPACKET

enum en_PACKET
{
	eBUFFER_DFAULT = 24
};

class CPacket
{
public:
	CPacket();
	//CPacket(int iBufferSize);

	virtual ~CPacket();

	//////////////////////////////////////////////////////////////////////////
	// 패킷 청소.
	//
	// Parameters: 없음.
	// Return: 없음.
	//////////////////////////////////////////////////////////////////////////
	void Clear(void);

	void Clear(int iHeaderSize);

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

	unsigned char GetCheckSum();

	void SetCheckSum();

	//////////////////////////////////////////////////////////////////////////
	// 이 직렬화 버퍼에 저장된 값을 인코딩
	//
	// Parameters: 고정키, 랜덤키.
	// Return: 없음
	//////////////////////////////////////////////////////////////////////////
	void Encode(unsigned char K, unsigned char RK);


	//////////////////////////////////////////////////////////////////////////
	// 이 직렬화 버퍼에 저장된 값을 디코딩후 체크섬까지 체크 후 결과 반환
	//
	// Parameters: 고정키, 랜덤키.
	// Return: (bool) 체크섬 일치 여부
	//////////////////////////////////////////////////////////////////////////
	bool Decode(unsigned char K, unsigned char RK);

	//void Initialize(int iBufferSize, int iHeaderSize);
	void Initialize(int iHeaderSize);

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
	int		MoveWritePos(int iSize);
	int		MoveReadPos(int iSize);

	void PushHeader(char* header, int headerSize);

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
		if (_tail + sizeof(unsigned char) > _iBufferSize)
			return *this;

		*(unsigned char*)(_iBuffer + _tail) = byValue;
		_tail += sizeof(unsigned char);
		_iDataSize += sizeof(unsigned char);
		return *this;
	}
	CPacket& operator << (const char chValue)
	{
		if (_tail + sizeof(char) > _iBufferSize)
			return *this;

		*(char*)(_iBuffer + _tail) = chValue;
		_tail += sizeof(char);
		_iDataSize += sizeof(char);
		return *this;
	}

	CPacket& operator << (const short shValue)
	{
		if (_tail + sizeof(short) > _iBufferSize)
			return *this;

		*(short*)(_iBuffer + _tail) = shValue;
		_tail += sizeof(short);
		_iDataSize += sizeof(short);
		return *this;
	}
	CPacket& operator << (const unsigned short wValue)
	{
		if (_tail + sizeof(unsigned short) > _iBufferSize)
			return *this;

		*(unsigned short*)(_iBuffer + _tail) = wValue;
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
		if (_tail + sizeof(DWORD) > _iBufferSize)
			return *this;

		*(int*)(_iBuffer + _tail) = dwValue;
		_tail += sizeof(DWORD);
		_iDataSize += sizeof(DWORD);
		return *this;
	}

	CPacket& operator << (const int iValue)
	{
		if (_tail + sizeof(int) > _iBufferSize)
			return *this;

		*(int*)(_iBuffer + _tail) = iValue;
		_tail += sizeof(int);
		_iDataSize += sizeof(int);
		return *this;
	}
	CPacket& operator << (const long lValue)
	{
		if (_tail + sizeof(long) > _iBufferSize)
			return *this;

		*(long*)(_iBuffer + _tail) = lValue;
		_tail += sizeof(long);
		_iDataSize += sizeof(long);
		return *this;
	}
	CPacket& operator << (const float fValue)
	{
		if (_tail + sizeof(float) > _iBufferSize)
			return *this;

		*((float*)(_iBuffer + _tail)) = fValue;
		_tail += sizeof(float);
		_iDataSize += sizeof(float);
		return *this;
	}

	CPacket& operator << (const __int64 iValue)
	{
		if (_tail + sizeof(__int64) > _iBufferSize)
			return *this;

		*(__int64*)(_iBuffer + _tail) = iValue;
		_tail += sizeof(__int64);
		_iDataSize += sizeof(__int64);
		return *this;
	}
	CPacket& operator << (const double dValue)
	{
		if (_tail + sizeof(double) > _iBufferSize)
			return *this;

		*((double*)(_iBuffer + _tail)) = dValue;
		_tail += sizeof(double);
		_iDataSize += sizeof(double);
		return *this;
	}

	CPacket& operator << (const ULONGLONG dValue)
	{
		if (_tail + sizeof(ULONGLONG) > _iBufferSize)
			return *this;

		*((ULONGLONG*)(_iBuffer + _tail)) = dValue;
		_tail += sizeof(ULONGLONG);
		_iDataSize += sizeof(ULONGLONG);
		return *this;
	}

	//////////////////////////////////////////////////////////////////////////
	// 빼기.	각 변수 타입마다 모두 만듬.
	//////////////////////////////////////////////////////////////////////////
	CPacket& operator >> (BYTE& byValue)
	{
		if (_iDataSize < sizeof(BYTE))
			return *this;

		byValue = *((BYTE*)(_iBuffer + _head));

		_iDataSize -= sizeof(BYTE);
		_head += sizeof(BYTE);
		return *this;
	}
	CPacket& operator >> (char& chValue)
	{
		if (_iDataSize < sizeof(char))
			return *this;

		chValue = *((char*)(_iBuffer + _head));

		_iDataSize -= sizeof(char);
		_head += sizeof(char);
		return *this;
	}

	CPacket& operator >> (short& shValue)
	{
		if (_iDataSize < sizeof(short))
			return *this;

		shValue = *((short*)(_iBuffer + _head));

		_iDataSize -= sizeof(short);
		_head += sizeof(short);
		return *this;
	}
	CPacket& operator >> (WORD& wValue)
	{
		if (_iDataSize < sizeof(WORD))
			return *this;

		wValue = *((WORD*)(_iBuffer + _head));

		_iDataSize -= sizeof(WORD);
		_head += sizeof(WORD);
		return *this;
	}

	CPacket& operator >> (int& iValue)
	{
		if (_iDataSize < sizeof(int))
			return *this;

		iValue = *((int*)(_iBuffer + _head));

		_iDataSize -= sizeof(int);
		_head += sizeof(int);
		return *this;
	}
	CPacket& operator >> (DWORD& dwValue)
	{
		if (_iDataSize < sizeof(DWORD))
			return *this;

		dwValue = *((DWORD*)(_iBuffer + _head));

		_iDataSize -= sizeof(DWORD);
		_head += sizeof(DWORD);
		return *this;
	}
	CPacket& operator >> (float& fValue)
	{
		if (_iDataSize < sizeof(float))
			return *this;

		fValue = *((float*)(_iBuffer + _head));

		_iDataSize -= sizeof(float);
		_head += sizeof(float);
		return *this;
	}

	CPacket& operator >> (__int64& iValue)
	{
		if (_iDataSize < sizeof(__int64))
			return *this;

		iValue = *((__int64*)(_iBuffer + _head));

		_iDataSize -= sizeof(__int64);
		_head += sizeof(__int64);
		return *this;
	}
	CPacket& operator >> (double& dValue)
	{
		if (_iDataSize < sizeof(double))
			return *this;

		dValue = *((double*)(_iBuffer + _head));

		_iDataSize -= sizeof(double);
		_head += sizeof(double);
		return *this;
	}

	CPacket& operator >> (ULONGLONG& dValue)
	{
		if (_iDataSize < sizeof(ULONGLONG))
			return *this;

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
	int	GetData(char* chpDest, int iSize);

	//////////////////////////////////////////////////////////////////////////
	// 데이타 삽입.
	//
	// Parameters: (char *)Src 포인터. (int)SrcSize.
	// Return: (int)복사한 사이즈.
	//////////////////////////////////////////////////////////////////////////
	int	PutData(char* chpSrc, int iSrcSize);

#ifdef LOG_CPACKET
	static DWORD _iLogFreeIdx;
	static LPVOID _freeLog[CPACKET_LOGSIZE];
	static DWORD _iLogAllocIdx;
	static LPVOID _allocLog[CPACKET_LOGSIZE];
#endif
	static TLSMemoryPoolManager<CPacket> _CPacketPool;
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