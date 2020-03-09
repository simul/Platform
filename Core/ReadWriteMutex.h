#ifndef READWRITEMUTEX_H
#define READWRITEMUTEX_H
#if 1
#ifdef _XBOX_ONE
#include <xdk.h>
#endif
#include <mutex>
#include <condition_variable>
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

#else

#ifdef _MSC_VER
#define RWMUTEX_WIN_HPP()0x0001UL
#undef WIN32_LEAN_AND_MEAN
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#include <windows.h>
#include <exception>
#include <cassert>
#include <climits>
#if ! defined(RWMUTEX_WIN_UNEXPECTED)
#if defined(_MSC_VER)
#define RWMUTEX_WIN_UNEXPECTED() \
	assert(false), unexpected()
#else
#define RWMUTEX_WIN_UNEXPECTED() \
	assert(false), std::unexpected()
#endif
#endif
#if ! defined(RWMUTEX_WIN_CTOR_UNEXPECTED)
#define RWMUTEX_WIN_CTOR_UNEXPECTED() \
	assert(false); throw std::exception()
#endif
#if ! defined(RWMUTEX_WIN_DTOR_UNEXPECTED)
#define RWMUTEX_WIN_DTOR_UNEXPECTED()assert(false)
#endif

namespace simul
{
	namespace base
	{
		namespace lockguard
		{
			template<typename T>
			class read
			{
				T* m_state;
				bool m_upgraded;
			public:
				read(T& state) throw() 
					: m_state(&state), 
					m_upgraded(false)
				{
					m_state->rdlock();
				}
				~read() throw()
				{
					if (m_state)
					{ 
						if (! m_upgraded)
						{
							m_state->rdunlock();
						} else
						{
							m_state->wrunlock();
						}
					}
				}
			public:
				void cancel() throw()
				{ m_state = 0; }

				bool tryrdtowr() throw()
				{
					return (m_state && ! m_upgraded) ? 
						(m_upgraded = m_state->tryrdtowr()) : false;
				}
			};


			template<typename T>
			class write
			{
				T* m_state;
			public:
				write(T& state) throw() : m_state(&state)
				{
					m_state->wrlock();
				}
				~write() throw()
				{
					if (m_state)
					{ m_state->wrunlock(); }
				}
			public:
				void cancel() throw()
				{ m_state = 0; }
			};
		}

		class ReadWriteMutex
		{
			ReadWriteMutex(ReadWriteMutex const&);
			ReadWriteMutex const& operator =(ReadWriteMutex const&);


		public:
			typedef lockguard::read<ReadWriteMutex> lock_read;
			typedef lockguard::write<ReadWriteMutex> lock_write;


		private:
			long m_count;
			long m_rdwake;
			long m_wrrecurse;
			CRITICAL_SECTION  m_wrlock;
			HANDLE m_wrwset;
			HANDLE m_rdwset;


		public:
			ReadWriteMutex()
				: m_count(LONG_MAX),
				m_rdwake(0),
				m_wrrecurse(0),
				m_wrwset(CreateEvent(0, false, false, 0)),
				m_rdwset(CreateSemaphore(0, 0, LONG_MAX, 0))
			{
				if (! m_rdwset || ! m_wrwset)
				{
					if (m_rdwset)
					{ CloseHandle(m_rdwset); }
					if (m_wrwset)
					{ CloseHandle(m_wrwset); }
					RWMUTEX_WIN_CTOR_UNEXPECTED();
				}
				InitializeCriticalSection(&m_wrlock);
			}

			~ReadWriteMutex() throw()
			{
				if (m_count != LONG_MAX || m_rdwake || m_wrrecurse)
				{
					RWMUTEX_WIN_DTOR_UNEXPECTED();
				}
				DeleteCriticalSection(&m_wrlock);
				if (! CloseHandle(m_rdwset)||! CloseHandle(m_wrwset))
				{
					RWMUTEX_WIN_DTOR_UNEXPECTED();
				}
			}


		public:
			void lock_for_write() throw()
			{
				EnterCriticalSection(&m_wrlock);
				if (! (m_wrrecurse++))
				{
					assert(m_count > -1);
					LONG count = InterlockedExchangeAdd(&m_count, -LONG_MAX);
					if (count < LONG_MAX)
					{
						if (InterlockedExchangeAdd(&m_rdwake,LONG_MAX - count) + LONG_MAX - count)
						{
							if (WaitForSingleObject(m_wrwset,INFINITE) != WAIT_OBJECT_0)
							{
								RWMUTEX_WIN_UNEXPECTED();
							}
						}
					}
				}
			}

			void unlock_from_write() throw()
			{
				assert(m_count < 1);
				if (! (--m_wrrecurse))
				{
					LONG count = InterlockedExchangeAdd(&m_count, LONG_MAX);
					if (count < 0)
					{
						if (! ReleaseSemaphore(m_rdwset, -count, 0))
						{
							RWMUTEX_WIN_UNEXPECTED();
						}
					}
				}
				LeaveCriticalSection(&m_wrlock);
			}


		public:
			void lock_for_read() throw()
			{
				assert(m_count <= LONG_MAX);
				LONG count = InterlockedDecrement(&m_count);
				if (count < 0)
				{
					if (WaitForSingleObject(m_rdwset,INFINITE) != WAIT_OBJECT_0)
					{
						assert(false); RWMUTEX_WIN_UNEXPECTED();
					}
				}
			}

			void unlock_from_read() throw()
			{
				assert(m_count < LONG_MAX);
				LONG count = InterlockedIncrement(&m_count);
				if (count < 1)
				{
					if (! InterlockedDecrement(&m_rdwake))
					{
						if (! SetEvent(m_wrwset))
						{
							RWMUTEX_WIN_UNEXPECTED();
						}
					}
				} 
			}


		public:
			/*bool tryrdtowr() throw()
			{
			assert(m_count < LONG_MAX);
			if (TryEnterCriticalSection(&m_wrlock))
			{
			assert(! m_wrrecurse);
			++m_wrrecurse;
			if (m_wrrecurse == 1)
			{
			assert(m_count > -1);
			LONG count = 
			InterlockedExchangeAdd(&m_count, (-LONG_MAX) + 1) + 1;
			if (count < LONG_MAX)
			{
			if (InterlockedExchangeAdd(&m_rdwake, 
			LONG_MAX - count) + LONG_MAX - count)
			{
			if (WaitForSingleObject(m_wrwset, 
			INFINITE) != WAIT_OBJECT_0)
			{
			RWMUTEX_WIN_UNEXPECTED();
			}
			}
			}
			}
			return true;
			}
			return false;
			}*/
		};
	}
}
#endif
#endif
#endif