#pragma once

#include <iostream>
#ifdef __GNUC__
// NOTE: gcc expects spaces after the commas in macros. No reason why.
#ifndef __COUNTER__
	#define __COUNTER__ __LINE__
#endif
// LLVM claims Gcc compatibility (__GNUC__=4) but uses __FUNCTION__ instead of __FUNC__
#ifndef __llvm__
    #define __FUNCTION__ __FUNC__
#endif
// also, gcc does NOT handle the pasting operator ## well. avoid if possible.
#endif
#ifdef _MSC_VER	// Using Visual Studio? Then this pragma works:
# pragma warning(push)
# pragma warning(disable:4284) // odd return type for operator->
#endif
// Note: __COUNTER__ is a Microsoft compiler macro that increments each time it is used.
// From gcc version 4.3,  __COUNTER__ also works in gcc!

template<class T> struct StreamingHelperTemplate
{
	static std::ostream &StreamOut(std::ostream &os, const T &t)
	{
		os.write((const char*)(&t), sizeof(t));
		return os;
	}
	static std::istream &StreamIn(std::istream &is, T &t)
	{
		is.read((char*)(&t), sizeof(t));
		return is;
	}
};

template<> struct StreamingHelperTemplate<std::string>
{
	static std::ostream &StreamOut(std::ostream &os, const std::string &t)
	{
		size_t len=t.length();
		os.write((const char*)(&len),		sizeof(size_t));
		if(len)
			os.write((const char*)(t.c_str()),	(std::streamsize)(len*sizeof(char)));
		return os;
	}
	static std::istream &StreamIn(std::istream &is, std::string &t)
	{
		size_t len=0;
		is.read((char*)(&len),sizeof(size_t));
		if(!len)
			return is;
		char *temp_str=new char[len];
		is.read(temp_str,(std::streamsize)(len*sizeof(char)));
		t=temp_str;
		delete [] temp_str;
		return is;
	}
};

template<> struct StreamingHelperTemplate<bool>
{
	static std::ostream &StreamOut(std::ostream &os, const bool &t)
	{
		bool temp=t;
		unsigned len=sizeof(temp);
		os.write((const char*)(&temp), len);
		return os;
	}
	static std::istream &StreamIn(std::istream &is, bool &t)
	{
		bool temp;
		unsigned len=sizeof(temp);
		is.read((char*)(&temp), len);
		t=temp;
		return is;
	}
};

// overloaded in META_EndProperties	
inline void StreamOutAutoProperties(std::ostream &)		
{															
}															
inline void StreamInAutoProperties(std::istream &)				
{															
}								
/*
inline void StreamOutAutoPropertiesState(std::ostream &)		
{															
}															
inline void StreamInAutoPropertiesState(std::istream &)				
{															
}*/

template<class T> void META_Limit(T &t, T minimum, T maximum)
{
	if(t<minimum)
		t=minimum;
	if(t>maximum)
		t=maximum;
}		

template<class T, T minimum, T maximum> void META_StaticLimit(T &t)
{
	if(t<minimum)
		t=minimum;
	if(t>maximum)
		t=maximum;
}

#define META_DeclareMaxAndMin(type, propname, minimum, maximum)			\
	public:																\
		/*! Get the largest value of propname for this class.*/			\
		static type GetMaximumOf##propname()							\
		{																\
			return (maximum);											\
		}																\
	public:																\
		/*! Get the smallest value of propname for this class.*/		\
		static type GetMinimumOf##propname()							\
		{																\
			return (minimum);											\
		}

#if !defined(DOXYGEN)
#define META_DeclareEnforceRange(type, propname, minimum, maximum)	\
	protected:														\
		void EnforceRangeOf##propname()								\
		{															\
			META_Limit<type>(propname, minimum, maximum);			\
		}															\
		META_DeclareMaxAndMin(type, propname, minimum, maximum)
		
#define META_PropertyGet(type, propname,info)								\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname (info) */											\
		const type &Get##propname() const												\
		{																				\
			return propname;															\
		}		

