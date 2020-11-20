/*********************************************
Author:Tanglx
Date:2020/05/22
FileName:iOperators.h
**********************************************/
#ifndef _FILENAME_IOPERATORS_
#define _FILENAME_IOPERATORS_


class iOperator
{
public:
	iOperator(){}
	~iOperator(){}


	void RegOperator(uint8 Op)
	{
		iLifeSpanLock _lk(&itsOperatorMapLock);
		itsOperatorMap[Op] = 1;
	}

	bool IsOperator(uint8 Chr)
	{
		iLifeSpanLock _lk(&itsOperatorMapLock);
		std::map<uint8,int32>::iterator it = itsOperatorMap.find(Chr);
		if (it != itsOperatorMap.end())
		{
			return true;
		}
		return false;
	}

private:
	std::map<uint8,int32> itsOperatorMap;
	iLock itsOperatorMapLock;
};

#endif