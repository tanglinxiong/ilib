/*********************************************
Author:Tanglx
Date:2020/10/09
FileName:_Win_Tcp.h
Windows ��TcpͨѶ,ʹ����ɶ˿�ʵ��
**********************************************/
#ifndef _FILENAME__WIN_TCP_
#define _FILENAME__WIN_TCP_

//OS
#include "wrn/coreip/netinet/in.h"
#include "wrn/coreip/arpa/inet.h"
#include <socket.h>
#include <sockLib.h>
#include <selectLib.h>

//ilib
#include "../../../../iPattern/iTrans.h"
#include "../../../../iTypes.h"
#include "../../../iThread/iThread.h"

//STL
#include <map>
#include <list>


//typedef int32 SOCKET;
#define SOCKET int32

#define VX_IPPROTO_TCP 0

template<int nMaxTransLen>
class _Impl_iTcp
{
public:
	_Impl_iTcp(){
	}
	~_Impl_iTcp(){
	}

	//�����͵Ķ���,ֻ�����ڽ����߳���DELETE,
	//�����ڽ����̹߳رպ�,�ſ��Խ���DELETE
	struct PerSocketHandleData
	{
		enum
		{
			Type_Listen,		//�������׽���
			Type_Server,		//�����ͨ��Accept����AcceptEx�������׽���
			Type_Client,		//�ͻ��˽������׽���
		};
		int32 Handle;
		int Type;	//����
		sockaddr_in local_addr;		//���ص�ַ
		sockaddr_in remote_addr;	//Զ�̵�ַ

		iTrans2::Branch<char*,int32>* itsOnRecvPtr;		//ʹ���³����ʱ����Ҫ�õ�,�����ֵ��ΪNULL,���ʾ�³����
		iTrans0::Branch* itsOnDisConnectPtr;			//ʹ���³����ʱ����Ҫ�õ�,�����ֵ��ΪNULL,���ʾ�³����

		PerSocketHandleData(){
			memset(this,0,sizeof(PerSocketHandleData));
		}
		~PerSocketHandleData(){
		}
	};

	struct PerListenSocketData
	{
		int32 Handle;
		sockaddr_in listen_addr;		//������ַ

		iTrans2::Branch<int32,uint32>* itsOnAcceptPtr;	//���ڸõ��������Ϊ��������ʱ����Ч
		iThreadEx* itsListenThreadPtr;	//��������
		PerListenSocketData(){
			memset(this,0,sizeof(PerListenSocketData));
		}
		~PerListenSocketData(){

		}
	};

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

	//����Tcp Core,����Ҫ����
	bool CreateCore(int32 nWorkerNum = 0)
	{
		uint32 nRet = itsRecvThread.Create(this,&_Impl_iTcp::Recv_Proc,256*1024,60,"Recv2");
		if (nRet == NULL)
		{
			return false;
		}

		nRet = itsSendThread.Create(this,&_Impl_iTcp::Send_Proc,256*1024,60,"Send2");
		if (nRet == NULL)
		{
			itsRecvThread.Stop();
			return false;
		}

		return true;
	}

	//����Tcp��,���˳�ǰ����
	void DestoryCore()
	{
		//�ȹر��߳�
		itsRecvThread.Stop();
		itsSendThread.Stop();

		//���
		itsPerHandleDataMapLock.Lock();
		typename std::map<int32,PerSocketHandleData*>::iterator it = itsPerHandleDataMap.begin();
		for (;it != itsPerHandleDataMap.end();++it)
		{
			int32 nSocket = it->first;
			delete it->second;
			shutdown(nSocket,0x2);
			close(nSocket);
		}
		itsPerHandleDataMap.clear();
		itsPerHandleDataMapLock.Unlock();

		//��շ��Ͷ���
		itsSendLstLock.Lock();
		typename std::list<SockBuf*>::iterator it2 = itsSendLst.begin();
		for (;it2 != itsSendLst.end();++it2)
		{
			delete *it2;
		}
		itsSendLst.clear();
		itsSendLstLock.Unlock();
	}

