/*********************************************
Author:Tanglx
Date:2020/01/03
FileName:iMemFs.h

��Ҫ����
�ڸ����Ŀ�������ʵ������洢�ռ���
�齨һ���򵥵��ļ�ϵͳ

����ʵ���ļ�����ɾ�Ĳ�

**********************************************/
#ifndef _FILENAME_IMEMFS_
#define _FILENAME_IMEMFS_

#include "../../iTypes.h"
#include "../../iPlatform/iLock/iLock.h"
#include "../../iPattern/iTrans.h"

#include <map>
#include <list>
#include <vector>
#include <string>

class iMemFs
{
public:
	enum
	{
		Sector_Size = 64 * 4,		//������С,����Ϊ64��������

		MemFile_Max_Sectors = 16,	//���ļ����ռ��16������

		Init_Mask_A = 0x5e4f6d7c,	//
		Init_Mask_B = 0xe1e2e3e4,	//
	};

	enum Error
	{
		Succes,					//�ɹ�

		Reader_Invalid,		//�Ķ���������
		Writer_Invalid,		//д����������
		InitFlag_Invalid,	//��ʼ״̬������
		BufLen_Invalid,		//���������ȷǷ�
		ReadBuf_Error,		//��ȡ����ʧ��
		WriteBuf_Error,		//д�뻺��ʧ��
		Format_Error,		//��ʽ��ʧ��
		MemFile_Exist,		//�ļ��Ѵ���
		MemFile_NotExist,	//�ļ�������
		FileDes_UseOut,		//�ļ����������
		SectorDes_UseOut,	//�����ռ�������
		MoreThan_SingleFileSize,	//д������ݳ����˵����ļ������ֵ
		Offset_TooLength,	//ƫ�Ƴ������ļ���С

		Logic_Fault,		//iMemFs ���������߼�����,
		Sys_Fault,			//iMemFs ������������,Ӧ�����и�ʽ��
	};


	struct iHeadFlag
	{
		uint32 MaskA;			//�Ƿ��ʼ��������A
		uint32 MaskB;			//�Ƿ��ʼ��������B
		int32 nLen;				//�ܳ���,��Ҫ����У��
		int32 nDes;			//��������

		iHeadFlag()
		{
			memset(this,0,sizeof(iHeadFlag));
		}
	};


	struct iFileDes
	{
		int32 nItsFileDesNo;			//������,���ڻ�ȡ��ַ
		int32 nSectorDesNo;				//���,���Ϊ-1,���ʾ��ָ��
		char Name[32];					//����
	};

	struct iSectorDes
	{
		int32 nItsSectorDesNo;			//������,���ڻ�ȡ��ַ
		int32 nNextSectorDesNo;			//��һ�����������ı��,�������һ��,��Ϊ-1;
		int32 nUsedBytes;				//������ʹ�õĿռ�Ĵ�С -1 ��ʾ������δʹ��
	};

	struct iSector
	{
		char Buf[Sector_Size];
	};


	//�����ṹ��
	struct TempSectorDesEx
	{
		iSectorDes* itsSectorDesPtr;
		int32 itsUsedSize;

		TempSectorDesEx(iSectorDes* p,int32 n)
			:itsSectorDesPtr(p),itsUsedSize(n){}
	};

	//
	struct FileSummary
	{
		char name[32];
		int32 nSize;
	};

public:
	typedef char* BaseOffsetAddr;
private:
	typedef std::map<std::string,iFileDes*> TFileDesMap;
	typedef std::map<int32,iSectorDes*> TSectorDesMap;
	typedef std::list<iFileDes*> TFreeFileDesList;
	typedef std::list<iSectorDes*> TFreeSectorList;

public:
	iMemFs()
	{
		itsReadPtr = NULL;
		itsWritePtr = NULL;

		itsBaseAddr = NULL;
		memset(&itsHeadFlag,0,sizeof(iHeadFlag));

		itsMngBuf = NULL;

		itsInUsedLock.Lock();
		itsFileDesMap.clear();
		itsSectorDesMap.clear();
		itsInUsedLock.Unlock();

		itsFreeListLock.Lock();
		itsFreeSectorList.clear();
		itsFreeFileDesList.clear();
		itsFreeListLock.Unlock();
	}
	~iMemFs()
	{
		itsInUsedLock.Lock();
		itsFileDesMap.clear();
		itsSectorDesMap.clear();
		itsInUsedLock.Unlock();

		itsFreeListLock.Lock();
		itsFreeSectorList.clear();
		itsFreeFileDesList.clear();
		itsFreeListLock.Unlock();

		if (itsMngBuf)
		{
			delete []itsMngBuf;
			itsMngBuf = NULL;
		}
	}

