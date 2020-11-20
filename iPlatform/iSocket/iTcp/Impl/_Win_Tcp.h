/*********************************************
Author:Tanglx
Date:2020/10/09
FileName:_Win_Tcp.h
Windows 下Tcp通讯,使用完成端口实现
**********************************************/
#ifndef _FILENAME__WIN_TCP_
#define _FILENAME__WIN_TCP_

//OS
#include <Winsock2.h>	//该头文件最好先于Windows.h包含,否则需要在预处理器中添加 WIN32_LEAN_AND_MEAN 宏
#pragma comment(lib,"Ws2_32.lib")
#include <MSWSock.h>

//ilib
#include "../../../iFrame/iMemCache/iMemCache.h"
#include "../../../iWindows/iIocp/iIocp.h"

//STL
#include <map>
#include <list>



template<int nMaxTransLen>
class _Impl_iTcp
{
public:
	_Impl_iTcp(){
	}
	~_Impl_iTcp(){
	}

	//
	struct PerSocketHandleData : public iIocp::PerHandleData
	{
		enum
		{
			Type_Listen,		//监听的套接字
			Type_Server,		//服务端通过Accept或者AcceptEx建立的套接字
			Type_Client,		//客户端通过connect建立的套接字
		};
		int Type;	//类型
		sockaddr_in local_addr;		//本地地址
		sockaddr_in remote_addr;		//远程地址	,如果是监听句柄,则该字段无效

		iTrans2::Branch<char*,int32>* itsOnRecvPtr;	//在该单句柄数据不为监听类型时候有效
		iTrans0::Branch* itsOnDisConnectPtr;		//在该单句柄数据不为监听类型时候有效

		iTrans2::Branch<int32,uint32>* itsOnAcceptPtr;		//仅在该单句柄数据为监听类型时候有效
		/***Tcp Server相关***/
		LPFN_ACCEPTEX itsAcceptEx;//AcceptEx指针 仅在该单句柄数据为监听类型时候有效
		LPFN_GETACCEPTEXSOCKADDRS itsGetAcceptSockAddr;//获取AcceptEx完成时候的获得的地址 仅在该单句柄数据为监听类型时候有效

		PerSocketHandleData(){
			memset(this,0,sizeof(PerSocketHandleData));
		}
		~PerSocketHandleData(){
		}
	};

	struct PerSocketIoData : public iIocp::PerIoData
	{
		SOCKET AcceptSocket;//该参数仅仅在OnAccept时候有效
		DWORD Flag;
		WSABUF WsaBuf;
		PerSocketIoData(){
			ZeroMemory(this,sizeof(PerSocketIoData));
			WsaBuf.buf=_buf_;
			WsaBuf.len=nMaxTransLen;
		}
	private:
		char _buf_[nMaxTransLen];
	};

	enum
	{
		OP_UNKNOW,
		OP_SOCKET_ACCEPT,
		OP_SOCKET_SEND,
		OP_SOCKET_RECV,
	};

	//创建Tcp Core,必须要调用
	bool CreateCore(int32 nWorkerNum = 0)
	{
		itsCache.CreateMem(100);
		bool bRet = itsIocp.CreateIocp();
		if (!bRet)
		{
			itsCache.DestoryMem();
			return false;
		}

		//创建工作线程
		SYSTEM_INFO si;
		GetSystemInfo(&si);
		if (nWorkerNum <= 0)
			nWorkerNum = si.dwNumberOfProcessors*2;


		for (int i=0;i<nWorkerNum;++i)
		{
			iThreadEx* pThread = new iThreadEx;
			if (pThread->Create(this,&_Impl_iTcp::Worker,64*1024,60,"Win_Serial_Core",pThread))
			{
				itsThreadList.push_back(pThread);
			}
		}

		if (itsThreadList.size() == 0)
		{
			itsIocp.DestoryIocp();
			itsCache.DestoryMem();
			return false;
		}

		return bRet;
	}

