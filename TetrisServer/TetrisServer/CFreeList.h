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
#ifndef  __PROCADEMY_MEMORY_POOL__
#define  __PROCADEMY_MEMORY_POOL__
#define DEFAULTSIZE 50
#include <new.h>
#include <Windows.h>

namespace procademy
{
	template <class DATA>
	class CMemoryPool
	{
		struct st_BLOCK_NODE
		{
			void* guardCode;
			DATA allocPtr;
			st_BLOCK_NODE* nextPtr;
		};
	public:
		//////////////////////////////////////////////////////////////////////////
		// 생성자, 파괴자.
		//
		// Parameters:	(int) 초기 블럭 개수.
		//				(bool) Alloc 시 생성자 / Free 시 파괴자 호출 여부
		//				(bool) malloc 시 생성자 / Free 시 파괴자 호출 여부
		// Return:
		//////////////////////////////////////////////////////////////////////////
		//CMemoryPool() {}

		CMemoryPool(int iBlockNum = 0, bool bPlacementNew = false, bool bCreateNew = false)
		{
			InitializeCriticalSection(&_poolCRT);

			m_iCreateCount = (iBlockNum == 0) ? DEFAULTSIZE : iBlockNum;
			m_iCapacity = iBlockNum;
			m_iUseCount = 0;
			m_bPlacementNew = bPlacementNew;
			m_bCreateNew = bCreateNew;

			m_guardCode = (void*)this;
			_pFreeNode = nullptr;

			if (m_iCapacity == 0) return;

			for (int i = 0; i < iBlockNum; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

				if (bCreateNew)
				{
					DATA* data;
					data = new(&(node->allocPtr)) DATA;
				}

				node->guardCode = m_guardCode;
				node->nextPtr = _pFreeNode;
				_pFreeNode = node;
			}
		}

		virtual	~CMemoryPool()
		{
			st_BLOCK_NODE* node = _pFreeNode;

			int subCount = (m_iCapacity - m_iUseCount);
			for (int i = 0; i < subCount; i++)
			{
				st_BLOCK_NODE* next = _pFreeNode->nextPtr;
				free(_pFreeNode);
				_pFreeNode = next;
			}
		}

		//////////////////////////////////////////////////////////////////////////
		// 블럭 하나를 할당받는다.  
		//
		// Parameters: 없음.
		// Return: (DATA *) 데이타 블럭 포인터.
		//////////////////////////////////////////////////////////////////////////
		DATA* Alloc(void)
		{
			EnterCriticalSection(&_poolCRT);
			if (m_iUseCount == m_iCapacity)
				Resize();

			st_BLOCK_NODE* allocNode = _pFreeNode;
			_pFreeNode = _pFreeNode->nextPtr;

#ifdef __GUARDTEST__
			allocNode->nextPtr = (st_BLOCK_NODE*)m_guardCode;
#endif

			DATA* data = &(allocNode->allocPtr);
			if (m_bPlacementNew)
			{
				data = new(data) DATA;
			}

			++m_iUseCount;

			LeaveCriticalSection(&_poolCRT);
			return data;
		}

		//////////////////////////////////////////////////////////////////////////
		// 사용중이던 블럭을 해제한다.
		//
		// Parameters: (DATA *) 블럭 포인터.
		// Return: (BOOL) TRUE, FALSE.
		//////////////////////////////////////////////////////////////////////////
		bool Free(DATA* pData)
		{
			EnterCriticalSection(&_poolCRT);
			st_BLOCK_NODE* ptr = (st_BLOCK_NODE*)((char*)pData - 8);

#ifdef __GUARDTEST__
			if (ptr->guardCode != m_guardCode || ptr->nextPtr != m_guardCode)
			{
				DebugBreak();
			}
#endif

			if (m_bPlacementNew || m_bCreateNew)
			{
				ptr->allocPtr.~DATA();
			}

			ptr->nextPtr = _pFreeNode;
			_pFreeNode = ptr;
			m_iUseCount--;

			LeaveCriticalSection(&_poolCRT);
			return true;
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
		st_BLOCK_NODE* _pFreeNode;
	private:
		void Resize(void)
		{
			for (int i = 0; i < m_iCreateCount; i++)
			{
				st_BLOCK_NODE* node = (st_BLOCK_NODE*)malloc(sizeof(st_BLOCK_NODE));

				if (m_bCreateNew)
				{
					DATA* data;
					data = new(&(node->allocPtr)) DATA;
				}

				node->guardCode = m_guardCode;
				node->nextPtr = _pFreeNode;
				_pFreeNode = node;
			}

			m_iCapacity += m_iCreateCount;
		}

		int m_iCreateCount;
		int m_iCapacity;
		int m_iUseCount;
		bool m_bPlacementNew;
		bool m_bCreateNew;
		void* m_guardCode;

		CRITICAL_SECTION _poolCRT;
	};
}
#endif