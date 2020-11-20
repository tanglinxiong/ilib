/*********************************************
Author:Tanglx
Date:2019/08/15
FileName:iLock.h
»¥³âËø,Í¬²½Ëø
**********************************************/
#ifndef _FILENAME_ILOCK_
#define _FILENAME_ILOCK_

#include "../iPlatform.h"
#include "../../iTypes.h"

#if (ITS_PLATFORM == Windows)
	#include "Impl/_Win_Lock.h"
#elif (ITS_PLATFORM == Vxworks)
	#include "Impl/_Vx_Lock.h"
#else
	#error "Can Not Find Adjust Platform"
#endif

class iLock
{
public:
	void Lock(){
		itsLock.Lock();
	}
	void Unlock(){
		itsLock.Unlock();
	}
private:
	_Impl_Lock itsLock;
};

class iLifeSpanLock
{
public:
	iLifeSpanLock(iLock* LockPtr){
		itsLockPtr = LockPtr;
		itsLockPtr->Lock();
	}
	~iLifeSpanLock(){
		itsLockPtr->Unlock();
	}
private:
	iLock* itsLockPtr;
};

class iSem
{
public:
	void Take(uint32 nTimeOut=-1){
		itsSem.Take(nTimeOut);
	}
	void Give(){
		itsSem.Give();
	}
private:
	_Impl_Sem itsSem;
};

#endif
