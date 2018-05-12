#pragma once


#include <deque>
#include <string>
#include <map>
#include <list>
#include <vector>
#include <memory>

#include <process.h>
#include <Windows.h>

namespace kis {

	class Mutex {
	public:
		Mutex(const Mutex&) = delete;
		Mutex(const Mutex&&) = delete;
		Mutex() {
			InitializeCriticalSection(&csection_);
		}
		~Mutex() {
			DeleteCriticalSection(&csection_);
		}
		inline void Lock() {
			EnterCriticalSection(&csection_);
		}
		inline void Unlock() {
			LeaveCriticalSection(&csection_);
		}
	private:
		CRITICAL_SECTION csection_;
	};

	template<typename T>
	class Guardlock {
	public:
		Guardlock() = delete;
		Guardlock(const Guardlock&) = delete;
		Guardlock(const Guardlock&&) = delete;
		Guardlock(Mutex& Lock) : lock_(Lock) {
			lock_.Lock();
		}
		~Guardlock() {
			lock_.Unlock();
		}
	private:
		T & lock_;
	};

	class Monitor;
	class Event
	{
		friend class Monitor;
	public:
		Event(const Event&) = delete;
		Event(const Event&&) = delete;
		Event(bool Manual = false) : event_(INVALID_HANDLE_VALUE) {
			event_ = CreateEvent(NULL, Manual ? TRUE : FALSE, FALSE, NULL);
		}
		~Event() {
			if (event_ != INVALID_HANDLE_VALUE) {
				CloseHandle(event_);
			}
		}
		inline void Signal() {
			SetEvent(event_);
		}
		inline bool WaitForSignal(int Timeout) const {
			bool lResult = false;
			switch (WaitForSingleObject(event_, Timeout)) {
				case WAIT_OBJECT_0:
					lResult = true; break;
				case WAIT_TIMEOUT:					
				default:
					break;		
			}
			return lResult;
		}
		inline bool operator == (const HANDLE Id) const {
			return event_ == Id;
		}
		inline bool operator == (const Event& Self) const {
			return event_ == Self.event_;
		}
		inline operator HANDLE() {
			return event_;
		}
	private:
		HANDLE event_;
	};
		
	typedef	std::shared_ptr<Event> EventerPtr;
	
	template<typename T>
	class Queue {
	public:
		Queue(const Queue<T>&) = delete;
		Queue(const Queue<T>&&) = delete;
		Queue()  {
			event_ = std::make_shared<Event>();
		}
		inline void Push(const T & Object) {
			{
				Guardlock<Mutex> lGuard(lock_);
				queue_.emplace_back(Object);
			}
			event_->Signal();
		}
		inline bool Pop(T & Object) {
			Guardlock<Mutex> lGuard(lock_);
			if(!queue_.empty()) {
				Object = queue_.front();
				queue_.pop_front();
				return true;
			}
			return false;
		}
		inline bool Wait(int Timeout) const {
			return event_->WaitForSignal(Timeout);
		}
		inline EventerPtr& GetEvent() { return event_; }
		inline auto Size() const {
			Guardlock<Mutex> lGuard(lock_);
			return queue_.size();
		}
	private:
		std::deque<T>	queue_;
		mutable Mutex	lock_;
		EventerPtr		event_;
	};

	class Monitor
	{
	public:
		Monitor(const Monitor&) = delete;
		Monitor(const Monitor&&) = delete;
		Monitor(std::list<EventerPtr>& Objects, int Timeout) : timeout_(Timeout) {
			objects_.resize(Objects.size());
			size_t lIt = 0;
			for(auto lObj = Objects.begin(); Objects.end() != lObj; ++lIt, ++lObj) {
				objects_[lIt] = (*lObj)->event_;
			}
		}
		inline HANDLE Wait() {
			const DWORD lObjectId = WaitForMultipleObjects(objects_.size(), (HANDLE*)(&objects_[0]), FALSE, timeout_);
			HANDLE lReturn = INVALID_HANDLE_VALUE;
			if(lObjectId != WAIT_FAILED && lObjectId != WAIT_TIMEOUT && (lObjectId >= WAIT_OBJECT_0 && lObjectId < objects_.size()) ) {
				lReturn = objects_[lObjectId];
			}else if(lObjectId == WAIT_TIMEOUT) {
				lReturn = 0;
			}
			return lReturn;
		}
	protected:
	private:
		std::vector<HANDLE> objects_;
		int					timeout_;
	};
	
	class Thread
	{
	public:
		Thread(const Thread&) = delete;
		Thread(const Thread&&) = delete;		
		Thread() :thread_(INVALID_HANDLE_VALUE) {
			stop_ = std::make_unique<Event>();
			thread_ = (HANDLE)_beginthreadex(NULL, 0, Thread::ThreadImpl, this, 0, NULL);
		}
		virtual ~Thread() {
			if (thread_ != INVALID_HANDLE_VALUE) {
				stop_->Signal();
				WaitForSingleObject(thread_, INFINITE);
			}
		}
		inline void Join() {
			if (thread_ != INVALID_HANDLE_VALUE && thread_ > 0) {
				WaitForSingleObject(thread_, INFINITE);
			}
		}
	protected:
		virtual unsigned int ExecuteImpl() = 0;
		
		static unsigned int __stdcall ThreadImpl(void* This) {
			Thread* lThis = static_cast<Thread*>(This);
			const unsigned int lReturn = lThis->ExecuteImpl();
			_endthreadex(lReturn);
			return lReturn;
		}
		inline EventerPtr Stop() { return stop_; }
	private:
		HANDLE		thread_;
		EventerPtr	stop_;
	};
	
	class Utils	{
	public:
		static int Random(int Min, int Max) {
			return rand() % (Max - Min + 1) + Min;
		}
	};
	
}

