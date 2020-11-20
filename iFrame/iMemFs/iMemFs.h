/*********************************************
Author:Tanglx
Date:2020/01/03
FileName:iMemFs.h

主要用于
在给定的可随机访问的连续存储空间中
组建一个简单的文件系统

可以实现文件的增删改查

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
		Sector_Size = 64 * 4,		//扇区大小,必须为64的整数倍

		MemFile_Max_Sectors = 16,	//单文件最大占用16个扇区

		Init_Mask_A = 0x5e4f6d7c,	//
		Init_Mask_B = 0xe1e2e3e4,	//
	};

	enum Error
	{
		Succes,					//成功

		Reader_Invalid,		//阅读器不可用
		Writer_Invalid,		//写入器不可用
		InitFlag_Invalid,	//初始状态不可用
		BufLen_Invalid,		//缓冲区长度非法
		ReadBuf_Error,		//读取缓冲失败
		WriteBuf_Error,		//写入缓冲失败
		Format_Error,		//格式化失败
		MemFile_Exist,		//文件已存在
		MemFile_NotExist,	//文件不存在
		FileDes_UseOut,		//文件句柄已用完
		SectorDes_UseOut,	//扇区空间已用完
		MoreThan_SingleFileSize,	//写入的数据超过了单个文件的最大值
		Offset_TooLength,	//偏移超过了文件大小

		Logic_Fault,		//iMemFs 发生代码逻辑错误,
		Sys_Fault,			//iMemFs 发生致命错误,应当进行格式化
	};


	struct iHeadFlag
	{
		uint32 MaskA;			//是否初始化的掩码A
		uint32 MaskB;			//是否初始化的掩码B
		int32 nLen;				//总长度,主要用于校验
		int32 nDes;			//扇区个数

		iHeadFlag()
		{
			memset(this,0,sizeof(iHeadFlag));
		}
	};


	struct iFileDes
	{
		int32 nItsFileDesNo;			//本身编号,用于获取地址
		int32 nSectorDesNo;				//序号,如果为-1,则表示无指向
		char Name[32];					//名称
	};

	struct iSectorDes
	{
		int32 nItsSectorDesNo;			//本身编号,用于获取地址
		int32 nNextSectorDesNo;			//下一个扇区描述的编号,如果无下一个,则为-1;
		int32 nUsedBytes;				//该扇区使用的空间的大小 -1 表示该扇区未使用
	};

	struct iSector
	{
		char Buf[Sector_Size];
	};


	//辅助结构体
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
		//检查是否有读写器
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

		//读取iHeadFlag信息,看是否有初始化过
		iHeadFlag HeadFlag;

		bool bSucces = false;
		itsReadPtr->Execute(pBase,(char*)&HeadFlag,sizeof(iHeadFlag),&nReadLen,&bSucces);
		if (!bSucces)
		{
			return ReadBuf_Error;
		}

		//检查是否有初始化
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

		//读取索引
		int32 nDesLen = HeadFlag.nDes * sizeof(iFileDes) + HeadFlag.nDes*sizeof(iSectorDes);
		itsMngBuf = new char[nDesLen];
		memset(itsMngBuf,0,nDesLen);

		//索引首地址
		BaseOffsetAddr pDesAddr = pBase + sizeof(HeadFlag);
		itsReadPtr->Execute(pDesAddr,itsMngBuf,nDesLen,&nReadLen,&bSucces);
		if (!bSucces)
		{
			delete []itsMngBuf;
			itsMngBuf = NULL;

			return ReadBuf_Error;
		}

		//初始化信息
		itsBaseAddr = pBase;
		memcpy(&itsHeadFlag,&HeadFlag,sizeof(iHeadFlag));

		//根据读取信息,做初始化
		iFileDes* pFileDes = (iFileDes*)itsMngBuf;

		itsInUsedLock.Lock();
		for (int i=0;i<HeadFlag.nDes;++i)
		{
			if (pFileDes->nSectorDesNo != -1)
			{
				//使用的文件描述句柄
				itsFileDesMap[pFileDes->Name] = pFileDes;
			}else
			{
				//空闲的文件描述句柄
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
				//使用的扇区描述句柄
				itsSectorDesMap[pSectorDes->nItsSectorDesNo] = pSectorDes;
			}else
			{
				//空闲的扇区描述句柄
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
			//文件已经存在
			return MemFile_Exist;
		}

		//文件不存在,可以新建
		//先查找文件句柄空闲队列,看是否有空闲的文件句柄可以使用

		iFileDes* pFileDes = NULL;

		{
			iLifeSpanLock _lk(&itsFreeListLock);
			if (itsFreeFileDesList.size() == 0)
			{
				//文件描述句柄已用完
				return FileDes_UseOut;
			}
			pFileDes = itsFreeFileDesList.front();
			itsFreeFileDesList.pop_front();
		}


		//文件信息初始化
		strncpy(pFileDes->Name,FileName,sizeof(pFileDes->Name));
		pFileDes->nSectorDesNo = -1;


		//写文件
		bool bSucces = false;
		itsWritePtr->Execute(GetFileDesAddr(pFileDes->nItsFileDesNo),(char*)pFileDes,sizeof(iFileDes),&bSucces);

		if (!bSucces)
		{
			//失败,需要将分配出来的文件描述,以及扇区描述回填
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
			//文件已经存在
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
		//获取当前的文件信息
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}

		//清空
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

			//清空
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
		//获取当前的文件信息
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

			//清空
			itsInUsedLock.Lock();
			itsFreeListLock.Lock();
			itsSectorDesMap.erase((*it)->nItsSectorDesNo);
			
			//加入到空闲扇区
			itsFreeSectorList.push_back((*it));
			itsFreeListLock.Unlock();
			itsInUsedLock.Unlock();
		}

		return Succes;
	}

	//当Offset == -1时,表示Append
	int32 WriteMemFile(const char* FileName,char* WriteBuf,int32 WriteLen,int32 Offset = -1)
	{
		iFileDes* pFileDes = NULL;
		int32 nUsedSize = 0;
		int32 nAvlSize = 0;
		std::list<iSectorDes *> OriSectorDesList;

		int32 ErrCode = Succes;
		//获取当前的文件信息
		ErrCode = GetFileInfo(FileName,&pFileDes,OriSectorDesList,nUsedSize,nAvlSize);
		if (ErrCode != Succes)
		{
			return ErrCode;
		}


		//偏移超过了文件大小
		if (Offset != -1 && Offset > nUsedSize)
		{
			return Offset_TooLength;
		}

		//当Offset == -1 时,则Offset 应当等于当前文件的使用大小
		if (Offset == -1)
		{
			Offset = nUsedSize;
		}

		//计算期望的文件长度
		int32 nFurtureFileSize = Offset + WriteLen;

		//先判断是否超过了单个文件的最大长度
		if (nFurtureFileSize > MemFile_Max_Sectors * Sector_Size)
		{
			return MoreThan_SingleFileSize;
		}

		//根据期望的文件长度,计算需要新增的文件扇区
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


		//建立一个临时的扇区数组
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



		//确定需要写入的扇区编号,以及对应的偏移地址
		{
			int32 nBeginSectorsIdx = Offset/Sector_Size;
			int32 nBeginSectorOffset = Offset%Sector_Size;


			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			it += nBeginSectorsIdx;

			//只将需要修改的Sector放入
			WriteTempList.erase(WriteTempList.begin(),it);
			it = WriteTempList.begin();


			char* pNeedWriteBuf = WriteBuf;
			int32 nNeedWriteLen = WriteLen;


			//计算需要第一次写入的长度
			int nFirstWriteLen = nNeedWriteLen;
			if (nNeedWriteLen >= Sector_Size - nBeginSectorOffset)
			{
				nFirstWriteLen = Sector_Size - nBeginSectorOffset;
			}else
			{
				nFirstWriteLen = nNeedWriteLen;
			}

			

			//需要写入的第一个扇区
			int32 nErrCode = WriteSectors(&*it,pNeedWriteBuf,nFirstWriteLen,nBeginSectorOffset);
			if (nErrCode != Succes)
			{
				//写入失败,释放分配的扇区
				FreeSectors(NewSectorDesList);
				return nErrCode;
			}

			//写入成功
			nNeedWriteLen -= nFirstWriteLen;
			pNeedWriteBuf += nFirstWriteLen;
			++it;

			//循环写入
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
					//不可能到达这里,到这里表示程序逻辑有问题
					return Logic_Fault;
				}

				int32 nErrCode = WriteSectors(&*it,pNeedWriteBuf,nPerWriteLen,0);
				if (nErrCode != Succes)
				{
					//写入失败,释放分配的扇区
					FreeSectors(NewSectorDesList);
					return nErrCode;
				}

				//写入成功
				nNeedWriteLen -= nPerWriteLen;
				pNeedWriteBuf += nPerWriteLen;

				//
				++it;
			}
		}

		{
			//写如成功后,需要更新索引信息
			int32 nNextSectorsNo = -1;
			std::vector<TempSectorDesEx>::reverse_iterator rit = WriteTempList.rbegin();
			for(;rit != WriteTempList.rend();++rit)
			{
				//更新扇区使用大小
				(*rit).itsSectorDesPtr->nUsedBytes = (*rit).itsUsedSize;

				//更新扇区链
				(*rit).itsSectorDesPtr->nNextSectorDesNo = nNextSectorsNo;
				nNextSectorsNo = (*rit).itsSectorDesPtr->nItsSectorDesNo;
			}

			//写入成功后,更新文件描述
			if (pFileDes->nSectorDesNo == -1)
			{
				//更新文件描述信息
				pFileDes->nSectorDesNo = WriteTempList.begin()->itsSectorDesPtr->nItsSectorDesNo;
			}
		}

		{
			//将这些扇区插入到已使用中
			iLifeSpanLock _lk(&itsInUsedLock);
			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			for (;it != WriteTempList.end();++it)
			{
				itsSectorDesMap[(*it).itsSectorDesPtr->nItsSectorDesNo] = (*it).itsSectorDesPtr;
			}
		}

		{
			//更新文件信息
			int32 nErrCode = WriteFileDes(pFileDes);
			if (nErrCode != Succes)
			{
				//发生严重异常
				FreeSectors(NewSectorDesList);
				return Sys_Fault;
			}

			//更新扇区信息
			std::vector<TempSectorDesEx>::iterator it = WriteTempList.begin();
			for (;it != WriteTempList.end();++it)
			{
				int32 nErrCode = WriteSectorsDes((*it).itsSectorDesPtr);
				if (nErrCode != Succes)
				{
					//发生严重异常,释放分配的扇区
					FreeSectors(NewSectorDesList);
					return Sys_Fault;
				}
			}
		}

		return Succes;
	}
	int32 ReadMemFile(const char* FileName,char* ReadBuf,int32 ReadLen,int32& HadReadLen,int32 Offset = 0)
	{
		//初始化
		HadReadLen = 0;


		//获取当前的文件信息
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

		//检查
		if (Offset != 0 && Offset > nUsedSize)
		{
			return Offset_TooLength;
		}

		//可读大小
		int32 nAvlReadLen = nUsedSize - Offset;
		if (ReadLen > nAvlReadLen)
		{
			ReadLen = nAvlReadLen;
		}

		//无可读内容,直接退出
		if (ReadLen == 0)
		{
			return Succes;
		}

		//将扇区链表取出
		std::vector<iSectorDes*> ReadTempList;
		std::list<iSectorDes *>::iterator it = OriSectorDesList.begin();
		for (;it != OriSectorDesList.end();++it)
		{
			ReadTempList.push_back(*it);
		}

		{
			//取出首地址,并且进行读取
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
	//写入&读取HeadFlag 信息
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

	//写入&读取FileDes信息
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

	//用于格式化的MemFs
	int32 FormatMemFs(BaseOffsetAddr pBase,int32 nLen)
	{
		//检查是否有读写器
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
		//计算出当前的索引区,和数据扇区
		int32 DesCount = (ValidLen / (sizeof(iFileDes) + sizeof(iSectorDes) + Sector_Size));

		//分配空间
		int32 nBufLen = sizeof(iHeadFlag) + DesCount*sizeof(iFileDes) + DesCount*sizeof(iSectorDes);
		char* pBuf = new char[nBufLen];
		memset(pBuf,0,nBufLen);

		//头初始化
		iHeadFlag* pHeadFlag = (iHeadFlag*)pBuf;
		pHeadFlag->MaskA = Init_Mask_A;
		pHeadFlag->MaskB = Init_Mask_B;
		pHeadFlag->nLen = nLen;
		pHeadFlag->nDes = DesCount;

		//索引初始化
		iFileDes* pFileDes = (iFileDes*)(pHeadFlag+1);
		for (int i=0;i<DesCount;++i)
		{
			memset(pFileDes,0,sizeof(iFileDes));
			pFileDes->nItsFileDesNo = i;
			pFileDes->nSectorDesNo = -1;
			pFileDes++;
		}

		//初始化Des
		iSectorDes* pSectorDes = (iSectorDes*)pFileDes;
		iSector* pSecotrHead = (iSector*)(pSectorDes + DesCount);
		for (int i=0;i<DesCount;++i)
		{
			pSectorDes->nItsSectorDesNo = i;
			pSectorDes->nNextSectorDesNo = -1;
			pSectorDes->nUsedBytes = -1;
			pSectorDes++;
		}

		//写入
		bool bSucces = false;
		itsWritePtr->Execute(pBase,pBuf,nBufLen,&bSucces);

		//释放存储
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
			//文件不存在
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

		//将新的扇区分配出来
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

	//基地址
	BaseOffsetAddr itsBaseAddr;

	//创建之后获取到的信息
	iHeadFlag itsHeadFlag;

	//管理缓存
	char* itsMngBuf;

	//已使用的MAP
	TFileDesMap itsFileDesMap;
	TSectorDesMap itsSectorDesMap;
	iLock itsInUsedLock;

	//空闲的LIST
	TFreeFileDesList itsFreeFileDesList;
	TFreeSectorList itsFreeSectorList;
	iLock itsFreeListLock;
};

#endif
