/*********************************************
Author:Tanglx
Date:2019/10/31
FileName:iTips.h
一些小的功能函数
**********************************************/
#ifndef _FILENAME_ITIPS_
#define _FILENAME_ITIPS_

#include <string>
#define MAX_PATH 255
namespace iTips
{
	//从文件路径获取到其所在目录
	static std::string GetDirFormPath(const char* path)
	{
		char _temp[MAX_PATH] = {0};
		strncpy(_temp,path,sizeof(_temp));
		char* idx1 = strrchr(_temp,char('/'));
		char* idx2 = strrchr(_temp,char('\\'));

		idx1 = idx1>=idx2?idx1:idx2;

		*idx1='\0';
		return _temp;
	}

	static std::string GetNameFromPath(const char* path)
	{
		char _temp[MAX_PATH] = {0};
		strncpy(_temp,path,sizeof(_temp));
		char* idx1 = strrchr(_temp,char('/'));
		char* idx2 = strrchr(_temp,char('\\'));

		idx1 = idx1>=idx2?idx1:idx2;
		return ++idx1;
	}

	//从文件路径获取到其根目录
	static std::string GetRootDirFormPath(const char* path)
	{
		char _temp[MAX_PATH] = {0};
		strncpy(_temp,path,sizeof(_temp));
		char* idx1 = strchr(_temp,char('/'));
		char* idx2 = strchr(_temp,char('\\'));

		idx1 = idx1>=idx2?idx2:idx1;

		*idx1='\0';
		return _temp;
	}

	//判断本机是大端还是小端
	static bool IsLittleEnd()
	{
		static bool bLittleEnd = false;
		static struct RunOnce{
			RunOnce(bool& bLittleEnd){
				unsigned short x = 0x1234;
				char* p = (char*)&x;
				if (*p == 0x34){
					bLittleEnd = true;
				}else{
				}
			}
		} _Inst(bLittleEnd);

		return bLittleEnd;
	}

	//转为另一种格式,仅支持转内置类型
	template<class T>
	void ConvertToOhterEnd(T* pValue)
	{
		switch(sizeof(T))
		{
		case 2:
			{//short
				char _Bytes[2] = {0};
				char _Byte;
				memcpy(&_Bytes[0],pValue,sizeof(T));
				//交换
				_Byte = _Bytes[0];
				_Bytes[0] = _Bytes[1];
				_Bytes[1] = _Byte;

				memcpy(pValue,_Bytes,sizeof(T));
			}break;
		case 4:
			{//int
				char _Bytes[4] = {0};
				char _Byte;

				memcpy(&_Bytes[0],pValue,sizeof(T));

				//0,3 交换
				_Byte = _Bytes[0];
				_Bytes[0] = _Bytes[3];
				_Bytes[3] = _Byte;

				//1,2 交换
				_Byte = _Bytes[1];
				_Bytes[1] = _Bytes[2];
				_Bytes[2] = _Byte;

				memcpy(pValue,_Bytes,sizeof(T));
			}break;
		}
	}


	//求绝对值,只能用于float和double
	template<class T>
	T abs(const T& val)
	{
		if(val>=0.000001){
			return val;
		}else{
			return -val;
		}
	}

	//自然数之计算最大公约数
	static unsigned int MaxCommonDivsor(unsigned int a,unsigned int b)
	{
		//采用辗转相除法 TODO
		unsigned int min = a>b?b:a;
		unsigned int max = a<=b?b:a;


		for(int i=1;i<=min;i++)
		{
			if(min%i == 0)
			{
				unsigned int divsor = min/i;

				if(max%divsor == 0)
				{
					return divsor;
				}
			}
		}

	}

	//是否是无穷的
	static bool isInf(double x)
	{
		unsigned char* ch = (unsigned char*)&x;
		if(IsLittleEnd())
		{
			return (ch[7] & 0x7f) == 0x7f & ch[6] == 0xf0;
		}else
		{
			return (ch[0] & 0x7f) == 0x7f & ch[1] == 0xf0;
		}
	}
}

#endif