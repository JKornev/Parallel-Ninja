#pragma once

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>
#include <stdint.h>

typedef bool(*pool_thread_entry)(uint32_t inx, void* param);

class CThreadPool {
public:
	CThreadPool();
	CThreadPool(uint32_t pool_size, pool_thread_entry proc, void* params);
	~CThreadPool();

	void create(uint32_t pool_size, pool_thread_entry proc, void* params);
	void destroy();

	void close_thread(uint32_t thr_inx);
	void close_all_threads();

	uint32_t get_active_count();

private:
	bool _created;
	boost::mutex _mutex;
	std::vector<boost::thread> _threads;
	std::vector<bool> _is_active_thread;
	uint32_t _active_threads;
	pool_thread_entry _entry;
	void* _entry_params;

private:
	static void thread_entry(void* owner, uint32_t inx);
	void worker(uint32_t inx);
};