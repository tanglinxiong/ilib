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
		Status_None,		//δ����
		Status_Runing,		//������
		Status_Pause,		//��ͣ
		Status_Stoping,		//ֹͣ��
		Status_Stoped,		//ֹͣ���
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

    //�ú���ֻ���̺߳���֮�����
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

	//�ú���ֻӦ���ڸ��̺߳����е���
	void Pause()
	{
		itsRunStatus = Status_Pause;
		itsPauseSem.Take();
	}

	//�ú���Ӧ���ڸ��̺߳��������,�Ͼ��߳��޷������Լ�
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
	int32 itsRunStatus;//����״̬,����Run��Pause
	int32 itsStopStatus;//ֹͣ״̬,��Stoping��Stoped
};

//iThreadEx , ��iThread����һЩ��չ
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
		Status_None,		//δ����
		Status_Runing,		//������
		Status_Pause,		//��ͣ
		Status_Stoping,		//ֹͣ��
		Status_Stoped,		//ֹͣ���
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

    //�ú���ֻ���̺߳���֮�����
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

    //�ú���ֻӦ���ڸ��̺߳����е���
    void Pause()
    {
		itsRunStatus = Status_Pause;
        itsPauseSem.Take();
    }

    //�ú���Ӧ���ڸ��̺߳��������,�Ͼ��߳��޷������Լ�
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
        //printf("�˳��߳�\r\n");
    }

private:
    bool itsQuit;
	iSem itsStopSem0;
    iSem itsStopSem1;
    iSem itsPauseSem;
    uint32 itsThreadHandle;
	int32 itsRunStatus;//����״̬,����Run��Pause
	int32 itsStopStatus;//ֹͣ״̬,��Stoping��Stoped
};


#endif

