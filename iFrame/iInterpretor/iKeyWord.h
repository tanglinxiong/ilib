/*********************************************
Author:Tanglx
Date:2020/05/22
FileName:iKeyWord.h
**********************************************/
#ifndef _FILENAME_IKEYWORD_
#define _FILENAME_IKEYWORD_

#include "../../iPlatform/iLock/iLock.h"

class iKeyWord
{
public:
	iKeyWord(){}
	~iKeyWord(){}

	void RegKeyWord(const char* KeyWord,int K)
	{
		iLifeSpanLock _lk(&itsKeyWordMapLock);
		itsKeyWordMap[KeyWord] = 1;
	}

	bool IsKeyWord(const char* Str)
	{
		iLifeSpanLock _lk(&itsKeyWordMapLock);
		std::map<std::string,int32>::iterator it = itsKeyWordMap.find(Str);
		if (it != itsKeyWordMap.end())
		{
			return true;
		}
		return false;
	}
private:
	std::map<std::string,int32> itsKeyWordMap;
	iLock itsKeyWordMapLock;
};

#endif