/*********************************************
Author:Tanglx
Date:2020/05/22
FileName:iSymbol.h
**********************************************/
#ifndef _FILENAME_ISYMBOL_
#define _FILENAME_ISYMBOL_

#include "../../iPlatform/iLock/iLock.h"

class iSymbol
{
public:
	iSymbol(){}
	~iSymbol(){}

	void RegSymbol(const char* Symbol)
	{
		iLifeSpanLock _lk(&itsSymbolMapLock);
		itsSymbolMap[Symbol] = 1;
	}

	bool IsSymbol(const char* Str)
	{
		iLifeSpanLock _lk(&itsSymbolMapLock);
		std::map<std::string,int>::iterator it = itsSymbolMap.find(Str);
		if (it != itsSymbolMap.end())
		{
			return true;
		}
		return false;
	}

private:
	std::map<std::string,int> itsSymbolMap;
	iLock itsSymbolMapLock;
};

#endif