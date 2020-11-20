/*********************************************
Author:     Tanglx
Date:       2019/08/28
FileName:   iSingleton.h
**********************************************/
#ifndef _FILENAME_ISINGLETON_
#define _FILENAME_ISINGLETON_

#include <new>
#include "stdio.h"

template<class T> struct iSingleton
{
	static void InitInst()
	{
		if (pThis != NULL)
		{
			throw - 1;      //÷±Ω”»√≥Ã–Ú±¿¿£
		}
		else
		{
			try
			{
				pThis = new T;
			}
			catch(std::bad_alloc &ba)
			{
				fprintf(stdout, "Singleton::InitInst Err<%s>\r\n", ba.what());
			}
		}
	}
	static T *GetInstance()
	{
		if (!pThis)
		{
			//throw - 1;
			return 0;
		}
		else
		{
			return pThis;
		}
	}

	static void DestoryInst()
	{
		if (pThis)
		{
			delete pThis;
			pThis = NULL;
		}
	}

private:
	static T *pThis;
};

template<class T> T *iSingleton<T>::pThis = NULL;

#endif