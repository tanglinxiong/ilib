/*********************************************
Author:Tanglx
Date:2019/08/16
FileName:_Win_Thread.h
**********************************************/
#ifndef _FILENAME__WIN_THREAD_
#define _FILENAME__WIN_THREAD_


#define ITHREADCALL WINAPI
#include <Windows.h>


class _Impl_Thread
{
public:
	template<class TFunc>
	static unsigned int Thread_Create(TFunc Proc,LPVOID lparam,UINT32 nStackSz,UINT32 nProprity,const char* name)
	{
		HANDLE itsHandle = CreateThread(NULL,nStackSz,Proc,lparam,0,NULL);
		return (unsigned int)itsHandle;
	}

	static void Thread_Destory(unsigned int hThread)
	{
		if ((HANDLE)hThread)
		{
			CloseHandle((HANDLE)hThread);
		}
	}
};

#endif