	//销毁Tcp核,在退出前调用
	void DestoryCore()
	{
		//投递信息,让Worker挂起
		for (uint32 i=0;i<itsThreadList.size();++i)
		{
			itsIocp.PostIocpNotify(0,0,0);
		}

		//将挂起的Worker线程Stop
		std::list<iThreadEx *>::iterator it = itsThreadList.begin();
		for (;it != itsThreadList.end();++it)
		{
			(*it)->Stop();
		}

		//释放单句柄数据,以及关闭句柄
		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSocketHandleData *>::iterator it2 = itsPerHandleDataMap.begin();
		for (;it2 != itsPerHandleDataMap.end();++it2)
		{
			delete it2->second;
			::CloseHandle((HANDLE)it2->first);
			//printf("DestoryCore : 关闭句柄0x%x \r\n",it2->first);
		}
		itsPerHandleDataMap.clear();
		itsPerHandleDataMapLock.Unlock();

		//释放Cache缓存
		itsCache.DestoryMem();

		//关闭Iocp
		itsIocp.DestoryIocp();
	}

	//创建Svr,如果需要使用Svr功能,则需要调用
	int32 CreateTcpSvr(const char* szIp,unsigned short ListenPort,int32 nPrePostAcceptSize = 100)
	{
		//创建监听端口
		WSADATA wsadata;
		if (WSAStartup(MAKEWORD(2,2), &wsadata) != 0)
			return 0;

		sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(ListenPort);
		if(szIp)
		{
			server_addr.sin_addr.S_un.S_addr = inet_addr(szIp);//htonl(inet_addr(szIp));
		}else
		{
			server_addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
		}

		SOCKET nListenSocket = ::WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,WSA_FLAG_OVERLAPPED);
		if (nListenSocket == NULL || nListenSocket == INVALID_SOCKET)
			return 0;

		setsockopt(nListenSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		//绑定到完成端口
		PerSocketHandleData* pPerListen=new PerSocketHandleData;
		pPerListen->Handle=(HANDLE)nListenSocket;
		pPerListen->Type = PerSocketHandleData::Type_Listen;
		memcpy(&pPerListen->local_addr,&server_addr,sizeof(sockaddr_in));

		if (!itsIocp.BindWithIocp(pPerListen->Handle,pPerListen))
			return 0;

		//绑定地址&开始监听
		if (SOCKET_ERROR == ::bind(nListenSocket,(sockaddr*)&server_addr,sizeof(sockaddr_in)))
			return 0;
		if (SOCKET_ERROR == ::listen(nListenSocket,SOMAXCONN))
			return 0;

		_GetAcceptExPtr(nListenSocket,&pPerListen->itsAcceptEx);
		_GetGetAcceotExAddrPtr(nListenSocket,&pPerListen->itsGetAcceptSockAddr);

		for (int i=0;i<nPrePostAcceptSize;i++)
		{
			_PostAccept(nListenSocket,pPerListen->itsAcceptEx);
		}

		itsPerHandleDataMapLock.Lock();
		itsPerHandleDataMap[(int32)pPerListen->Handle] = pPerListen;
		itsPerHandleDataMapLock.Unlock();

		return (int32)nListenSocket;
	}

	//销毁Svr,如果调用了CreateIocpSvr,则需要在退出前调用
	void DestoryTcpSvr(int32 nListenSocket)
	{
		//关闭监听Socket
		::closesocket(nListenSocket);
	}

	//Client使用
	int32 Connect(const char* szIp,unsigned short nPort,uint32 nFlag)
	{
		sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(nPort);
		server_addr.sin_addr.S_un.S_addr = inet_addr(szIp);

		//创建重叠Socket
		PerSocketHandleData* pPerSocket = new PerSocketHandleData;
		pPerSocket->Type = PerSocketHandleData::Type_Client;
		pPerSocket->Handle = (HANDLE)::WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,WSA_FLAG_OVERLAPPED);
		if (pPerSocket->Handle == NULL || (SOCKET)pPerSocket->Handle == INVALID_SOCKET)
		{
			printf("创建重叠Socket失败\n");
			delete pPerSocket;
			return NULL;
		}

