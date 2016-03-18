#ifndef DFSPARKS_TASK_H
#define DFSPARKS_TASK_H
#include <atomic>
#include <cstdio>
#include <thread>
#include <functional>

namespace dfsparks {
   
	class Task {
	public:
	    template<typename Func, typename... Args>
		Task(const Func &f, Args... args) : stopped(true) {
			func = std::bind(f, std::placeholders::_1, args...);
		}

		Task(const Task&) = delete;

		~Task() {
			stop();
		}

		Task& operator=(Task&) = delete;
		
		void run() noexcept {
			if (stopped.exchange(false)) {			
				run_and_handle_exceptions();
				stopped = true;
			}
		}

		void start() {
			if (stopped.exchange(false)) {
				thread = std::thread(&Task::run_and_handle_exceptions, this);
			}
		}

		void stop() {
			if (!stopped.exchange(true)) {
				thread.join();
			}
		}
	private:
		void run_and_handle_exceptions() noexcept {
			try {
				func(stopped);
			}	
			catch(const std::exception& ex) {
    			std::fprintf(stderr, "ERROR: %s\n", ex.what());
  			}
  			stopped.exchange(true);  
		}
		std::atomic<bool> stopped;
		std::thread thread;
		std::function<void(std::atomic<bool>&)> func;
	};

} // namespace dfsparks

#endif /* DFSPARKS_TASK_H */