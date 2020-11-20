/*********************************************
Author:     Tanglx
Date:       2019/08/31
FileName:   _Vx_Socket.h
**********************************************/
#ifndef _FILENAME__VX_SOCKET_
#define _FILENAME__VX_SOCKET_

#include "../../../iTypes.h"
#include "wrn/coreip/netinet/in.h"
#include "wrn/coreip/arpa/inet.h"
#include "string.h"
#include "stdio.h"
#include <socket.h>
#include <sockLib.h>
#include <selectLib.h>

//typedef int32 SOCKET;
#define SOCKET int32


#define iFD_SET(n,p) FD_SET(n,p)
#define iFD_CLR(n,p) FD_CLR(n,p)
#define iFD_ISSET(n,p) FD_ISSET(n,p)
#define iFD_ZERO(p) FD_ZERO(p)


struct ifd_set
{
	ifd_set(){
		memset(this,0,sizeof(ifd_set));
	}
	fd_set itsv;
};

struct itimeval
{
	itimeval(){
		memset(this,0,sizeof(itimeval));
	}
	timeval itsv;
};

struct isockaddr
{
	isockaddr(){
		itsaddr.sin_len = sizeof(sockaddr_in);
	}
	sockaddr_in itsaddr;
};

class _Impl_Socket
{
public:
    //一些宏,如果有用到,则添加进来
	static const uint32 iInvalidSocket = ERROR;
	static const uint32 iOk = OK;
	static const uint32 iError = ERROR;
    static const uint32 iAF_INET = AF_INET;
	static const uint32 iSOCK_STREAM = SOCK_STREAM;
    static const uint32 iSOCK_DGRAM = SOCK_DGRAM;
    static const uint32 iADDR_ANY = 0;
    static const uint32 iIPPROTO_UDP = 0;//IPPROTO_UDP; ,vxworks 下协议类型为0
	static const uint32 iIPPROTO_TCP = 0;//IPPROTO_TCP; ,vxworks 下协议类型为0

public:
    static SOCKET Create(int32 afinet, int32 type, int32 protocol)
    {
        return socket(afinet, type, protocol);
    }

    static void Close(SOCKET socket)
    {
        close(socket);
    }

    static int32 Bind(SOCKET socket,const isockaddr* addr)
    {
        return bind(socket, reinterpret_cast <const sockaddr *>(&addr->itsaddr), sizeof(sockaddr_in));
    }

	static int32 Listen(SOCKET socket,int32 backlog)
	{
		return listen(socket,backlog);
	}

	static SOCKET Accept(SOCKET socket,isockaddr* addr)
	{
		int32 addrlen=0;
		return accept(socket,(sockaddr*)&addr->itsaddr,(socklen_t*)&addrlen);
	}

	static int32 Connect(SOCKET socket,const isockaddr* addr)
	{
		return connect(socket,reinterpret_cast <const sockaddr *>(&addr->itsaddr),sizeof(sockaddr_in));
	}

	static int32 Recv(SOCKET socket,char* buf,int32 len)
	{
		return recv(socket,buf,len,0);
	}

	static int32 Send(SOCKET socket,const char* buf,int32 len)
	{
		return send(socket,buf,len,0);
	}

	static int32 Shutdown(SOCKET socket)
	{
		return shutdown(socket,0x2);//0x2 means SD_BOTH
	}
	
	static int32 Select(int32 Maxfd1,ifd_set* Readfds,ifd_set* WriteFds,ifd_set* ExceptFds,itimeval* timeout)
	{
		return select(Maxfd1,&Readfds->itsv,&WriteFds->itsv,&ExceptFds->itsv,&timeout->itsv);
	}

    static int32 RecvFrom(SOCKET socket, char *buf, uint32 buflen, int32 flag, isockaddr *sendaddr, int32 *sendaddrlen)
    {
        return recvfrom(socket, buf, buflen, flag, reinterpret_cast <sockaddr *> (&sendaddr->itsaddr),reinterpret_cast <socklen_t *> (sendaddrlen));
    }

    static int32 SendTo(SOCKET socket, char *buf, uint32 buflen, int32 flag, isockaddr *recvaddr)
    {
        return sendto(socket, buf, buflen, 0, reinterpret_cast <const sockaddr *> (&recvaddr->itsaddr),sizeof(sockaddr_in));
    }
};


#endif