		memcpy(&pPerSocket->remote_addr,&server_addr,sizeof(sockaddr_in));
		setsockopt((SOCKET)pPerSocket->Handle,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		//链接服务端
		if (SOCKET_ERROR == ::connect((SOCKET)pPerSocket->Handle,(sockaddr*)&server_addr,sizeof(sockaddr_in)))
		{
			printf("链接服务端:<%s,%u,%d>失败\n",szIp,nPort,WSAGetLastError());
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//获取本地地址
		int local_addr_size = sizeof(sockaddr_in);
		getsockname((SOCKET)pPerSocket->Handle,(sockaddr*)&pPerSocket->local_addr,&local_addr_size);

		//绑定完成端口
		if (!itsIocp.BindWithIocp(pPerSocket->Handle,pPerSocket))
		{
			printf("绑定完成端口失败\n");
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//发送一个初次连接上的标识
		if (!_PostSend((SOCKET)pPerSocket->Handle,(char*)&nFlag,sizeof(nFlag)))
		{
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//投递第一个PostRecv
		if (!_PostRecv((SOCKET)pPerSocket->Handle))
		{
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		itsPerHandleDataMapLock.Lock();
		itsPerHandleDataMap[(int32)pPerSocket->Handle] = pPerSocket;
		itsPerHandleDataMapLock.Unlock();
		return (int32)pPerSocket->Handle;
	}

	void DisConnect(int32 nSocket)
	{
		//::shutdown(nSocket,SD_BOTH);
		//::closesocket(nSocket);
		_PostSend(nSocket,NULL,0);
	}

	bool PostSend(int32 nSocket,char* buf,int32 len)
	{
		return _PostSend(nSocket,buf,len);
	}


	//下沉设计接口
	template<class T>
	void RegOnPerSocketRecv(int32 nSocket,T* pObj,void(T::* pFunc)(char*,int32))
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			if (pPerSocketData->itsOnRecvPtr)
				delete pPerSocketData->itsOnRecvPtr;

			pPerSocketData->itsOnRecvPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
		}
	}

	void RegOnListenAccept(int32 ListenSocket,iTrans2::Branch<int32,uint32>* OnAcceptPtr)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(ListenSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			if (pPerSocketData->Type != PerSocketHandleData::Type_Listen)
				throw "RegOnListenAccept PerSocketData Type Error";
			pPerSocketData->itsOnAcceptPtr = OnAcceptPtr;
		}
	}

	void UnregOnListenAccept(int32 ListenSocket)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(ListenSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			pPerSocketData->itsOnAcceptPtr = NULL;
		}
	}

	void RegOnPerSocketRecv(int32 nSocket,iTrans2::Branch<char*,int32>* OnRecvPtr)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			pPerSocketData->itsOnRecvPtr = OnRecvPtr;
		}
	}

	void UnregOnPerSocketRecv(int32 nSocket)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			pPerSocketData->itsOnRecvPtr = NULL;
		}
	}

	void RegOnPerSocketDisConnect(int32 nSocket,iTrans0::Branch* OnDisconnectPtr)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			pPerSocketData->itsOnDisConnectPtr = OnDisconnectPtr;
		}
	}

	void UnregOnPerSocketDisConnect(int32 nSocket)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			pPerSocketData->itsOnDisConnectPtr = NULL;
		}
	}
