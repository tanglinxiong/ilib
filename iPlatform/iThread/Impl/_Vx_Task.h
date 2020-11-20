/*********************************************
Author:Tanglx
Date:2019/08/19
FileName:_Vx_Task.h
**********************************************/
#ifndef _FILENAME__VX_TASK_
#define _FILENAME__VX_TASK_


#include "taskLibCommon.h"
#include "string.h"

#define ITHREADCALL

class _Impl_Thread
{
public:
    template<class TFunc>
    static unsigned int Thread_Create(TFunc Proc, void* lparam, UINT32 nStackSz, UINT32 nProprity, const char* name)
    {
        char _name[256] = {0};
        strncpy(_name, name, sizeof(_name));
        unsigned int itsHandle = taskSpawn(_name, nProprity, 0, 128 * 1024, (FUNCPTR)Proc, (int)lparam, 0, 0, 0, 0, 0, 0, 0, 0, 0);
        return (unsigned int)itsHandle;
    }

    static void Thread_Destory(unsigned int hThread)
    {
    }
};

#endif
