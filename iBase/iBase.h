/*********************************************
Author:Tanglx
Date:2019/09/04
FileName:iBase.h
一些与平台无关的基础功能函数
**********************************************/
#ifndef _FILENAME_IBASE_
#define _FILENAME_IBASE_

struct iBase
{
	static bool IsLittleEndin(){
		static bool bLittleEndin = false;
		static struct RunOnce{
			RunOnce(bool& bLittleEnd){
				unsigned short x = 0x1234;
				char* p = (char*)&x;
				if (*p == 0x34){
					bLittleEnd = true;
				}else{
				}
			}
		} _Inst(bLittleEndin);

		return bLittleEndin;
	}

	//转为另一种格式,仅支持转内置类型
	template<class T>
	static void ConvertToOhterEndian(T* pValue)
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
		case 8:
			{
				char _Bytes[8] = {0};
				char _Byte;

				memcpy(&_Bytes[0],pValue,sizeof(T));

				//0,7 交换
				_Byte = _Bytes[0];
				_Bytes[0] = _Bytes[7];
				_Bytes[7] = _Byte;

				//1,6 交换
				_Byte = _Bytes[1];
				_Bytes[1] = _Bytes[6];
				_Bytes[6] = _Byte;

				//2,5 交换
				_Byte = _Bytes[2];
				_Bytes[2] = _Bytes[5];
				_Bytes[5] = _Byte;

				//3,4 交换
				_Byte = _Bytes[3];
				_Bytes[3] = _Bytes[4];
				_Bytes[4] = _Byte;

				memcpy(pValue,_Bytes,sizeof(T));
			}
		}
	}

	template<class T>
	static void ConvertToBigEndian(T* pValue)
	{
		if (IsLittleEndin())
		{
			ConvertToOhterEndian(pValue);
		}
	}

	template<class T>
	static void ConvertToLittleEndian(T* pValue)
	{
		if (!IsLittleEndin())
		{
			ConvertToOhterEndian(pValue);
		}
	}


	static uint16 CheckSum(const char *data, uint32 len)
	{
		static uint16 crc_tbl[16] = 
		{
			0x0000, 0x1081, 0x2102, 0x3183,
			0x4204, 0x5285, 0x6306, 0x7387,
			0x8408, 0x9489, 0xa50a, 0xb58b,
			0xc60c, 0xd68d, 0xe70e, 0xf78f
		};

		register uint16 crc = 0xffff;
		uint8 c;
		const uint8 *p = reinterpret_cast<const uint8 *>(data);
		while (len--) 
		{
			c = *p++;
			crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
			c >>= 4;
			crc = ((crc >> 4) & 0x0fff) ^ crc_tbl[((crc ^ c) & 15)];
		}
		return ~crc & 0xffff;
	}
	
	static bool IsValidFloat(const float& v)
	{
		if(v+1 == v-1)
			return false;
		return true;
	}
	
	static bool IsValidFloat(const double& v)
	{
		if(v+1 == v-1)
			return false;
		return true;
	}
};

#endif
