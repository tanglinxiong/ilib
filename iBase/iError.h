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
		ErrLevel_Sys,		//系统级别错误,应当记录错误并且直接退出
		ErrLevel_Logic,		//代码逻辑错误,应当记录错误并且直接退出
		ErrLevel_Connect,	//连接错误,包含本地连接,远程连接等,根据情况处理
		ErrLevel_Input,		//输入错误,根据情况处理
	};

public:
	int32 itsLevel;			//错误等级
	char itsFuncName[64];	//函数
	int32 itsLine;			//行号
	char itsDesc[64];		//描述
	int32 itsParamLen;		//附加信息长度
	void* itsParam;			//附加参数,用户自定义使用
};

#endif