	//����Svr,�����Ҫʹ��Svr����,����Ҫ����
	int32 CreateTcpSvr( const char* szIp,unsigned short nPort)
	{
		//���������׽���
		int32 nListenSocket = socket(AF_INET,SOCK_STREAM,VX_IPPROTO_TCP);
		if (nListenSocket == ERROR)
		{
			return NULL;
		}

		//�󶨶˿�
		sockaddr_in server_addr;
		server_addr.sin_family = AF_INET;
		server_addr.sin_port = htons(nPort);
		if(szIp)
		{
			server_addr.sin_addr.s_addr = inet_addr(szIp);//htonl(inet_addr(szIp));
		}else
		{
			server_addr.sin_addr.s_addr = htonl(0);
		}

		int32 nRet = bind(nListenSocket,(sockaddr*)&server_addr,sizeof(sockaddr_in));
		if (nRet == ERROR)
		{
			printf("bind %s:%d Failed\r\n",szIp,nPort);
			close(nListenSocket);
			return NULL;
		}

		//��ʼ����
		nRet = listen(nListenSocket,64);
		if (nRet == ERROR)
		{
			close(nListenSocket);
			return NULL;
		}
		
		//int flag = fcntl(nListenSocket, F_GETFL, 0);
		//fcntl(nListenSocket, F_SETFL, flag | O_NONBLOCK);
		//setsockopt(nListenSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		PerListenSocketData* pPerListen = new PerListenSocketData;
		pPerListen->Handle = nListenSocket;
		memcpy(&pPerListen->listen_addr,&server_addr,sizeof(sockaddr_in));

		//������������
		pPerListen->itsListenThreadPtr = new iThreadEx;
		nRet = pPerListen->itsListenThreadPtr->Create(this,&_Impl_iTcp::Accept_Proc,32*1024,60,"Accpet2",(void*)pPerListen);
		if (nRet == NULL)
		{
			printf("������������ʧ��\r\n");
			close(nListenSocket);
			delete pPerListen;
			return NULL;
		}

		itsListenSocketMapLock.Lock();
		itsListenSocketMap[nListenSocket] = pPerListen;
		itsListenSocketMapLock.Unlock();

		printf("CreateTcpSvr %d\n",nListenSocket);
		return nListenSocket;
	}

	//����Svr,���������CreateIocpSvr,����Ҫ���˳�ǰ����
	void DestoryTcpSvr(int32 nListenSocket)
	{
		//�رռ����׽���
		printf("DestoryTcpSvr close %d\n",nListenSocket);
		shutdown(nListenSocket,0x2);
		//close(nListenSocket);

		PerListenSocketData* pPerListen = NULL;
		itsListenSocketMapLock.Lock();
		typename std::map<int32, PerListenSocketData *>::iterator it = itsListenSocketMap.find(nListenSocket);
		if(it != itsListenSocketMap.end())
		{
			pPerListen = it->second;
			itsListenSocketMap.erase(it);
		}
		itsListenSocketMapLock.Unlock();
		
		//ֹͣ��������,�������
		pPerListen->itsListenThreadPtr->Stop();
		printf("%d Stop Listen Done\n",nListenSocket);
		delete pPerListen->itsListenThreadPtr;
		delete pPerListen;
	}

	//Clientʹ��
	int32 Connect(const char* szIp,unsigned short nPort,uint32 nFlag)
	{
		//�����������ӵ��׽���
		SOCKET nSocket = socket(AF_INET,SOCK_STREAM,VX_IPPROTO_TCP);
		if (nSocket == ERROR)
		{
			printf("socket failed\n");
			return NULL;
		}

		//��װ��ַ
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		addr.sin_port = htons(nPort);
		addr.sin_addr.s_addr = inet_addr(szIp);
		addr.sin_len = sizeof(addr);

		//��������
		uint32 nRet = connect(nSocket,(sockaddr*)&addr,sizeof(addr));
		if (nRet == ERROR)
		{
			printf("connect failed\n");
			close(nSocket);
			return NULL;
		}

		//����SOCKET������
		//setsockopt(nSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		//���ӳɹ�֮��,����һ��Flag��ǹ�ȥ
		PostSend(nSocket,(char*)&nFlag,sizeof(nFlag));

		//
		PerSocketHandleData* pPerSocketData = new PerSocketHandleData;
		pPerSocketData->Handle = nSocket;
		pPerSocketData->Type = PerSocketHandleData::Type_Client;
		memcpy(&pPerSocketData->local_addr,&addr,sizeof(sockaddr_in));
		int local_addr_size = sizeof(sockaddr_in);
		getsockname((SOCKET)pPerSocketData->Handle,(sockaddr*)&pPerSocketData->local_addr,&local_addr_size);

		itsPerHandleDataMapLock.Lock();
		itsPerHandleDataMap[nSocket] = pPerSocketData;
		itsPerHandleDataMapLock.Unlock();

		return nSocket;
	}

	void DisConnect(int32 nSocket)
	{
		shutdown(nSocket,0x2);
		//close(nSocket);
		//_PostSend(nSocket,NULL,0);
	}

	bool PostSend(int32 nSocket,char* buf,int32 len)
	{
		return _PostSend(nSocket,buf,len);
	}

