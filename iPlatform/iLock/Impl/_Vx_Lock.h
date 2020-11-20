/*********************************************
Author:Tanglx
Date:2019/08/16
FileName:_Vx_Lock.h

VxWorks 6.9 œ¬À¯ µœ÷
**********************************************/
#ifndef _FILENAME__VX_LOCK_
#define _FILENAME__VX_LOCK_

#include "semLibCommon.h"

class _Impl_Lock
{
public:
	_Impl_Lock(){
		itsCs = semMCreate(SEM_Q_FIFO);
	}
	~_Impl_Lock(){
		semDelete(itsCs);
	}
	void Lock(){
		semTake(itsCs,WAIT_FOREVER);
	}
	void Unlock(){
		semGive(itsCs);
	}
private:
	SEM_ID itsCs;
};

class _Impl_Sem
{
public:
	_Impl_Sem(){
		itsSem = semBCreate(SEM_Q_FIFO,SEM_EMPTY);
	}
	~_Impl_Sem(){
		semDelete(itsSem);
	}

	void Take(unsigned int nTimeOut){
		semTake(itsSem,nTimeOut);
	}
	void Give(){
		semGive(itsSem);
	}
private:
	SEM_ID itsSem;
};

#endif
