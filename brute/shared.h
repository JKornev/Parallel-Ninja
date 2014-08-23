#pragma once

#include <boost/mpi/environment.hpp>
#include <boost/mpi/communicator.hpp>
#include <boost/asio.hpp>
#include <boost/property_tree/ptree.hpp>

#if !defined(_DEBUG)
# define debug(msg) std::cout << msg << std::endl;
#else
# define debug(msg) 
#endif

class INode {
public:
	INode(boost::mpi::environment& e, boost::mpi::communicator& w) : _env(e), _world(w) { }
	virtual ~INode() { }

	virtual void init() = 0;
	virtual void start() = 0;

protected:
	boost::mpi::environment&  _env;
	boost::mpi::communicator& _world;
};

struct _WorkInfo;
struct _ApproveInfo;

class IModule {
public:
	virtual ~IModule() { }

	virtual void init(boost::property_tree::ptree &cfg) = 0;
	virtual bool check_login(std::string& login) = 0;
	virtual bool try_login(_ApproveInfo& work) = 0;
};

class ICore {
public:
	ICore(IModule& mod) : _mod(mod), _start_work(1), _counter(0) { }
	virtual ~ICore() { }

	virtual bool destroy() = 0;

	virtual bool put_work(_WorkInfo& work) = 0;
	virtual bool cancel_work(uint32_t inx) = 0;
	virtual void get_approved(_ApproveInfo& approve) = 0;

	uint32_t start_work_count() { return _start_work; }
	uint64_t get_counter() { return _counter; }

	static bool thread_entry(uint32_t inx, void* params)
	{
		ICore* pthis = reinterpret_cast<ICore*>(params);
		return pthis->worker_entry(inx);
	}

protected:
	uint32_t _start_work;
	uint64_t _counter;
	IModule& _mod;

	virtual bool worker_entry(uint32_t inx) = 0;
};

enum CoreTypes {
	C_CORE_UNKNOWN,
	C_CORE_LINEAR
};

enum ModuleTypes {
	C_MOD_UNKNOWN,
	C_MOD_MUONLINE
};

// Protocol

enum RequestTypes {
	//server
	C_REQ_REG,
	C_REQ_UNREG,
	C_REQ_WORK,
	C_REQ_APPROVE,
	C_REQ_REDIRECT,
	C_REQ_COUNTER,

	//client
	S_REQ_PUT_WORK,
	S_REQ_CANCEL,
	S_REQ_COMPLETE,
	S_REQ_QUIT,
	S_REQ_COUNTER,
};

enum StatusTypes {
	S_STATUS_OK,
	S_STATUS_FAIL
};


#pragma pack(push, 4)

typedef struct _Configs {
	uint16_t core;
	uint16_t module;

	bool IsInited() const { return _inited; }
	void SetInited(bool val) { _inited = val; }

	_Configs() : _inited(false), core(C_CORE_UNKNOWN), module(C_MOD_UNKNOWN) { }

	//serialize
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & _inited;
		ar & core;
		ar & module;
	}
	friend class boost::serialization::access;

private:
	bool _inited;
} Configs, *pConfigs;

typedef struct _ClientRequest {
	uint32_t req;
	uint32_t val;

	_ClientRequest& set(uint32_t r, uint32_t v) { req = r; val = v; return *this; }

	//serialize
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & req;
		ar & val;
	}
	friend class boost::serialization::access;
} ClientRequest, *pClientRequest;

#define WORKING_SET_SIZE 10

typedef struct _WorkInfo {
	uint32_t inx;
	std::string login;
	uint32_t passwd_inx;
	uint32_t passwd_count;

	_WorkInfo& set(uint32_t i, std::string& l, uint32_t pi, uint32_t ps)
		{ inx = i; login = l; passwd_inx = pi; passwd_count = ps; return *this; }

	//serialize
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & login;
		ar & inx;
		ar & passwd_inx;
		ar & passwd_count;
	}
	friend class boost::serialization::access;
} WorkInfo, *pWorkInfo;

typedef struct _ApproveInfo {
	std::string login;
	std::string passwd;
	uint32_t inx;

	//serialize
	template<class Archive>
	void serialize(Archive & ar, const unsigned int version)
	{
		ar & login;
		ar & passwd;
		ar & inx;
	}	
	friend class boost::serialization::access;
} ApproveInfo, *pApproveInfo;

typedef uint32_t ServerStatus;

#pragma pack(pop)