#define META_PropertyGetAndSet(type, propname,info)										\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname (info) */											\
		const type &Get##propname() const												\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname (info) */											\
		void Set##propname(const type &value)											\
		{																				\
			propname=value;																\
		}

#define META_PropertyGetAndSetOverride(type, propname,info)										\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname (info) */											\
		const type &Get##propname() const override										\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname (info) */											\
		void Set##propname(const type &value)											\
		{																				\
			propname=value;																\
		}

#define META_PropertyGetSetAndCall(type, propname, call_on_set)					\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const												\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname, then call call_on_set().  */						\
		void Set##propname(const type &value)											\
		{																				\
			if(propname==value)															\
				return;																	\
			propname=value;																\
			call_on_set();																\
		}

#define META_Get(type, propname)												\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const														\
		{																				\
			return propname;															\
		}																				\
	private:																			\
		void Set##propname(const type &value)													\
		{																				\
			propname=value;																\
		}																				\
	public:																				\
		/*! Set the value of propname.*/												\
		void Init##propname(const type &value)													\
		{																				\
			propname=value;																\
		}

#define META_GetAndSet(type, propname)										\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const														\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value)													\
		{																				\
			propname=value;																\
		}																				\
		/*! Set the value of propname.*/												\
		void Init##propname(const type &value)													\
		{																				\
			propname=value;																\
		}

#define META_GetSetAndCall(type, propname, call_on_set)						\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const														\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname, then call call_on_set().  */						\
		void Set##propname(const type &value)													\
		{																				\
			if(propname==value)															\
				return;																	\
			propname=value;																\
			call_on_set();																\
		}																				\
		/*! Set the value of propname, then call call_on_set().  */						\
		void Init##propname(const type &value)													\
		{																				\
			propname=value;																\
			call_on_set();																\
		}

#define META_GetSetAndCall2(type, propname, call_on_set1, call_on_set2)		\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const														\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname, then call call_on_set1() and call_on_set2(). */	\
		void Set##propname(const type &value)													\
		{																				\
			if(propname==value)															\
				return;																	\
			propname=value;																\
			call_on_set1();																\
			call_on_set2();																\
		}																				\
		/*! Set the value of propname, then call call_on_set1() and call_on_set2(). */	\
		void Init##propname(const type &value)													\
		{																				\
			propname=value;																\
			call_on_set1();																\
			call_on_set2();																\
		}

#define META_RangeGetAndSet(type, propname, minimum, maximum)					\
	protected:																				\
		type propname;																		\
	public:																					\
		/*! Get the value of propname.*/													\
		const type &Get##propname() const															\
		{																					\
			return propname;																\
		}																					\
		META_DeclareMaxAndMin(type, propname, minimum, maximum)								\
		/*! Set the value of propname, then call call_on_set1() and call_on_set2(). */		\
		void Set##propname(const type &value)														\
		{																					\
			type v=value; \
			if(v<minimum)																\
				v=minimum;																\
			if(v>maximum)																\
				v=maximum;																\
			if(propname==v)																\
				return;																		\
			propname=value;																	\
		}																					\
		/*! Set the value of propname, then call call_on_set(). */		\
		void Init##propname(const type &value)														\
		{																					\
			propname=value;																	\
		}


#define META_RangeGetSetAndCall(type, propname, call_on_set, minimum, maximum)	\
	protected:																				\
		type propname;																		\
	public:																					\
		/*! Get the value of propname.*/													\
		const type &Get##propname() const															\
		{																					\
			return propname;																\
		}																					\
		META_DeclareMaxAndMin(type, propname, minimum, maximum)								\
		/*! Set the value of propname, then call call_on_set1() and call_on_set2(). */		\
		void Set##propname( const type &value)												\
		{																					\
			type v=value;																	\
			if(v<minimum)																	\
				v=minimum;																	\
			if(v>maximum)																	\
				v=maximum;																	\
			if(propname==v)																	\
				return;																		\
			propname=v;																		\
			call_on_set();																	\
		}																					\
		/*! Set the value of propname, then call call_on_set(). */							\
		void Init##propname(const type &value)														\
		{																					\
			propname=value;																	\
			call_on_set();																	\
		}


