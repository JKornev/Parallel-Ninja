#include "server.h"

#include <boost/mpi.hpp>
#include <boost/mpi/status.hpp>
#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/thread/thread.hpp>

#include <string>
#include <iostream>
#include <set>
#include <exception>
#include <time.h>

using namespace boost;
using namespace std;

CBruteServer::CBruteServer(boost::mpi::environment& e, boost::mpi::communicator& w) : 
	INode(e, w),
	_inited(false)
{
}

void CBruteServer::init()
{
	enum { PASSWORD_BLOCK_SIZE = 1000 };

	_inited = false;

	cout << "Loading configs ...";

	try {
		property_tree::ptree pt;
		property_tree::ini_parser::read_ini("config.ini", pt);
		string temp;

		temp = pt.get<string>("Settings.file_logins");
		_parser.Parse(temp);
		if (_parser.Count() == 0)
			throw std::exception("logins list is empty");

		temp = pt.get<string>("Settings.file_output");
		_output.open(temp, fstream::out | fstream::app | fstream::binary);
		if (!_output.is_open())
			throw std::exception("can't open output file");

		temp = pt.get<string>("Settings.core");
		if (temp == "linear") {
			_config.core = C_CORE_LINEAR;
		} else {
			throw std::exception("unknown core type");
		}

		temp = pt.get<string>("Settings.module");
		_config.module = get_mod_type(temp);
		if (_config.module == C_MOD_UNKNOWN)
			throw std::exception("unknown module type");

		cout << "\b\b\bOK " << endl;

		_config.SetInited(true);
		_inited = true;
	} catch (std::exception const& e) {
		cout << "\b\b\bFailed" << endl
			 << "   init() exception: " << e.what() << endl;
	}

	// broadcast configs
	broadcast(_world, _config, 0);

	// recv passwords count
	uint32_t blocks_count;
	passwd_range range;

	_world.recv(1, 0, _passwd_count);

	blocks_count = _passwd_count / PASSWORD_BLOCK_SIZE;
	_passwds.reserve(blocks_count + 1);

	for (uint32_t i = 0; i < blocks_count; i++) {
		range.start = i * PASSWORD_BLOCK_SIZE;
		range.size = PASSWORD_BLOCK_SIZE;
		_passwds.push_back(range);
	}
	if (_passwd_count % PASSWORD_BLOCK_SIZE > 0) {
		range.start = blocks_count * PASSWORD_BLOCK_SIZE;
		range.size = _passwd_count % PASSWORD_BLOCK_SIZE;
		_passwds.push_back(range);
	}

	_counter.insert(_counter.begin(), _world.size() - 1, 0);
}

void CBruteServer::start()
{
	vector<mpi::request> reqs;
	vector<ClientRequest> commands;
	set<uint32_t> workers;

	time_t tstamp = time(NULL);
	tm* t = gmtime(&tstamp);

	_output << "// started " 
		<< t->tm_mday << "." << t->tm_mon << "." << t->tm_year + 1900 << " "
		<< t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec << endl;
	cout << "Computing beginning at " 
		<< t->tm_mday << "." << t->tm_mon << "." << t->tm_year + 1900 << " "
		<< t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec << endl
		<< "  module: " << get_mod_str(_config.module) << endl
		<< "  logins: " << _parser.Count() << endl
		<< "  passwords: " << _passwd_count << " (blocks:" << _passwds.size() << ")" << endl;

	commands.insert(commands.begin(), _world.size() - 1, ClientRequest());
	for (uint32_t i = 0, count = _world.size() - 1; i < count; i++) {
		reqs.push_back( _world.irecv(i + 1, 0, commands[i]) );
		workers.insert(i + 1);
	}

	_logins_counter = 0;
	_found_counter = 0;
	_total_counter = 0ull;
	_timer = time(NULL);

	while (workers.size() > 0) {
		std::pair<mpi::status, mpi::request*> res = mpi::wait_any(&reqs[0], &reqs[0] + reqs.size());
		uint32_t source = res.first.source();
		uint32_t inx = source - 1;

		dispatch(workers, source, commands[inx]);

		reqs[source - 1] = _world.irecv(source, 0, commands[inx]);
	}

	cout << "Computing stopped at " 
		<< t->tm_mday << "." << t->tm_mon << "." << t->tm_year + 1900 << " "
		<< t->tm_hour << ":" << t->tm_min << ":" << t->tm_sec << endl;
}


