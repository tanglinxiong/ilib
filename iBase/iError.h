/*********************************************
Author:Tanglx
Date:2019/09/18
FileName:iError.h
**********************************************/
#ifndef _FILENAME_IERROR_
#define _FILENAME_IERROR_

#include "../iTypes.h"
#include "string.h"
class iError
{
public:
	iError(int32 lv,const char* funcname,int32 lineno,const char* desc,int32 paramlen = 0,void* param = NULL)
	{
		itsLevel = lv;
		strncpy(itsFuncName,funcname,sizeof(itsFuncName));
		itsLine = lineno;
		strncpy(itsDesc,desc,sizeof(itsDesc));
		itsParamLen = paramlen;
		itsParam = param;
	}
	~iError(){}

	enum ErrLevel
	{
		ErrLevel_Sys,		//ϵͳ�������,Ӧ����¼������ֱ���˳�
		ErrLevel_Logic,		//�����߼�����,Ӧ����¼������ֱ���˳�
		ErrLevel_Connect,	//���Ӵ���,������������,Զ�����ӵ�,�����������
		ErrLevel_Input,		//�������,�����������
	};

public:
	int32 itsLevel;			//����ȼ�
	char itsFuncName[64];	//����
	int32 itsLine;			//�к�
	char itsDesc[64];		//����
	int32 itsParamLen;		//������Ϣ����
	void* itsParam;			//���Ӳ���,�û��Զ���ʹ��
};

#endif
