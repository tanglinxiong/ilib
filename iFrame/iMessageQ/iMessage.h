/*********************************************
Author:Tanglx
Date:2019/08/28
FileName:iMessage.h
��Ϣ�ṹ��
**********************************************/
#ifndef _FILENAME_IMESSAGE_
#define _FILENAME_IMESSAGE_

#include "../../iTypes.h"

typedef uint32 MSGHANDLE;

#include "string.h"
struct iMessage
{
    MSGHANDLE itsHandle;        //���ֶα������й��ڴ�������Ψһ��ʶ��,һ�����ʹ�ô������ĵ�ַ��Ϊ��������
    uint32 itsMsgNo;
    uint8 *itsBuf;
    uint32 itsLen;

    iMessage()
    {
        memset(this, 0, sizeof(iMessage));
    }
};
#endif
