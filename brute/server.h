#pragma once

#include "shared.h"
#include "parser.h"

#include <fstream>
#include <deque>
#include <vector>

class CBruteServer : public INode {
public:
	CBruteServer(boost::mpi::environment& e, boost::mpi::communicator& w);

	virtual void init();
	virtual void start();
	
private:
	struct passwd_range {
		uint32_t start;
		uint32_t size;
	};

	struct worker_entry {
		uint32_t login_inx;
		uint32_t passw_inx;
	};

private:
	bool _inited;
	
	std::fstream _output;

	Configs _config;
	CStrParser _parser;
	std::vector<passwd_range> _passwds;
	std::deque<worker_entry> _work;

	//work context
	uint32_t _passwd_count;
	uint32_t _logins_counter;
	uint32_t _found_counter;
	std::vector<uint64_t> _counter;
	uint64_t _total_counter;
	time_t _timer;

private:
	void dispatch(std::set<uint32_t>& workers, uint32_t source, ClientRequest& req);
	void check_counters();

	bool next_work();
	static ModuleTypes get_mod_type(std::string& name);
	static const char* get_mod_str(uint32_t type);
};
