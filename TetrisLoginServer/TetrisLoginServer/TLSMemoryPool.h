#pragma once
#include <Windows.h>
#include "ProcademyProfiler.h"
// 메인 메모리 풀은 청크 단위로 오브젝트들을 관리하는 풀이다. 
// 락프리 구조로 구현.
#define ALLOCCOUNT 2
#define MAXCAPACITY_CHUNK 7
#define LOGSIZE 10000
//#define DEBUG_TLSMEMORYPOOL
#define DEBUG_GUARDCODE

enum LOG_WORKTYPE
{
	ALLOC_TLSPOOL,
	FREE_TLSPOOL
};

enum LOG_NODESTATE
{
	NONE,
	IN_USE,
	IN_TLS,
	IN_MANAGER
};

template <typename DATA>
class TLSMemoryPoolManager
{
	struct st_BLOCK_NODE
	{
		st_BLOCK_NODE* guardCode;
		DATA allocData;
		st_BLOCK_NODE* nextPtr;
	};

	struct st_ALLOCLOG
	{
		LOG_WORKTYPE type;
		st_BLOCK_NODE* ptr;
	};

	//friend class TLSMemoryPool;
public:
	// 매개변수 (1청크에 들어가는 노드 개수, 스레드에 할당할 기본 청크 개수, 스레드 개수, 
	// 할당 받을때 생성자 호출 여부, 생성 때 생성자 호출 여부)
	TLSMemoryPoolManager(unsigned int iChunkSize = 0, unsigned int iChunkPerThread = 0, unsigned int iThreadCount = 0,
		bool bPlacementNew = false, bool bCreateNew = false)
	{
		_dwIDCnt = 0;

		_TlsIdx = TlsAlloc();
		_ThreadCount = iThreadCount;

		_iChunkSize = iChunkSize;
		_iChunkPerThread = iChunkPerThread;

		// 총 생성 청크는 전체 스레드의 요구 청크 * 2만큼
		_iCreateChunkCount = iThreadCount * iChunkPerThread * 2;
		_iLeftChunk = 0;
		_iUseChunk = 0;

		_bPlacementNew = bPlacementNew;
		_bCreateNew = bCreateNew;

		_MainPoolTopNode = NULL;

		_pGuardCode = (LPVOID)this;

		InitChunk();
	}

	void InitChunk()
	{
		for (int i = 0; i < _iCreateChunkCount; i++)
		{
			CreateChunk();
		}
	}

	// 스레드 별 스택 생성
	void Thread_Init()
	{
#ifdef DEBUG_TLSMEMORYPOOL
		if (_TlsIdx == 0)
			DebugBreak();
#endif

		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = new TLSMemoryPool(_iChunkPerThread, MAXCAPACITY_CHUNK, this);
		TlsSetValue(_TlsIdx, (LPVOID)pMemoryPool);

		// 여기에 내가 생성해놓은 노드들 단체로 이동
		pMemoryPool->AllocChunkFromPool();

		InterlockedIncrement(&_iTLSPoolCount);
	}

	// 종료할 때 동적할당 해제용도
	void Thread_CleanUp();

