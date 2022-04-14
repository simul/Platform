#pragma once
namespace platform
{
	namespace core
	{
		//! A virtual interface class for classes that can return how much memory they use.
		class MemoryUsageInterface
		{
		public:
			//! Get the amount of memory used by this class instance, in bytes.
			virtual unsigned GetMemoryUsage() const=0;
		};
	}
}

