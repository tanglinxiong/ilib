/*********************************************
Author:Tanglx
Date:2020/08/18
FileName:_Win_Serial.h
Windows �µĴ���ͨѶʹ����ɶ˿�ʵ��
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

		//���������߳�
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
		//Ͷ����Ϣ,��Worker����
		for (uint32 i=0;i<itsThreadList.size();++i)
		{
			itsIocp.PostIocpNotify(0,0,0);
		}

		//�������Worker�߳�Stop
		std::list<iThreadEx *>::iterator it = itsThreadList.begin();
		for (;it != itsThreadList.end();++it)
		{
			(*it)->Stop();
		}

		//�ͷŵ��������,�Լ��رվ��
		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSerialHandleData *>::iterator it2 = itsPerHandleDataMap.begin();
		for (;it2 != itsPerHandleDataMap.end();++it2)
		{
			delete it2->second;
			::CloseHandle((HANDLE)it2->first);
			printf("DestoryCore : �رվ��0x%x \r\n",it2->first);
		}
		itsPerHandleDataMap.clear();
		itsPerHandleDataMapLock.Unlock();

		//�ͷ�Cache����
		itsCache.DestoryMem();

		//�ر�Iocp
		itsIocp.DestoryIocp();
	}


	int32 OpenSerial(const char* sName,int32 nBaud,int32 nDataBits,int32 nStopBits,int32 nParity)
	{
		//Windows �� 0��ʾֹͣλΪ1
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

		//��ȡ�豸���ƿ�
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


		//Ĭ�ϲ�ʹ�����е�������
		if (!SetCommState((HANDLE)hSerial,&dcb))
		{
			::CloseHandle((HANDLE)hSerial);
			hSerial=NULL;
			return NULL;
		}

		//����ɶ˿�
		PerSerialHandleData* pPerHandleData = new PerSerialHandleData;
		pPerHandleData->Handle = (HANDLE)hSerial;
		bool bStatus = itsIocp.BindWithIocp((HANDLE)hSerial,pPerHandleData);
		if (bStatus)
		{
			//�󶨳ɹ�
			itsPerHandleDataMapLock.Lock();
			itsPerHandleDataMap[hSerial] = pPerHandleData;
			itsPerHandleDataMapLock.Unlock();

			//Ͷ�ݵ�һ������
			_PostReadSerial(hSerial);

			return hSerial;
		}else
		{
			//��ʧ��
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
			printf("���������ɴ���ĳ���\r\n");
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
			//�˳���ɶ˿�,��Ҫ�ر��߳�
			//���ｫ�������,�ȴ��˳�
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
				{//�Ͽ�
					itsCache.FreeMem(pPerIoData);
					if (itsOnErrorPtr)
						itsOnErrorPtr->Execute((int32)pPerHandleData->Handle);
					::CloseHandle(pPerHandleData->Handle);
				}else
				{//����Ƿ������
					if (nCompletionBytes == pPerIoData->Len)
					{//�������,ɾ�����͵�IO����
						itsCache.FreeMem(pPerIoData);
					}
					else if (nCompletionBytes < (DWORD)pPerIoData->Len)
					{//����δ���,����Ͷ��Send
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

	//�߳���
	std::list<iThreadEx*> itsThreadList;
	iLock itsThreadListLock;

	iTrans1::Branch<int32>* itsOnErrorPtr;
	iTrans3::Branch<int32,char*,int32>* itsOnRecvPtr;
};

#endif