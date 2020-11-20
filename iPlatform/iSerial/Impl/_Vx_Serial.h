/*********************************************
Author:Tanglx
Date:2020/08/06
FileName:_Vx_Serial.h

在Vx下,Serial模块总是两个任务,一个收,一个发
**********************************************/
#ifndef _FILENAME__VX_SERIAL_
#define _FILENAME__VX_SERIAL_

#include "../../../iTypes.h"
#include "../../../iPlatform/iThread/iThread.h"
#include "../../../iPattern/iTrans.h"

//OS Lib
#include <ioLib.h>
#include <sioLib.h>
#include <drv/sio/ns16552Sio.h>
#include <sys/time.h>
#include <selectLib.h>

//STL
#include <map>
#include <list>


template<int nMaxTransLen>
class _Impl_iSerialCore
{
	enum
	{
		// 校验
		_COM_PARITY_NONE ='N',
		_COM_PARITY_ODD  ='O',	//奇校验
		_COM_PARITY_EVEN ='E',	//偶校验
		// 不支持其它校验
	};

	struct SendBuf
	{
		int32 Fd;
		char Buf[nMaxTransLen];
		int32 Len;

		SendBuf(){
			memset(this,0,sizeof(SendBuf));
		}

		SendBuf(int32 Fd_,const char* Buf_,int32 Len_)
		{
			Fd = Fd_;
			Len = Len_;
			memcpy(Buf,Buf_,sizeof(Buf));
		}
	};
public:
	_Impl_iSerialCore(){
		itsOnErrorPtr = NULL;
		itsOnRecvPtr = NULL;
	}
	~_Impl_iSerialCore(){
	}

	bool CreateCore(int32 nWorkerNum = 0)
	{
		uint32 nRet = 32;
		nRet = itsSendThread.Create(this,&_Impl_iSerialCore::Send_Proc,64*1024,60,"Vx_Serial_Send");
		if (nRet<=0)
		{
			return false;
		}

		nRet = itsRecvThread.Create(this,&_Impl_iSerialCore::Recv_Proc,64*1024,60,"Vx_Serial_Recv");
		if (nRet<=0)
		{
			itsSendThread.Stop();
			return false;
		}

		return true;
	}

	void DestoryCore()
	{
	}

	int32 OpenSerial(const char* sName,int32 nBaud,int32 nDataBits,int32 nStopBits,int32 nParity)
	{
		if ((nDataBits<5U || nDataBits>8U) || (nStopBits<1U  || nStopBits>2U) )
		{
			return 0;
		}

		//打开设备
		int fd = open(sName,O_RDWR,0644);
		if (fd == ERROR)
			return 0;

		//设置波特率
		if (ioctl(fd, FIOBAUDRATE, nBaud)==ERROR)
		{
			//如果失败,便使用默认的波特率
		}

		//设置TTY驱动选项
		if (ioctl(fd, FIOSETOPTIONS, OPT_RAW)==ERROR)
		{
			//如果失败,便使用默认的驱动选项
		}

		int opt = CREAD|CLOCAL|HUPCL; // 忽略MODEM状态，禁止CTS/RTS流控

		switch(nDataBits)
		{
		default:
			nDataBits = 8;
			// here no break;
		case 8:
			opt |= CS8;
			break;
		case 7:
			opt |= CS7;
			break;
		case 6:
			opt |= CS6;
			break;
		case 5:
			opt |= CS5;
			break;
		}

		if (nStopBits==2)
			opt |= STOPB;
		else
			nStopBits = 1;

		switch(nParity)
		{
		default:
			nParity = PARITY_NONE;
			// here no break;
		case _COM_PARITY_NONE:
			break;
		case _COM_PARITY_ODD:
			opt |= PARENB|PARODD;
			break;
		case _COM_PARITY_EVEN:
			opt |= PARENB;
			break;
		}

		//设置帧格式
		if (ioctl(fd, SIO_HW_OPTS_SET, opt)==ERROR)
		{
			//如果设置失败,便使用默认帧格式
		}

		ioctl(fd, SIO_OPEN, 0);


		//将句柄加入到处理列表中
		itsSerialHandleMapLock.Lock();
		itsSerialHandleMap[fd] = fd;
		itsSerialHandleMapLock.Unlock();

		return fd;
	}

