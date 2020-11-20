/*********************************************
Author:     Tanglx
Date:       2019/08/16
FileName:   iThread.h
**********************************************/
#ifndef _FILENAME_ITHREAD_
#define _FILENAME_ITHREAD_

#include "../iPlatform.h"

#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Thread.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Task.h"

#else

#error "Can Not Find Adjust Platform"
#endif


#include "../../iTypes.h"
#include "../iLock/iLock.h"
#include "../iPublic/iPublic.h"


class iThread
{
    struct IObjData
    {
        IObjData(iThread *pThread)
			: itsThreadPtr(pThread) {}
        virtual ~IObjData() {}

        virtual void Execute() = 0;
        iThread *itsThreadPtr;
    };

    template <class T> 
    struct ObjectData: public IObjData
    {
        ObjectData(T *pObj, void(T::*pMainfunc) (), iThread *pThread)
			: itsObjPtr(pObj), itsFuncPtr(pMainfunc), IObjData(pThread)
        {
        }
        virtual ~ObjectData()
        {
        }

        virtual void Execute()
        {
            ((*itsObjPtr).*itsFuncPtr)();
        }

    private:
        T *itsObjPtr;
        void(T::*itsFuncPtr) ();
    };

public:
	enum
	{
		Status_None,		//未创建
		Status_Runing,		//运行中
		Status_Pause,		//暂停
		Status_Stoping,		//停止中
		Status_Stoped,		//停止完成
	};

    iThread()
    {
        itsQuit             = false;
        itsThreadHandle     = NULL;
		itsRunStatus = Status_None;
		itsStopStatus = Status_None;
    }
    ~iThread()
    {
		
    }

    template <class T> uint32 Create(T *pObj, void(T::*pMf) (), uint32 nStackSz, uint32 nProprity, const char *name)
    {
        IObjData *lparam = (IObjData *) (new ObjectData <T> (pObj, pMf, this));
        itsThreadHandle = _Impl_Thread::Thread_Create(threadCall, (void *) lparam, nStackSz, nProprity, name);
		itsRunStatus = Status_Runing;
        return itsThreadHandle;
    }

    //该函数只在线程函数之外调用
    void Stop()
    {
		if (itsStopStatus == Status_Stoped)
		{
			return;
		}

		itsQuit = true;
		while (itsStopStatus != Status_Stoping)
		{
			Resume();
			iPublic::iSleep(200);
		}
		iPublic::iSleep(200);

		if (itsStopStatus == Status_Stoping)
		{
			itsStopSem0.Give();
			itsStopSem1.Take();
		}
		_Impl_Thread::Thread_Destory(itsThreadHandle);
    }

	//该函数只应该在该线程函数中调用
	void Pause()
	{
		itsRunStatus = Status_Pause;
		itsPauseSem.Take();
	}

	//该函数应该在该线程函数外调用,毕竟线程无法唤醒自己
	void Resume()
	{
		itsPauseSem.Give();
		itsRunStatus = Status_Runing;
	}
private:
    static uint32 ITHREADCALL threadCall(void *lparam)
    {
        IObjData *pObjDat = (IObjData *)lparam;
        pObjDat->itsThreadPtr->Process(lparam);
        delete pObjDat;
        return 0;
    }

public:
    void Process(void *lparam)
    {
        IObjData *pObjDat = (IObjData *)lparam;

        while (!itsQuit)
        {
            pObjDat->Execute();
        }
		itsStopStatus = Status_Stoping;

		itsStopSem0.Take();
		itsStopSem1.Give();

		itsStopStatus = Status_Stoped;
    }

private:
    bool    itsQuit;
    iSem    itsStopSem0;
    iSem    itsStopSem1;
    iSem    itsPauseSem;
    uint32  itsThreadHandle;
	int32 itsRunStatus;//运行状态,仅有Run和Pause
	int32 itsStopStatus;//停止状态,有Stoping和Stoped
};

//iThreadEx , 对iThread做了一些扩展
class iThreadEx
{
	struct IObjData{
		IObjData(iThreadEx* pThreadEx, void* addp = NULL)
			: itsThreadPtr(pThreadEx),itsAddp(addp){}
		virtual ~IObjData(){}
		virtual void Execute() = 0;
		iThreadEx* itsThreadPtr;
		void* itsAddp;
	};

	template<class T>
	struct ObjectData : public IObjData
	{
		ObjectData(T* pObj,void(T::* pMf)(void*),iThreadEx* itsThreadPtr,void* addp = NULL)
			:itsObjPtr(pObj),itsFuncPtr(pMf),IObjData(itsThreadPtr,addp){}
		virtual ~ObjectData(){}

		virtual void Execute()
		{
			((*itsObjPtr).*itsFuncPtr)(itsAddp);
		}

	private:
		T* itsObjPtr;
		void(T::* itsFuncPtr)(void*);
	};
public:
	enum
	{
		Status_None,		//未创建
		Status_Runing,		//运行中
		Status_Pause,		//暂停
		Status_Stoping,		//停止中
		Status_Stoped,		//停止完成
	};

	iThreadEx(){
		itsQuit = false;
		itsThreadHandle = NULL;
		itsRunStatus = Status_None;
		itsStopStatus = Status_None;
	}
	~iThreadEx(){
	}

	template<class T>
	uint32 Create(T* pObj,void(T::* pMf)(void*),uint32 nStackSz,uint32 nProprity,const char* name,void* addp)
	{
		IObjData* lparam = (IObjData*)(new ObjectData<T>(pObj,pMf,this,addp));
		itsThreadHandle = _Impl_Thread::Thread_Create(ThreadCall,(void*)lparam,nStackSz,nProprity,name);
		itsRunStatus = Status_Runing;
		return itsThreadHandle;
	}

    //该函数只在线程函数之外调用
    void Stop()
    {
		if (itsStopStatus == Status_Stoped)
		{
			return;
		}

		itsQuit = true;
		while (itsStopStatus != Status_Stoping)
		{
			Resume();
			iPublic::iSleep(200);
		}

		if (itsStopStatus == Status_Stoping)
		{
			itsStopSem0.Give();
			itsStopSem1.Take();
		}
        _Impl_Thread::Thread_Destory(itsThreadHandle);
    }

    //该函数只应该在该线程函数中调用
    void Pause()
    {
		itsRunStatus = Status_Pause;
        itsPauseSem.Take();
    }

    //该函数应该在该线程函数外调用,毕竟线程无法唤醒自己
    void Resume()
    {
        itsPauseSem.Give();
		itsRunStatus = Status_Runing;
    }
private:
    static uint32 ITHREADCALL ThreadCall(void *lparam)
    {
    	IObjData* pObjData = (IObjData*)lparam;
        pObjData->itsThreadPtr->Process(lparam);
        delete pObjData;
        
        return 0;
    }

    void Process(void* lparam)
    {
    	IObjData* pObjData = (IObjData*)lparam;

        while (!itsQuit)
        {
            pObjData->Execute();
        }
		itsStopStatus = Status_Stoping;

		itsStopSem0.Take();
        itsStopSem1.Give();

		itsStopStatus = Status_Stoped;
        //printf("退出线程\r\n");
    }

private:
    bool itsQuit;
	iSem itsStopSem0;
    iSem itsStopSem1;
    iSem itsPauseSem;
    uint32 itsThreadHandle;
	int32 itsRunStatus;//运行状态,仅有Run和Pause
	int32 itsStopStatus;//停止状态,有Stoping和Stoped
};


#endif

