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
		//创建用于连接的套接字
		SOCKET s = _Impl_Socket::Create(_Impl_Socket::iAF_INET,_Impl_Socket::iSOCK_STREAM,_Impl_Socket::iIPPROTO_TCP);
		if (s == _Impl_Socket::iInvalidSocket)
		{
			return NULL;
		}

		//组装地址
		isockaddr addr;
		addr.itsaddr.sin_family = _Impl_Socket::iAF_INET;
		addr.itsaddr.sin_port = htons(nPort);
		addr.itsaddr.sin_addr.s_addr = inet_addr(szIp);

		//进行连接
		uint32 nRet = _Impl_Socket::Connect(s,&addr);
		if (nRet == _Impl_Socket::iError)
		{
			_Impl_Socket::Close(s);
			return NULL;
		}
		
		//设置SOCKET的属性
		setsockopt(s,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		return s;
	}

	static iTcpPeer* BindPeer(SOCKET s)
	{
		//绑定到iTcpCore
		iTcpPeer* pPeer = new iTcpPeer;
		iTcpCore::GetInstance()->Bind(s,pPeer);
		return pPeer;
	}
	
	static void DestoryConn(iTcpPeer*& pPeer)
	{
		if (iTcpCore::GetInstance()->IsBinding(pPeer))
		{
			//解绑
			iTcpCore::GetInstance()->Unbind(pPeer->GetSocket());

			//断开和关闭
			_Impl_Socket::Shutdown(pPeer->GetSocket());
			_Impl_Socket::Close(pPeer->GetSocket());

			//删除
			delete pPeer;
			pPeer = NULL;
		}
	}
};

#endif