private:
	BOOL _PostAccept(int32 nListenSocket,LPFN_ACCEPTEX AcceptExPtr,PerSocketIoData* pIoBuf = NULL)
	{
		SOCKET AcceptSocket = ::WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,WSA_FLAG_OVERLAPPED);
		if (AcceptSocket == NULL||AcceptSocket == INVALID_SOCKET)
			return FALSE;

		//setsockopt(AcceptScok,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		PerSocketIoData* pPerIoData = NULL;
		if (!pIoBuf)
		{
			//printf("_PostAccept:");
			pPerIoData = itsCache.AllocMem();
		}else
		{
			pPerIoData = pIoBuf;
		}
		new(pPerIoData) PerSocketIoData();
		pPerIoData->OpType = OP_SOCKET_ACCEPT;
		pPerIoData->AcceptSocket = AcceptSocket;

		DWORD addrlen = sizeof(sockaddr_in)+16;
		BOOL bRet = 
			AcceptExPtr(
			nListenSocket,
			AcceptSocket,
			pPerIoData->WsaBuf.buf,
			sizeof(uint32),
			sizeof(sockaddr_in)+16,
			sizeof(sockaddr_in)+16,
			&pPerIoData->WsaBuf.len,
			&pPerIoData->OverLapped
			);

		if (!bRet)
		{
			DWORD ErrCode = WSAGetLastError();
			if (ErrCode != ERROR_IO_PENDING)
			{
				if (!pIoBuf)
					itsCache.FreeMem(pPerIoData);
				closesocket(AcceptSocket);
				return bRet;
			}
			else
			{
				bRet = TRUE;
			}
		}

		return bRet;
	}

	BOOL _PostRecv( SOCKET AcceptSocket,PerSocketIoData* pIoBuf = NULL)
	{
		if (!AcceptSocket || AcceptSocket == INVALID_SOCKET)
		{
			return FALSE;
		}

		PerSocketIoData* pPerIoData = NULL;
		if (!pIoBuf)
		{
			//printf("_PostRecv:");
			pPerIoData = itsCache.AllocMem();
		}else
		{
			pPerIoData = pIoBuf;
		}
		new(pPerIoData) PerSocketIoData();
		pPerIoData->OpType = OP_SOCKET_RECV;
		pPerIoData->Flag = 0;
		int nRet = WSARecv(
			AcceptSocket,
			&pPerIoData->WsaBuf,
			1,
			NULL,
			&pPerIoData->Flag,
			&pPerIoData->OverLapped,
			NULL);
		if (nRet != 0)
		{
			nRet = WSAGetLastError();
			if (nRet != WSA_IO_PENDING)
			{
				if (!pIoBuf)
					itsCache.FreeMem(pPerIoData);
				return FALSE;
			}
		}
		return TRUE;
	}


	bool _PostSend( SOCKET AcceptSocket,const char* buf,int buflen )
	{
		if (!AcceptSocket || AcceptSocket == INVALID_SOCKET)
			return false;
		if (buflen>nMaxTransLen)
		{
			printf("长度：%d，超过了最大值%d\r\n",buflen,nMaxTransLen);
			return false;
		}

		//printf("_PostSend:");
		PerSocketIoData* pPerIoData = new(itsCache.AllocMem()) PerSocketIoData();
		pPerIoData->OpType = OP_SOCKET_SEND;
		memcpy(pPerIoData->WsaBuf.buf,buf,buflen);
		pPerIoData->WsaBuf.len = buflen;

		int nRet = WSASend(
			AcceptSocket,
			&pPerIoData->WsaBuf,
			1,
			NULL,
			0,
			&pPerIoData->OverLapped,
			NULL
			);
		if (nRet != 0)
		{
			nRet = WSAGetLastError();
			if (nRet != WSA_IO_PENDING)
			{
				itsCache.FreeMem(pPerIoData);
				return false;
			}
		}
		return true;
	}

	//断开的一些处理
	//nHandle	:断开的句柄
	//nCode		:断开的原因
	void _DoDisConnect(PerSocketHandleData* pPerHandleData,int32 nCode)
	{
		int32 nSocket = (int32)pPerHandleData->Handle;

		//OnDisConnect
		if (pPerHandleData->itsOnDisConnectPtr)
			pPerHandleData->itsOnDisConnectPtr->Execute();

		memset(pPerHandleData,0,sizeof(PerSocketHandleData));

		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			delete it->second;
			itsPerHandleDataMap.erase(it);
		}
		itsPerHandleDataMapLock.Unlock();

		::shutdown((SOCKET)nSocket,SD_BOTH);
		::closesocket((SOCKET)nSocket);
	}


	void Worker(void* lpParam)
	{
		iThreadEx* pThread = (iThreadEx*)lpParam;

		DWORD nCompletionBytes = 0;
		void* pContext = NULL;
		LPOVERLAPPED pOverLapped = NULL;
		BOOL bRet = itsIocp.GetIocpNotify(&nCompletionBytes,(PULONG_PTR)&pContext,&pOverLapped,INFINITE);

		PerSocketHandleData* pPerHandleData = (PerSocketHandleData*)pContext;
		PerSocketIoData* pPerIoData = CONTAINING_RECORD(pOverLapped,PerSocketIoData,OverLapped);

		printf("nRet:%d,PerSocketHandleData:0x%x,PerSocketIoData:0x%x,nBytes:%d\n",bRet,pPerHandleData,pPerIoData,nCompletionBytes);

		if (!bRet)
		{
			if (pPerIoData)
			{
				//出现了错误,需要将该PerSocketIoData释放
				//printf("Worker Error\n");
				itsCache.FreeMem(pPerIoData);
			}

			//删除该单句柄数据
			if (pPerHandleData)
			{
				itsPerHandleDataMapLock.Lock();
				std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find((int32)pPerHandleData->Handle);
				if (it != itsPerHandleDataMap.end())
				{
					delete pPerHandleData;
					itsPerHandleDataMap.erase(it);
				}
				itsPerHandleDataMapLock.Unlock();
			}
			return ;
		}

		if (nCompletionBytes == 0 && pContext == NULL && pOverLapped == NULL)
		{
			//退出完成端口,需要关闭线程
			//这里将任务挂起,等待退出
			pThread->Puase();
			return;
		}


		switch(pPerIoData->OpType)
		{
		case OP_SOCKET_ACCEPT:
			{
				int LocalAddrLen = 0;
				int RemoteAddrLen = 0;
				PerSocketHandleData* pNewPerSocket = new PerSocketHandleData;
				pNewPerSocket->Handle = (HANDLE)pPerIoData->AcceptSocket;
				pNewPerSocket->Type = PerSocketHandleData::Type_Server;

				sockaddr_in* pLocalAddr = NULL;
				sockaddr_in* pRemoteAddr = NULL;
				pPerHandleData->itsGetAcceptSockAddr(
					pPerIoData->WsaBuf.buf,
					sizeof(uint32),
					(sizeof(sockaddr_in))+16,
					(sizeof(sockaddr_in))+16,
					(sockaddr**)&pLocalAddr,
					&LocalAddrLen,
					(sockaddr**)&pRemoteAddr,
					&RemoteAddrLen
					);

				//将Flag保存
				uint32 Flag;
				memcpy(&Flag,pPerIoData->WsaBuf.buf,sizeof(uint32));

				memcpy(&(pNewPerSocket->local_addr),pLocalAddr,sizeof(sockaddr_in));
				memcpy(&(pNewPerSocket->remote_addr),pRemoteAddr,sizeof(sockaddr_in));
				itsIocp.BindWithIocp(pNewPerSocket->Handle,pNewPerSocket);
				if (!_PostAccept((int32)pPerHandleData->Handle,pPerHandleData->itsAcceptEx,pPerIoData))
				{
					itsCache.FreeMem(pPerIoData);
				}
				if (!_PostRecv((SOCKET)pNewPerSocket->Handle))
				{
					::shutdown((SOCKET)pNewPerSocket->Handle,SD_BOTH);
					::closesocket((SOCKET)pNewPerSocket->Handle);
					delete pNewPerSocket;
					break;
				}

				
				//
				itsPerHandleDataMapLock.Lock();
				itsPerHandleDataMap[(int32)pNewPerSocket->Handle] = pNewPerSocket;
				itsPerHandleDataMapLock.Unlock();

				if (pPerHandleData->Type != PerSocketHandleData::Type_Listen)
					throw "OP_SOCKET_ACCEPT PerSocketData Type Error";

				if (pPerHandleData->itsOnAcceptPtr)
					pPerHandleData->itsOnAcceptPtr->Execute((int32)pNewPerSocket->Handle,Flag);
			}break;
		case OP_SOCKET_SEND:
			{
				if (nCompletionBytes == 0)
				{//断开
					_DoDisConnect(pPerHandleData,0);
					itsCache.FreeMem(pPerIoData);
				}else
				{//检查是否发送完成
					if (nCompletionBytes == pPerIoData->WsaBuf.len)
					{//发送完成,删除发送单IO数据
						itsCache.FreeMem(pPerIoData);
					}
					else if (nCompletionBytes < pPerIoData->WsaBuf.len)
					{//发送未完成,继续投递Send
						pPerIoData->WsaBuf.buf += nCompletionBytes;
						pPerIoData->WsaBuf.len -= nCompletionBytes;
						if (!_PostSend((SOCKET)pPerHandleData->Handle,pPerIoData->WsaBuf.buf,pPerIoData->WsaBuf.len))
						{
							_DoDisConnect(pPerHandleData,0);
							itsCache.FreeMem(pPerIoData);
							delete pPerHandleData;
						}
					}
				}
			}break;
		case OP_SOCKET_RECV:
			{
				if (nCompletionBytes == 0)
				{//断开
					_DoDisConnect(pPerHandleData,0);
					itsCache.FreeMem(pPerIoData);
				}
				else
				{
					//
					if (pPerHandleData->itsOnRecvPtr)
					{
						//存在下沉的指针,则按照指针调用
						pPerHandleData->itsOnRecvPtr->Execute(pPerIoData->WsaBuf.buf,nCompletionBytes);
					}
					if (!_PostRecv((SOCKET)pPerHandleData->Handle,pPerIoData))
					{
						_DoDisConnect(pPerHandleData,0);
						itsCache.FreeMem(pPerIoData);
					}
				}
			}break;
		default:
			break;
		}
	}
