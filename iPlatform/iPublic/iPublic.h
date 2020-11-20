/*********************************************
Author:Tanglx
Date:2019/08/16
FileName:iPublic.h
**********************************************/
#ifndef _FILENAME_IPUBLIC_
#define _FILENAME_IPUBLIC_

#include "../iPlatform.h"

#if (ITS_PLATFORM == Windows)
#include "Impl/_Win_Public.h"
#elif (ITS_PLATFORM == Vxworks)
#include "Impl/_Vx_Public.h"
#else
#error "Can Not Find Adjust Platform"
#endif

class iPublic
{
public:
    static void iSleep(int ms)
    {
        _Impl_Public::_Sleep(ms);
    }
};

#endif