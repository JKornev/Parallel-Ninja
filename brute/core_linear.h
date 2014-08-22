#pragma once

#include "shared.h"
#include "parser.h"

#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <vector>
#include <list>

class CBruteClient;

class CLinearCore : public ICore {
public:
	CLinearCore(CBruteClient* owner, IModule* mod, CStrParser* passwds);
	~CLinearCore();

	virtual bool destroy();

	virtual bool put_work(WorkInfo& work);
	virtual bool cancel_work(uint32_t inx);

	virtual void get_approved(ApproveInfo& approve);

	bool get_work_from_queue(ApproveInfo& work);

private:
	bool _enabled;

	// worked queue sync
	boost::mutex _queue_mutex;
	boost::mutex _access_mutex;

	CBruteClient& _owner;
	CStrParser& _parser;

	std::list<WorkInfo> _works;
	std::list<ApproveInfo> _approved;

protected:
	virtual bool worker_entry(uint32_t inx);
};