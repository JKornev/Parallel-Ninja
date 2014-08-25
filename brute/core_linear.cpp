#include "core_linear.h"
#include "client.h"

using namespace boost;
using namespace std;


CLinearCore::CLinearCore(CBruteClient* owner, IModule* mod, CStrParser* passwds) : 
	ICore(*mod), _owner(*owner), _parser(*passwds), _enabled(true)
{
	//_queue_mutex.lock();
}

CLinearCore::~CLinearCore()
{
}

bool CLinearCore::destroy()
{
	lock_guard<mutex> lock(_access_mutex);

	if (!_enabled)
		return false;

	if (_works.size() != 0)
		return false;

	_enabled = false;
	//_queue_mutex.unlock();
	return true;
}

bool CLinearCore::put_work(WorkInfo& work)
{
	lock_guard<mutex> lock(_access_mutex);

	if (!_enabled)
		return false;
	
	if (!_mod.check_login(work.login))
		return false;

	if (work.passwd_inx + work.passwd_count > _parser.Count())
		return false;

	work.flags = (work.passwd_inx == 0 ? 1 : 0);

	_works.push_back(work);
	//_queue_mutex.unlock();
	return true;
}

bool CLinearCore::cancel_work(uint32_t work_inx)
{
	lock_guard<mutex> lock(_access_mutex);
	list<WorkInfo>::iterator it = _works.begin();
	bool found = false;

	if (!_enabled)
		return false;

	while (it != _works.end()) {
		if (it->inx == work_inx) {
			_works.erase(it);
			found = true;
			break;
		}
		it++;
	}
	
	/*if (_works.size() == 0) {
		_queue_mutex.try_lock();
	}*/

	return found;
}

void CLinearCore::get_approved(ApproveInfo& approve)
{
	lock_guard<mutex> lock(_access_mutex);

	if (_approved.size() == 0)
		throw std::exception("core: can't approve list is empty");

	approve = _approved.front();
	_approved.pop_front();
}

bool CLinearCore::get_work_from_queue(ApproveInfo& work)
{
	lock_guard<mutex> lock(_access_mutex);

	if (!_enabled)
		return false;

	if (_works.size() == 0)
		return false;

	list<WorkInfo>::iterator it = _works.begin();
	work.inx = it->inx;
	work.login = it->login;

	if (it->flags == 1) {
		work.passwd = it->login;
		it->flags = 0;
	} else {
		work.passwd = _parser[it->passwd_inx];
		it->passwd_count--;
		it->passwd_inx++;
	}
	
	if (it->passwd_count == 0) {
		_works.erase(it);
		/*if (_works.size() == 0) {
			_queue_mutex.try_lock();
		}*/

		_owner.push_request(S_REQ_PUT_WORK);
	}

	return true;
}

bool CLinearCore::worker_entry(uint32_t inx)
{
	ApproveInfo work;
	bool res ;

	this_thread::sleep( posix_time::milliseconds(1) );//TOFIX
	//{ lock_guard<mutex> locker(_queue_mutex); } // locked if queue is empty

	if (!get_work_from_queue(work))
		return _enabled;

	// requests can be overlapped on MU gameserver, so we need retry for remove incorrect results
	for (int i = 0; i < 3; i++) {
		res = _mod.try_login(work);
		if (!res)
			break;
	}

	lock_guard<mutex> lock(_access_mutex);
	_counter++;
	if (res) {
		_approved.push_back(work);
		_owner.push_request(S_REQ_COMPLETE);
	}

	return true;
}