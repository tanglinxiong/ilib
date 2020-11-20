/*********************************************
Author:Tanglx
Date:2020/10/16
FileName:iTcpServer2.h
**********************************************/
#ifndef _FILENAME_ITCPSERVER2_
#define _FILENAME_ITCPSERVER2_

#include "iTcpCore2.h"

class iTcpServer2
{
public:
	iTcpServer2(){
		itsListenSocket = 0;
		itsOnAcceptPtr = NULL;
	}
	~iTcpServer2(){
		if (itsOnAcceptPtr)
		{
			UnbindOnAccept();
			delete itsOnAcceptPtr;
		}
	}

	bool CreateServer(const char* szIp,unsigned short nPort)
	{
		itsListenSocket = iTcpCore2::GetInstance()->itsImpl.CreateTcpSvr(szIp,nPort);
		if (itsListenSocket)
		{
			return true;
		}
		else
			return false;
	}

	void DestoryServer()
	{
		UnbindOnAccept();
		iTcpCore2::GetInstance()->itsImpl.DestoryTcpSvr(itsListenSocket);
	}

	template<class T>
	void RegOnAccept(T* pObj,void(T::* pFunc)(int32,uint32))
	{
		if (itsOnAcceptPtr)
		{
			UnbindOnAccept();
			delete itsOnAcceptPtr;
		}
		itsOnAcceptPtr = new iTrans2::Leaf<T,int32,uint32>(pObj,pFunc);
		BindOnAccept();
	}
private:
	void BindOnAccept()
	{
		iTcpCore2::GetInstance()->itsImpl.RegOnListenAccept(itsListenSocket,itsOnAcceptPtr);
	}

	void UnbindOnAccept()
	{
		iTcpCore2::GetInstance()->itsImpl.UnregOnListenAccept(itsListenSocket);
	}

	int32 itsListenSocket;
	iTrans2::Branch<int32,uint32>* itsOnAcceptPtr;
};

#endif