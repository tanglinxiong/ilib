/*********************************************
Author:         Tanglx
Date:           2019/08/28
FileName:       iMessageQ.h
Description:    消息处理队列
**********************************************/
#ifndef _FILENAME_IMESSAGEQ_
#define _FILENAME_IMESSAGEQ_


#include "iMessage.h"

//iPattern
#include "../../iPattern/iSingleton.h"

//iPlatform
#include "../../iTypes.h"
#include "../../iPlatform/iThread/iThread.h"
#include "../../iPlatform/iLock/iLock.h"

//stl
#include <map>
#include <list>
#include <vector>


class iMessageQ
{
    struct Key
    {
        MSGHANDLE itsHandler;
        uint32 itsMsgNo;
        Key() : itsHandler(0), itsMsgNo(0) {}
        Key(MSGHANDLE hHandler, uint32 nMsgNo)  : itsHandler(hHandler), itsMsgNo(nMsgNo) {}
        friend bool operator<(const Key &lv, const Key &rv)
        {
            return memcmp(&lv, &rv, sizeof(Key)) < 0;
        }
    };
    struct IObjectCall
    {
        IObjectCall() {}
        virtual ~IObjectCall() {}
        virtual void Execute(void *pData) = 0;
    };
    
    template<class TObj, class TDat> struct ObjectCallData : public IObjectCall
    {
        ObjectCallData(TObj *pObj, void(TObj::* pFunc)(TDat *))       : itsObjPtr(pObj), itsFuncPtr(pFunc) {}
        virtual ~ObjectCallData() {}
        virtual void Execute(void *pData)
        {
            ((*itsObjPtr).*itsFuncPtr)((TDat *)pData);
        }
        
    private:
        TObj *itsObjPtr;
        void(TObj::* itsFuncPtr)(TDat *);
    };
    
public:
	iMessageQ(const char* szThreadName,int nThreadNum = 1)
	{	
		iLifeSpanLock _lifelock(&itsThreadPoolLock);
		for (int i=0;i<nThreadNum;++i)
		{
			iThreadEx* p = new iThreadEx;
			char iThreadName[64] = {0};
			sprintf(iThreadName,"iMsgQ_%s_%d",szThreadName,i);
			uint32 nRet = p->Create(this,&iMessageQ::iMessageQ_Proc,256*1024,20,iThreadName,(void*)p);
			if (nRet <= 0)
				throw 0;

            itsThreadPool.push_back(p);
        }
    }
    ~iMessageQ()
    {
        itsThreadPoolLock.Lock();
        std::vector<iThreadEx *>::iterator it = itsThreadPool.begin();
        for (; it != itsThreadPool.end(); ++it)
        {
            (*it)->Stop();
        }
        itsThreadPool.clear();
        itsThreadPoolLock.Unlock();


        itsMsgListLock.Lock();
        std::list<iMessage *>::iterator it2 = itsMsgList.begin();
        for (; it2 != itsMsgList.end(); ++it2)
        {
            iMessage *pMsg = *it2;

            if(pMsg->itsBuf)
                delete[] pMsg->itsBuf;
            delete pMsg;
        }
        itsMsgList.clear();
        itsMsgListLock.Unlock();


        itsHandlerMapLock.Lock();
        std::map<Key, IObjectCall *>::iterator it3 = itsHandlerMap.begin();
        for (; it3 != itsHandlerMap.end(); ++it3)
        {
            delete it3->second;
        }
        itsHandlerMap.clear();
        itsHandlerMapLock.Unlock();
    }

    //投递消息的接口
    void PostMessage(MSGHANDLE hMsg, uint32 nMsgNo, char *buf, int len)
    {
        iMessage *pMsg = new iMessage;
        pMsg->itsHandle = hMsg;
        pMsg->itsMsgNo = nMsgNo;
        pMsg->itsBuf = new uint8[len];
        memcpy(pMsg->itsBuf, buf, len);
        pMsg->itsLen = len;
        
        //printf("PostMessage:<0x%x,0x%x>\r\n",pMsg,pMsg->itsBuf);
        push(pMsg);
    }