	template<class T>
	void RegOnPerSocketRecv(int32 nSocket,T* pObj,void(T::* pFunc)(char*,int32))
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			if (pPerSocketData->itsOnRecvPtr)
				delete pPerSocketData->itsOnRecvPtr;

			pPerSocketData->itsOnRecvPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
		}
	}

	//�³���ƽӿ�
	void RegOnListenAccept(int32 ListenSocket,iTrans2::Branch<int32,uint32>* OnAcceptPtr)
	{
		iLifeSpanLock _lk(&itsListenSocketMapLock);
		typename std::map<int32, PerListenSocketData *>::iterator it = itsListenSocketMap.find(ListenSocket);
		if (it != itsListenSocketMap.end())
		{
			PerListenSocketData* pPerSocketData = it->second;
			printf("%d RegOnListenAccept\n",(int)ListenSocket);
			pPerSocketData->itsOnAcceptPtr = OnAcceptPtr;
		}
	}

	void UnregOnListenAccept(int32 ListenSocket)
	{
		iLifeSpanLock _lk(&itsListenSocketMapLock);
		typename std::map<int32, PerListenSocketData *>::iterator it = itsListenSocketMap.find(ListenSocket);
		if (it != itsListenSocketMap.end())
		{
			PerListenSocketData* pPerSocketData = it->second;
			printf("%d UnregOnListenAccept\n",(int)ListenSocket);
			pPerSocketData->itsOnAcceptPtr = NULL;
		}
	}

	void RegOnPerSocketRecv(int32 nSocket,iTrans2::Branch<char*,int32>* OnRecvPtr)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			printf("%d RegOnPerSocketRecv\n",(int)nSocket);
			pPerSocketData->itsOnRecvPtr = OnRecvPtr;
		}
	}

	void UnregOnPerSocketRecv(int32 nSocket)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			printf("%d UnregOnPerSocketRecv\n",(int)nSocket);
			pPerSocketData->itsOnRecvPtr = NULL;
		}
	}

	void RegOnPerSocketDisConnect(int32 nSocket,iTrans0::Branch* OnDisconnectPtr)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			printf("%d RegOnPerSocketDisConnect\n",(int)nSocket);
			pPerSocketData->itsOnDisConnectPtr = OnDisconnectPtr;
		}
	}

	void UnregOnPerSocketDisConnect(int32 nSocket)
	{
		iLifeSpanLock _lk(&itsPerHandleDataMapLock);
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find(nSocket);
		if (it != itsPerHandleDataMap.end())
		{
			PerSocketHandleData* pPerSocketData = it->second;
			printf("%d UnregOnPerSocketDisConnect\n",(int)nSocket);
			pPerSocketData->itsOnDisConnectPtr = NULL;
		}
	}