	int32 CreateMemFs(BaseOffsetAddr pBase,int32 nLen)
	{
		//����Ƿ��ж�д��
		if (!itsReadPtr)
		{
			return Reader_Invalid;
		}

		if (!itsWritePtr)
		{
			return Writer_Invalid;
		}

		BaseOffsetAddr pRead = pBase;
		int32 nReadLen = 0;
		int32 nErrCode = Succes;

		//��ȡiHeadFlag��Ϣ,���Ƿ��г�ʼ����
		iHeadFlag HeadFlag;

		bool bSucces = false;
		itsReadPtr->Execute(pBase,(char*)&HeadFlag,sizeof(iHeadFlag),&nReadLen,&bSucces);
		if (!bSucces)
		{
			return ReadBuf_Error;
		}

		//����Ƿ��г�ʼ��
		if (HeadFlag.MaskA != Init_Mask_A || HeadFlag.MaskB != Init_Mask_B)
		{
			if(FormatMemFs(pBase,nLen) != Succes)
			{
				return Format_Error;
			}else
			{
				itsReadPtr->Execute(pBase,(char*)&HeadFlag,sizeof(iHeadFlag),&nReadLen,&bSucces);
				if (!bSucces)
				{
					return ReadBuf_Error;
				}
			}
		}

		if (HeadFlag.nLen != nLen)
		{
			//
			return BufLen_Invalid;
		}

		//��ȡ����
		int32 nDesLen = HeadFlag.nDes * sizeof(iFileDes) + HeadFlag.nDes*sizeof(iSectorDes);
		itsMngBuf = new char[nDesLen];
		memset(itsMngBuf,0,nDesLen);

		//�����׵�ַ
		BaseOffsetAddr pDesAddr = pBase + sizeof(HeadFlag);
		itsReadPtr->Execute(pDesAddr,itsMngBuf,nDesLen,&nReadLen,&bSucces);
		if (!bSucces)
		{
			delete []itsMngBuf;
			itsMngBuf = NULL;

			return ReadBuf_Error;
		}

		//��ʼ����Ϣ
		itsBaseAddr = pBase;
		memcpy(&itsHeadFlag,&HeadFlag,sizeof(iHeadFlag));

		//���ݶ�ȡ��Ϣ,����ʼ��
		iFileDes* pFileDes = (iFileDes*)itsMngBuf;

		itsInUsedLock.Lock();
		for (int i=0;i<HeadFlag.nDes;++i)
		{
			if (pFileDes->nSectorDesNo != -1)
			{
				//ʹ�õ��ļ��������
				itsFileDesMap[pFileDes->Name] = pFileDes;
			}else
			{
				//���е��ļ��������
				itsFreeListLock.Lock();
				itsFreeFileDesList.push_back(pFileDes);
				itsFreeListLock.Unlock();
			}

			pFileDes++;
		}
		itsInUsedLock.Unlock();

		iSectorDes* pSectorDes = (iSectorDes*)pFileDes;
		itsFreeListLock.Lock();
		for (int i=0;i<HeadFlag.nDes;++i)
		{
			if (pSectorDes->nUsedBytes > 0)
			{
				//ʹ�õ������������
				itsSectorDesMap[pSectorDes->nItsSectorDesNo] = pSectorDes;
			}else
			{
				//���е������������
				itsFreeSectorList.push_back(pSectorDes);
			}
			pSectorDes++;
		}
		itsFreeListLock.Unlock();


		return Succes;
	}
	void DestoryMemFs()
	{
		
	}

