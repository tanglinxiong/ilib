/*********************************************
Author:Tanglx
Date:2019/08/16
FileName:_Vx_Public.h
**********************************************/
#ifndef _FILENAME__VX_PUBLIC_
#define _FILENAME__VX_PUBLIC_

#include "taskLibCommon.h"

class _Impl_Public
{
public:
	static void _Sleep(int ms){
		taskDelay(ms);
	}
};

#endif