private:
	bool _PostSend(int32 nSocket,const char* buf,int len )
	{
		//printf("_PostSend:%d,%s\n",nSocket,buf);
		
		SockBuf* pBuf = new SockBuf;
		pBuf->itsSocket = nSocket;
		pBuf->itsBuf = new char[len];
		memcpy(pBuf->itsBuf,buf,len);
		pBuf->itsLen = len;

		itsSendLstLock.Lock();
		itsSendLst.push_back(pBuf);
		itsSendLstLock.Unlock();

		itsSendThread.Resume();
		
		return true;
	}

	//�Ͽ���һЩ����
	//nHandle	:�Ͽ��ľ��
	//nCode		:�Ͽ���ԭ��
	void _DoDisConnect(PerSocketHandleData* pPerHandleData,int32 nCode)
	{
		if (pPerHandleData->itsOnDisConnectPtr)
			pPerHandleData->itsOnDisConnectPtr->Execute();

		itsPerHandleDataMapLock.Lock();
		typename std::map<int32, PerSocketHandleData *>::iterator it = itsPerHandleDataMap.find((int32)pPerHandleData->Handle);
		if (it != itsPerHandleDataMap.end())
		{
			delete it->second;
			itsPerHandleDataMap.erase(it);
		}
		itsPerHandleDataMapLock.Unlock();

		shutdown((SOCKET)pPerHandleData->Handle,0x2);
		close((SOCKET)pPerHandleData->Handle);
	}


	void Recv_Proc()
	{
		fd_set fdset;
		int32 max_fd = -1;
		FD_ZERO(&fdset);

		itsPerHandleDataMapLock.Lock();
		std::map<int32,PerSocketHandleData*> TempSocketMap = itsPerHandleDataMap;
		itsPerHandleDataMapLock.Unlock();

		typename std::map<int32,PerSocketHandleData*>::iterator it = TempSocketMap.begin();
		for (;it != TempSocketMap.end();++it)
		{
			max_fd = max_fd>it->first?max_fd:it->first;
			FD_SET(it->first,&fdset);
		}
		

		if (max_fd == -1)
		{
			iPublic::iSleep(100);
			return;
		}

		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		int ret = select(max_fd+1, &fdset, NULL, NULL, &timeout);
		if (ret > 0)
		{
			typename std::map<int32,PerSocketHandleData*>::iterator it2 = TempSocketMap.begin();
			for (;it2 != TempSocketMap.end();++it2)
			{
				int32 nSocket = it2->first;
				PerSocketHandleData* pPerSocketData = it2->second;
				
				if (FD_ISSET(nSocket,&fdset))
				{	
					char buf[4*1024] = {0};
					int32 len = recv(nSocket,buf,sizeof(buf),0);
					if (len > 0)
					{
						if (pPerSocketData->itsOnRecvPtr)
						{
							pPerSocketData->itsOnRecvPtr->Execute(buf,len);
						}
					}
					else
					{
						_DoDisConnect(pPerSocketData,0);
					}
				}
			}
		}
	}

	void Send_Proc()
	{
		//������ܴ�������ƿ��
		//��Ϊ���е����Ӷ�ʹ��һ������������
		//��ᵼ��,��ͬ���׽������޷����з���
		SockBuf* pSendBuf = NULL;

		itsSendLstLock.Lock();
		if (itsSendLst.size() > 0)
		{
			pSendBuf = itsSendLst.front();
			itsSendLst.pop_front();
		}
		itsSendLstLock.Unlock();

		if(pSendBuf)
		{
			int32 nNeedSendLen = pSendBuf->itsLen;
			int32 nHadSendLen = 0;
			while(nNeedSendLen > 0)
			{
				int32 SendLen = send(pSendBuf->itsSocket,pSendBuf->itsBuf+ nHadSendLen,nNeedSendLen,0);
				if (SendLen == ERROR)
				{
					if (pSendBuf->itsBuf)
						delete []pSendBuf->itsBuf;
					delete pSendBuf;
					return;
				}
				nHadSendLen += SendLen;
				nNeedSendLen -= SendLen;
			}

			if (pSendBuf->itsBuf)
				delete []pSendBuf->itsBuf;
			delete pSendBuf;
		}else
		{
			itsSendThread.Pause();
		}
	}

	void Accept_Proc(void* Param)
	{
		PerListenSocketData* pPerListen = (PerListenSocketData*)Param;

		sockaddr_in addr;
		int32 addrlen=0;
		SOCKET nSocket = accept(pPerListen->Handle,(sockaddr*)&addr,(socklen_t*)&addrlen);
		if (nSocket==ERROR)
		{
			printf("%d accept error\n");
			iPublic::iSleep(200);
			return;
		}
		
		iPublic::iSleep(200);
		
		printf("%d accept %d\n",pPerListen->Handle,nSocket);

		//����SOCKET������
		//setsockopt(nSocket,SOL_SOCKET,SO_KEEPALIVE,(char*)TRUE,sizeof(BOOL));

		//�������һ��nFlag
		int32 nFlag = -1;
		int nRet = recv(nSocket,&nFlag,sizeof(nFlag),0);
		if (nRet == ERROR)
		{
			printf("recv flag Error\n");
			shutdown(nSocket,0x2);
			close(nSocket);
		}
		
		//����PerSocketHandleData
		PerSocketHandleData* pPerSocketData = new PerSocketHandleData;
		pPerSocketData->Handle = nSocket;
		pPerSocketData->Type = PerSocketHandleData::Type_Server;

		memcpy(&pPerSocketData->remote_addr,&addr,sizeof(addr));
		int local_addr_size = sizeof(sockaddr_in);
		getsockname((SOCKET)pPerSocketData->Handle,(sockaddr*)&pPerSocketData->local_addr,&local_addr_size);

		itsPerHandleDataMapLock.Lock();
		itsPerHandleDataMap[nSocket] = pPerSocketData;
		itsPerHandleDataMapLock.Unlock();

		//ִ��֪ͨ�¼�
		if (pPerListen->itsOnAcceptPtr)
			pPerListen->itsOnAcceptPtr->Execute(nSocket,nFlag);
	}

private:
	std::map<int32,PerSocketHandleData*> itsPerHandleDataMap;
	iLock itsPerHandleDataMapLock;

	std::map<int32,PerListenSocketData*> itsListenSocketMap;
	iLock itsListenSocketMapLock;

	iThread itsRecvThread;
	iThread itsSendThread;

	iTrans4::Branch<int32,const char*,uint16,uint32>* itsOnConnectPtr;
	iTrans1::Branch<int32>* itsOnDisConnectPtr;

	std::list<SockBuf*> itsSendLst;		//���Ͷ���
	iLock itsSendLstLock;
};

#endif
