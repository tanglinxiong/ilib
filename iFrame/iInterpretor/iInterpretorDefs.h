/*********************************************
Author:Tanglx
Date:2020/05/26
FileName:iInterpretorDefs.h
**********************************************/
#ifndef _FILENAME_IINTERPRETORDEFS_
#define _FILENAME_IINTERPRETORDEFS_

#include "../../iTypes.h"

namespace iInterpretorDefines
{
	struct Error
	{
		enum
		{
			Parse_First_Symbol_Is_Operator,
		};


		uint32 ErrCode;
	};


	struct Elements
	{
		enum
		{
			Max_Element_Size = 128;
		};
		char pStr[Max_Element_Size];

		Elements(){
			memset(this,0,sizeof(Elements));
		}
	};
};

#endif