	int32 NewMemFile(const char* FileName)
	{
		if (IsExistMemFile(FileName))
		{
			//�ļ��Ѿ�����
			return MemFile_Exist;
		}

		//�ļ�������,�����½�
		//�Ȳ����ļ�������ж���,���Ƿ��п��е��ļ��������ʹ��

		iFileDes* pFileDes = NULL;

		{
			iLifeSpanLock _lk(&itsFreeListLock);
			if (itsFreeFileDesList.size() == 0)
			{
				//�ļ��������������
				return FileDes_UseOut;
			}
			pFileDes = itsFreeFileDesList.front();
			itsFreeFileDesList.pop_front();
		}


		//�ļ���Ϣ��ʼ��
		strncpy(pFileDes->Name,FileName,sizeof(pFileDes->Name));
		pFileDes->nSectorDesNo = -1;


		//д�ļ�
		bool bSucces = false;
		itsWritePtr->Execute(GetFileDesAddr(pFileDes->nItsFileDesNo),(char*)pFileDes,sizeof(iFileDes),&bSucces);

		if (!bSucces)
		{
			//ʧ��,��Ҫ������������ļ�����,�Լ�������������
			memset(pFileDes->Name,0,sizeof(pFileDes->Name));
			pFileDes->nSectorDesNo = -1;
			
			iLifeSpanLock _lk(&itsFreeListLock);
			itsFreeFileDesList.push_back(pFileDes);

			return WriteBuf_Error;
		}

		itsInUsedLock.Lock();
		itsFileDesMap[pFileDes->Name] = pFileDes;
		itsInUsedLock.Unlock();

		return Succes;
	}
	bool IsExistMemFile(const char* FileName)
	{
		iLifeSpanLock _lk(&itsInUsedLock);
		TFileDesMap::iterator it = itsFileDesMap.find(FileName);
		if (it != itsFileDesMap.end())
		{
			//�ļ��Ѿ�����
			return true;
		}
		return false;
	}
	int32 DelMemFile(const char* FileName)
	{
		if (!IsExistMemFile(FileName))
		{
			return MemFile_NotExist;
		}

		iFileDes* pFileDes = NULL;
		int32 nUsedSize = 0;
		int32 nAvlSize = 0;
		std::list<iSectorDes *> OriSectorDesList;

		int32 ErrCode = Succes;
		//��ȡ��ǰ���ļ���Ϣ
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}

		//���
		itsInUsedLock.Lock();
		itsFileDesMap.erase(pFileDes->Name);
		itsInUsedLock.Unlock();

		pFileDes->nSectorDesNo = -1;
		memset(pFileDes->Name,0,sizeof(pFileDes->Name));

		ErrCode = WriteFileDes(pFileDes);
		if (ErrCode != Succes)
		{
			return Sys_Fault;
		}

		std::list<iSectorDes *>::iterator it = OriSectorDesList.begin();
		for (;it != OriSectorDesList.end();++it)
		{
			(*it)->nUsedBytes = -1;
			(*it)->nNextSectorDesNo = -1;

			ErrCode = WriteSectorsDes(*it);
			if (ErrCode != Succes)
			{
				return Sys_Fault;
			}

			//���
			itsInUsedLock.Lock();
			itsSectorDesMap.erase((*it)->nItsSectorDesNo);
			itsInUsedLock.Unlock();
		}

