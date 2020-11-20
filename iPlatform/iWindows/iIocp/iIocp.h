/*********************************************
Author:Tanglx
Date:2020/08/20
FileName:iIocp.h
对于IO完成端口的一些封装
**********************************************/
#ifndef _FILENAME_IIOCP_
#define _FILENAME_IIOCP_

//iLib
#include "../../iThread/iThread.h"
#include "../../../iPattern/iTrans.h"

//OS
#include <Windows.h>
class iIocp
{
public:
	iIocp(){}
	~iIocp(){}

	//BasePerHandle Data
	struct PerHandleData{
		HANDLE Handle;
	};

	//BasePerIo Data
	struct PerIoData{
		OVERLAPPED OverLapped;
		DWORD OpType;
	};

	bool CreateIocp()
	{
		itsIocpHandle=::CreateIoCompletionPort(INVALID_HANDLE_VALUE,NULL,NULL,0);
		if (!itsIocpHandle)
			return false;
		return true;
	}

	bool BindWithIocp( HANDLE hIo,PerHandleData* pPerHandleData )
	{
		HANDLE _h=CreateIoCompletionPort(hIo,itsIocpHandle,(ULONG_PTR)pPerHandleData,0);
		if (_h==itsIocpHandle)
			return true;
		else
			return false;
	}

	BOOL GetIocpNotify( LPDWORD lpNumberOfBytes,PULONG_PTR lpKey,LPOVERLAPPED *lpOverlap,DWORD dwMs/*=INFINITE*/ )
	{
		return GetQueuedCompletionStatus(itsIocpHandle,lpNumberOfBytes,lpKey,lpOverlap,dwMs);
	}

	BOOL PostIocpNotify(DWORD NumberOfBytes,ULONG_PTR Key,LPOVERLAPPED lpOverlap)
	{
		return PostQueuedCompletionStatus(itsIocpHandle,NumberOfBytes,Key,lpOverlap);
	}

	void DestoryIocp()
	{
		if (itsIocpHandle && itsIocpHandle != INVALID_HANDLE_VALUE)
			CloseHandle(itsIocpHandle);
	}
private:
	HANDLE itsIocpHandle;//IocpHandle
};
#endif