/*********************************************
Author:Tanglx
Date:2020/08/17
FileName:iSerialCore.h
**********************************************/
#ifndef _FILENAME_ISERIALCORE_
#define _FILENAME_ISERIALCORE_

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
#include "../../iPlatform/iLock/iLock.h"
#include "iSerial.h"


class iSerialCore : public iSingleton<iSerialCore>
{
public:
	iSerialCore(){}
	~iSerialCore(){}

	bool CreateCore(int32 nWorkerNum = 0)
	{
		itsImpl.RegOnError(this,&iSerialCore::OnError);
		itsImpl.RegOnRecv(this,&iSerialCore::OnRecv);
		return itsImpl.CreateCore(nWorkerNum);
	}
	void DestoryCore()
	{
		itsImpl.DestoryCore();
	}

	int32 OpenSerial(const char* sName,int32 nBaud,int32 nDataBits=8,int32 nStopBits=1,int32 nParity=0)
	{
		return (int32)itsImpl.OpenSerial(sName,nBaud,nDataBits,nStopBits,nParity);
	}

	void CloseSerial(int32 hSerial)
	{
		itsImpl.CloseSerial((int32)hSerial);
	}

	int32 PostSend(int32 hSerial,char* buf,int32 len)
	{
		return (int32)itsImpl.PostSend((int32)hSerial,buf,len);
	}

	void Bind(int32 hSerial,iSerial* pSerial)
	{
		pSerial->itsFd = hSerial;
		pSerial->RegPostSend(this,&iSerialCore::PostSend);

		itsSerialMapLock.Lock();
		itsSerialMap[hSerial] = pSerial;
		itsSerialMapLock.Unlock();
	}

	void UnBind(int32 hSerial)
	{
		iLifeSpanLock _lk(&itsSerialMapLock);
		std::map<int32, iSerial*>::iterator it = itsSerialMap.find(hSerial);
		if (it != itsSerialMap.end())
		{
			itsSerialMap.erase(it);
		}
	}

	void OnError(int32 fd)
	{
		iLifeSpanLock _lk(&itsSerialMapLock);
		std::map<int32,iSerial*>::iterator it = itsSerialMap.find(fd);
		if (it != itsSerialMap.end())
		{
			if (it->second->itsOnErrorPtr)
				it->second->itsOnErrorPtr->Execute();
		}else
		{
			throw "iSerialCore::OnError Impossible!";
		}
	}

	void OnRecv(int32 fd,char* buf,int32 len)
	{
		iLifeSpanLock _lk(&itsSerialMapLock);
		std::map<int32,iSerial*>::iterator it = itsSerialMap.find(fd);
		if (it != itsSerialMap.end())
		{
			if (it->second->itsOnRecvPtr)
				it->second->itsOnRecvPtr->Execute(buf,len);
		}else
		{
			throw "iSerialCore::OnRecv Impossible!";
		}
	}
private:
	_Impl_iSerialCore<1024> itsImpl;

	std::map<int32, iSerial*> itsSerialMap;
	iLock itsSerialMapLock;
};

#endif
