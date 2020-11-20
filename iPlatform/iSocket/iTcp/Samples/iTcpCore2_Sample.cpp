/*********************************************
Author:Tanglx
Date:2020/10/12
FileName:iTcpCore2_Sample.cpp

编写一个使用iTcpCore2.h的例子
这里为了简便,就没写返回值检查,在实际的应用中还是需要检查返回值的
**********************************************/

#include "../iTcpServer2.h"

namespace
{
	const uint32 AppFlag1 = 1;
	const uint32 AppFlag2 = 2;

	//客户端1
	class MyClient
	{
	public:
		MyClient(){
			itsPeer.RegOnDisConnectPtr(this,&MyClient::OnDisConnect);
			itsPeer.RegOnRecvPtr(this,&MyClient::OnRecv);
		}
		~MyClient(){}

		iTcpPeer2* GetPeer(){return &itsPeer;}
		void OnRecv(char* buf,int32 len){
			printf("ClientApp1 Recv:%s\n",buf);
		}
		void OnDisConnect()
		{
			printf("ClientApp1 BreakDown\n");
		}
	private:
		iTcpPeer2 itsPeer;
	};


	//服务端Peer
	class TcpServerPeer_For_App1
	{
	public:
		TcpServerPeer_For_App1(){
			itsPeer.RegOnDisConnectPtr(this,&TcpServerPeer_For_App1::OnDisConnect);
			itsPeer.RegOnRecvPtr(this,&TcpServerPeer_For_App1::OnRecv);
		}
		~TcpServerPeer_For_App1(){}

		void OnDisConnect()
		{
			printf("TcpServerPeer_For_App1 Break\n");
		}

		void OnRecv(char* buf,int32 len)
		{
			printf("TcpServerPeer_For_App1 : %s\n",buf);
			itsPeer.PostSend("Hello Client",strlen("Hello Client"));
		}

		iTcpPeer2* GetPeer(){return &itsPeer;}
	private:
		iTcpPeer2 itsPeer;
	};

	class TcpServerPeer_For_App2
	{
	public:
		TcpServerPeer_For_App2(){
			itsPeer.RegOnDisConnectPtr(this,&TcpServerPeer_For_App2::OnDisConnect);
			itsPeer.RegOnRecvPtr(this,&TcpServerPeer_For_App2::OnRecv);
		}
		~TcpServerPeer_For_App2(){}


		void OnDisConnect()
		{
			printf("TcpServerPeer_For_App2 Break\n");
		}

		void OnRecv(char* buf,int32 len)
		{
			printf("TcpServerPeer_For_App2 : %s\n",buf);
			itsPeer.PostSend("Hello Client",strlen("Hello Client"));
		}

		iTcpPeer2* GetPeer(){return &itsPeer;}
	private:
		iTcpPeer2 itsPeer;
	};

	class MyTcpServer
	{
	public:
		MyTcpServer(unsigned short nPort){
			itsServer.CreateServer(NULL,nPort);
			itsServer.RegOnAccept(this,&MyTcpServer::OnAccept);
		}
		~MyTcpServer(){
			itsServer.DestoryServer();
		}

		void OnAccept(int32 nSocket,uint32 nFlag)
		{
			switch(nFlag)
			{
			case AppFlag1:
				{
					printf("OnAccept:AppFlag1\n");
					TcpServerPeer_For_App1* p = new TcpServerPeer_For_App1;
					p->GetPeer()->SetFlag(nFlag);
					iTcpCore2::GetInstance()->Bind(nSocket,p->GetPeer());
					itsApp1MapLock.Lock();
					itsApp1Map[nSocket] = p;
					itsApp1MapLock.Unlock();
				}break;
			case AppFlag2:
				{
					printf("OnAccept:AppFlag2\n");
					TcpServerPeer_For_App2* p = new TcpServerPeer_For_App2;
					p->GetPeer()->SetFlag(nFlag);
					iTcpCore2::GetInstance()->Bind(nSocket,p->GetPeer());
					itsApp2MapLock.Lock();
					itsApp2Map[nSocket] = p;
					itsApp2MapLock.Unlock();
				}break;
			default:
				break;
			}
		}
	private:
		iTcpServer2 itsServer;

		std::map<int32, TcpServerPeer_For_App1 *> itsApp1Map;
		iLock itsApp1MapLock;

		std::map<int32, TcpServerPeer_For_App2 *> itsApp2Map;
		iLock itsApp2MapLock;
	};
};


extern "C" void iTcpCore2_Sample()
{
	printf("iTcpCore2_Sample Begin\n");
	//必须要初始化
	iTcpCore2::InitInst();

	MyTcpServer* pSvr = new MyTcpServer(8888);

	MyClient Cli;

	int32 nSocket = iTcpCore2::GetInstance()->Connect("127.0.0.1",8888,AppFlag2);
	if(nSocket > 0)
	{
		iTcpCore2::GetInstance()->Bind(nSocket,Cli.GetPeer());
		Cli.GetPeer()->PostSend("123456",6);
		iPublic::iSleep(10000);
		iTcpCore2::GetInstance()->DisConnect(nSocket);
		iPublic::iSleep(10000);
		iTcpCore2::GetInstance()->UnBind(nSocket);
		iPublic::iSleep(10000);
	}

	delete pSvr;

	iPublic::iSleep(10000);

	iTcpCore2::DestoryInst();

	iPublic::iSleep(10000);

	printf("iTcpCore2_Sample End\n");
	return;
};
