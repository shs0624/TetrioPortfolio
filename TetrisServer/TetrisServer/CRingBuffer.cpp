#include "CRingBuffer.h"

CRingBuffer::CRingBuffer()
{
	arr = (char*)malloc(DEFAULTRINGBUFFERSIZE + 1);
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	max = DEFAULTRINGBUFFERSIZE + 1;
}

CRingBuffer::CRingBuffer(int size)
{
	arr = (char*)malloc(size + 1);
	if (arr == NULL)
	{
		DebugBreak();
	}

	head = 0;
	tail = 0;
	max = size + 1;
}

CRingBuffer::~CRingBuffer(void)
{
	free(arr);
}

int CRingBuffer::GetBufferSize()
{
	return max;
}

int CRingBuffer::GetFreeSize()
{
	int tempHead = head;
	int tempTail = tail;

	if (tempTail == tempHead)
		return max - 1;

	if (tempTail > tempHead)
	{
		return  max - (tempTail - tempHead) - 1;
	}
	else
	{
		return tempHead - tempTail - 1;
	}
}

int CRingBuffer::GetUseSize()
{
	int tempHead = head;
	int tempTail = tail;

	if (tempTail == tempHead)
		return 0;

	if (tempTail > tempHead)
	{
		return tempTail - tempHead;
	}
	else
	{
		return max - (tempHead - tempTail);
	}
}

int CRingBuffer::Enqueue(char* input, int size)
{
	// 넣을 수 있는 사이즈 얻기
	int tempTail = tail;
	int freeSize = GetFreeSize();
	int enqueueSize = (freeSize >= size) ? size : freeSize;

	if (tempTail + enqueueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - tempTail;
		memcpy((void*)(arr + tempTail), (void*)input, cutSize);
		memcpy((void*)arr, (void*)(input + cutSize), enqueueSize - cutSize);
	}
	else
	{
		memcpy((void*)(arr + tempTail), (void*)input, enqueueSize);
	}

	tail = (tempTail + enqueueSize) % max;
	return enqueueSize;
}

int CRingBuffer::Dequeue(char* output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int tempHead = head;
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;	


	if (tempHead + dequeueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - tempHead;
		memcpy((void*)output, (void*)(arr + tempHead), cutSize);
		memcpy((void*)(output + cutSize), (void*)arr, dequeueSize - cutSize);
	}
	else
	{
		memcpy((void*)output, (void*)(arr + tempHead), dequeueSize);
	}

	head = (tempHead + dequeueSize) % max;
	return dequeueSize;
}

int CRingBuffer::Peek(char* output, int size)
{
	// 뺄 수 있는 사이즈 얻기
	int tempHead = head;
	int useSize = GetUseSize();
	int dequeueSize = (useSize < size) ? useSize : size;

	if (tempHead + dequeueSize > max)
	{
		// 경계를 넘는다면
		int cutSize = max - tempHead;
		memcpy((void*)output, (void*)(arr + tempHead), cutSize);
		memcpy((void*)(output + cutSize), (void*)arr, dequeueSize - cutSize);
	}
	else
	{
		memcpy((void*)output, (void*)(arr + tempHead), dequeueSize);
	}

	return dequeueSize;
}

int CRingBuffer::DirectEnqueueSize(void)
{
	return max - tail;
}

int CRingBuffer::DirectDequeueSize(void)
{
	return max - head;
}

// 호출 전에 FreeSize를 체크하고 넣을거다.
int CRingBuffer::MoveRear(int iSize)
{
	tail += iSize;
	tail = tail % max;
	return iSize;
}

// 호출 전에 UseSize를 체크하고 넣을거다.
int CRingBuffer::MoveFront(int iSize)
{
	head += iSize;
	head = head % max;
	return iSize;
}

char* CRingBuffer::GetFrontBufferPtr(void)
{
	return (arr + head);
}

char* CRingBuffer::GetRearBufferPtr(void)
{
	return (arr + tail);
}

char* CRingBuffer::GetArrPtr(void)
{
	return arr;
}

void CRingBuffer::ClearBuffer(void)
{
	head = 0;
	tail = 0;
}