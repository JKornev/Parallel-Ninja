#include "pool.h"

using namespace std;
using namespace boost;

CThreadPool::CThreadPool() : _created(false), _active_threads(0)
{
}

CThreadPool::CThreadPool(uint32_t pool_size, pool_thread_entry proc, void* params) : _created(false), _active_threads(0)
{
	create(pool_size, proc, params);
}

CThreadPool::~CThreadPool()
{
	destroy();
}

void CThreadPool::create(uint32_t pool_size, pool_thread_entry proc, void* params)
{
	lock_guard<mutex> guard(_mutex);
	if (_created)
		throw std::exception("pool: already created");

	_created = false;
	_threads.clear();
	_is_active_thread.clear();
	_active_threads = 0;
	_entry = proc;
	_entry_params = params;

	if (pool_size == 0)
		throw std::exception("pool: invalid pool size");

	for (uint32_t i = 0; i < pool_size; i++)
		_threads.push_back(thread(boost::bind(thread_entry, this, i)));

	_is_active_thread.insert(_is_active_thread.begin(), pool_size, true);

	_created = true;
}

void CThreadPool::destroy()
{
	close_all_threads();

	_mutex.lock();
	_created = false;
	_mutex.unlock();

	//wait
	for (uint32_t i = 0, count = _threads.size(); i < count; i++)
		_threads[i].join();
}

void CThreadPool::close_thread(uint32_t thr_inx)
{
	lock_guard<mutex> guard(_mutex);
	if (!_created)
		return;

	if (_is_active_thread[thr_inx]) {
		_is_active_thread[thr_inx] = false;
	}
}

void CThreadPool::close_all_threads()
{
	lock_guard<mutex> guard(_mutex);
	if (!_created)
		return;

	for (uint32_t i = 0, count = _is_active_thread.size(); i < count; i++) {
		if (_is_active_thread[i]) {
			_is_active_thread[i] = false;
		}
	}
}

uint32_t CThreadPool::get_active_count()
{
	lock_guard<mutex> guard(_mutex);
	return _active_threads;
}

void CThreadPool::thread_entry(void* owner, uint32_t inx)
{
	CThreadPool* pthis = reinterpret_cast<CThreadPool*>(owner);
	pthis->worker(inx);
}

void CThreadPool::worker(uint32_t inx)
{
	pool_thread_entry entry;
	bool active;

	_mutex.lock();
	entry = _entry;
	_active_threads++;
	_mutex.unlock();

	while (_created) {
		_mutex.lock();
		active = _is_active_thread[inx];
		_mutex.unlock();

		if (!active)
			break;

		if (!entry(inx, _entry_params)) {
			_mutex.lock();
			_is_active_thread[inx] = false;
			_mutex.unlock();
		}
	}

	_mutex.lock();
	_active_threads--;
	_mutex.unlock();
}