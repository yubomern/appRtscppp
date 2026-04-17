#pragma  once  
#include <string>
#include  <thread>
#include <sstream>
#include <functional>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <atomic>
#include <fstream>
#include <future>

enum class TaskState {

	Ready,
	Running,
	Finished ,
	Stopped  , 
	Error  
};


using TaskFn = std::function <void(std::atomic<bool>& stopFlag)>;
class Task {



private  :

	void run_wrapper();

	std::string m_name;

	TaskFn m_fn;

	std::atomic <TaskState> m_state;

	std::atomic <bool> m_stopFlag;


	std::future<void> m_future;
	std::thread m_thread;

	std::mutex m_joinMutex;


public  :

	Task(std::string name, TaskFn fn);
	~Task();


	void start();


	void request_stop();


	void force_stop();


	TaskState  get_state() const;
	TaskState set_state(TaskState s);
	std::string get_name()const;


	void join();


	std::future<void > get_future();
};





