/*********************************************
Author:Tanglx
Date:2019/08/28
FileName:iMessage.h
消息结构体
**********************************************/
#ifndef _FILENAME_IMESSAGE_
#define _FILENAME_IMESSAGE_

#include "../../iTypes.h"

typedef uint32 MSGHANDLE;

#include "string.h"
struct iMessage
{
    MSGHANDLE itsHandle;        //该字段必须是有关于处理对象的唯一标识符,一般可以使用处理对象的地址作为参数传入
    uint32 itsMsgNo;
    uint8 *itsBuf;
    uint32 itsLen;

    iMessage()
    {
        memset(this, 0, sizeof(iMessage));
    }
};
#endif
