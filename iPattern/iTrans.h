/*********************************************
Author:Tanglx
Date:2019/11/14
FileName:iTrans.h
**********************************************/
#ifndef _FILENAME_ITRANS_
#define _FILENAME_ITRANS_

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans0
{
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute() = 0;
	};

	template<class T>
	struct Leaf : public Branch
	{
		Leaf(T* pObj,TRet(T::* pFunc)()) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute()
		{
			return ((*itsObjPtr).*itsFuncPtr)();
		}
	private:
		T* itsObjPtr;
		TRet(T::* itsFuncPtr)();
	};
};

typedef _iTrans0<void> iTrans0;

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans1
{
	template<class P1>
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute(P1) = 0;
	};

	template<class T,class P1>
	struct Leaf : public Branch<P1>
	{
		Leaf(T* pObj,TRet(T::* pFunc)(P1)) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute(P1 p1)
		{
			return ((*itsObjPtr).*itsFuncPtr)(p1);
		}
	private:
		T* itsObjPtr;
		TRet(T::* itsFuncPtr)(P1);
	};
};
typedef _iTrans1<void> iTrans1;

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans2
{
	template<class P1,class P2>
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute(P1,P2) = 0;
	};

	template<class T,class P1,class P2>
	struct Leaf : public Branch<P1,P2>
	{
		Leaf(T* pObj,TRet(T::* pFunc)(P1,P2)) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute(P1 p1,P2 p2)
		{
			return ((*itsObjPtr).*itsFuncPtr)(p1,p2);
		}
	private:
		T* itsObjPtr;
		TRet(T::* itsFuncPtr)(P1,P2);
	};
};

typedef _iTrans2<void> iTrans2;

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans3
{
	template<class P1,class P2,class P3>
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute(P1,P2,P3) = 0;
	};

	template<class T,class P1,class P2,class P3>
	struct Leaf : public Branch<P1,P2,P3>
	{
		Leaf(T* pObj,TRet(T::* pFunc)(P1,P2,P3)) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute(P1 p1,P2 p2,P3 p3)
		{
			return ((*itsObjPtr).*itsFuncPtr)(p1,p2,p3);
		}
	private:
		T* itsObjPtr;
		TRet(T::* itsFuncPtr)(P1,P2,P3);
	};
};

typedef _iTrans3<void> iTrans3;

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans4
{
	template<class P1,class P2,class P3,class P4>
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute(P1,P2,P3,P4) = 0;
	};

	template<class T,class P1,class P2,class P3,class P4>
	struct Leaf : public Branch<P1,P2,P3,P4>
	{
		Leaf(T* pObj,TRet(T::* pFunc)(P1,P2,P3,P4)) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute(P1 p1,P2 p2,P3 p3,P4 p4)
		{
			return ((*itsObjPtr).*itsFuncPtr)(p1,p2,p3,p4);
		}
	private:
		T* itsObjPtr;
		TRet(T::* itsFuncPtr)(P1,P2,P3,P4);
	};
};

typedef _iTrans4<void> iTrans4;

template<class TRet>			//为后期兼容而加,因为之前没有考虑返回值类型的问题,而现在需要操心返回值类型
struct _iTrans5
{
	template<class P1,class P2,class P3,class P4,class P5>
	struct Branch
	{
		Branch(){}
		virtual ~Branch(){}
		virtual TRet Execute(P1,P2,P3,P4,P5) = 0;
	};

	template<class T,class P1,class P2,class P3,class P4,class P5>
	struct Leaf : public Branch<P1,P2,P3,P4,P5>
	{
		Leaf(T* pObj,TRet(T::* pFunc)(P1,P2,P3,P4,P5)) 
			: itsObjPtr(pObj),itsFuncPtr(pFunc){}
		virtual ~Leaf(){}
		virtual TRet Execute(P1 p1,P2 p2,P3 p3,P4 p4,P5 p5)
		{
			return ((*itsObjPtr).*itsFuncPtr)(p1,p2,p3,p4,p5);
		}
	private:
		T* itsObjPtr;
		void(T::* itsFuncPtr)(P1,P2,P3,P4,P5);
	};
};

typedef _iTrans5<void> iTrans5;

#endif