		return Succes;
	}

	int32 ClearMemFile(const char* FileName)
	{
		if (!IsExistMemFile(FileName))
		{
			return MemFile_NotExist;
		}

		iFileDes* pFileDes = NULL;
		int32 nUsedSize = 0;
		int32 nAvlSize = 0;
		std::list<iSectorDes *> OriSectorDesList;

		int32 ErrCode = Succes;
		//��ȡ��ǰ���ļ���Ϣ
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}

		pFileDes->nSectorDesNo = -1;

		ErrCode = WriteFileDes(pFileDes);
		if (ErrCode != Succes)
		{
			return Sys_Fault;
		}

		std::list<iSectorDes *>::iterator it = OriSectorDesList.begin();
		for (;it != OriSectorDesList.end();++it)
		{
			(*it)->nUsedBytes = -1;
			(*it)->nNextSectorDesNo = -1;

			ErrCode = WriteSectorsDes(*it);
			if (ErrCode != Succes)
			{
				return Sys_Fault;
			}

			//���
			itsInUsedLock.Lock();
			itsFreeListLock.Lock();
			itsSectorDesMap.erase((*it)->nItsSectorDesNo);
			
			//���뵽��������
			itsFreeSectorList.push_back((*it));
			itsFreeListLock.Unlock();
			itsInUsedLock.Unlock();
		}

		return Succes;
	}

	//��Offset == -1ʱ,��ʾAppend
	int32 WriteMemFile(const char* FileName,char* WriteBuf,int32 WriteLen,int32 Offset = -1)
	{
		iFileDes* pFileDes = NULL;
		int32 nUsedSize = 0;
		int32 nAvlSize = 0;
		std::list<iSectorDes *> OriSectorDesList;

		int32 ErrCode = Succes;
		//��ȡ��ǰ���ļ���Ϣ
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}


		//ƫ�Ƴ������ļ���С
		if (Offset != -1 && Offset > nUsedSize)
		{
			return Offset_TooLength;
		}

		//��Offset == -1 ʱ,��Offset Ӧ�����ڵ�ǰ�ļ���ʹ�ô�С
		if (Offset == -1)
		{
			Offset = nUsedSize;
		}

		//�����������ļ�����
		int32 nFurtureFileSize = Offset + WriteLen;

		//���ж��Ƿ񳬹��˵����ļ�����󳤶�
		if (nFurtureFileSize > MemFile_Max_Sectors * Sector_Size)
		{
			return MoreThan_SingleFileSize;
		}

		//�����������ļ�����,������Ҫ�������ļ�����
		std::list<iSectorDes *> NewSectorDesList;
		int32 nNeedNewBytes = nFurtureFileSize - nAvlSize;
		if (nNeedNewBytes > 0)
		{
			int32 nNeedNewSectors = nNeedNewBytes/Sector_Size + ((nNeedNewBytes%Sector_Size)?1:0);

			int32 nErrCode = AllocSectors(NewSectorDesList,nNeedNewSectors);
			if (nErrCode != Succes)
			{
				return nErrCode;
			}
		}


		//����һ����ʱ����������
		std::vector<TempSectorDesEx> WriteTempList;
		std::list<iSectorDes*>::iterator it = OriSectorDesList.begin();
		for (;it != OriSectorDesList.end();++it)
		{
			WriteTempList.push_back(TempSectorDesEx(*it,0));
		}

		it = NewSectorDesList.begin();
		for (;it != NewSectorDesList.end();++it)
		{
			WriteTempList.push_back(TempSectorDesEx(*it,0));
		}



		//ȷ����Ҫд����������,�Լ���Ӧ��ƫ�Ƶ�ַ
		{
			int32 nBeginSectorsIdx = Offset/Sector_Size;
			int32 nBeginSectorOffset = Offset%Sector_Size;


			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			it += nBeginSectorsIdx;

			//ֻ����Ҫ�޸ĵ�Sector����
			WriteTempList.erase(WriteTempList.begin(),it);
			it = WriteTempList.begin();


			char* pNeedWriteBuf = WriteBuf;
			int32 nNeedWriteLen = WriteLen;


			//������Ҫ��һ��д��ĳ���
			int nFirstWriteLen = nNeedWriteLen;
			if (nNeedWriteLen >= Sector_Size - nBeginSectorOffset)
			{
				nFirstWriteLen = Sector_Size - nBeginSectorOffset;
			}else
			{
				nFirstWriteLen = nNeedWriteLen;
			}

			

			//��Ҫд��ĵ�һ������
			int32 nErrCode = WriteSectors(&*it,pNeedWriteBuf,nFirstWriteLen,nBeginSectorOffset);
			if (nErrCode != Succes)
			{
				//д��ʧ��,�ͷŷ��������
				FreeSectors(NewSectorDesList);
				return nErrCode;
			}

			//д��ɹ�
			nNeedWriteLen -= nFirstWriteLen;
			pNeedWriteBuf += nFirstWriteLen;
			++it;

			//ѭ��д��
			while (it != WriteTempList.end() && nNeedWriteLen > 0)
			{
				int nPerWriteLen = nNeedWriteLen;
				if (nNeedWriteLen >= Sector_Size)
				{
					nPerWriteLen = Sector_Size;
				}else
				{
					nPerWriteLen = nNeedWriteLen;
				}

				if (nPerWriteLen <= 0)
				{
					//�����ܵ�������,�������ʾ�����߼�������
					return Logic_Fault;
				}

				int32 nErrCode = WriteSectors(&*it,pNeedWriteBuf,nPerWriteLen,0);
				if (nErrCode != Succes)
				{
					//д��ʧ��,�ͷŷ��������
					FreeSectors(NewSectorDesList);
					return nErrCode;
				}

				//д��ɹ�
				nNeedWriteLen -= nPerWriteLen;
				pNeedWriteBuf += nPerWriteLen;

				//
				++it;
			}
		}

		{
			//д��ɹ���,��Ҫ����������Ϣ
			int32 nNextSectorsNo = -1;
			std::vector<TempSectorDesEx>::reverse_iterator rit = WriteTempList.rbegin();
			for(;rit != WriteTempList.rend();++rit)
			{
				//��������ʹ�ô�С
				(*rit).itsSectorDesPtr->nUsedBytes = (*rit).itsUsedSize;

				//����������
				(*rit).itsSectorDesPtr->nNextSectorDesNo = nNextSectorsNo;
				nNextSectorsNo = (*rit).itsSectorDesPtr->nItsSectorDesNo;
			}

			//д��ɹ���,�����ļ�����
			if (pFileDes->nSectorDesNo == -1)
			{
				//�����ļ�������Ϣ
				pFileDes->nSectorDesNo = WriteTempList.begin()->itsSectorDesPtr->nItsSectorDesNo;
			}
		}

		{
			//����Щ�������뵽��ʹ����
			iLifeSpanLock _lk(&itsInUsedLock);
			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			for (;it != WriteTempList.end();++it)
			{
				itsSectorDesMap[(*it).itsSectorDesPtr->nItsSectorDesNo] = (*it).itsSectorDesPtr;
			}
		}

		{
			//�����ļ���Ϣ
			int32 nErrCode = WriteFileDes(pFileDes);
			if (nErrCode != Succes)
			{
				//���������쳣
				FreeSectors(NewSectorDesList);
				return Sys_Fault;
			}

			//����������Ϣ
			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			for (;it != WriteTempList.end();++it)
			{
				int32 nErrCode = WriteSectorsDes((*it).itsSectorDesPtr);
				if (nErrCode != Succes)
				{
					//���������쳣,�ͷŷ��������
					FreeSectors(NewSectorDesList);
					return Sys_Fault;
				}
			}
		}

		return Succes;
	}
	int32 ReadMemFile(const char* FileName,char* ReadBuf,int32 ReadLen,int32& HadReadLen,int32 Offset = 0)
	{
		//��ʼ��
		HadReadLen = 0;


		//��ȡ��ǰ���ļ���Ϣ
		iFileDes* pFileDes = NULL;
		int32 nUsedSize = 0;
		int32 nAvlSize = 0;
		std::list<iSectorDes *> OriSectorDesList;

		int32 ErrCode = Succes;
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}

		//���
		if (Offset != 0 && Offset > nUsedSize)
		{
			return Offset_TooLength;
		}

		//�ɶ���С
		int32 nAvlReadLen = nUsedSize - Offset;
		if (ReadLen > nAvlReadLen)
		{
			ReadLen = nAvlReadLen;
		}

		//�޿ɶ�����,ֱ���˳�
		if (ReadLen == 0)
		{
			return Succes;
		}

		//����������ȡ��
		std::vector<iSectorDes*> ReadTempList;
		std::list<iSectorDes *>::iterator it = OriSectorDesList.begin();
		for (;it != OriSectorDesList.end();++it)
		{
			ReadTempList.push_back(*it);
		}

		{
			//ȡ���׵�ַ,���ҽ��ж�ȡ
			int32 nBeginSectorsIdx = Offset/Sector_Size;
			int32 nBeginSectorsOffset = Offset%Sector_Size;

			std::vector<iSectorDes*>::iterator it = ReadTempList.begin();
			it += nBeginSectorsIdx;

			int32 nNeedReadLen = ReadLen;
			char* pNeedReadBuf = ReadBuf;


			int32 nFirstReadLen = nNeedReadLen;
			if (nFirstReadLen > Sector_Size-nBeginSectorsOffset)
			{
				nFirstReadLen = Sector_Size-nBeginSectorsOffset;
			}else
			{
				nFirstReadLen = nNeedReadLen;
			}

			int32 nHadReadLen = 0;
			ErrCode = ReadSectors((*it)->nItsSectorDesNo,pNeedReadBuf,nFirstReadLen,&nHadReadLen,nBeginSectorsOffset);
			if (ErrCode != Succes)
			{
				return ErrCode;
			}
			++it;

			nNeedReadLen -= nFirstReadLen;
			pNeedReadBuf += nFirstReadLen;

			HadReadLen += nFirstReadLen;

			while(nNeedReadLen > 0 && it != ReadTempList.end())
			{
				int32 nPerReadLen = nNeedReadLen;
				if (nNeedReadLen > Sector_Size)
				{
					nPerReadLen = Sector_Size;
				}else
				{
					nPerReadLen = nNeedReadLen;
				}

				ErrCode = ReadSectors((*it)->nItsSectorDesNo,pNeedReadBuf,nPerReadLen,&nHadReadLen,0);
				if (ErrCode != Succes)
				{
					return ErrCode;
				}
				HadReadLen += nPerReadLen;

				nNeedReadLen -= nPerReadLen;
				pNeedReadBuf += nPerReadLen;

				++it;
			}

			return Succes;
		}
	}

	void GetAllFilesInfo(std::list<FileSummary>& SummaryList)
	{
		iLifeSpanLock _lk(&itsInUsedLock);
		TFileDesMap::iterator it = itsFileDesMap.begin();
		for(;it != itsFileDesMap.end();++it)
		{
			iFileDes* pFD = it->second;
			std::list<iSectorDes *> OriSectorsList;
			int32 nInUsedSize = 0;
			int32 nAvlSize = 0;
			GetFileInfo(pFD->Name,&pFD,OriSectorsList,nInUsedSize,nAvlSize);

			FileSummary FS;
			strncpy(FS.name,pFD->Name,sizeof(FS.name));
			FS.nSize = nInUsedSize;
		}
	}

