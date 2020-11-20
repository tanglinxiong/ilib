/*********************************************
Author:Tanglx
Date:2019/11/20
FileName:iTcpPeer.h

//֧�ּ̳л���������ַ�ʽ

**********************************************/
#ifndef _FILENAME_ITCPPEER_
#define _FILENAME_ITCPPEER_

#include "../../iPlatform.h"
#if (ITS_PLATFORM == Windows)
#include "../Impl/_Win_Socket.h"
#elif (ITS_PLATFORM == Vxworks)
#include "../Impl/_Vx_Socket.h"
#else
#error "Can Not Find Adjust Platform"
#endif

#include "../../../iPattern/iTrans.h"

class iTcpPeer
{
	friend class iTcpClient;
	friend class iTcpServer;
	friend class iTcpCore;
private:
	iTcpPeer()
	{
		itsOnRecvPtr = NULL;
		itsOnDisConnectPtr = NULL;
	}
	~iTcpPeer()
	{
		//����ʱ�����
		if (itsOnRecvPtr)
		{
			delete itsOnRecvPtr;
			itsOnRecvPtr = NULL;
		}

		if (itsOnDisConnectPtr)
		{
			delete itsOnDisConnectPtr;
			itsOnDisConnectPtr = NULL;
		}
	}

public:
	SOCKET GetSocket()
	{
		return itsSocket;
	}

	void SetSocket(SOCKET s)
	{
		itsSocket = s;
	}
	
	bool IsConnected()
	{
		
	}
	
	void DisConnect()
	{
		
	}
private:

	//�ص�
	virtual void OnRecv(SOCKET s,char* buf,int32 len)
	{
		if (itsOnRecvPtr)
		{
			itsOnRecvPtr->Execute(s,buf,len);
		}
	}

	virtual void OnDisConnect(SOCKET s)
	{
		if(itsOnDisConnectPtr)
		{
			itsOnDisConnectPtr->Execute(s);
		}
	}

public:
	//�ص�ע��
	template<class T>
	void RegOnRecv(T* pObj,void(T::* pFunc)(SOCKET,char*,int32))
	{
		itsOnRecvPtr = new iTrans3::Leaf<T,SOCKET,char*,int32>(pObj,pFunc);
	}

	template<class T>
	void RegOnDisConnect(T* pObj,void(T::* pFunc)(SOCKET))
	{
		itsOnDisConnectPtr = new iTrans1::Leaf<T,SOCKET>(pObj,pFunc);
	}
	
private:
	iTrans3::Branch<SOCKET,char*,int32>* itsOnRecvPtr;
	iTrans1::Branch<SOCKET>* itsOnDisConnectPtr;
	SOCKET itsSocket;
}; 

#endif
