/*********************************************
Author:Tanglx
Date:2020/08/18
FileName:_Win_Serial.h
Windows 下的串口通讯使用完成端口实现
**********************************************/
#ifndef _FILENAME__WIN_SERIAL_
#define _FILENAME__WIN_SERIAL_

//iLib
#include "../../../iTypes.h"
#include "../../../iPlatform/iThread/iThread.h"
#include "../../../iPattern/iTrans.h"
#include "../../../iFrame/iMemCache/iMemCache.h"
#include "../../iWindows/iIocp/iIocp.h"
//OS

//STL
#include <map>
#include <list>

template<int nMaxTransLen>
class _Impl_iSerialCore
{
public:
	_Impl_iSerialCore(){
		itsOnErrorPtr = NULL;
		itsOnRecvPtr = NULL;
	}
	~_Impl_iSerialCore(){
		if (itsOnErrorPtr)
			delete itsOnErrorPtr;
		if (itsOnRecvPtr)
			delete itsOnRecvPtr;

		itsOnErrorPtr = NULL;
		itsOnRecvPtr = NULL;
	}

	//
	struct PerSerialHandleData : public iIocp::PerHandleData
	{

	};

	struct PerSerialIoData : public iIocp::PerIoData
	{
		char Buf[nMaxTransLen];
		int32 Len;
		PerSerialIoData(){
			memset(this,0,sizeof(PerSerialIoData));
		}
	};

	enum
	{
		OP_UNKNOW,
		OP_SERIAL_WRITE,
		OP_SERIAL_READ
	};

	bool CreateCore(int32 nWorkerNum = 0)
	{
		itsCache.CreateMem(100);
		bool bRet = itsIocp.CreateIocp();
		if (!bRet)
		{
			itsCache.DestoryMem();
			return false;
		}

		//创建工作线程
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		if (nWorkerNum <= 0)
			nWorkerNum = si.dwNumberOfProcessors*2;


		for (int i=0;i<nWorkerNum;++i)
		{
			iThreadEx* pThread = new iThreadEx;
			if (pThread->Create(this,&_Impl_iSerialCore::Worker,64*1024,60,"Win_Serial_Core",pThread))
			{
				itsThreadList.push_back(pThread);
			}
		}

		if (itsThreadList.size() == 0)
		{
			itsIocp.DestoryIocp();
			itsCache.DestoryMem();
			return false;
		}

		return bRet;
	}

	void DestoryCore()
	{
		//投递信息,让Worker挂起
		for (uint32 i=0;i<itsThreadList.size();++i)
		{
			itsIocp.PostIocpNotify(0,0,0);
		}

		//将挂起的Worker线程Stop
		std::list<iThreadEx *>::iterator it = itsThreadList.begin();
		for (;it != itsThreadList.end();++it)
		{
			(*it)->Stop();
		}

		//释放单句柄数据,以及关闭句柄
		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSerialHandleData *>::iterator it2 = itsPerHandleDataMap.begin();
		for (;it2 != itsPerHandleDataMap.end();++it2)
		{
			delete it2->second;
			::CloseHandle((HANDLE)it2->first);
			printf("DestoryCore : 关闭句柄0x%x \r\n",it2->first);
		}
		itsPerHandleDataMap.clear();
		itsPerHandleDataMapLock.Unlock();

		//释放Cache缓存
		itsCache.DestoryMem();

		//关闭Iocp
		itsIocp.DestoryIocp();
	}