private:
public:
	//д��&��ȡHeadFlag ��Ϣ
	int32 WriteHeadFlag(const iHeadFlag& HeadFlag)
	{
		bool bSucces = false;
		itsWritePtr->Execute(itsBaseAddr,(char*)&HeadFlag,sizeof(iHeadFlag),&bSucces);
		if (bSucces)
		{
			return Succes;
		}
		return WriteBuf_Error;
	}

	int32 ReadHeadFlag(iHeadFlag& HeadFlag)
	{
		int32 nReadLen = 0;
		bool bSucces = false;
		itsReadPtr->Execute(itsBaseAddr,(char*)&HeadFlag,sizeof(iHeadFlag),&nReadLen,&bSucces);

		if (bSucces)
		{
			return Succes;
		}
		return ReadBuf_Error;
	}

	//д��&��ȡFileDes��Ϣ
	int32 WriteFileDes(const iFileDes* pFileDes)
	{
		bool bSucces = false;
		BaseOffsetAddr addr = GetFileDesAddr(pFileDes->nItsFileDesNo);
		itsWritePtr->Execute(addr,(char*)pFileDes,sizeof(iFileDes),&bSucces);
		if (bSucces)
		{
			return Succes;
		}
		return WriteBuf_Error;
	}

	int32 ReadFileDes(int32 nFileDesNo,iFileDes* pFileDes)
	{
		int32 nReadLen = 0;
		bool bSucces = false;
		BaseOffsetAddr addr = GetFileDesAddr(nFileDesNo);
		itsReadPtr->Execute(itsBaseAddr,(char*)pFileDes,sizeof(iFileDes),&nReadLen,&bSucces);

		if (bSucces)
		{
			return Succes;
		}
		return ReadBuf_Error;
	}

	//���ڸ�ʽ����MemFs
	int32 FormatMemFs(BaseOffsetAddr pBase,int32 nLen)
	{
		//����Ƿ��ж�д��
		if (!itsReadPtr)
		{
			return Reader_Invalid;
		}

		if (!itsWritePtr)
		{
			return Writer_Invalid;
		}

		int nErrorCode = Succes;

		int32 ValidLen = nLen - sizeof(iHeadFlag);
		//�������ǰ��������,����������
		int32 DesCount = (ValidLen / (sizeof(iFileDes) + sizeof(iSectorDes) + Sector_Size));

		//����ռ�
		int32 nBufLen = sizeof(iHeadFlag) + DesCount*sizeof(iFileDes) + DesCount*sizeof(iSectorDes);
		char* pBuf = new char[nBufLen];
		memset(pBuf,0,nBufLen);

		//ͷ��ʼ��
		iHeadFlag* pHeadFlag = (iHeadFlag*)pBuf;
		pHeadFlag->MaskA = Init_Mask_A;
		pHeadFlag->MaskB = Init_Mask_B;
		pHeadFlag->nLen = nLen;
		pHeadFlag->nDes = DesCount;

		//������ʼ��
		iFileDes* pFileDes = (iFileDes*)(pHeadFlag+1);
		for (int i=0;i<DesCount;++i)
		{
			memset(pFileDes,0,sizeof(iFileDes));
			pFileDes->nItsFileDesNo = i;
			pFileDes->nSectorDesNo = -1;
			pFileDes++;
		}

		//��ʼ��Des
		iSectorDes* pSectorDes = (iSectorDes*)pFileDes;
		iSector* pSecotrHead = (iSector*)(pSectorDes + DesCount);
		for (int i=0;i<DesCount;++i)
		{
			pSectorDes->nItsSectorDesNo = i;
			pSectorDes->nNextSectorDesNo = -1;
			pSectorDes->nUsedBytes = -1;
			pSectorDes++;
		}

		//д��
		bool bSucces = false;
		itsWritePtr->Execute(pBase,pBuf,nBufLen,&bSucces);

		//�ͷŴ洢
		delete []pBuf;

		if (nErrorCode != Succes)
		{
			return WriteBuf_Error;
		}
		return Succes;
	}

	int32 GetFileInfo(const char* FileName,iFileDes** ppFileDes,std::list<iSectorDes *>& SectorDesList,int32& nUsedSize,int32& nAvlSize)
	{
		iFileDes* pFileDes = NULL;
		iSectorDes* pSectorDes = NULL;
		nUsedSize = 0;

		iLifeSpanLock _lk(&itsInUsedLock);
		TFileDesMap::iterator it = itsFileDesMap.find(FileName);
		if (it == itsFileDesMap.end())
		{
			//�ļ�������
			return MemFile_NotExist;
		}
		pFileDes = it->second;

		int32 nSectorDesNo = pFileDes->nSectorDesNo;

		while(nSectorDesNo != -1)
		{
			TSectorDesMap::iterator it2 = itsSectorDesMap.find(nSectorDesNo);
			if (it2 == itsSectorDesMap.end())
			{
				return Sys_Fault;
			}
			pSectorDes = it2->second;

			//
			SectorDesList.push_back(pSectorDes);

			//
			nSectorDesNo = pSectorDes->nNextSectorDesNo;

			//
			nUsedSize += pSectorDes->nUsedBytes;
		}

		nAvlSize = SectorDesList.size() * Sector_Size;
		*ppFileDes = pFileDes;

		return Succes;
	}

	BaseOffsetAddr GetFileDesAddr(int32 nFileDesNo)
	{
		BaseOffsetAddr p = itsBaseAddr;
		p += sizeof(iHeadFlag);
		p += nFileDesNo * sizeof(iFileDes);
		return p;
	}

	BaseOffsetAddr GetSectorDesAddr(int32 nSectorDesNo)
	{
		BaseOffsetAddr p = itsBaseAddr;
		p += sizeof(iHeadFlag);
		p += itsHeadFlag.nDes * sizeof(iFileDes);
		p += nSectorDesNo * sizeof(iSectorDes);
		return p;
	}

	BaseOffsetAddr GetSectorAddr(int32 nSectorDesNo)
	{
		BaseOffsetAddr p = itsBaseAddr;
		p += sizeof(iHeadFlag);
		p += itsHeadFlag.nDes * sizeof(iFileDes);
		p += itsHeadFlag.nDes * sizeof(iSectorDes);
		p += nSectorDesNo * Sector_Size;
		return p;
	}

	int32 WriteSectorsDes(iSectorDes* pSectorDes)
	{
		BaseOffsetAddr addr = GetSectorDesAddr(pSectorDes->nItsSectorDesNo);
		bool bSucces = false;
		itsWritePtr->Execute(addr,(char*)pSectorDes,sizeof(iSectorDes),&bSucces);

		if (bSucces)
		{
			return Succes;
		}
		return WriteBuf_Error;
	}

	int32 ReadSectorsDes(int32 nSectorDesNo,iSectorDes* pSectorDes)
	{
		return Succes;
	}

	int32 WriteSectors(TempSectorDesEx* pTsd,char* WriteBuf,int32 WriteLen,int32 Offset = 0)
	{
		BaseOffsetAddr addr = GetSectorAddr(pTsd->itsSectorDesPtr->nItsSectorDesNo);
		addr += Offset;
		bool bSucces = false;

		//printf("WritSectors:0x%x,Len:%d,SectorNo:%d,Base:0x%x,Offset:%d,",addr,WriteLen,pTsd->itsSectorDesPtr->nItsSectorDesNo,itsBaseAddr,Offset);

		itsWritePtr->Execute(addr,WriteBuf,WriteLen,&bSucces);

		//printf("%s\r\n",bSucces?"Succes":"Failed");

		pTsd->itsUsedSize = (Offset+WriteLen);

		if (bSucces)
		{
			return Succes;
		}
		return WriteBuf_Error;
	}

	int32 ReadSectors(int32 nSectorDesNo,char* ReadBuf,int32 ReadLen,int32* nHadReadLen,int32 Offset = 0)
	{
		BaseOffsetAddr addr = GetSectorAddr(nSectorDesNo);
		addr += Offset;
		bool bSucces = false;

		//printf("ReadSectors:0x%x,Len:%d,SectorNo:%d,Base:0x%x,Offset:%d,",addr,ReadLen,nSectorDesNo,itsBaseAddr,Offset);

		itsReadPtr->Execute(addr,ReadBuf,ReadLen,nHadReadLen,&bSucces);

		//printf("%s\r\n",bSucces?"Succes":"Failed");

		if (bSucces)
		{
			return Succes;
		}
		return ReadBuf_Error;
	}

	int32 AllocSectors(std::list<iSectorDes *>& AllocSectorsList,int32 nSectorsCount)
	{
		iLifeSpanLock _lk(&itsFreeListLock);
		if (nSectorsCount > itsFreeSectorList.size())
		{
			return SectorDes_UseOut;
		}

		//���µ������������
		for (int i=0;i<nSectorsCount;++i)
		{
			AllocSectorsList.push_back(itsFreeSectorList.front());
			itsFreeSectorList.pop_front();
		}

		return Succes;
	}

	int32 FreeSectors(const std::list<iSectorDes *>& FreeSectorsList)
	{
		iLifeSpanLock _lk(&itsFreeListLock);

		std::list<iSectorDes *>::const_iterator it = FreeSectorsList.begin();
		for (;it != FreeSectorsList.end();++it)
		{
			itsFreeSectorList.push_back(*it);
		}
		return Succes;
	}