	// 청크 할당 메인 -> TLS
	st_BLOCK_NODE* AllocChunkToTLS()
	{
		// 호출했으니까, 데이터를 반환할 때까지 루프
		while (1)
		{
			// 청크가 없다면 생성
			while (_MainPoolTopNode == NULL)
			{
				CreateChunk();
			}

			st_BLOCK_NODE* oldTopChunk = _MainPoolTopNode;

			st_BLOCK_NODE* chunkPtr = (st_BLOCK_NODE*)(0x0000ffffffffffff & (ULONGLONG)oldTopChunk);
			st_BLOCK_NODE* newTopChunk = (st_BLOCK_NODE*)(chunkPtr->guardCode);

			if (InterlockedCompareExchange64((__int64*)&_MainPoolTopNode, (__int64)newTopChunk, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
#ifdef DEBUG_TLSMEMORYPOOL
				DWORD localCnt = InterlockedIncrement(&_logIdx) % LOGSIZE;
				_LogArr[localCnt].ptr = oldTopChunk;
				_LogArr[localCnt].type = ALLOC_TLSPOOL;				
#endif
				chunkPtr->guardCode = (st_BLOCK_NODE*)_pGuardCode;
				// 할당하려는게 깨졌는지 - 걸림
				/*st_BLOCK_NODE<DATA>* node = chunkPtr;
				for (int i = 0; i < _iChunkSize - 1; i++)
				{
					if (node->nextPtr == NULL)
						DebugBreak();

					node = node->nextPtr;
				}*/


				InterlockedIncrement(&_iUseChunk);
				InterlockedDecrement(&_iLeftChunk);

				return chunkPtr;
			}
		}
	}

	// 청크 해제 TLS -> 메인
	void FreeChunkToPool(st_BLOCK_NODE* chunk)
	{
		//st_BLOCK_NODE<DATA>* chunkPtr = (st_BLOCK_NODE<DATA>*)(0x0000fffffffffff & (ULONGLONG)chunk);
		ULONGLONG localIdx = (ULONGLONG)InterlockedIncrement(&_dwIDCnt) % (USHRT_MAX + 1);
		localIdx = localIdx << 48;

		st_BLOCK_NODE* newTop = (st_BLOCK_NODE*)((ULONGLONG)chunk | localIdx);
#ifdef DEBUG_TLSMEMORYPOOL
		if (chunk->guardCode != _pGuardCode)
		{
			DebugBreak();
		}
#endif

		while (1)
		{
			st_BLOCK_NODE* oldTopChunk = _MainPoolTopNode;
			chunk->guardCode = oldTopChunk;

			if (InterlockedCompareExchange64((__int64*)&_MainPoolTopNode, (__int64)newTop, (__int64)oldTopChunk) == (__int64)oldTopChunk)
			{
#ifdef DEBUG_TLSMEMORYPOOL
				DWORD localCnt = InterlockedIncrement(&_logIdx) % LOGSIZE;
				_LogArr[localCnt].ptr = newTop;
				_LogArr[localCnt].type = FREE_TLSPOOL;
#endif

				InterlockedIncrement(&_iLeftChunk);
				InterlockedDecrement(&_iUseChunk);
				break;
			}
		}
	}

	// TLS 스택에서 할당
	DATA* Alloc()
	{
		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		if (pMemoryPool == NULL)
		{
			Thread_Init();

			pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		}

		DATA* data = pMemoryPool->Alloc();

		if (_bPlacementNew)
		{
			data = new(data) DATA();
		}

		InterlockedIncrement(&_dwAllocCount);

		return data;
	}

	// 스레드 스택에 반환
	void Free(DATA* pData)
	{
		// 스레드의 메모리풀 주소 얻어오기
		TLSMemoryPool* pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		if (pMemoryPool == NULL)
		{
			Thread_Init();

			pMemoryPool = (TLSMemoryPool*)TlsGetValue(_TlsIdx);
		}

		if (_bPlacementNew)
		{
			pData->~DATA();
		}

		pMemoryPool->Free(pData);

		InterlockedIncrement(&_dwFreeCount);
	}

	void CreateChunk()
	{
		st_BLOCK_NODE* pChunkNode = NULL;
		st_BLOCK_NODE* prevNode = NULL;

		for (int i = 0; i < _iChunkSize; i++)
		{
			pChunkNode = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));
			pChunkNode->guardCode = (st_BLOCK_NODE*)_pGuardCode;
			pChunkNode->nextPtr = prevNode;

			if (_bCreateNew)
				new(&(pChunkNode->allocData))DATA;

			prevNode = pChunkNode;
		}

		ULONGLONG localIdx = (ULONGLONG)(InterlockedIncrement(&_dwIDCnt)) % (USHRT_MAX + 1);
		localIdx = localIdx << 48;

