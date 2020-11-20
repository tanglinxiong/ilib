/*********************************************
Author:Tanglx
Date:2020/10/09
FileName:_Win_Tcp.h
Windows ��TcpͨѶ,ʹ����ɶ˿�ʵ��
**********************************************/
#ifndef _FILENAME__WIN_TCP_
#define _FILENAME__WIN_TCP_

//OS
#include <Winsock2.h>	//��ͷ�ļ��������Windows.h����,������Ҫ��Ԥ����������� WIN32_LEAN_AND_MEAN ��
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
			Type_Listen,		//�������׽���
			Type_Server,		//�����ͨ��Accept����AcceptEx�������׽���
			Type_Client,		//�ͻ���ͨ��connect�������׽���
		};
		int Type;	//����
		sockaddr_in local_addr;		//���ص�ַ
		sockaddr_in remote_addr;		//Զ�̵�ַ	,����Ǽ������,����ֶ���Ч

		iTrans2::Branch<char*,int32>* itsOnRecvPtr;	//�ڸõ�������ݲ�Ϊ��������ʱ����Ч
		iTrans0::Branch* itsOnDisConnectPtr;		//�ڸõ�������ݲ�Ϊ��������ʱ����Ч

		iTrans2::Branch<int32,uint32>* itsOnAcceptPtr;		//���ڸõ��������Ϊ��������ʱ����Ч
		/***Tcp Server���***/
		LPFN_ACCEPTEX itsAcceptEx;//AcceptExָ�� ���ڸõ��������Ϊ��������ʱ����Ч
		LPFN_GETACCEPTEXSOCKADDRS itsGetAcceptSockAddr;//��ȡAcceptEx���ʱ��Ļ�õĵ�ַ ���ڸõ��������Ϊ��������ʱ����Ч

		PerSocketHandleData(){
			memset(this,0,sizeof(PerSocketHandleData));
		}
		~PerSocketHandleData(){
		}
	};

	struct PerSocketIoData : public iIocp::PerIoData
	{
		SOCKET AcceptSocket;//�ò���������OnAcceptʱ����Ч
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

	//����Tcp Core,����Ҫ����
	bool CreateCore(int32 nWorkerNum = 0)
	{
		itsCache.CreateMem(100);
		bool bRet = itsIocp.CreateIocp();
		if (!bRet)
		{
			itsCache.DestoryMem();
			return false;
		}

		//���������߳�
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

	//����Tcp��,���˳�ǰ����
	void DestoryCore()
	{
		//Ͷ����Ϣ,��Worker����
		for (uint32 i=0;i<itsThreadList.size();++i)
		{
			itsIocp.PostIocpNotify(0,0,0);
		}

		//�������Worker�߳�Stop
		std::list<iThreadEx *>::iterator it = itsThreadList.begin();
		for (;it != itsThreadList.end();++it)
		{
			(*it)->Stop();
		}

		//�ͷŵ��������,�Լ��رվ��
		itsPerHandleDataMapLock.Lock();
		std::map<int32, PerSocketHandleData *>::iterator it2 = itsPerHandleDataMap.begin();
		for (;it2 != itsPerHandleDataMap.end();++it2)
		{
			delete it2->second;
			::CloseHandle((HANDLE)it2->first);
			//printf("DestoryCore : �رվ��0x%x \r\n",it2->first);
		}
		itsPerHandleDataMap.clear();
		itsPerHandleDataMapLock.Unlock();

		//�ͷ�Cache����
		itsCache.DestoryMem();

		//�ر�Iocp
		itsIocp.DestoryIocp();
	}

	//����Svr,�����Ҫʹ��Svr����,����Ҫ����
	int32 CreateTcpSvr(const char* szIp,unsigned short ListenPort,int32 nPrePostAcceptSize = 100)
	{
		//���������˿�
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

		//�󶨵���ɶ˿�
		PerSocketHandleData* pPerListen=new PerSocketHandleData;
		pPerListen->Handle=(HANDLE)nListenSocket;
		pPerListen->Type = PerSocketHandleData::Type_Listen;
		memcpy(&pPerListen->local_addr,&server_addr,sizeof(sockaddr_in));

		if (!itsIocp.BindWithIocp(pPerListen->Handle,pPerListen))
			return 0;

		//�󶨵�ַ&��ʼ����
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

	//����Svr,���������CreateIocpSvr,����Ҫ���˳�ǰ����
	void DestoryTcpSvr(int32 nListenSocket)
	{
		//�رռ���Socket
		::closesocket(nListenSocket);
	}

	//Clientʹ��
	int32 Connect(const char* szIp,unsigned short nPort,uint32 nFlag)
	{
		sockaddr_in server_addr = {0};
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(nPort);
		server_addr.sin_addr.S_un.S_addr = inet_addr(szIp);

		//�����ص�Socket
		PerSocketHandleData* pPerSocket = new PerSocketHandleData;
		pPerSocket->Type = PerSocketHandleData::Type_Client;
		pPerSocket->Handle = (HANDLE)::WSASocket(AF_INET,SOCK_STREAM,IPPROTO_TCP,NULL,NULL,WSA_FLAG_OVERLAPPED);
		if (pPerSocket->Handle == NULL || (SOCKET)pPerSocket->Handle == INVALID_SOCKET)
		{
			printf("�����ص�Socketʧ��\n");
			delete pPerSocket;
			return NULL;
		}

		memcpy(&pPerSocket->remote_addr,&server_addr,sizeof(sockaddr_in));
		setsockopt((SOCKET)pPerSocket->Handle,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		//���ӷ����
		if (SOCKET_ERROR == ::connect((SOCKET)pPerSocket->Handle,(sockaddr*)&server_addr,sizeof(sockaddr_in)))
		{
			printf("���ӷ����:<%s,%u,%d>ʧ��\n",szIp,nPort,WSAGetLastError());
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//��ȡ���ص�ַ
		int local_addr_size = sizeof(sockaddr_in);
		getsockname((SOCKET)pPerSocket->Handle,(sockaddr*)&pPerSocket->local_addr,&local_addr_size);

		//����ɶ˿�
		if (!itsIocp.BindWithIocp(pPerSocket->Handle,pPerSocket))
		{
			printf("����ɶ˿�ʧ��\n");
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//����һ�����������ϵı�ʶ
		if (!_PostSend((SOCKET)pPerSocket->Handle,(char*)&nFlag,sizeof(nFlag)))
		{
			closesocket((SOCKET)pPerSocket->Handle);
			delete pPerSocket;
			return NULL;
		}

		//Ͷ�ݵ�һ��PostRecv
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


	//�³���ƽӿ�
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
			printf("���ȣ�%d�����������ֵ%d\r\n",buflen,nMaxTransLen);
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

	//�Ͽ���һЩ����
	//nHandle	:�Ͽ��ľ��
	//nCode		:�Ͽ���ԭ��
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
				//�����˴���,��Ҫ����PerSocketIoData�ͷ�
				//printf("Worker Error\n");
				itsCache.FreeMem(pPerIoData);
			}

			//ɾ���õ��������
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
			//�˳���ɶ˿�,��Ҫ�ر��߳�
			//���ｫ�������,�ȴ��˳�
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

				//��Flag����
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
				{//�Ͽ�
					_DoDisConnect(pPerHandleData,0);
					itsCache.FreeMem(pPerIoData);
				}else
				{//����Ƿ������
					if (nCompletionBytes == pPerIoData->WsaBuf.len)
					{//�������,ɾ�����͵�IO����
						itsCache.FreeMem(pPerIoData);
					}
					else if (nCompletionBytes < pPerIoData->WsaBuf.len)
					{//����δ���,����Ͷ��Send
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
				{//�Ͽ�
					_DoDisConnect(pPerHandleData,0);
					itsCache.FreeMem(pPerIoData);
				}
				else
				{
					//
					if (pPerHandleData->itsOnRecvPtr)
					{
						//�����³���ָ��,����ָ�����
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
	/***��ɶ˿����***/
	iIocp itsIocp;
	iMemCache<PerSocketIoData> itsCache;
	std::map<int32,PerSocketHandleData*> itsPerHandleDataMap;
	iLock itsPerHandleDataMapLock;

	//�߳���
	std::list<iThreadEx*> itsThreadList;
	iLock itsThreadListLock;
};

#endif