/*********************************************
Author:Tanglx
Date:2020/05/22
FileName:iInterpretor.hxx
**********************************************/
#ifndef _FILENAME_IINTERPRETOR_
#define _FILENAME_IINTERPRETOR_

#include "iOperator.h"
#include "iInterpretorDefs.h"

using namespace iInterpretorDefines;

class iInterpretor
{
public:
	iInterpretor(){}
	~iInterpretor(){}

	bool Parse(const char* Src)
	{
		char* pIdx = Src;

		while(*pIdx)
		{
			char* p = pIdx;
			int32 pLen = 0;
			if (itsOp.IsOperator(*pIdx))
			{
				itsErr.ErrCode = itsErr.Parse_First_Symbol_Is_Operator;
				return false;
			}

			while(!itsOp.IsOperator(*pIdx))
			{
				pIdx++;
				pLen++;
			}

			//
			Elements* pElement = new Elements;
			memcpy(pElement->pStr,p,pLen);
		}
	}
private:
	iOperator itsOp;
	Error itsErr;


	std::list<Elements*> itsParseStr;
};

#endif