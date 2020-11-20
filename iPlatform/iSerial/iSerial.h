/*********************************************
Author:Tanglx
Date:2020/08/17
FileName:iSerial.h
Must Be Standard C++ And Platform Independent Code
**********************************************/
#ifndef _FILENAME_ISERIAL_
#define _FILENAME_ISERIAL_

#include "../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Serial.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Serial.h"
#else
#error "Can Not Find Adjust Platform"
#endif

#include "../../iPattern/iTrans.h"


class iSerial
{
	friend class iSerialCore;
public:
	iSerial(){
		itsOnErrorPtr = NULL;
		itsOnRecvPtr = NULL;
		itsPostSendPtr = NULL;
	}
	~iSerial(){}

	template<class T>
	void RegOnError(T* pObj,void(T::* pFunc)())
	{
		if (itsOnErrorPtr)
			delete itsOnErrorPtr;
		itsOnErrorPtr = new iTrans0::Leaf<T>(pObj,pFunc);
	}

	template<class T>
	void RegOnRecv(T* pObj,void(T::* pFunc)(char*,int32))
	{
		if (itsOnRecvPtr)
			delete itsOnRecvPtr;
		itsOnRecvPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
	}

	int32 GetFd()
	{
		return itsFd;
	}
public:
	int32 itsFd;
	iTrans0::Branch* itsOnErrorPtr;
	iTrans2::Branch<char*,int32>* itsOnRecvPtr;


public:
	int32 PostSend(char* buf,int32 len)
	{
		if (itsPostSendPtr)
			return itsPostSendPtr->Execute(itsFd,buf,len);
		else
			return 0;
	}

private:
	//¸øiSerialCoreÊµÏÖ
	template<class T>
	void RegPostSend(T* pObj,int32(T::* pFunc)(int32,char*,int32))
	{
		if (itsPostSendPtr)
			delete itsPostSendPtr;
		itsPostSendPtr = new _iTrans3<int32>::Leaf<T,int32,char*,int32>(pObj,pFunc);
	}
	_iTrans3<int32>::Branch<int32,char*,int32>* itsPostSendPtr;
};

#endif
