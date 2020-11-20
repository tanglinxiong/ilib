/*********************************************
Author:Tanglx
Date:2020/08/24
FileName:iSerialCore_Direct.h
直接使用SerialCore
**********************************************/
#ifndef _FILENAME_ISERIALCORE_DIRECT_
#define _FILENAME_ISERIALCORE_DIRECT_

#include "../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Serial.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Serial.h"
#else
#error "Can Not Find Adjust Platform"
#endif

//iLib
#include "../../iPattern/iSingleton.h"

template<int32 nMaxTransLen>
class iSerialCore_Direct
{
public:
	iSerialCore_Direct(){}
	~iSerialCore_Direct(){}

	bool CreateCore(int32 nWorkerNum = 0)
	{
		return itsSerialCore.CreateCore(nWorkerNum);
	}
	void DestoryCore()
	{
		itsSerialCore.DestoryCore();
	}

	int32 OpenSerial(const char* sName,int32 nBaud,int32 nDataBits=8,int32 nStopBits=ONESTOPBIT,int32 nParity=NOPARITY)
	{
		return (int32)itsSerialCore.OpenSerial(sName,nBaud,nDataBits,nStopBits,nParity);
	}

	void CloseSerial(int32 hSerial)
	{
		itsSerialCore.CloseSerial((HANDLE)hSerial);
	}

	int32 PostSend(int32 hSerial,char* buf,int32 len)
	{
		return (int32)itsSerialCore.PostSend(hSerial,buf,len);
	}

	template<class T>
	void RegOnError(T* pObj,void(T::* pFunc)(int32))
	{
		void(T::* pFunc_Tmp)(int32) = (void(T::*)(int32))pFunc;
		itsSerialCore.RegOnError(pObj,pFunc_Tmp);
	}

	template<class T>
	void RegOnRecv(T* pObj,void(T::* pFunc)(int32,char*,int32))
	{
		void(T::* pFunc_Tmp)(int32,char*,int32) = (void(T::*)(int32,char*,int32))pFunc;
		itsSerialCore.RegOnRecv(pObj,pFunc_Tmp);
	}
private:
	_Impl_iSerialCore<nMaxTransLen> itsSerialCore;
};

#endif
