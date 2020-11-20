/*********************************************
Author:Tanglx
Date:2020/08/21
FileName:iMemCache.h
Describe:用于管理需要经常分配和释放的存储,以提高程序访问存储的速度.
也可以一定程度上的减小内存碎片化的问题。
**********************************************/
#ifndef _FILENAME_IMEMCACHE_
#define _FILENAME_IMEMCACHE_

//iLib
#include "../../iPlatform/iLock/iLock.h"

//STL
#include <list>

#define MEM_INCREASE_SIZE 1000		//每次内存元素个数不足时候,增加的量

template<class T>
class iMemCache
{
public:
	iMemCache(){}
	~iMemCache(){}

	void CreateMem(int32 Count = MEM_INCREASE_SIZE)
	{
		T* pData = new T[Count];

		itsHeaderListLock.Lock();
		itsHeaderList.push_back(pData);
		itsHeaderListLock.Unlock();

		itsUnUsedListLock.Lock();
		for (int i=0;i<Count;++i)
		{
			itsUnUsedList.push_back(pData+i);
		}
		itsUnUsedListLock.Unlock();

	}
	T* AllocMem()
	{
		iLifeSpanLock _lk(&itsUnUsedListLock);
		if (itsUnUsedList.size() <= 0)
		{
			CreateMem();
		}

		T* p = itsUnUsedList.front();
		itsUnUsedList.pop_front();
		//printf("AllocMem:0x%x\n",p);
		return p;
	}
	void FreeMem(T* pData)
	{
		//printf("FreeMem:0x%x\n",pData);
		iLifeSpanLock _lk(&itsUnUsedListLock);
		itsUnUsedList.push_back(pData);
	}
	void DestoryMem(){
		itsUnUsedListLock.Lock();
		itsUnUsedList.clear();
		itsUnUsedListLock.Unlock();

		itsHeaderListLock.Lock();
		std::list<T*>::iterator it = itsHeaderList.begin();
		for (;it != itsHeaderList.end();++it)
		{
			delete []*it;
		}
		itsHeaderList.clear();
		itsHeaderListLock.Unlock();
	}
private:
	std::list<T*> itsHeaderList;
	iLock itsHeaderListLock;

	std::list<T*> itsUnUsedList;
	iLock itsUnUsedListLock;
};
#endif