	int32 OpenSerial(const char* sName,int32 nBaud,int32 nDataBits,int32 nStopBits,int32 nParity)
	{
		//Windows 下 0表示停止位为1
		nStopBits-=1;

		char OpName[128] = {0};

		if (strlen(sName) >= strlen("COM10"))
		{
			sprintf_s(OpName,"\\\\.\\%s",sName);
		}else
		{
			sprintf_s(OpName,"%s",sName);
		}

		int32 hSerial = (int32)::CreateFileA(
			OpName,
			GENERIC_READ|GENERIC_WRITE,
			0,
			NULL,
			OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED,
			NULL
			);

		if (hSerial==(int32)INVALID_HANDLE_VALUE)
			return NULL;

		//获取设备控制块
		DCB dcb;
		SecureZeroMemory(&dcb, sizeof(DCB));
		dcb.DCBlength = sizeof(DCB);

		if(!GetCommState((HANDLE)hSerial,&dcb)){
			::CloseHandle((HANDLE)hSerial);
			hSerial=NULL;
			return NULL;
		}


		dcb.BaudRate = nBaud;				// set the baud rate
		dcb.ByteSize = (BYTE)nDataBits;		// data size, xmit, and rcv
		dcb.Parity = (BYTE)nParity;			// no parity bit
		dcb.StopBits = (BYTE)nStopBits;		// one stop bit


		//默认不使用所有的流控制
		if (!SetCommState((HANDLE)hSerial,&dcb))
		{
			::CloseHandle((HANDLE)hSerial);
			hSerial=NULL;
			return NULL;
		}

		//绑定完成端口
		PerSerialHandleData* pPerHandleData = new PerSerialHandleData;
		pPerHandleData->Handle = (HANDLE)hSerial;
		bool bStatus = itsIocp.BindWithIocp((HANDLE)hSerial,pPerHandleData);
		if (bStatus)
		{
			//绑定成功
			itsPerHandleDataMapLock.Lock();
			itsPerHandleDataMap[hSerial] = pPerHandleData;
			itsPerHandleDataMapLock.Unlock();

			//投递第一个接收
			_PostReadSerial(hSerial);

			return hSerial;
		}else
		{
			//绑定失败
			::CloseHandle((HANDLE)hSerial);
			delete pPerHandleData;
			return NULL;
		}
	}

	void CloseSerial(int32 hSerial)
	{
		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSerialHandleData *>::iterator it = itsPerHandleDataMap.find(hSerial);
		if (it != itsPerHandleDataMap.end())
		{
			itsPerHandleDataMap.erase(it);
		}
		itsPerHandleDataMapLock.Unlock();

		if ((HANDLE)hSerial == INVALID_HANDLE_VALUE || !hSerial)
			return;
		::CloseHandle((HANDLE)hSerial);
		printf("CloseSerial:0x%x\r\n",hSerial);
	}

	BOOL PostSend(int32 hSerial,char* buf,int32 len)
	{
		return _PostWriteSerial(hSerial,buf,len);
	}

	template<class T>
	void RegOnError(T* pObj,void(T::* pFunc)(int32))
	{
		if (itsOnErrorPtr)
			delete itsOnErrorPtr;
		itsOnErrorPtr = new iTrans1::Leaf<T,int32>(pObj,pFunc);
	}

	template<class T>
	void RegOnRecv(T* pObj,void(T::* pFunc)(int32,char*,int32))
	{
		if (itsOnRecvPtr)
			delete itsOnRecvPtr;
		itsOnRecvPtr = new iTrans3::Leaf<T,int32,char*,int32>(pObj,pFunc);
	}
private:
	BOOL _PostReadSerial(int32 hSerial,PerSerialIoData* pPerIoData = NULL)
	{
		if (!pPerIoData)
		{
			pPerIoData = itsCache.AllocMem();
		}
		new(pPerIoData) PerSerialIoData();

		pPerIoData->OpType = OP_SERIAL_READ;
		pPerIoData->Len = nMaxTransLen;

		BOOL bRet = ::ReadFile(
			(HANDLE)hSerial,
			pPerIoData->Buf,
			pPerIoData->Len,
			NULL,
			&pPerIoData->OverLapped
			);

		if (!bRet)
		{
			if (GetLastError() == ERROR_IO_PENDING)
			{
				return TRUE;
			}
		}
		return FALSE;
	}

