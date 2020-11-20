/*********************************************
Author:Tanglx
Date:2019/08/12
FileName:_Win_Lock.h
**********************************************/
#ifndef _FILENAME__WIN_LOCK_
#define _FILENAME__WIN_LOCK_

#include <Windows.h>
class _Impl_Lock
{
public:
	_Impl_Lock(){
		InitializeCriticalSection(&itsCs);
	}
	~_Impl_Lock(){
		DeleteCriticalSection(&itsCs);
	}
	void Lock(){
		EnterCriticalSection(&itsCs);
	}
	void Unlock(){
		LeaveCriticalSection(&itsCs);
	}
private:
	CRITICAL_SECTION itsCs;
};

class _Impl_Sem
{
public:
	_Impl_Sem(){
		itsEvent = CreateEvent(NULL,TRUE,FALSE,NULL);
	}
	~_Impl_Sem(){
		CloseHandle(itsEvent);
	}

	void Take(unsigned int nTimeOut){
		ResetEvent(itsEvent);
		WaitForSingleObject(itsEvent,nTimeOut);
	}
	void Give(){
		SetEvent(itsEvent);
	}
private:
	HANDLE itsEvent;
};

#endif		//_FILENAME__WIN_LOCK_
