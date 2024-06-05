#ifndef READWRITEMUTEX_H
#define READWRITEMUTEX_H

#ifdef _XBOX_ONE
#include <xdk.h>
#endif
#include <mutex>
#include <condition_variable>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>         // std::chrono::seconds


namespace simul
{
	namespace base
	{
		class ReadWriteMutex
		{
			std::mutex mutex = {};
			bool writer = false;
			int readers = 0;

		public:
			ReadWriteMutex()
			{
			}

			~ReadWriteMutex()
			{
			}

			void lock_for_read()
			{
#if !defined(_GAMING_XBOX)
				mutex.lock();
				readers++;
#endif
			}

			void unlock_from_read()
			{
#if !defined(_GAMING_XBOX)
				mutex.unlock();
				readers--;
#endif
			}

			void lock_for_write()
			{
#if !defined(_GAMING_XBOX)
				mutex.lock();
				writer=true;
#endif
			}

			void unlock_from_write()
			{
#if !defined(_GAMING_XBOX)
				mutex.unlock();
				writer=false;
#endif
			}
		};
	}
}
#endif