/*********************************************
Author:Tanglx
Date:2019/11/20
FileName:iTcpServer.h
**********************************************/
#ifndef _FILENAME_ITCPSERVER_
#define _FILENAME_ITCPSERVER_

#include "iTcpCore.h"

class iTcpServer
{
public:
	iTcpServer()
	{
		itsListenSocket = NULL;
		itsOnConnectPtr = NULL;
	}
	~iTcpServer()
	{
		if (itsOnConnectPtr)
		{
			delete itsOnConnectPtr;
			itsOnConnectPtr = NULL;
		}
	}

	bool Create(const char* szIp,uint16 nPort)
	{
		//���������׽���
		itsListenSocket = _Impl_Socket::Create(_Impl_Socket::iAF_INET,_Impl_Socket::iSOCK_STREAM,_Impl_Socket::iIPPROTO_TCP);
		if (itsListenSocket == _Impl_Socket::iInvalidSocket)
		{
			return false;
		}

		//�󶨶˿�
		isockaddr recvAddr;
		recvAddr.itsaddr.sin_family = _Impl_Socket::iAF_INET;
		recvAddr.itsaddr.sin_port = htons(nPort);
		if(szIp)
		{
			recvAddr.itsaddr.sin_addr.s_addr = inet_addr(szIp);//htonl(inet_addr(szIp));
		}else
		{
			recvAddr.itsaddr.sin_addr.s_addr = htonl(_Impl_Socket::iADDR_ANY);
		}

		int32 nRet = _Impl_Socket::Bind(itsListenSocket, &recvAddr);
		if (nRet == _Impl_Socket::iError)
		{
			printf("bind %s:%d Failed\r\n",szIp,nPort);
			_Impl_Socket::Close(itsListenSocket);
			return false;
		}

		//��ʼ����
		nRet = _Impl_Socket::Listen(itsListenSocket,64);
		if (nRet == _Impl_Socket::iError)
		{
			_Impl_Socket::Close(itsListenSocket);
			return false;
		}

		//������������
		nRet = itsListenThread.Create(this,&iTcpServer::Accept_Proc,256*1024,60,"Accept_Proc");
		if (nRet == NULL)
		{
			printf("������������ʧ��\r\n");
			_Impl_Socket::Close(itsListenSocket);
			return false;
		}
		return true;
	}

	void Destory()
	{
		//�رռ����׽���
		_Impl_Socket::Close(itsListenSocket);

		//ֹͣ��������
		itsListenThread.Stop();

		//��ջص�
		if(itsOnConnectPtr)
			delete itsOnConnectPtr;
	}


	static iTcpPeer* InConnect(SOCKET s,iTcpPeer* pPeer)
	{
		iTcpCore::GetInstance()->Bind(s,pPeer);
		return pPeer;
	}
	
	static void DistoryConn(iTcpPeer*& pPeer)
	{
		if (iTcpCore::GetInstance()->IsBinding(pPeer))
		{
			//���
			iTcpCore::GetInstance()->Unbind(pPeer->GetSocket());

			//�Ͽ��͹ر�
			_Impl_Socket::Shutdown(pPeer->GetSocket());
			_Impl_Socket::Close(pPeer->GetSocket());

			//ɾ��
			delete pPeer;
			pPeer = NULL;
		}
	}


	//ע��
	template<class T>
	void RegOnConnect(T* pObj,void(T::* pFunc)(SOCKET,iTcpPeer*))
	{
		itsOnConnectPtr = new iTrans2::Leaf<T,SOCKET,iTcpPeer*>(pObj,pFunc);
	}
	
	SOCKET GetListenSocket()
	{
		return itsListenSocket;
	}
private:
	void Accept_Proc()
	{
		isockaddr addr;
		SOCKET s = _Impl_Socket::Accept(itsListenSocket,&addr);
		if (s==ERROR)
		{
			iPublic::iSleep(100);
			return;
		}
		
		//����SOCKET������
		setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));
		
		if (itsOnConnectPtr)
		{
			//
			iTcpPeer* pPeer = new iTcpPeer;
			itsOnConnectPtr->Execute(s,pPeer);
		}else
		{
			throw 0;
		}
	}
private:
	SOCKET itsListenSocket;		//�����׽���
	iThread itsListenThread;	//��������
	iTrans2::Branch<SOCKET,iTcpPeer*>*		itsOnConnectPtr;		//����
};

#endif
