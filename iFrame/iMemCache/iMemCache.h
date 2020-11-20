/*********************************************
Author:Tanglx
Date:2020/08/21
FileName:iMemCache.h
Describe:���ڹ�����Ҫ����������ͷŵĴ洢,����߳�����ʴ洢���ٶ�.
Ҳ����һ���̶��ϵļ�С�ڴ���Ƭ�������⡣
**********************************************/
#ifndef _FILENAME_IMEMCACHE_
#define _FILENAME_IMEMCACHE_

//iLib
#include "../../iPlatform/iLock/iLock.h"

//STL
#include <list>

#define MEM_INCREASE_SIZE 1000		//ÿ���ڴ�Ԫ�ظ�������ʱ��,���ӵ���

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