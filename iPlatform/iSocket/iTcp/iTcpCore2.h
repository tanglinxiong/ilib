/*********************************************
Author:Tanglx
Date:2020/10/12
FileName:iTcpCore2.h

该头文件需要和iTcpPeer2.h一起使用
**********************************************/
#ifndef _FILENAME_ITCPCORE2_
#define _FILENAME_ITCPCORE2_

#include "../../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Tcp.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Tcp.h"
#else
#error "Can Not Find Adjust Platform"
#endif
#include "../../../iTypes.h"
#include "../../../iPattern/iSingleton.h"

#include "iTcpPeer2.h"

class iTcpCore2 : public iSingleton<iTcpCore2>
{
	friend class iTcpServer2;
public:
	iTcpCore2(int WorkNum = 0)
	{
		if(!itsImpl.CreateCore(WorkNum))
			throw "iTcpCore Construct Error!";
	}
	~iTcpCore2(){
		itsImpl.DestoryCore();
	}

	int32 Connect(const char* ip,unsigned short port,uint32 Flag)
	{
		return itsImpl.Connect(ip,port,Flag);
	}

	void DisConnect(int32 nSocket)
	{
		return itsImpl.DisConnect(nSocket);
	}

	bool PostSend(int32 nSocket,char* buf,int32 len)
	{
		return itsImpl.PostSend(nSocket,buf,len);
	}

	bool Bind(int32 nSocket,iTcpPeer2* pPeer)
	{
		if(nSocket == NULL)
			return false;
			
		//Bind之后,必须进行了UnBind才能对pPeer进行Delete,否则可能导致程序崩溃
		pPeer->itsFd = nSocket;
		pPeer->RegPostSend(this,&iTcpCore2::PostSend);

		itsImpl.RegOnPerSocketRecv(nSocket,pPeer->itsOnRecvPtr);
		itsImpl.RegOnPerSocketDisConnect(nSocket,pPeer->itsOnDisConnectPtr);

		itsBindPeerMapLock.Lock();
		itsBindPeerMap[nSocket] = pPeer;
		itsBindPeerMapLock.Unlock();
		
		return true;
	}

	void UnBind(int32 nSocket)
	{
		//Bind之后,必须进行了UnBind才能对pPeer进行Delete,否则可能导致程序崩溃
		iLifeSpanLock _lk(&itsBindPeerMapLock);
		std::map<int32, iTcpPeer2*>::iterator it = itsBindPeerMap.find(nSocket);
		if (it != itsBindPeerMap.end())
		{
			iTcpPeer2* pPeer = it->second;
			pPeer->ClearPostSendPtr();
			itsBindPeerMap.erase(it);
		}

		itsImpl.UnregOnPerSocketRecv(nSocket);
		itsImpl.UnregOnPerSocketDisConnect(nSocket);
	}
private:
	_Impl_iTcp<4096> itsImpl;

	//绑定表
	std::map<int32,iTcpPeer2*> itsBindPeerMap;
	iLock itsBindPeerMapLock;
};
#endif
