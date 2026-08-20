#pragma once
#include <iostream>
#include <Windows.h>
#define DEFAULTRINGBUFFERSIZE 1024

class CRingBuffer
{
public:
	CRingBuffer();

	CRingBuffer(int size);

	~CRingBuffer(void);

	void Resize(int size);

	// 버퍼 사이즈 얻기
	int GetBufferSize(void);
	// 사용중인 용량 얻기
	int GetUseSize(void);
	// 버퍼에 남은 용량 얻기
	int GetFreeSize(void);

	// 데이터 넣고 넣은 크기 반환, end 이동
	int Enqueue(char* input, int size);

	// 앞에서 데이터 빼고 가져온 크기 반환, front 이동
	int Dequeue(char* output, int size);

	// 앞에서 데이터 빼고 가져온 크기 반환, front 이동 X
	int Peek(char* output, int size);

	// 버퍼 비우기 -> front, rear만 조정해서 데이터 밀지 않기
	void ClearBuffer(void);

	/////////////////////////////////////////////////////////////////////////
	// 버퍼 포인터로 외부에서 한방에 읽고, 쓸 수 있는 길이.
	// (끊기지 않은 길이)
	//
	// 원형 큐의 구조상 버퍼의 끝단에 있는 데이터는 끝 -> 처음으로 돌아가서
	// 2번에 데이터를 얻거나 넣을 수 있음. 이 부분에서 끊어지지 않은 길이를 의미
	//
	// Parameters: 없음.
	// Return: (int)사용가능 용량.
	////////////////////////////////////////////////////////////////////////
	int DirectEnqueueSize(void);
	int DirectDequeueSize(void);

	/////////////////////////////////////////////////////////////////////////
	// 원하는 길이만큼 읽기위치 에서 삭제 / 쓰기 위치 이동
	//
	// Parameters: 없음.
	// Return: (int)이동크기
	/////////////////////////////////////////////////////////////////////////
	int MoveRear(int iSize);
	int MoveFront(int iSize);

	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 Front 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char* GetFrontBufferPtr(void);


	/////////////////////////////////////////////////////////////////////////
	// 버퍼의 RearPos 포인터 얻음.
	//
	// Parameters: 없음.
	// Return: (char *) 버퍼 포인터.
	/////////////////////////////////////////////////////////////////////////
	char* GetRearBufferPtr(void);

	// arr 포인터 반환
	char* GetArrPtr(void);


public:
	char* arr;
	int head;
	int tail;
	int max;
};