	void CloseSerial(int32 hSerial)
	{
		itsSerialHandleMapLock.Lock();
		std::map<int32, int32>::iterator it = itsSerialHandleMap.find(hSerial);
		if (it != itsSerialHandleMap.end())
		{
			itsSerialHandleMap.erase(it);
		}
		itsSerialHandleMapLock.Unlock();

		if (hSerial <= 0)
			return ;

		close(hSerial);
	}

	BOOL PostSend(int32 hSerial,char* buf,int32 len)
	{
		SendBuf* pSendBuf = new SendBuf(hSerial,buf,len);
		iLifeSpanLock _lk(&itsSendListLock);
		itsSendList.push_back(pSendBuf);
		itsSendThread.Resume();
		return TRUE;
	}

	template<class T>
	void RegOnError(T* pObj,void(T::* pFunc)(int32))
	{
		if (itsOnErrorPtr)
			delete itsOnErrorPtr;
		itsOnErrorPtr = new iTrans1::Leaf<T,int32>(pObj,pFunc);
	}

	template<class T>
	void RegOnRecv(T* pObj,void(T::* pFunc)(int32,char*,int32))
	{
		if (itsOnRecvPtr)
			delete itsOnRecvPtr;
		itsOnRecvPtr = new iTrans3::Leaf<T,int32,char*,int32>(pObj,pFunc);
	}
private:
	void Send_Proc()
	{
		SendBuf* pSendBuf = NULL;

		itsSendListLock.Lock();
		if (itsSendList.size() > 0)
		{
			pSendBuf = itsSendList.front();
			itsSendList.pop_front();
		}
		itsSendListLock.Unlock();

		if (pSendBuf)
		{
			int nNeedSend = pSendBuf->Len;
			int nHadSend = 0;
			while(nNeedSend > 0)
			{
				int nSends = write(pSendBuf->Fd,pSendBuf->Buf + nHadSend,nNeedSend);
				if (nSends <= 0)
				{
					//出现了错误,退出
					break;
				}
				nNeedSend -= nSends;
				nHadSend += nSends;
			}

			delete pSendBuf;
		}else
		{
			itsSendThread.Pause();
		}
	}

	void Recv_Proc()
	{
		fd_set ReadFdset;
		int32 max_fd = -1;
		FD_ZERO(&ReadFdset);

		itsSerialHandleMapLock.Lock();
		std::map<int32,int32>::iterator it=itsSerialHandleMap.begin();
		for (;it != itsSerialHandleMap.end();++it)
		{
			max_fd = max_fd>it->first?max_fd:it->first;
			FD_SET(it->first,&ReadFdset);
		}
		itsSerialHandleMapLock.Unlock();

		if (max_fd == -1)
		{
			//不存在需要接收操作的句柄
			iPublic::iSleep(200);
			return;
		}

		timeval timeout;
		timeout.tv_sec = 5;
		timeout.tv_usec = 0;

		int nRet = select(max_fd+1,&ReadFdset,NULL,NULL,&timeout);
		if (nRet > 0)
		{
			itsSerialHandleMapLock.Lock();
			std::map<int32, int32>::iterator it = itsSerialHandleMap.begin();
			for (;it != itsSerialHandleMap.end();++it)
			{
				if (FD_ISSET(it->first,&ReadFdset))
				{
					char buf[nMaxTransLen] = {0};
					int32 len = read(it->first,buf,sizeof(buf));
					if (len > 0)
					{
						if (itsOnRecvPtr)
						{
							itsOnRecvPtr->Execute(it->first,buf,len);
						}
					}else
					{
						if (itsOnErrorPtr)
						{
							itsOnErrorPtr->Execute(it->first);
						}
						close(it->first);
						itsSerialHandleMap.erase(it);
					}
				}
			}
			itsSerialHandleMapLock.Unlock();
		}
	}
private:
	std::map<int32,int32> itsSerialHandleMap;
	iLock itsSerialHandleMapLock;

	iThread itsSendThread;
	iThread itsRecvThread;

	//发送的list
	std::list<SendBuf*> itsSendList;
	iLock itsSendListLock;

	iTrans1::Branch<int32>* itsOnErrorPtr;
	iTrans3::Branch<int32,char*,int32>* itsOnRecvPtr;
};

#endif