void CBruteServer::dispatch(set<uint32_t> &workers, uint32_t client_rank, ClientRequest& req)
{
	ServerStatus status;
	WorkInfo work;
	ApproveInfo approve;
	uint64_t counter;

	switch (req.req) {
	case C_REQ_REG:

		status = (_inited ? S_STATUS_OK : S_STATUS_FAIL);
		_world.send(client_rank, 1, status);

		if (status == S_STATUS_OK)
			cout << "[node #" << _world.rank() << "] the node#" << client_rank << " is registered" << endl;
		else
			cout << "[node #" << _world.rank() << "] registration aborted for the node#" << client_rank << endl;

		break;

	case C_REQ_UNREG:

		status = S_STATUS_OK;
		_world.send(client_rank, 1, status);
		workers.erase(client_rank);
		cout << "[node #" << _world.rank() << "] the node#" << client_rank << " is unregistered" << endl;

		break;

	case C_REQ_WORK: {

		status = (_work.size() == 0 && !next_work() ? S_STATUS_FAIL : S_STATUS_OK);
		_world.send(client_rank, 1, status);

		if (status == S_STATUS_FAIL) {
			cout << "[node #" << _world.rank() << "] work-request canceled for the node#" << client_rank << endl;
			break;
		}

		worker_entry& entry = _work.front();
		work.set(
			entry.login_inx, 
			string(_parser[entry.login_inx]), 
			_passwds[entry.passw_inx].start, 
			_passwds[entry.passw_inx].size
		);
		_work.pop_front();

		_world.send(client_rank, 2, work);
		cout << "[node #" << _world.rank() << "] work is sended to the node#" << client_rank 
			<< " (index: " << work.inx << ")" << " pass " << work.passwd_inx << " count " << work.passwd_count << endl;

	} break;

	case C_REQ_APPROVE:

		status = S_STATUS_OK;
		_world.send(client_rank, 1, status);

		_world.recv(client_rank, 2, approve);
		_output << approve.login << ":" << approve.passwd << endl;

		_found_counter++;

		// broadcast cancel work
		/*			for (set<uint32_t>::iterator entry = workers.begin(); entry != workers.end(); entry++) {
			if (*entry == client_rank)
				continue;
			_world.send(*entry, 5, command.set(S_REQ_CANCEL, approve.inx));
		}*/

		cout << "[node #" << _world.rank() << "] found login:" << approve.login 
			<< " password:" << approve.passwd << " index:" << approve.inx << " rank:" << client_rank << endl;

		break;

	case C_REQ_COUNTER:

		status = S_STATUS_OK;
		_world.send(client_rank, 1, status);
		_world.recv(client_rank, 2, counter);

		_counter[client_rank - 1] = counter;

	//	cout << "[node #" << _world.rank() << "] counter " << counter << " from rank:" << client_rank << endl;

		break;

	default:
		cout << "[node #" << _world.rank() << "] unknown request " 
			<< "(source:" << client_rank << " request:" << req.req << " value:" << req.val << ")" << endl;
		break;
	}

	check_counters();
}

void CBruteServer::check_counters()
{
	enum { COUNTER_REFRESH_TIME = 60 };
	time_t tm = time(NULL);

	if (tm - _timer < COUNTER_REFRESH_TIME)
		return;

	uint64_t counter = 0;
	for (uint32_t i = 0, count = _counter.size(); i < count; i++)
		counter += _counter[i];

	cout << "Statistics: processed " << counter << ", found " << _found_counter 
		<< ", speed " << (counter -  _total_counter) << " passwd/min" << endl;

	_total_counter = counter;
	_timer = tm;
}

bool CBruteServer::next_work()
{
	worker_entry entry;

	if (_logins_counter >= _parser.Count())
		return false;

	entry.login_inx = _logins_counter++;
	for (uint32_t i = 0, count = _passwds.size(); i < count; i++) {
		entry.passw_inx = i;
		_work.push_back(entry);
	}

	return true;
}

ModuleTypes CBruteServer::get_mod_type(string& name)
{
	if (name == "muonline")
		return C_MOD_MUONLINE;
	
	return C_MOD_UNKNOWN;
}

const char* CBruteServer::get_mod_str(uint32_t type)
{
	switch (type) {
	case C_MOD_MUONLINE:
		return "muonline";
	default:
		break;
	}
	return "unknown";
}