#else
#define META_DeclareEnforceRange(type, propname, minimum, maximum)

#define META_PropertyGet(type, propname,info)		\
	protected:																			\
		/*! info.
			Read-only, use Get##propname() to access. */								\
		type propname;	
		
#define META_PropertyGetAndSet(type, propname, info)								\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname (info).*/											\
		const type &Get##propname() const;												\
		/*! Set the value of propname (info).*/											\
		void Set##propname(const type &value);
		
#define META_PropertyGetAndSetOverride(type, propname, info)							\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname (info) */											\
		const type &Get##propname() const override										\
		{																				\
			return propname;															\
		}																				\
		/*! Set the value of propname (info) */											\
		void Set##propname(const type &value)											\
		{																				\
			propname=value;																\
		}

#define META_PropertyGetSetAndCall(type, propname, call_on_set)					\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;												\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value);

#define META_Get(type, propname)												\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;														\
	private:																			\
		void Set##propname(const type &value);													\
	public:																				\
		/*! Set the value of propname.*/												\
		void Init##propname(const type &value);

#define META_GetAndSet(type, propname)										\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;														\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value);

#define META_GetSetAndCall(type, propname, call_on_set)						\
	protected:																			\
		type propname;																	\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;														\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value);

#define META_GetSetAndCall2(type, propname, call_on_set1, call_on_set2)		\
	protected:																				\
		type propname;																		\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;														\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value);

#define META_RangeGetAndSet(type, propname, minimum, maximum)				\
	protected:																				\
		type propname;																		\
	public:																				\
		/*! Get the value of propname.*/												\
		const type &Get##propname() const;														\
		/*! Set the value of propname.*/												\
		void Set##propname(const type &value);

#define META_RangeGetSetAndCall(type, propname, call_on_set, minimum, maximum)	\
	protected:																				\
		type propname;																		\
	public:																					\
		/*! Get the value of propname.*/													\
		const type &Get##propname() const;															\
		/*! Set the value of propname.*/													\
		void Set##propname(const type &value);

#endif
			
// we want to accumulate a function that saves each property.
#if !defined(DOXYGEN) && defined(_MSC_VER)
#define META_BeginProperties																	\
	private: template<int N> void Templ_AccumulateSetInitial()									\
	{																							\
	}																							\
	private: template<int N> void Templ_AccumulateStreamOut(std::ostream &) const				\
	{																							\
	}																							\
	private: template<int N> void Templ_AccumulateStreamIn(std::istream &)						\
	{																							\
	}


