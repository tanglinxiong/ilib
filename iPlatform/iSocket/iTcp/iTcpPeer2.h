/*********************************************
Author:Tanglx
Date:2020/10/12
FileName:iTcpPeer2.h
**********************************************/
#ifndef _FILENAME_ITCPPEER2_
#define _FILENAME_ITCPPEER2_

#include "../../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Tcp.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Tcp.h"
#else
#error "Can Not Find Adjust Platform"
#endif
#include "../../../iTypes.h"
#include "../../../iPattern/iTrans.h"

class iTcpPeer2
{
	friend class iTcpCore2;
public:
	iTcpPeer2(){
		itsOnDisConnectPtr = NULL;
		itsOnRecvPtr = NULL;
		itsPostSendPtr = NULL;

		itsFd = 0;
		itsFlag = -1;
	}
	~iTcpPeer2(){}

	void SetFd(int32 nSocket){itsFd = nSocket;}
	int32 GetFd(){return itsFd;}
	void SetFlag(uint32 nFlag){itsFlag = nFlag;}
	int32 GetFlag(){return itsFlag;}

	template<class T>
	void RegOnDisConnectPtr(T* pObj,void(T::* pFunc)())
	{
		if (itsOnDisConnectPtr)
		{
			delete itsOnDisConnectPtr;
		}
		itsOnDisConnectPtr = new iTrans0::Leaf<T>(pObj,pFunc);
	}

	template<class T>
	void RegOnRecvPtr(T* pObj,void(T::* pFunc)(char*,int32))
	{
		if (itsOnRecvPtr)
		{
			delete itsOnRecvPtr;
		}
		itsOnRecvPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
	}
private:
	iTrans0::Branch* itsOnDisConnectPtr;
	iTrans2::Branch<char*,int32>* itsOnRecvPtr;

	int32 itsFd;
	uint32 itsFlag;

	//发送相关
public:
	int32 PostSend(char* buf,int32 len)
	{
		if (itsPostSendPtr)
			return itsPostSendPtr->Execute(itsFd,buf,len);
		else
			return 0;
	}

private:
	//给iTcpCore2实现
	template<class T>
	void RegPostSend(T* pObj,bool(T::* pFunc)(int32,char*,int32))
	{
		if (itsPostSendPtr)
			delete itsPostSendPtr;
		itsPostSendPtr = new _iTrans3<bool>::Leaf<T,int32,char*,int32>(pObj,pFunc);
	}

	void ClearPostSendPtr()
	{
		if (itsPostSendPtr)
			delete itsPostSendPtr;
		itsPostSendPtr = NULL;
	}

	_iTrans3<bool>::Branch<int32,char*,int32>* itsPostSendPtr;
};

#endif
