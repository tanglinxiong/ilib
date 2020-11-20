/*********************************************
Author:Tanglx
Date:2020/10/26
FileName:iRecycleMem2.h
这是一个以一段buf构成的循环队列
由于其摆脱了类型的束缚,可以用于某些特定的场景
比如:波形回放方面
**********************************************/
#ifndef _FILENAME_IRECYCLEMEM2_
#define _FILENAME_IRECYCLEMEM2_

#include <stdio.h>
#include <string.h>

class iRecycleMem2
{
	class _Iterator
	{
		friend class iRecycleMem2;
	public:
		typedef char* pointer;
		typedef char& reference;
		typedef iRecycleMem2* ownnerptr;

		//construct
		_Iterator(){
			memset(this,0,sizeof(_Iterator));
		}
		_Iterator(pointer pt,ownnerptr o,int round){
			itsPtr=pt;
			itsOwnnerPtr=o;
			itsRound=round;
			itsElementSize=o->itsElementSize;
			itsElementNums=o->itsElementNums;
		}
		_Iterator& operator=(const _Iterator& other){
			itsPtr=other.itsPtr;
			itsOwnnerPtr=other.itsOwnnerPtr;
			itsRound=other.itsRound;
			itsElementSize=other.itsElementSize;
			itsElementNums=other.itsElementNums;
			return *this;
		}

		friend bool operator==(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if(_lv.itsRound != _rv.itsRound)
				return false;
			else
				return _lv.itsPtr==_rv.itsPtr;
		}
		friend bool operator!=(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if(_lv.itsRound != _rv.itsRound)
				return true;
			else
				return _lv.itsPtr!=_rv.itsPtr;
		}
		friend bool operator<=(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if (_lv.itsRound < _rv.itsRound)
				return true;
			else if (_lv.itsRound > _rv.itsRound)
				return false;
			else
				return _lv.itsPtr<=_rv.itsPtr;
		}
		friend bool operator>=(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if (_lv.itsRound < _rv.itsRound)
				return false;
			else if (_lv.itsRound > _rv.itsRound)
				return true;
			else
				return _lv.itsPtr>=_rv.itsPtr;
		}
		friend bool operator<(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if (_lv.itsRound < _rv.itsRound)
				return true;
			else if (_lv.itsRound > _rv.itsRound)
				return false;
			else
				return _lv.itsPtr<_rv.itsPtr;
		}
		friend bool operator>(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return false;
			}
			if (_lv.itsRound < _rv.itsRound)
				return false;
			else if (_lv.itsRound > _rv.itsRound)
				return true;
			else
				return _lv.itsPtr>_rv.itsPtr;
		}

		reference operator*(){
			if (itsPtr < itsOwnnerPtr->begin().itsPtr || itsPtr > itsOwnnerPtr->tag().itsPtr)
			{
				printf("operator* out of range\r\n");
			}
			return *itsPtr;
		}

		pointer operator->(){
			if (itsPtr < itsOwnnerPtr->begin().itsPtr || itsPtr > itsOwnnerPtr->tag().itsPtr)
			{
				printf("operator-> out of range\r\n");
			}
			return itsPtr;
		}

		pointer operator&(){
			if (itsPtr < itsOwnnerPtr->begin().itsPtr || itsPtr > itsOwnnerPtr->tag().itsPtr)
			{
				printf("operator& out of range\r\n");
			}
			return itsPtr;
		}

		_Iterator& operator++(){
			if (&*this==&itsOwnnerPtr->tag()){
				*this=itsOwnnerPtr->begin(++itsRound);
			}else{
				itsPtr += itsElementSize;
			}
			return *this;
		}

		_Iterator operator++(int){
			_Iterator _ret=*this;
			if (&*this==&itsOwnnerPtr->tag()){
				*this=itsOwnnerPtr->begin(++itsRound);
			}else{
				itsPtr += itsElementSize;
			}
			return _ret;
		}

		_Iterator& operator--(){
			if (&*this==&itsOwnnerPtr->begin()){
				*this=itsOwnnerPtr->tag(--itsRound);
			}else{
				itsPtr -= itsElementSize;
			}
			return *this;
		}

		_Iterator operator--(int){
			_Iterator _ret=*this;
			if (&*this==&itsOwnnerPtr->begin()){
				*this=itsOwnnerPtr->tag(--itsRound);
			}else{
				itsPtr -= itsElementSize;
			}
			return _ret;
		}

		_Iterator& operator+=(size_t sz){
			itsRound+=sz/itsOwnnerPtr->element_size();

			sz%=itsOwnnerPtr->element_size();
			if (sz>_distance())
			{
				sz-=(_distance()+1);
				*this=itsOwnnerPtr->begin(++itsRound);
				itsPtr += sz*itsElementSize;
			}else{
				itsPtr += sz*itsElementSize;
			}
			return *this;
		}

