#pragma once
#include "CSerializationBuffer.h"
#include "ProcademyProfiler.h"
#include <Windows.h>

class RefCountPointer
{
public:
	RefCountPointer()
	{

	}

	/*~RefCountPointer()
	{
		if (InterlockedDecrement(_iRefCount) == 0)
		{
			delete(_iRefCount);
			CPacket::_CPacketPool.Free(ptr); 
		}
	}*/

	static RefCountPointer MakeSharedPtr()
	{
		RefCountPointer result;
		result._iRefCount = new long(1);
		{
			//Profiler("Alloc");
			result.ptr = CPacket::_CPacketPool.Alloc();
		}

		return result;
	}

	/*
	static RefCountPointer<T> MakeSharedPtr(bool isAuto)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T;
		result._isAuto = isAuto;

		return result;
	}

	static RefCountPointer<T> MakeSharedPtr(bool isAuto, int arg1)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T(arg1);
		result._isAuto = isAuto;

		return result;
	}

	static RefCountPointer<T> MakeSharedPtr(bool isAuto, int arg1, int arg2)
	{
		RefCountPointer<T> result;
		result._iRefCount = new unsigned int(1);
		result.ptr = new T(arg1, arg2);
		result._isAuto = isAuto;

		return result;
	}
	*/

	CPacket* operator*()
	{
		//현재 노드의 데이터를 뽑음
		return ptr;
	}

	/*RefCountPointer& operator= (const RefCountPointer& copy)
	{
		ptr = copy.ptr;
		_iRefCount = copy._iRefCount;

		InterlockedIncrement((LONG*)_iRefCount);

		return *this;
	}

	RefCountPointer(const RefCountPointer& copy)
	{
		ptr = copy.ptr;
		_iRefCount = copy._iRefCount;

		InterlockedIncrement((LONG*)_iRefCount);
	}*/

	void IncRefCount()
	{
		InterlockedIncrement((LONG*)_iRefCount);
	}

	bool DecRefCount()
	{
		if (*_iRefCount < 0)
			DebugBreak();

		if (InterlockedDecrement((LONG*)_iRefCount) == 0)
		{
			//Profiler("Free");
			delete(_iRefCount);
			ptr->Clear();
			CPacket::_CPacketPool.Free(ptr);

			return false;
		}

		/*if (*_iRefCount < 0)
			DebugBreak();*/
		return true;
	}

private:
	CPacket* ptr;
	long* _iRefCount;

	//void IncRefCount()
	//{
	//	InterlockedIncrement((LONG*)_iRefCount);
	//}

	//void DecRefCount()
	//{
	//	if (*_iRefCount < 0)
	//		DebugBreak();

	//	if (InterlockedDecrement((LONG*)_iRefCount) == 0)
	//	{
	//		//Profiler("Free");
	//		delete(_iRefCount);
	//		CPacket::_CPacketPool.Free(ptr);
	//	}

	//	/*if (*_iRefCount < 0)
	//		DebugBreak();*/
	//}

	friend class CNetServer;
};