/*********************************************
Author:Tanglx
Date:2020/05/26
FileName:iRecycleMem.h
**********************************************/
#ifndef _FILENAME_IRECYCLEMEM_
#define _FILENAME_IRECYCLEMEM_

#include <stdio.h>
template<class T>class iRecycleMem;


template<class T>
class iRecycleMem
{
	template<class T>
	class _Iterator
	{
	public:
		typedef T* pointer;
		typedef T& reference;
		typedef iRecycleMem<T>* ownnerptr;

		//construct
		_Iterator(){
			memset(this,0,sizeof(_Iterator));
		}
		_Iterator(pointer pt,ownnerptr o,int round){
			itsPtr=pt;
			itsOwnnerPtr=o;
			itsRound=round;
		}
		_Iterator& operator=(const _Iterator& other){
			itsPtr=other.itsPtr;
			itsOwnnerPtr=other.itsOwnnerPtr;
			itsRound=other.itsRound;
			return *this;
		}

		friend bool operator==(const _Iterator& _lv,const _Iterator& _rv){
			if(_lv.itsRound != _rv.itsRound)
				return false;
			else
				return _lv.itsPtr==_rv.itsPtr;
		}
		friend bool operator!=(const _Iterator& _lv,const _Iterator& _rv){
			if(_lv.itsRound != _rv.itsRound)
				return true;
			else
				return _lv.itsPtr!=_rv.itsPtr;
		}
		friend bool operator<=(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsRound < _rv.itsRound)
				return true;
			else if (_lv.itsRound > _rv.itsRound)
				return false;
			else
				return _lv.itsPtr<=_rv.itsPtr;
		}
		friend bool operator>=(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsRound < _rv.itsRound)
				return false;
			else if (_lv.itsRound > _rv.itsRound)
				return true;
			else
				return _lv.itsPtr>=_rv.itsPtr;
		}
		friend bool operator<(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsRound < _rv.itsRound)
				return true;
			else if (_lv.itsRound > _rv.itsRound)
				return false;
			else
				return _lv.itsPtr<_rv.itsPtr;
		}
		friend bool operator>(const _Iterator& _lv,const _Iterator& _rv){
			if (_lv.itsRound < _rv.itsRound)
				return false;
			else if (_lv.itsRound > _rv.itsRound)
				return true;
			else
				return _lv.itsPtr>_rv.itsPtr;
		}

		reference operator*(){
			if (itsPtr < itsOwnnerPtr->begin()._pt || itsPtr > itsOwnnerPtr->tag()._pt)
			{
				printf("operator* out of range\r\n");
			}
			return *itsPtr;
		}

		pointer operator->(){
			if (itsPtr < itsOwnnerPtr->begin()._pt || itsPtr > itsOwnnerPtr->tag()._pt)
			{
				printf("operator-> out of range\r\n");
			}
			return itsPtr;
		}

		pointer operator&(){
			if (itsPtr < itsOwnnerPtr->begin()._pt || itsPtr > itsOwnnerPtr->tag()._pt)
			{
				printf("operator& out of range\r\n");
			}
			return itsPtr;
		}

		_Iterator& operator++(){
			if (&*this==&itsOwnnerPtr->tag()){
				*this=itsOwnnerPtr->begin(++itsRound);
			}else{
				++itsPtr;
			}
			return *this;
		}

		_Iterator operator++(int){
			_Iterator _ret=*this;
			if (&*this==&itsOwnnerPtr->tag()){
				*this=itsOwnnerPtr->begin(++itsRound);
			}else{
				++itsPtr;
			}
			return _ret;
		}

		_Iterator& operator--(){
			if (&*this==&itsOwnnerPtr->begin()){
				*this=itsOwnnerPtr->tag(--itsRound);
			}else{
				--itsPtr;
			}
			return *this;
		}

		_Iterator operator--(int){
			_Iterator _ret=*this;
			if (&*this==&itsOwnnerPtr->begin()){
				*this=itsOwnnerPtr->tag(--itsRound);
			}else{
				--itsPtr;
			}
			return _ret;
		}

		_Iterator& operator+=(size_t sz){
			itsRound+=sz/itsOwnnerPtr->size();

			sz%=itsOwnnerPtr->size();
			if (sz>_distance())
			{
				sz-=(_distance()+1);
				*this=itsOwnnerPtr->begin(++itsRound);
				itsPtr+=sz;
			}else{
				itsPtr+=sz;
			}
			return *this;
		}

		_Iterator& operator-=(size_t sz){
			itsRound-=sz/itsOwnnerPtr->size();

			sz%=itsOwnnerPtr->size();
			if (sz>_rdistance())
			{
				sz-=(_rdistance()+1);
				*this=itsOwnnerPtr->tag(--itsRound);
				itsPtr-=sz;
			}else{
				itsPtr-=sz;
			}
			return *this;
		}

		friend _Iterator operator+(const _Iterator& _lv,int v){
			_Iterator _Ret = _lv;
			_Ret+=v;
			return _Ret;
		}
		friend _Iterator operator-(const _Iterator& _lv,int v){
			_Iterator _Ret = _lv;
			_Ret-=v;
			return _Ret;
		}


		ownnerptr getownner(){
			return itsOwnnerPtr;
		}


	private:
		inline size_t _distance(){
			//size_t sz=(&*_ownner->end()-_pt);
			return (&*itsOwnnerPtr->tag()-itsPtr);
		}
		inline size_t _rdistance(){
			return (itsPtr-&*itsOwnnerPtr->begin());
		}
	private:
		pointer itsPtr;
		ownnerptr itsOwnnerPtr;
		int itsRound;		//圈数,从0开始,不可能发生数据翻转
	};
public:
	typedef T* pointer;
	typedef _Iterator<T> iterator;
public:
	iRecycleMem(){
		memset(this,0,sizeof(iRecycleMem));
	}
	~iRecycleMem(){
		delloc();
	}

	//alloc 和 delloc 应该成对使用
	void alloc(int nElementNums){
		delloc();
		_itsDataPtr_ = new T[nElementNums];
		itsHeadPtr = _itsDataPtr_;
		itsTagPtr = _itsDataPtr_+(nElementNums-1);
		itsEndPtr = _itsDataPtr_+nElementNums;

		printf("Addr[0x%x,0x%x]\r\n",itsHeadPtr,itsTagPtr);
		itsElementNums = nElementNums;
	}
	void delloc(){
		if (_itsDataPtr_){
			delete[] _itsDataPtr_;
			_itsDataPtr_ = NULL;
		}
		memset(this,0,sizeof(iRecycleMem));
	}

	//attach 和 detach 应该成对使用
	void attach(char* pBuffer,int nSize)
	{
		int nElementNum = nSize / sizeof(T);
#define MINIUM_ELEMENT_SIZE 10
		if (nElementNum < MINIUM_ELEMENT_SIZE)
		{
			//元素太少,没有意义
			return ;
		}
		_itsDataPtr_ = (T*)pBuffer;
		itsHeadPtr = _itsDataPtr_;
		itsTagPtr = _itsDataPtr_+(nElementNum-1);
		itsEndPtr = _itsDataPtr_+nElementNum;
		itsElementNums = nElementNum;
	}

	char* detach()
	{
		char* pBuffer = _itsDataPtr_;
		memset(this,0,sizeof(iRecycleMem));
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

private:
	pointer _itsDataPtr_;	//数据指针
	pointer itsHeadPtr;		//头指针 ,应当等于_itsDataPtr_
	pointer itsTagPtr;		//尾指针
	pointer itsEndPtr;		//End指针
	int itsElementNums;	//长度
};

#endif
