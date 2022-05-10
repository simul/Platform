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
			std::condition_variable unlocked;
			std::mutex mutex;
			bool writer;
			int readers;
		public:
			ReadWriteMutex():writer(false),readers(0)
			{
			}
			void lock_for_read()
			{
				mutex.lock();
				readers++;
			}

			void unlock_from_read()
			{
				mutex.unlock();
				readers--;
			}

			void lock_for_write()
			{
				/*while (!mutex.try_lock())
				{
					std::this_thread::sleep_for(std::chrono::seconds(1));
				}*/
				mutex.lock();
				writer=true;
			}

			void unlock_from_write()
			{
				mutex.unlock();
				writer=false;
			}
		};
	}
}
#endif