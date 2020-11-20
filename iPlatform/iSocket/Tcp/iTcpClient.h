/*********************************************
Author:Tanglx
Date:2019/11/20
FileName:iTcpClient.h

iTcpPeer Builder For Client 

**********************************************/
#ifndef _FILENAME_ITCPCLIENT_
#define _FILENAME_ITCPCLIENT_

#include "iTcpCore.h"

class iTcpClient
{
public:
	static SOCKET Connect(const char* szIp,uint16 nPort)
	{
		//�����������ӵ��׽���
		SOCKET s = _Impl_Socket::Create(_Impl_Socket::iAF_INET,_Impl_Socket::iSOCK_STREAM,_Impl_Socket::iIPPROTO_TCP);
		if (s == _Impl_Socket::iInvalidSocket)
		{
			return NULL;
		}

		//��װ��ַ
		isockaddr addr;
		addr.itsaddr.sin_family = _Impl_Socket::iAF_INET;
		addr.itsaddr.sin_port = htons(nPort);
		addr.itsaddr.sin_addr.s_addr = inet_addr(szIp);

		//��������
		uint32 nRet = _Impl_Socket::Connect(s,&addr);
		if (nRet == _Impl_Socket::iError)
		{
			_Impl_Socket::Close(s);
			return NULL;
		}
		
		//����SOCKET������
		setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		return s;
	}

	static iTcpPeer* BindPeer(SOCKET s)
	{
		//�󶨵�iTcpCore
		iTcpPeer* pPeer = new iTcpPeer;
		iTcpCore::GetInstance()->Bind(s,pPeer);
		return pPeer;
	}
	
	static void DestoryConn(iTcpPeer*& pPeer)
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
};

#endif
