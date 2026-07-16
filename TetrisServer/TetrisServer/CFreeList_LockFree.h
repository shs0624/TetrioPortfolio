/*---------------------------------------------------------------

	procademy MemoryPool.

	메모리 풀 클래스 (오브젝트 풀 / 프리리스트)
	특정 데이타(구조체,클래스,변수)를 일정량 할당 후 나눠쓴다.

	- 사용법.

	procademy::CMemoryPool<DATA> MemPool(300, FALSE);
	DATA *pData = MemPool.Alloc();

	pData 사용

	MemPool.Free(pData);


----------------------------------------------------------------*/
#pragma once
#define DEFAULTSIZE 500
//#define LOG_LOCKFREELIST
#include <new.h>
#include <Windows.h>

enum FreeList_LockFree_LOG
{
	ALLOC_LOCKFREEPOOL,
	FREE_LOCKFREEPOOL
};

namespace procademy
{
	template <class DATA>
	class CMemoryPool_LockFree
	{
		struct st_BLOCK_NODE
		{
			void* guardCode;
			DATA allocPtr;
			st_BLOCK_NODE* nextPtr;
		};
#ifdef LOG_LOCKFREELIST
		struct st_ALLOC_LOG
		{
			FreeList_LockFree_LOG type;
			st_BLOCK_NODE* ptr;
		};
#endif
	public:
		//////////////////////////////////////////////////////////////////////////
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수.
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		//				(bool) malloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		CMemoryPool_LockFree() {}

		CMemoryPool_LockFree(int iBlockNum = 0, bool bPlacementNew = false, bool bCreateNew = false)
		{
			m_iCreateCount = (iBlockNum == 0) ? DEFAULTSIZE : iBlockNum;
			m_iCapacity = iBlockNum;
			m_iUseCount = 0;
			m_bPlacementNew = bPlacementNew;
			m_bCreateNew = bCreateNew;

			m_guardCode = (void*)this;
			_pTopNode = nullptr;

			if (m_iCapacity == 0) return;

			for (int i = 0; i < iBlockNum; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

				ULONGLONG localIdx = _IDCnt++;
				localIdx = localIdx << 47;

				if (bCreateNew)
				{
					new(&(node->allocPtr))DATA;
				}
	
				node->guardCode = m_guardCode;
				node->nextPtr = _pTopNode;
				//memset(&node->allocPtr, 0, sizeof(DATA));
				//node->allocPtr = NULL;
				node = (st_BLOCK_NODE*)((ULONGLONG)node | localIdx);
				_pTopNode = node;				
			}

			int a = 50;
		}

		virtual	~CMemoryPool_LockFree()
		{
			while (_pTopNode != nullptr)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)((ULONGLONG)_pTopNode & 0x00007fffffffffff);
				st_BLOCK_NODE* next = node->nextPtr;

				if (m_bCreateNew || m_bPlacementNew)
					node->allocPtr.~DATA();

				_pTopNode = next;
				free(node);
			}

