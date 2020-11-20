/*********************************************
Author:Tanglx
Date:2019/11/20
FileName:iTcpCore.h
**********************************************/
#ifndef _FILENAME_ITCPCORE_
#define _FILENAME_ITCPCORE_

#include "../../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "../Impl/_Win_Socket.h"
#elif (ITS_PLATFORM == Vxworks)
#include "../Impl/_Vx_Socket.h"
#else
#error "Can Not Find Adjust Platform"
#endif

#include "../../../iTypes.h"
#include "../../iThread/iThread.h"
#include "../../iLock/iLock.h"
#include "../../iPublic/iPublic.h"
#include "../../../iPattern/iTrans.h"
#include "../../../iPattern/iSingleton.h"

#include <map>
#include <list>
#include "iTcpPeer.h"

class iTcpCore : public iSingleton<iTcpCore>
{
	struct SockBuf
	{
		SOCKET itsSocket;
		char* itsBuf;
		int32 itsLen;
		SockBuf()
		{
			memset(this,0,sizeof(SockBuf));
		}
	};
public:
	iTcpCore()
	{
		uint32 nRet = itsRecvThread.Create(this,&iTcpCore::Recv_Proc,256*1024,60,"Recv_Proc");
		if (nRet == NULL)
		{
			//return false;
		}

		nRet = itsSendThread.Create(this,&iTcpCore::Send_Proc,256*1024,60,"Send_Proc");
		if (nRet == NULL)
		{
			//return false;
		}
	}
	~iTcpCore()
	{
		//清空
		iLifeSpanLock _lk(&itsSocketMapLock);
		std::map<SOCKET,iTcpPeer*>::iterator it = itsSocketMap.begin();
		for (;it != itsSocketMap.end();++it)
		{
			_Impl_Socket::Shutdown(it->first);
			_Impl_Socket::Close(it->first);
		}
		itsSocketMap.clear();

		itsRecvThread.Stop();
		itsSendThread.Stop();

		//清空发送队列
		itsSendLstLock.Lock();
		std::list<SockBuf*>::iterator it2 = itsSendLst.begin();
		for (;it2 != itsSendLst.end();++it2)
		{
			delete *it2;
		}
		itsSendLst.clear();
		itsSendLstLock.Unlock();
	}

	void Bind(SOCKET s,iTcpPeer* pPeer)
	{
		//将SOCKET 和 Peer对象绑定
		pPeer->SetSocket(s);
		iLifeSpanLock _lk(&itsSocketMapLock);
		itsSocketMap[s] = pPeer;
	}

	void Unbind(SOCKET s)
	{
		//将SOCKET 和 Peer对象解绑定
		iLifeSpanLock _lk(&itsSocketMapLock);
		std::map<SOCKET,iTcpPeer*>::iterator it = itsSocketMap.find(s);
		if (it != itsSocketMap.end())
		{
			//从管理的Map中移除
			itsSocketMap.erase(it);
			//断开
			//_Impl_Socket::Shutdown(it->first);
			//关闭
			//_Impl_Socket::Close(it->first);
		}
	}

	bool IsBinding(iTcpPeer* pPeer)
	{
		if (pPeer && pPeer->GetSocket() != NULL)
		{
			iLifeSpanLock _lk(&itsSocketMapLock);
			std::map<SOCKET,iTcpPeer*>::iterator it = itsSocketMap.find(pPeer->GetSocket());
			if (it != itsSocketMap.end())
			{
				return true;
			}
		}

		return false;
	}

	void Send(SOCKET s,char* buf,int32 len)
	{
		SockBuf* p = new SockBuf;
		p->itsSocket = s;
		p->itsBuf = new char[len];
		memcpy(p->itsBuf,buf,len);
		p->itsLen = len;

		itsSendLstLock.Lock();
		itsSendLst.push_back(p);
		itsSendLstLock.Unlock();

		itsSendThread.Resume();
	}
private:
	void Recv_Proc()
	{
		ifd_set fdset;
		int32 max_fd = -1;
		iFD_ZERO(&fdset.itsv);

		itsSocketMapLock.Lock();
		std::map<SOCKET,iTcpPeer*> TempSocketMap = itsSocketMap;
		itsSocketMapLock.Unlock();

		std::map<SOCKET,iTcpPeer*>::iterator it = TempSocketMap.begin();
		for (;it != TempSocketMap.end();++it)
		{
			max_fd = max_fd>it->first?max_fd:it->first;
			iFD_SET(it->first,&fdset.itsv);
		}

		if (max_fd == -1)
		{
			iPublic::iSleep(100);
			return;
		}

		itimeval timeout;
		timeout.itsv.tv_sec = 5;
		timeout.itsv.tv_usec = 0;

		int ret = _Impl_Socket::Select(max_fd+1, &fdset, NULL, NULL, &timeout);
		if (ret > 0)
		{
			std::map<SOCKET,iTcpPeer*>::iterator it2 = TempSocketMap.begin();
			for (;it2 != TempSocketMap.end();++it2)
			{
				if (FD_ISSET(it2->first,&fdset.itsv))
				{
					char buf[4*1024] = {0};
					int32 len = _Impl_Socket::Recv(it2->first,buf,sizeof(buf));
					if (len > 0)
					{
						//接收数据
						//itsOnRecvPtr->Execute(it2->first,buf,len);
						it2->second->OnRecv(it2->first,buf,len);
					}
					else
					{
						{
							//断开,移除
							Unbind(it2->first);
						}
						//itsOnDisConnectPtr->Execute(it2->first);
						it2->second->OnDisConnect(it2->first);
					}
				}
			}
		}
	}

	void Send_Proc()
	{
		//这里可能存在性能瓶颈
		//因为所有的连接都使用一个任务来发送
		//这会导致,不同的套接字上无法并行发送
		SockBuf* p = NULL;

		itsSendLstLock.Lock();
		if (itsSendLst.size() > 0)
		{
			p = itsSendLst.front();
			itsSendLst.pop_front();
		}
		itsSendLstLock.Unlock();

		if(p)
		{
			int32 nNeedSendLen = p->itsLen;
			int32 nHadSendLen = 0;
			while(nNeedSendLen > 0)
			{
				int32 SendLen = _Impl_Socket::Send(p->itsSocket,p->itsBuf+ nHadSendLen,nNeedSendLen);
				if (SendLen == _Impl_Socket::iError)
				{
					if (p->itsBuf)
						delete []p->itsBuf;
					delete p;
					return;
				}
				nHadSendLen += SendLen;
				nNeedSendLen -= SendLen;
			}

			if (p->itsBuf)
				delete []p->itsBuf;
			delete p;
		}else
		{
			itsSendThread.Pause();
		}
	}
private:
	iThread itsRecvThread;
	iTrans3::Branch<SOCKET,char*,int32>*	itsOnRecvPtr;			//接收
	iTrans1::Branch<SOCKET>*				itsOnDisConnectPtr;		//断开

	iThread itsSendThread;
	std::list<SockBuf*> itsSendLst;		//发送队列
	iLock itsSendLstLock;

	std::map<SOCKET,iTcpPeer*> itsSocketMap;
	iLock itsSocketMapLock;
};

#endif
