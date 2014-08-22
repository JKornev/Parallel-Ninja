#pragma once

#include "shared.h"
#include "parser.h"
#include "pool.h"

#include <memory>
#include <queue>

#include <boost/thread/mutex.hpp>

class CBruteClient : public INode {
public:
	CBruteClient(boost::mpi::environment& e, boost::mpi::communicator& w);
	~CBruteClient();

	virtual void init();
	virtual void start();

	void send_command(ClientRequest& req, ServerStatus& status);

	void push_request(uint32_t req);
	bool pop_request(ClientRequest& req);

private:
	bool _inited;

	std::auto_ptr<ICore> _core;
	std::auto_ptr<IModule> _module;

	CStrParser _parser;
	CThreadPool _pool;

	std::queue<ClientRequest> _requests;
	boost::mutex _sync;
	boost::mpi::request _ireq;

	time_t _timer;

private:
	void check_counter();

};
