// KISTest.cpp : Defines the entry point for the console application.
//

#include "AuxKis.hxx"

#include <tchar.h>
#include<chrono>
#include<thread>
#include<memory>
#include<iostream>

namespace core {
	class Request {
	public:
		Request() {}
	};
	
	using RequestPtr = std::shared_ptr<Request>;
		
	class Appfacade {
	public:
		static Appfacade& instance() {
			kis::Guardlock<kis::Mutex> lLock(mutex_);
			static Appfacade lFacade;
			return lFacade;
		}
		static kis::Queue<RequestPtr>& Queue() {
			return instance().queue_;
		}
		static kis::EventerPtr Stop() {
			return instance().stop_;
		}
	protected:
		Appfacade() {
			stop_ = std::make_shared<kis::Event>(true);
		};
	private:
		kis::Queue<RequestPtr>	queue_;
		kis::EventerPtr			stop_;
		static kis::Mutex		mutex_;
	};

	kis::Mutex core::Appfacade::mutex_;

	class SomeProcessing {
	public:
		static void ProcessRequest(RequestPtr Req) {
			const int lMax = kis::Utils::Random(1, 5);
			for(int i = 0; i < lMax; ++i) {
				if(Appfacade::Stop()->WaitForSignal(0)) {
					break;
				}
				//Some do ...
				Req;
				//std::cout << "Do..." << std::endl;
				std::this_thread::sleep_for(std::chrono::nanoseconds(kis::Utils::Random(1, 100)));
			}
		}
	protected:
	private:
	};
	
	class SomeRequestSource	{
	public:
		static RequestPtr GetRequest() {
			if (Appfacade::Stop()->WaitForSignal(0)) {
				return nullptr;
			}
			return std::make_shared<Request>();
		}
	protected:
	private:
	};

	class GenRequest : public kis::Thread {
	public:
		GenRequest() {}
	protected:
		inline unsigned int ExecuteImpl() override {
			for (;;) {
				std::this_thread::sleep_for(std::chrono::nanoseconds(kis::Utils::Random(1, 100)));
				RequestPtr lRequest = SomeRequestSource::GetRequest();
				if(!lRequest) {
					break;
				}
				Appfacade::Queue().Push(lRequest);
			}
			return 0;
		}
	private:
	};

	class ProcRequest : public kis::Thread {
	public:
		ProcRequest() {}
	protected:
		inline unsigned int ExecuteImpl() override {
			for(;;) {				
				std::list<kis::EventerPtr> lEvents = {Appfacade::Stop(), Appfacade::Queue().GetEvent(), kis::Thread::Stop() };
				kis::Monitor	lMonitor(lEvents, INFINITE);
				HANDLE			lId = lMonitor.Wait();

				if (lId == INVALID_HANDLE_VALUE || lId == (*Appfacade::Stop()) || lId == (*kis::Thread::Stop())) {
					break;
				} else if(*(Appfacade::Queue().GetEvent()) == lId) {
					RequestPtr lRequest;
					if(!Appfacade::Queue().Pop(lRequest)) {
						continue;
					}
					
					SomeProcessing::ProcessRequest(lRequest);
				}
			}
			return 0;
		}
	private:
	};

}

using ProcThreadPtr = std::shared_ptr<core::ProcRequest>;
using GenThreadPtr = std::shared_ptr<core::GenRequest>;

int _tmain(int argc, _TCHAR* argv[])
{
	std::srand((unsigned int)time(NULL));
	
	std::list<GenThreadPtr>		lGen;
	std::list<ProcThreadPtr>	lProc;

	const int lGenCount = kis::Utils::Random(2, 10);
	const int lProcCount = kis::Utils::Random(2, 10);

	for (int i = 0; i< lGenCount; ++i) {
		lGen.push_back(std::make_shared<core::GenRequest>());
	}

	for(int i = 0; i < lProcCount; ++i) {
		lProc.push_back(std::make_shared<core::ProcRequest>());
	}

	std::this_thread::sleep_for(std::chrono::seconds(3));

	core::Appfacade::Stop()->Signal();

	for (auto lThread : lGen) {
		lThread->Join();
	}
	
	for(auto lThread : lProc) {
		lThread->Join();
	}

	std::cout << "Proc - " << lProcCount << std::endl << "Gen - " << lGenCount << std::endl;
	std::cout << "Size - " << core::Appfacade::Queue().Size() << std::endl;
	for (;;) {
		core::RequestPtr lRequest;
		if(!core::Appfacade::Queue().Pop(lRequest)) {
			break;
		}
	}
	
	return 0;
}