	BOOL _PostWriteSerial(int32 hSerial,char* buf,int len)
	{
		while(len > nMaxTransLen)
		{
			printf("超过了最大可传输的长度\r\n");
			return FALSE;
		}

		PerSerialIoData* pPerIoData = new(itsCache.AllocMem()) PerSerialIoData();
		pPerIoData->OpType=OP_SERIAL_WRITE;
		memcpy(pPerIoData->Buf,buf,len);
		pPerIoData->Len = len;

		BOOL bRet=::WriteFile(
			(HANDLE)hSerial,
			pPerIoData->Buf,
			pPerIoData->Len,
			NULL,
			&pPerIoData->OverLapped
			);

		if (!bRet)
		{
			if (GetLastError()==ERROR_IO_PENDING)
				return TRUE;
			else
				return FALSE;
		}
		return TRUE;
	}


	void Worker(void* lpParam)
	{
		iThreadEx* pThread = (iThreadEx*)lpParam;

		DWORD nCompletionBytes = 0;
		void* pContext = NULL;
		LPOVERLAPPED pOverLapped = NULL;
		BOOL bRet = itsIocp.GetIocpNotify(&nCompletionBytes,(PULONG_PTR)&pContext,&pOverLapped,INFINITE);

		if (!bRet)
		{
			return ;
		}

		if (nCompletionBytes == 0 && pContext == NULL && pOverLapped == NULL)
		{
			//退出完成端口,需要关闭线程
			//这里将任务挂起,等待退出
			pThread->Puase();
			return;
		}

		PerSerialHandleData* pPerHandleData = (PerSerialHandleData*)pContext;
		PerSerialIoData* pPerIoData = CONTAINING_RECORD(pOverLapped,PerSerialIoData,OverLapped);
		

		switch(pPerIoData->OpType)
		{
		case OP_SERIAL_READ:
			{
				if (nCompletionBytes == 0)
				{
					itsCache.FreeMem(pPerIoData);
					if (itsOnErrorPtr)
						itsOnErrorPtr->Execute((int32)pPerHandleData->Handle);
					::CloseHandle(pPerHandleData->Handle);
				}else
				{
					if (itsOnRecvPtr)
						itsOnRecvPtr->Execute((int32)pPerHandleData->Handle,pPerIoData->Buf,nCompletionBytes);
					if (!_PostReadSerial((int32)pPerHandleData->Handle,pPerIoData))
					{
						itsCache.FreeMem(pPerIoData);
						if (itsOnErrorPtr)
							itsOnErrorPtr->Execute((int32)pPerHandleData->Handle);
						::CloseHandle(pPerHandleData->Handle);
						delete pPerHandleData;
					}
				}
			}break;
		case OP_SERIAL_WRITE:
			{
				if (nCompletionBytes == 0)
				{//断开
					itsCache.FreeMem(pPerIoData);
					if (itsOnErrorPtr)
						itsOnErrorPtr->Execute((int32)pPerHandleData->Handle);
					::CloseHandle(pPerHandleData->Handle);
				}else
				{//检查是否发送完成
					if (nCompletionBytes == pPerIoData->Len)
					{//发送完成,删除发送单IO数据
						itsCache.FreeMem(pPerIoData);
					}
					else if (nCompletionBytes < (DWORD)pPerIoData->Len)
					{//发送未完成,继续投递Send
						if (!_PostWriteSerial((int32)pPerHandleData->Handle,pPerIoData->Buf+nCompletionBytes,pPerIoData->Len-nCompletionBytes))
						{
							itsCache.FreeMem(pPerIoData);
							if (itsOnErrorPtr)
								itsOnErrorPtr->Execute((int32)pPerHandleData->Handle);
							::CloseHandle(pPerHandleData->Handle);
							delete pPerHandleData;
							break;
						}
						itsCache.FreeMem(pPerIoData);
					}else{
						throw "CantRun There!";
					}
				}
			}break;
		default:
			break;
		}
	}
private:
	iIocp itsIocp;
	iMemCache<PerSerialIoData> itsCache;
	std::map<int32,PerSerialHandleData*> itsPerHandleDataMap;
	iLock itsPerHandleDataMapLock;

	//线程链
	std::list<iThreadEx*> itsThreadList;
	iLock itsThreadListLock;

	iTrans1::Branch<int32>* itsOnErrorPtr;
	iTrans3::Branch<int32,char*,int32>* itsOnRecvPtr;
};

#endif