#define META_AccumulateStreaming(type, propname)											\
		static unsigned GetOrdinalOf##propname()													\
		{																							\
			return __COUNTER__;																		\
		}																							\
		template<> void Templ_AccumulateSetInitial<__COUNTER__>()							\
		{																							\
			Templ_AccumulateSetInitial<__COUNTER__-8>();									\
		}																							\
		template<> void Templ_AccumulateStreamOut<__COUNTER__>(std::ostream &os)	const	\
		{																							\
			Templ_AccumulateStreamOut<__COUNTER__-8>(os);									\
			StreamingHelperTemplate<type>::StreamOut(os, propname);									\
		};																							\
		template<> void Templ_AccumulateStreamIn<__COUNTER__>(std::istream &is)			\
		{																							\
			Templ_AccumulateStreamIn<__COUNTER__-8>(is);									\
			type temp##propname;																	\
			StreamingHelperTemplate<type>::StreamIn(is, temp##propname);							\
			Set##propname(temp##propname);															\
		};

					
#define META_AccumulateStreamingAndInit(type, propname, initial)							\
		static unsigned GetOrdinalOf##propname()													\
		{																							\
			return __COUNTER__;																		\
		}																							\
		template<> void Templ_AccumulateSetInitial<__COUNTER__>()							\
		{																							\
			Templ_AccumulateSetInitial<__COUNTER__-8>();									\
			Init##propname(initial);																\
		};																							\
		template<> void Templ_AccumulateStreamOut<__COUNTER__>(std::ostream &os)	const	\
		{																							\
			Templ_AccumulateStreamOut<__COUNTER__-8>(os);									\
			StreamingHelperTemplate<type>::StreamOut(os, propname);									\
		};																							\
		template<> void Templ_AccumulateStreamIn<__COUNTER__>(std::istream &is)			\
		{																							\
			Templ_AccumulateStreamIn<__COUNTER__-8>(is);									\
			type temp##propname;																	\
			StreamingHelperTemplate<type>::StreamIn(is, temp##propname);							\
			Set##propname(temp##propname);															\
		};

#else
#define META_BeginProperties
#define META_AccumulateStreaming(type, propname)
#define META_AccumulateStreamingAndInit(type, propname, initial)
	
#endif

#if !defined(DOXYGEN) && defined(_MSC_VER)
#define META_EndProperties												\
	public:																				\
	void StreamOutAutoProperties(std::ostream &os) const						\
	{																					\
		Templ_AccumulateStreamOut<__COUNTER__-4>(os);							\
	}																					\
	void StreamInAutoProperties(std::istream &is)								\
	{																					\
		Templ_AccumulateStreamIn<__COUNTER__-3>(is);							\
	}																					\
	void InitializeProperties()												\
	{																					\
		Templ_AccumulateSetInitial<__COUNTER__-8>();							\
	}

#else
#define META_EndProperties												\
	public:																				\
	void StreamOutAutoProperties(std::ostream &) const						\
	{																					\
	}																					\
	void StreamInAutoProperties(std::istream &)								\
	{																					\
	}																					\
	void InitializeProperties()												\
	{																					\
	}
#endif


#ifndef DOXYGEN
#define META_Info(propname, info)														\
	static const char *GetInfoFor##propname()											\
	{																					\
		return info;																	\
	}
#else
#define META_Info(propname, info)

#endif


#define META_Property(type, propname, info)											\
		/*! info */																	\
		/*! \name Property: type propname	*/										\
		/*@{*/																		\
		META_PropertyGetAndSet(type, propname,info)									\
		META_AccumulateStreaming(type, propname)									\
		META_Info(propname, info)													\
		/*@}*/ 

#define META_PropertyOverride(type, propname, info)											\
		/*! info */																	\
		/*! \name Property: type propname	*/										\
		/*@{*/																		\
		META_PropertyGetAndSetOverride(type, propname,info)									\
		META_AccumulateStreaming(type, propname)									\
		META_Info(propname, info)													\
		/*@}*/ 


#define META_PropertyReadOnly(type, propname, info)									\
		protected:																	\
		void Set##propname(const type &value)										\
		{																			\
			propname=value;															\
		}																			\
		/*! info */																	\
		/*! \name Property: type propname	*/										\
		/*@{*/																		\
		META_PropertyGet(type, propname, info)										\
		META_AccumulateStreaming(type, propname)									\
		META_Info(propname, info)													\
		/*@}*/ 


#define VIRTUAL_Get(type, propname, info)					\
	public:														\
		/*! info */												\
		/*! \name Property: type propname	*/					\
		/*@{*/													\
		/*! Get the value of propname (info) */					\
		virtual const type &Get##propname() const = 0;					\
		META_Info(propname, info)								\
		/*@}*/

#define VIRTUAL_Set(type, propname, info)					\
	public:														\
		/*! info */												\
		/*! \name Property: type propname	*/					\
		/*@{*/													\
		/*! Set the value of propname (info). */				\
		virtual void Set##propname(const type &value) = 0;				\
		META_Info(propname, info)								\
		/*@}*/

#define VIRTUAL_GetAndSetNOINFO(type, propname)			\
	public:														\
		/*! info */												\
		/*! \name Property: type propname	*/					\
		/*@{*/													\
		/*! Get the value of propname. */						\
		virtual const type &Get##propname() const = 0;					\
		/*! Set the value of propname. */						\
		virtual void Set##propname(const type &value) = 0;				\
		/*@}*/

#define VIRTUAL_GetAndSet(type, propname, info)			\
	public:														\
		/*! info */												\
		/*! \name Property: type propname	*/					\
		/*@{*/													\
		/*! Get the value of propname (info) */					\
		virtual const type &Get##propname() const = 0;			\
		/*! Set the value of propname (info) */					\
		virtual void Set##propname(const type &value) = 0;		\
		META_Info(propname, info)								\
		/*@}*/



#if !defined(DOXYGEN)
#ifdef _MSC_VER
#define META_PassOnStreaming															\
		template<> void Templ_AccumulateSetInitial<__COUNTER__>()							\
		{																							\
			Templ_AccumulateSetInitial<__COUNTER__-8>();									\
		}																							\
		template<> void Templ_AccumulateStreamOut<__COUNTER__>(std::ostream &os)	const	\
		{																							\
			Templ_AccumulateStreamOut<__COUNTER__-8>(os);									\
		};																							\
		template<> void Templ_AccumulateStreamIn<__COUNTER__>(std::istream &is)			\
		{																							\
			Templ_AccumulateStreamIn<__COUNTER__-8>(is);									\
		};
#else
	#define META_PassOnStreaming		
#endif
	#define META_Category(category)												\
	/*! \name Category: category	*/														\
		public:																				\
			static unsigned BeginOrdinalOfCategory##category()								\
			{																				\
				return __COUNTER__;															\
			}																				\
			META_PassOnStreaming

	#define META_EndCategory(category)											\
		public:																				\
			static unsigned EndOrdinalOfCategory##category()								\
			{																				\
				return __COUNTER__;															\
			}																				\
			META_PassOnStreaming
#else
	
	#define META_PassOnStreaming		

	#define META_Category(category) \
	/*! \name Category: category	*/

		
	#define META_EndCategory(category) \
	/*! \name General	*/

#endif



#define META_RangeProperty(type, propname, initial, minimum, maximum, info)	\
		/*! info */																				\
		/*! \name Property: type propname	*/													\
		/*@{*/																					\
		META_RangeGetAndSet(type, propname, minimum, maximum)						\
		META_AccumulateStreamingAndInit(type, propname, initial)							\
		META_Info(propname, info)																\
		/*@}*/ 

#define META_RangePropertyWithSetCall(type, propname, minimum, maximum, call_on_set, info)	\
		/*! info */																								\
		/*! \name Property: type propname	*/																	\
		/*@{*/																									\
		META_RangeGetSetAndCall(type, propname, call_on_set, minimum, maximum)						\
		META_AccumulateStreaming(type, propname)														\
		META_Info(propname, info)																				\
		/*@}*/
			

#define META_RangePropertyWithInitAndSetCall(type, propname, initial, minimum, maximum, call_on_set, info)	\
		/*! info */																								\
		/*! \name Property: type propname	*/																	\
		/*@{*/																									\
		META_RangeGetSetAndCall(type, propname, call_on_set, minimum, maximum)						\
		META_AccumulateStreamingAndInit(type, propname, initial)											\
		META_Info(propname, info)																				\
		/*@}*/

#define META_PropertyWithSetCall(type, propname, call_on_set, info)							\
		/*! info */																				\
		/*! \name Property: type propname	*/													\
		/*@{*/																					\
		META_PropertyGetSetAndCall(type, propname, call_on_set)									\
		META_AccumulateStreaming(type, propname)												\
		META_Info(propname, info)																\
		/*@}*/ 

		
#define META_OpenProperty(type, propname, info)												\
		/*! info */																				\
		/*! \name Property: type propname	*/													\
		/*@{*/																					\
		META_PropertyGetAndSet(type, propname, info)											\
		META_AccumulateStreaming(type, propname)												\
		META_Info(propname, info)																\
	public:																						\
		type &Get##propname()																	\
		{																						\
			return propname;																	\
		}																						\
		/*@}*/ 


#define META_PropertyWithInit(type, propname, initial, info)									\
		/*! info */																			\
		/*! \name Property: type propname	*/												\
		/*@{*/																				\
		META_GetAndSet(type, propname)														\
		META_AccumulateStreamingAndInit(type, propname, Definition, initial)					\
		META_Info(propname, info)															\
		/*@}*/ 


#define META_PropertyWithInitAndSetCall(type, propname, initial, call_on_set, info)			\
		/*! info */																			\
		/*! \name Property: type propname	*/												\
		/*@{*/																				\
		META_GetSetAndCall(type, propname, call_on_set)										\
		META_AccumulateStreamingAndInit(type, propname, Definition, initial)					\
		META_Info(propname, info)															\
		/*@}*/ 
		

#define VIRTUAL_(propname)															\
		VIRTUAL_GetAndSet(propname)


#define Macro_EnableRecursiveStreaming(parent_node_class)										\
		virtual std::ostream &RecursiveStreamOut(std::ostream &os, bool include_subgraph) const	\
		{																						\
			parent_node_class::RecursiveStreamOut(os, include_subgraph);						\
			StreamOutAutoPropertiesDefinition(os);												\
			return StreamOut(os);																\
		}																						\
		virtual std::istream &RecursiveStreamIn(std::istream &is, bool include_subgraph)		\
		{																						\
			parent_node_class::RecursiveStreamIn(is, include_subgraph);							\
			StreamInAutoPropertiesDefinition(is);												\
			return StreamIn(is);																\
		}

#ifndef DOXYGEN

	#define DECLARE_PropertyAction(action_name,info)				\
		public:														\
			static unsigned GetOrdinalOf##action_name()				\
			{														\
				return __COUNTER__;									\
			}														\
			META_PassOnStreaming(Definition)						\
			void DoAction##action_name();



	#define DECLARE_StateAction(action_name,info)					\
		public:														\
			static unsigned GetOrdinalOf##action_name()				\
			{														\
				return __COUNTER__;									\
			}														\
			META_PassOnStreaming(State)								\
			void DoAction##action_name();

#else

	#define DECLARE_PropertyAction(action_name,info)				\
		public:														\
		/*! info */													\
		/*! \name Action: action_name */							\
		/*@{*/														\
			void DoAction##action_name();							\
		/*@}*/


	#define DECLARE_StateAction(action_name,info)					\
		public:														\
		/*! info */													\
		/*! \name Action: action_name */							\
		/*@{*/														\
			void DoAction##action_name();							\
		/*@}*/


#endif

#define DECLARE_Action(action_name,info)						\
	public:														\
		/*! info */												\
		/*! \name Action: action_name */						\
		/*@{*/													\
		void DoAction##action_name();							\
		/*@}*/ 

#define VIRTUAL_Action(action_name)								\
		/*! info */												\
		/*! \name Action: action_name */						\
		/*@{*/													\
	virtual void DoAction##action_name() = 0;					\
		/*@}*/ 

#define IMPLEMENT_Action(class_name, action_name)				\
	void class_name::DoAction##action_name()

#ifdef _MSC_VER
	#pragma warning(pop)
#endif

#define SIMUL_BIT_FLAG(Enum,T)															\
inline Enum operator|(const Enum &a, const Enum &b)										\
{																						\
	return static_cast<Enum>(static_cast<T>(a) | static_cast<T>(b));					\
}																						\
inline Enum operator&(const Enum &a, const Enum &b)										\
{																						\
	return static_cast<Enum>(static_cast<T>(a) & static_cast<T>(b));					\
}																						\
inline const Enum &operator&=(Enum &a, const Enum &b)									\
{																						\
	a = a&b;																			\
	return a;																			\
}																						\
inline const Enum &operator|=(Enum &a, const Enum &b)									\
{																						\
	a = a | b;																			\
	return a;																			\
}																						\
inline Enum operator~(const Enum &a)													\
{																						\
	Enum b = static_cast<Enum>(~static_cast<T>(a));										\
	return b;																			\
}

