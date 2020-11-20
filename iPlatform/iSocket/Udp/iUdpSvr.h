/*********************************************
Author:         Tanglx
Date:           2019/08/31
FileName:       iUdpSvr.h
Description:    Udp����� ��֧��ipv4.
**********************************************/
#ifndef _FILENAME_IUDPSVR_
#define _FILENAME_IUDPSVR_

#include "../../iPlatform.h"

#if (ITS_PLATFORM == Windows)
#include "../Impl/_Win_Socket.h"
#elif (ITS_PLATFORM == Vxworks)
#include "../Impl/_Vx_Socket.h"
#else
#error "Can Not Find Adjust Platform"
#endif

#include <list>
#include "../../iThread/iThread.h"
#include "../../iLock/iLock.h"

#include "ioLib.h"

class iUdpService
{
    struct iSendBuf
    {
        char        *itsBuf;
        int32       itsLen;
        isockaddr   *itsDstAddr;

        iSendBuf(char *buf, int32 len, isockaddr *addr) : itsLen(len)
        {
            itsBuf = new char[len];
            memcpy(itsBuf, buf, len);
            
            itsDstAddr = new isockaddr;
            memcpy(itsDstAddr,addr,sizeof(isockaddr));
        }

        ~iSendBuf()
        {
            if (itsBuf)
            {
                delete []itsBuf;
            }
            
            if(itsDstAddr)
            {
            	delete itsDstAddr;
            }
        }
    };

    struct iRecvReflexBase
    {
        iRecvReflexBase() {}
        virtual ~iRecvReflexBase() {}
        
        virtual void Execute(isockaddr *, char *, uint32) = 0;
    };

    template <class T> struct RecvReflex : public iRecvReflexBase
    {
        RecvReflex(T *pObj, void(T::*pHandleRecvFunc)(isockaddr *, char *, uint32)) : itsObjPtr(pObj), itsFuncPtr(pHandleRecvFunc) {}
        virtual ~RecvReflex() {}

        virtual void Execute(isockaddr *peerAddr, char *buf, uint32 buflen)
        {
            ((*itsObjPtr).*itsFuncPtr)(peerAddr, buf, buflen);
        }
        
    private:
        T *itsObjPtr;
        void(T::* itsFuncPtr)(isockaddr *, char *, uint32);
    };

public:
    iUdpService()
    {
        itsSocket = NULL;
        itsRecvReflexPtr = NULL;
    }
    ~iUdpService() {}

    //���� szIp ����ָ������,���ڶ�����ʱ��Ч
    bool Create(uint16 nPort,const char* szIp = NULL)
    {
        //����udp�׽���
        itsSocket = _Impl_Socket::Create(_Impl_Socket::iAF_INET, _Impl_Socket::iSOCK_DGRAM, _Impl_Socket::iIPPROTO_UDP);
		if (itsSocket == _Impl_Socket::iInvalidSocket)
        {
            return false;
        }

        //�󶨶˿�
        isockaddr recvAddr;
        recvAddr.itsaddr.sin_family = _Impl_Socket::iAF_INET;
        recvAddr.itsaddr.sin_port = htons(nPort);
        if(szIp)
        {
        	recvAddr.itsaddr.sin_addr.s_addr = inet_addr(szIp);
        }else
        {
        	recvAddr.itsaddr.sin_addr.s_addr = htonl(_Impl_Socket::iADDR_ANY);
        }

        int32 nRet = _Impl_Socket::Bind(itsSocket, &recvAddr);
        if (nRet == ERROR)
        {
            _Impl_Socket::Close(itsSocket);
            return false;
        }

        //������������
        nRet = itsRecvThread.Create(this, &iUdpService::RecvProc, 512 * 1024, 60, "tUdpSvrRecv");
        if (nRet == 0)
        {
            _Impl_Socket::Close(itsSocket);
            return false;
        }

        //������������
        nRet = itsSendThread.Create(this, &iUdpService::SendProc, 512 * 1024, 60, "tUdpSvrSend");
        if (nRet == 0)
        {
			itsSendThread.Stop();
            _Impl_Socket::Close(itsSocket);
            return false;
        }

        return true;

    }

    void Destory()
    {
        //ֹͣ��������
        itsSendThread.Stop();

		//ֹͣ��������
		itsRecvThread.Stop();

        itsSendLstLock.Lock();
        
        std::list<iSendBuf *>::iterator it = itsSendLst.begin();
        for (; it != itsSendLst.end(); ++it)
        {
            delete (*it);
        }
        itsSendLst.clear();
        
        itsSendLstLock.Unlock();

        //�ر��׽���
        _Impl_Socket::Close(itsSocket);
    }

    template <class T> void OnRecvReflex(T *pObj, void(T::*pReflex)(isockaddr *, char *, uint32))
    {
        itsRecvReflexPtr = (iRecvReflexBase *)new RecvReflex<T>(pObj, pReflex);
    }

    void SendTo(char *buf, int32 len, isockaddr *addr)
    {
        //taskSuspend(taskIdSelf());
        
        iSendBuf *pSendBuf = new iSendBuf(buf, len, addr);
        itsSendLstLock.Lock();
        itsSendLst.push_back(pSendBuf);
        itsSendLstLock.Unlock();

        itsSendThread.Resume();
    }
    
private:
    void RecvProc()
    {
        char buf[64 * 1024] = {0};
        isockaddr peerAddr;
        int32 addrLen = sizeof(sockaddr_in);
        int32 nRecvLen = _Impl_Socket::RecvFrom(itsSocket, buf, 64 * 1024, 0, &peerAddr, &addrLen);
        if (nRecvLen > 0)
        {
            if (itsRecvReflexPtr)
                itsRecvReflexPtr->Execute(&peerAddr, buf, nRecvLen);
        }
    }

    void SendProc()
    {
        iSendBuf *pSendBuf = NULL;
        
        itsSendLstLock.Lock();
        
        if (itsSendLst.size() > 0)
        {
            pSendBuf = itsSendLst.front();
            itsSendLst.pop_front();
        }
        
        itsSendLstLock.Unlock();

        if(pSendBuf)
        {
            int32 nSendLen = _Impl_Socket::SendTo(itsSocket, pSendBuf->itsBuf, pSendBuf->itsLen, 0, pSendBuf->itsDstAddr);
            if (nSendLen != pSendBuf->itsLen)
            {
                //�����˴���
            	printf("/**************udp send error beg***********/\r\n");
            	printf("addr:%s|len:%d\r\n",inet_ntoa(pSendBuf->itsDstAddr->itsaddr.sin_addr),pSendBuf->itsLen);
            	printf("/**************udp send error end***********/\r\n");
            }

            delete pSendBuf;
        }
        else
        {
            itsSendThread.Pause();
        }
    }
    
private:
    SOCKET itsSocket;

    //�߳�
    iThread itsRecvThread;
    iThread itsSendThread;

    //���亯��ָ�룬���亯������ʵ�ʴ�����ձ��ĵĺ���
    iRecvReflexBase *itsRecvReflexPtr;

    //
    std::list<iSendBuf *> itsSendLst;
    iLock itsSendLstLock;
};

#endif
