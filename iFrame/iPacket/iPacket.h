/*********************************************
Author:Tanglx
Date:2019/11/16
FileName:iPacket.hxx

��Ҫ��������:
Package -> Content
Content -> Package

�������ֽ���ʽ��ͨѶ����,�紮��,TCP��

�ò��ͨѶʹ�������ֽ���

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
		char HeadFlag[HEAD_LEN];	//ͷ��HEAD_LEN���ֽ���Ϊ��ʼ��ʶ
		int32 BufLen;				//��Ϣ���ݳ���
		int32 BufCrc;				//��Ϣ����CRCУ��
		char Buf[0];				//��Ϣ����

		Package()
		{
			memset(this,0,sizeof(Package));
		}
	};

	enum
	{
		MaxPacketLen = 64*1024		//��������
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

		//�������ݵ�������
		memcpy(itsParseBuf+itsParseBufLen,buf,len);
		itsParseBufLen += len;

		//��������
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
			{//��ͷ,ƫ��
				//������Ҫ�������ֽڳ���
				int discardLen = pfind - itsParseBuf;
				if (discardLen>0){
					itsParseBufLen -= discardLen;	//���³���
					memmove(itsParseBuf,pfind,itsParseBufLen);	//���»�����
					memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
				}
			}else{
				//��ͷ,��˵��ֻ�����3���ֽڻ�δ��֤
				pfind = itsParseBuf + itsParseBufLen - 3;
				itsParseBufLen = 3;
				memmove(itsParseBuf,pfind,3);
				memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
				return;
			}


			if (itsParseBufLen>sizeof(Package))
			{//�㹻ͷ�ĳ���,����
				Package* pPackage = (Package*)itsParseBuf;

				if (iBase::IsLittleEndin())
				{
					//���������С��ģʽ,�����ת��
					iBase::ConvertToOhterEndian(&pPackage->BufLen);
					iBase::ConvertToOhterEndian(&pPackage->BufCrc);
				}

				if (pPackage->BufLen > MaxPacketLen || pPackage->BufLen < 0)
				{
					//��ⳤ�Ȳ��Ϸ�,���,������һ�β���
					strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
					continue;
				}


				if (itsParseBufLen >= (int32)(sizeof(Package) + pPackage->BufLen))
				{
					//���ݳ����㹻
					if (pPackage->BufCrc == iBase::CheckSum(pPackage->Buf,pPackage->BufLen))
					{
						//CRCУ��ͨ��
						itsOnContentPtr->Execute(pPackage->Buf,pPackage->BufLen);

						//����֮��,������һ�β���
						//strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
						//2020/4/29 ������֪,��������ڱ��Ľϴ�ʱ,������ϴ�����ܿ���
						
						int discardLen = pPackage->BufLen + sizeof(Package);
						if (discardLen>0){
							itsParseBufLen -= discardLen;	//���³���
							memmove(itsParseBuf,itsParseBuf+discardLen,itsParseBufLen);	//���»�����
							memset(itsParseBuf+itsParseBufLen,0,sizeof(itsParseBuf)-itsParseBufLen);
						}
						
						continue;
					}else
					{
						//CRCУ�鲻��,������һ�β���
						strncpy(itsParseBuf,HEAD_PAS,HEAD_LEN);
						continue;
					}
				}else
				{
					//���ݳ��Ȳ���,�ȴ�

					//��Ϊ�˴�������ת��,������Ҫ��ԭʼ���Ļ�ԭ
					if (iBase::IsLittleEndin())
					{
						//���������С��ģʽ,�����ת��
						iBase::ConvertToOhterEndian(&pPackage->BufLen);
						iBase::ConvertToOhterEndian(&pPackage->BufCrc);
					}

					return;
				}
			}else
			{//����ͷ�ĳ���,�ȴ�
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
			//���ڳ���64KB����Ϣ,�����д���
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
			//���������С��ģʽ,�����ת��
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
