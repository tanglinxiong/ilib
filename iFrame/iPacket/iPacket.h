/*********************************************
Author:Tanglx
Date:2019/11/16
FileName:iPacket.hxx

主要功能如下:
Package -> Content
Content -> Package

适用于字节流式的通讯方案,如串口,TCP等

该层次通讯使用网络字节序

**********************************************/
#ifndef _FILENAME_IPACKET_
#define _FILENAME_IPACKET_

#include "../../iTypes.h"
#include "../../iPattern/iTrans.h"
#include "../../iBase/iBase.h"

#define HEAD_TAG "IPAK"
#define HEAD_LEN 4

#define HEAD_PAS "PASS"

class iPacket
{
	struct Package
	{
		char HeadFlag[HEAD_LEN];	//头上HEAD_LEN个字节作为起始标识
		int32 BufLen;				//消息内容长度
		int32 BufCrc;				//消息内容CRC校验
		char Buf[0];				//消息内容

		Package()
		{
			memset(this,0,sizeof(Package));
		}
	};

	enum
	{
		MaxPacketLen = 64*1024		//最大包长度
	};

public:
	iPacket()
	{
		itsOnContentPtr = NULL;
		itsOnPacketPtr = NULL;

		memset(itsParseBuf,0,sizeof(itsParseBuf));
		itsParseBufLen = 0;
	}
	~iPacket()
	{
		if (itsOnContentPtr)
		{
			delete itsOnContentPtr;
			itsOnContentPtr = NULL;
		}

		if (itsOnPacketPtr)
		{
			delete itsOnPacketPtr;
			itsOnPacketPtr = NULL;
		}
	}


	template<class T>
	void RegOnContent(T* pObj,void(T::* pFunc)(char*,int32))
	{
		itsOnContentPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
	}


	//Package -> Content
	void Parse(char* buf,int32 len)
	{
		if (!itsOnContentPtr)
		{
			return;
		}

		//拷贝数据到缓冲区
		memcpy(itsParseBuf+itsParseBufLen,buf,len);
		itsParseBufLen += len;

		//解析数据
		while(itsParseBufLen>HEAD_LEN)
		{
			char* pfind = NULL;
			for(int32 i=0;i<=itsParseBufLen-HEAD_LEN;++i)
			{
				if(memcmp(&itsParseBuf[i],HEAD_TAG,HEAD_LEN) == 0)
				{
					pfind = &itsParseBuf[i];
					break;
				}
			}

			if(pfind)
			{//有头,偏移
				//计算需要丢弃的字节长度
				int discardLen = pfind - itsParseBuf;
				if (discardLen>0){
					itsParseBufLen -= discardLen;	//更新长度
					memmove(itsParseBuf,pfind,itsParseBufLen);	//更新缓冲区
					memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
				}
			}else{
				//无头,则说明只有最后3个字节还未验证
				pfind = itsParseBuf + itsParseBufLen - 3;
				itsParseBufLen = 3;
				memmove(itsParseBuf,pfind,3);
				memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
				return;
			}


			if (itsParseBufLen>sizeof(Package))
			{//足够头的长度,解析
				Package* pPackage = (Package*)itsParseBuf;

				if (iBase::IsLittleEndin())
				{
					//如果本机是小端模式,则进行转换
					iBase::ConvertToOhterEndian(&pPackage->BufLen);
					iBase::ConvertToOhterEndian(&pPackage->BufCrc);
				}

				if (pPackage->BufLen > MaxPacketLen || pPackage->BufLen < 0)
				{
					//检测长度不合法,则过,进行下一次查找
					strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
					continue;
				}


				if (itsParseBufLen >= (int32)(sizeof(Package) + pPackage->BufLen))
				{
					//内容长度足够
					if (pPackage->BufCrc == iBase::CheckSum(pPackage->Buf,pPackage->BufLen))
					{
						//CRC校验通过
						itsOnContentPtr->Execute(pPackage->Buf,pPackage->BufLen);

						//处理之后,进行下一次查找
						//strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
						//2020/4/29 分析得知,上述语句在报文较大时,会带来较大的性能开销
						
						int discardLen = pPackage->BufLen + sizeof(Package);
						if (discardLen>0){
							itsParseBufLen -= discardLen;	//更新长度
							memmove(itsParseBuf,itsParseBuf+discardLen,itsParseBufLen);	//更新缓冲区
							memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
						}
						
						continue;
					}else
					{
						//CRC校验不过,进行下一次查找
						strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
						continue;
					}
				}else
				{
					//内容长度不够,等待

					//因为此处经过了转换,所以需要将原始报文还原
					if (iBase::IsLittleEndin())
					{
						//如果本机是小端模式,则进行转换
						iBase::ConvertToOhterEndian(&pPackage->BufLen);
						iBase::ConvertToOhterEndian(&pPackage->BufCrc);
					}

					return;
				}
			}else
			{//不够头的长度,等待
				return;
			}
		}
	}

	template<class T>
	void RegOnPacket(T* pObj,void(T::* pFunc)(char*,int32))
	{
		itsOnPacketPtr = new iTrans2::Leaf<T,char*,int32>(pObj,pFunc);
	}

	//Content -> Package
	void Sticky(char* buf,int32 len)
	{
		if (!itsOnPacketPtr)
		{
			return;
		}

		int32 nPackageLen = sizeof(Package) + len;
		if (nPackageLen > MaxPacketLen)
		{
			//对于超过64KB的消息,不进行处理
			return;
		}

		char* pBuf = new char[nPackageLen];
		memset(pBuf,0,nPackageLen);

		Package* pPackage = (Package*)pBuf;
		memcpy(pPackage->HeadFlag,HEAD_TAG,HEAD_LEN);
		pPackage->BufLen = len;
		pPackage->BufCrc = iBase::CheckSum(buf,len);
		if (iBase::IsLittleEndin())
		{
			//如果本机是小端模式,则进行转换
			iBase::ConvertToOhterEndian(&pPackage->BufLen);
			iBase::ConvertToOhterEndian(&pPackage->BufCrc);
		}
		memcpy(pPackage->Buf,buf,len);

		itsOnPacketPtr->Execute(pBuf,nPackageLen);

		delete []pBuf;
	}
private:
	iTrans2::Branch<char*,int32>* itsOnContentPtr;
	iTrans2::Branch<char*,int32>* itsOnPacketPtr;

	char itsParseBuf[MaxPacketLen];
	int32 itsParseBufLen;
};

#endif