    void push(iMessage *pMsg)
    {
        itsMsgListLock.Lock();
        itsMsgList.push_back(pMsg);
        itsMsgListLock.Unlock();

        itsThreadPoolLock.Lock();
        
        std::vector<iThreadEx *>::iterator it = itsThreadPool.begin();
        for (; it != itsThreadPool.end(); ++it)
        {
            (*it)->Resume();
        }
        
        itsThreadPoolLock.Unlock();
    }

    template<class TObj, class TDat> void RegisterMessage(uint32 MsgNo, TObj *pObj, void(TObj::* pFunc)(TDat *) )
    {
        IObjectCall *p = (IObjectCall *)new ObjectCallData<TObj, TDat>(pObj, pFunc);
        Key key((MSGHANDLE)pObj, MsgNo);

        iLifeSpanLock _lifelock(&itsHandlerMapLock);
        std::map<Key, IObjectCall *>::iterator it = itsHandlerMap.find(key);
        if (it != itsHandlerMap.end())
        {
            //通知上一个处理器,其被卸载了
            //it->second->OnUnreg();
        }
        itsHandlerMap[key] = p;
    }

    template<class TObj> void UnregisterMessage(uint32 MsgNo, TObj *pObj)
    {
		iLifeSpanLock _lifelock(&itsHandlerMapLock);
		std::map<Key, IObjectCall *>::iterator it = itsHandlerMap.find(Key((MSGHANDLE)pObj, MsgNo));
		if (it != itsHandlerMap.end())
		{
			delete it->second;
			itsHandlerMap.erase(it);
		}
    }

	void iMessageQ_Proc(void* lparam)
	{
		iThreadEx* pThread = (iThreadEx*)lparam;
		
		iMessage* pMsg = NULL;
		itsMsgListLock.Lock();
		if(itsMsgList.size() > 0)
		{
			pMsg = itsMsgList.front();
			itsMsgList.pop_front();
		}
		itsMsgListLock.Unlock();

		if(pMsg)
		{
			if(pMsg->itsHandle != 0)
			{
				IObjectCall* pObjCall = NULL;
				itsHandlerMapLock.Lock();
				std::map<Key,IObjectCall*>::iterator it = itsHandlerMap.find(Key(pMsg->itsHandle,pMsg->itsMsgNo));
				if (it != itsHandlerMap.end())
				{
					pObjCall = it->second;
				}
				itsHandlerMapLock.Unlock();

				if (pObjCall)
				{
					pObjCall->Execute(pMsg->itsBuf);
				}
			}else
			{
				itsHandlerMapLock.Lock();
				std::map<Key,IObjectCall*>::iterator it = itsHandlerMap.begin();
				for (;it != itsHandlerMap.end();++it)
				{
					const Key& rKey = it->first;
					if(pMsg->itsMsgNo == rKey.itsMsgNo)
					{
						PostMessage(rKey.itsHandler,rKey.itsMsgNo,(char*)pMsg->itsBuf,pMsg->itsLen);
					}
				}
				itsHandlerMapLock.Unlock();
			}
			
			//
			//printf("DeleteMessage:<0x%x,%d>\r\n",pMsg,pMsg->itsMsgNo);
			if(pMsg->itsBuf)
				delete []pMsg->itsBuf;
			
			delete pMsg;
		}
        else
        {
            pThread->Puase();
        }
    }
    
private:
    std::list<iMessage*> itsMsgList;
    iLock itsMsgListLock;

    std::map<Key, IObjectCall*> itsHandlerMap;
    iLock itsHandlerMapLock;

    std::vector<iThreadEx*> itsThreadPool;
    iLock itsThreadPoolLock;
};
#endif
