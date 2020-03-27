#ifndef SIMUL_BASE_MEMORYUSAGEINTERFACE_H
#define SIMUL_BASE_MEMORYUSAGEINTERFACE_H
namespace simul
{
	namespace base
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
#endif