private:
	void _GetAcceptExPtr(SOCKET nListenSocket,LPFN_ACCEPTEX* pAcceptExPtr)
	{
		//Get AcceptEx Function Ptr
		GUID GuidAcceptEx = WSAID_ACCEPTEX;
		DWORD dwBytes = 0;



		WSAIoctl(
			nListenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx,
			sizeof(GuidAcceptEx),
			pAcceptExPtr,
			sizeof(*pAcceptExPtr),
			&dwBytes,
			NULL,
			NULL);
	}
	void _GetGetAcceotExAddrPtr(SOCKET nListenSocket,LPFN_GETACCEPTEXSOCKADDRS* pGetAcceptExSockAddrPtr)
	{
		//Get AcceptEx Function Ptr
		GUID GuidAcceptEx = WSAID_GETACCEPTEXSOCKADDRS;
		DWORD dwBytes = 0;

		WSAIoctl(
			nListenSocket,
			SIO_GET_EXTENSION_FUNCTION_POINTER,
			&GuidAcceptEx,
			sizeof(GuidAcceptEx),
			pGetAcceptExSockAddrPtr,
			sizeof(*pGetAcceptExSockAddrPtr),
			&dwBytes,
			NULL,
			NULL);
	}


private:
	/***完成端口相关***/
	iIocp itsIocp;
	iMemCache<PerSocketIoData> itsCache;
	std::map<int32,PerSocketHandleData*> itsPerHandleDataMap;
	iLock itsPerHandleDataMapLock;

	//线程链
	std::list<iThreadEx*> itsThreadList;
	iLock itsThreadListLock;
};

#endif