		_Iterator& operator-=(size_t sz){
			itsRound-=sz/itsOwnnerPtr->element_size();

			sz%=itsOwnnerPtr->element_size();
			if (sz>_rdistance())
			{
				sz-=(_rdistance()+1);
				*this=itsOwnnerPtr->tag(--itsRound);
				itsPtr -= sz*itsElementSize;
			}else{
				itsPtr -= sz*itsElementSize;
			}
			return *this;
		}

		friend _Iterator operator+(const _Iterator& _lv,int v){
			_Iterator _Ret = _lv;
			_Ret += v*_Ret.itsElementSize;
			return _Ret;
		}
		friend _Iterator operator-(const _Iterator& _lv,int v){
			_Iterator _Ret = _lv;
			_Ret -= v*_Ret.itsElementSize;
			return _Ret;
		}

		friend int operator-(const _Iterator& _lv,const _Iterator& _rv)
		{
			if (_lv.itsOwnnerPtr != _rv.itsOwnnerPtr)
			{
				throw "operator- iterator error";
				return 0;
			}
			int nRound = _lv.itsRound - _rv.itsRound;
			int nOffset = _lv.itsPtr - _rv.itsPtr;

			return nRound*_rv.itsElementNums + nOffset / _rv.itsElementSize;
		}


		ownnerptr getownner(){
			return itsOwnnerPtr;
		}


	private:
		inline size_t _distance(){
			//size_t sz=(&*_ownner->end()-_pt);
			return (&*itsOwnnerPtr->tag()-itsPtr) / itsElementSize;
		}
		inline size_t _rdistance(){
			return (itsPtr-&*itsOwnnerPtr->begin()) / itsElementSize;
		}
	private:
		pointer itsPtr;
		ownnerptr itsOwnnerPtr;
		int itsRound;		//圈数,从0开始,不可能发生数据翻转
		int itsElementSize;
		int itsElementNums;
	};
	friend class _Iterator;
public:
	typedef char* pointer;
	typedef _Iterator iterator;
public:
	iRecycleMem2(){
		memset(this,0,sizeof(iRecycleMem2));
	}
	~iRecycleMem2(){
		delloc();
	}

	//alloc 和 delloc 应该成对使用
	void alloc(int nElementNums,int nElementSize){
		delloc();
		_itsDataPtr_ = new char[nElementNums*nElementSize];
		itsHeadPtr = _itsDataPtr_;
		itsTagPtr = _itsDataPtr_+((nElementNums-1)*nElementSize);
		itsEndPtr = _itsDataPtr_+nElementNums*nElementSize;

		//printf("Addr[0x%x,0x%x]\r\n",itsHeadPtr,itsTagPtr);
		itsElementNums = nElementNums;
		itsElementSize = nElementSize;
	}
	void delloc(){
		if (_itsDataPtr_){
			delete[] _itsDataPtr_;
			_itsDataPtr_ = NULL;
		}
		memset(this,0,sizeof(iRecycleMem2));
	}

	//attach 和 detach 应该成对使用
	void attach(char* pBuffer,int nSize,int nElementSize)
	{
		detach();
		int nElementNum = nSize / nElementSize;
#define MINIUM_ELEMENT_SIZE 10
		if (nElementNum < MINIUM_ELEMENT_SIZE)
		{
			//元素太少,没有意义
			return ;
		}
		_itsDataPtr_ = pBuffer;
		itsHeadPtr = _itsDataPtr_;
		itsTagPtr = _itsDataPtr_+((nElementNum-1)*nElementSize);
		itsEndPtr = _itsDataPtr_+(nElementNum*nElementSize);
		itsElementNums = nElementNum;
		itsElementSize = nElementSize;
	}

	char* detach()
	{
		char* pBuffer = _itsDataPtr_;
		memset(this,0,sizeof(iRecycleMem2));
		return pBuffer;
	}

	iterator begin(){
		return iterator(itsHeadPtr,this,0);
	}

	iterator begin(int rcyround){
		return iterator(itsHeadPtr,this,rcyround);
	}

	iterator end(){
		return iterator(itsEndPtr,this,0);
	}

	iterator end(int rcyround){
		return iterator(itsEndPtr,this,rcyround);
	}

	iterator tag(){
		return iterator(itsTagPtr,this,0);
	}

	iterator tag(int rcyround){
		return iterator(itsTagPtr,this,rcyround);
	}

	size_t count(){
		return itsElementNums;
	}

	size_t element_size(){
		return itsElementSize;
	}

private:
	char* _itsDataPtr_;		//数据指针
	char* itsHeadPtr;		//头指针 ,应当等于_itsDataPtr_
	char* itsTagPtr;		//尾指针
	char* itsEndPtr;		//End指针
	int itsElementNums;		//元素个数
	int itsElementSize;		//元素大小
};

#endif