		while (1)
		{
			st_BLOCK_NODE* oldChunkTop = _MainPoolTopNode;
			pChunkNode->guardCode = oldChunkTop;

			st_BLOCK_NODE* newChunk = (st_BLOCK_NODE*)((ULONGLONG)pChunkNode | localIdx);
			// 청크의 다음 노드 주소는 guardCode에 넣자. 어차피 청크 안에서의 guard처리는 안할거다.
			
			if (InterlockedCompareExchange64((__int64*)&_MainPoolTopNode, (__int64)newChunk, (__int64)oldChunkTop) == (__int64)oldChunkTop)
			{
				InterlockedIncrement(&_iLeftChunk);
				break;
			}
		}
	}

	// 얘가 스레드 별로 생성해서 TLS에 저장하는 클래스
	// 여러 스레드가 사용할 일이 없으니, 그냥 스택 구조로 구현
	class TLSMemoryPool
	{
	public:
		TLSMemoryPool(int baseChunk, int maxChunk, TLSMemoryPoolManager* manager)
		{
			_TopNode = NULL;
			_iTlsChunkSize = manager->_iChunkSize;

			_dwSize = 0;
			_iBaseChunk = baseChunk;
			_iBaseSize = baseChunk * _iTlsChunkSize;

			_guardCode = manager;
			_Manager = manager;
		}

		// 메인 풀에서 청크를 할당받아 TLS 스택의 노드들과 연결해주고, 청크 배열에 저장
		void AllocChunkFromPool()
		{
			for (int i = 0; i < _iBaseChunk; i++)
			{
				// Chunk Data노드를 받는다.chunk만 인덱스를 사용하니 비트연산 필요
				st_BLOCK_NODE* chunkTop = _Manager->AllocChunkToTLS();
				st_BLOCK_NODE* bottomNode = chunkTop;

				// 그 노드를 타고 들어가서 최하단 노드를 찾기
				for (int j = 0; j < _iTlsChunkSize - 1; j++)
				{
					bottomNode = bottomNode->nextPtr;
					if (bottomNode == NULL)
						DebugBreak();
				}

				// bottomNode는 Top과 연결,Top은 청크로 받은 노드로 변경.
				bottomNode->nextPtr = _TopNode;
				_TopNode = chunkTop;

				//_dwSize += _iTlsChunkSize;
				InterlockedAdd((LONG*)&_dwSize, _iTlsChunkSize);
			}
		}

		// 일단 하나씩 순회하며 카운팅해주고, 청크에서 꺼내서 반환하기. 그림은 그렸다.
		void FreeChunk()
		{
//#ifdef DEBUG_TLSMEMORYPOOL
			if (_dwSize < _iTlsChunkSize * ALLOCCOUNT)
				DebugBreak();
//#endif

			// 반환할 청크 만큼 반복
			for (int allocCnt = 0; allocCnt < ALLOCCOUNT; allocCnt++)
			{ 
				// 현재 노드에서 Size만큼 탐색하며 그 다음 노드를 Top으로 설정
				st_BLOCK_NODE* returnChunk = _TopNode;
				st_BLOCK_NODE* newTopNode = _TopNode;
				st_BLOCK_NODE* tailNode = _TopNode;
				for (int i = 0; i < _iTlsChunkSize; i++)
				{
					if (newTopNode == NULL)
						DebugBreak();

					tailNode = newTopNode;
					newTopNode = newTopNode->nextPtr;
				}

				tailNode->nextPtr = NULL;
				_TopNode = newTopNode;

				// 청크 데이터를 반환
				_Manager->FreeChunkToPool(returnChunk);
				//_dwSize -= _iTlsChunkSize;
				InterlockedAdd((LONG*) & _dwSize, -_iTlsChunkSize);
			}
		}

		bool Free(DATA* pData)
		{
			st_BLOCK_NODE* nodePtr = (st_BLOCK_NODE*)((char*)pData - offsetof(st_BLOCK_NODE, allocData));
			//st_BLOCK_NODE* nodePtr = (st_BLOCK_NODE*)((BYTE*)pData - sizeof(st_BLOCK_NODE*));
#ifdef DEBUG_GUARDCODE
			if (nodePtr->guardCode != _guardCode)
				DebugBreak();
#endif

#ifdef DEBUG_TLSMEMORYPOOL
			DWORD localCnt = InterlockedIncrement(&_dwTLSLogIdx) % LOGSIZE;
			_TLSLogArr[localCnt].ptr = nodePtr;
			_TLSLogArr[localCnt].type = FREE_TLSPOOL;
#endif

			nodePtr->nextPtr = _TopNode;
			_TopNode = nodePtr;

			++_dwSize;
			//InterlockedIncrement(&_dwSize);

			if (_dwSize >= _iBaseSize * 2)
			{
				FreeChunk();
			}

			return true;
		}

		DATA* Alloc()
		{
			// 그냥 부족할 때 할당
			if (_TopNode == NULL)
			{
				AllocChunkFromPool();
			}

			st_BLOCK_NODE* oldTop = _TopNode;

#ifdef DEBUG_GUARDCODE
			oldTop->guardCode = (st_BLOCK_NODE*)_guardCode;
#endif

#ifdef DEBUG_TLSMEMORYPOOL
			DWORD localCnt = InterlockedIncrement(&_dwTLSLogIdx) % LOGSIZE;
			_TLSLogArr[localCnt].ptr = oldTop;
			_TLSLogArr[localCnt].type = ALLOC_TLSPOOL;
#endif

			_TopNode = _TopNode->nextPtr ;
			//_workArr[_logIdx++] = { POP, oldTop };

			--_dwSize;
			//InterlockedDecrement(&_dwSize);

			return &(oldTop->allocData);
		}
	private:
		DWORD _dwTLSLogIdx;
		st_ALLOCLOG _TLSLogArr[LOGSIZE];
		
		LONG _dwSize = 0;
		unsigned long _logIdx = 0;

		unsigned int _iBaseChunk;
		unsigned int _iBaseSize;
		unsigned int _iMaxSize;

		int _iTlsChunkCount;
		int _iTlsChunkSize;
		LPVOID _guardCode;

		st_BLOCK_NODE* _TopNode;
		TLSMemoryPoolManager* _Manager;
	};

	// 해당 스레드의 메모리풀이 몇번 TLS 인덱스에 박혀있는지 -> 각각 스레드의 _TlsIdx에 메모리풀 주소 저장
	DWORD _TlsIdx = -1;
private:
	st_BLOCK_NODE* _MainPoolTopNode;

	// 스레드 개수
	DWORD _ThreadCount;

	// 청크 사이즈, 생성 청크 개수
	unsigned int _iChunkSize;
	unsigned int _iCreateChunkCount;
	unsigned int _iThreadCount;
	unsigned int _iChunkPerThread;

	// 16비트 카운터
	volatile DWORD _dwIDCnt;

	unsigned int _iTLSPoolCount;

	// 현재 사용량, 남은 양, 용량
	unsigned int _iUseChunk;
	unsigned int _iLeftChunk;

	DWORD _logIdx;
	st_ALLOCLOG _LogArr[LOGSIZE];

	DWORD _dwFreeCount;
	DWORD _dwAllocCount;

	bool _bPlacementNew;
	bool _bCreateNew;
	LPVOID _pGuardCode;
};