public:
	template<class T>
	void RegReadPtr(T* pObj,void(T::* pFunc)(BaseOffsetAddr,char*,int32,int32*,bool*))
	{
		itsReadPtr = new iTrans5::Leaf<T,BaseOffsetAddr,char*,int32,int32*,bool*>(pObj,pFunc);
	}

	template<class T>
	void RegWritePtr(T* pObj,void(T::* pFunc)(BaseOffsetAddr,char*,int32,bool*))
	{
		itsWritePtr = new iTrans4::Leaf<T,BaseOffsetAddr,char*,int32,bool*>(pObj,pFunc);
	}

private:
	iTrans5::Branch<BaseOffsetAddr,char*,int32,int32*,bool*>* itsReadPtr;
	iTrans4::Branch<BaseOffsetAddr,char*,int32,bool*>* itsWritePtr;

	//����ַ
	BaseOffsetAddr itsBaseAddr;

	//����֮���ȡ������Ϣ
	iHeadFlag itsHeadFlag;

	//������
	char* itsMngBuf;

	//��ʹ�õ�MAP
	TFileDesMap itsFileDesMap;
	TSectorDesMap itsSectorDesMap;
	iLock itsInUsedLock;

	//���е�LIST
	TFreeFileDesList itsFreeFileDesList;
	TFreeSectorList itsFreeSectorList;
	iLock itsFreeListLock;
};

#endif