			/*int subCount = (m_iCapacity - m_iUseCount);
			for (int i = 0; i < subCount; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)((ULONGLONG)_pTopNode & 0x00007fffffffffff);
				st_BLOCK_NODE* next = node->nextPtr;

				if(m_bCreateNew || m_bPlacementNew)
					node->allocPtr.~DATA();

				delete(node);
				_pTopNode = next;
			}*/
		}

		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void)
		{
			// 호출했으니까, 데이터를 반환할 때까지 루프
			while (1)
			{
				//if (m_iUseCount == m_iCapacity)
				if(_pTopNode == NULL)
				{
					return Resize();
				}
					
				st_BLOCK_NODE* oldTopNode = _pTopNode;

				st_BLOCK_NODE* NodePtr = (st_BLOCK_NODE*)(0x00007fffffffffff & (ULONGLONG)oldTopNode);
				st_BLOCK_NODE* newNode = NodePtr->nextPtr;

#ifdef __GUARDTEST__
				NodePtr->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

 				if (InterlockedCompareExchange64((__int64*)&_pTopNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
				{
					// 바뀌었다!
					DATA* data = &(NodePtr->allocPtr);
					if (m_bPlacementNew)
					{
						data = new(data) DATA;
					}

#ifdef LOG_LOCKFREELIST
					DWORD localCnt = InterlockedIncrement(&_logIdx);
					_LogArr[localCnt].ptr = NodePtr;
					_LogArr[localCnt].type = ALLOC_LOCKFREEPOOL;
#endif

					InterlockedIncrement(&m_iUseCount);
					return data;
				}
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool Free(DATA* pData)
		{
			LONGLONG localIdx = InterlockedIncrement(&_IDCnt);
			localIdx = localIdx << 47;
			st_BLOCK_NODE* nodePtr = (st_BLOCK_NODE*)((char*)pData - sizeof(void*));
			st_BLOCK_NODE* newNode = (st_BLOCK_NODE*)((ULONGLONG)nodePtr | localIdx);

#ifdef __GUARDTEST__
			if (nodePtr->guardCode != m_guardCode || nodePtr->nextPtr != m_guardCode)
			{
				DebugBreak();
			}
#endif

			while (1)
			{
				st_BLOCK_NODE* oldTopNode = _pTopNode;
				nodePtr->nextPtr = oldTopNode;				

				if (InterlockedCompareExchange64((__int64*)&_pTopNode, (__int64)newNode, (__int64)oldTopNode) == (__int64)oldTopNode)
				{
					// 바뀌었다. 반환해야지.
					if (m_bPlacementNew || m_bCreateNew)
					{
						nodePtr->allocPtr.~DATA();
					}

#ifdef LOG_LOCKFREELIST
					DWORD localCnt = InterlockedIncrement(&_logIdx);
					_LogArr[localCnt].ptr = newNode;
					_LogArr[localCnt].type = FREE_LOCKFREEPOOL;
#endif
					
					InterlockedDecrement(&m_iUseCount);
					return true;
				}
			}

			return false;
		}


		//////////////////////////////////////////////////////////////////////////
		// 현재 확보 된 블럭 개수를 얻는다. (메모리풀 내부의 전체 개수)
		//
		// Parameters: 없음.
		// Return: (int) 메모리 풀 내부 전체 개수
		//////////////////////////////////////////////////////////////////////////
		int		GetCapacityCount(void) { return m_iCapacity; }

		//////////////////////////////////////////////////////////////////////////
		// 현재 사용중인 블럭 개수를 얻는다.
		//
		// Parameters: 없음.
		// Return: (int) 사용중인 블럭 개수.
		//////////////////////////////////////////////////////////////////////////
		int		GetUseCount(void) { return m_iUseCount; }


		// 스택 방식으로 반환된 (미사용) 오브젝트 블럭을 관리. - 스택의 탑 포인터.
		st_BLOCK_NODE* _pTopNode;
	private:

		// 그냥 하나 만들어서, 반환하는 형태
		DATA* Resize(void)
		{
			st_BLOCK_NODE* nodePtr = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

			nodePtr->guardCode = m_guardCode;
			nodePtr->nextPtr = _pTopNode;
			
			DATA* data = &(nodePtr->allocPtr);
			if (m_bPlacementNew || m_bCreateNew)
			{
				data = new(data) DATA;
			}

			InterlockedIncrement(&_IDCnt);
			InterlockedIncrement(&m_iUseCount);
			InterlockedIncrement(&m_iCapacity);

			return data;
		}

		ULONGLONG _IDCnt = 1;

		DWORD m_iCreateCount;
		DWORD m_iCapacity;
		DWORD m_iUseCount;
		bool m_bPlacementNew;
		bool m_bCreateNew;
		void* m_guardCode;

#ifdef LOG_LOCKFREELIST
		st_ALLOC_LOG _LogArr[30001];
		DWORD _logIdx = 0;
#endif
	};
}