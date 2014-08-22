#include "mod_muonline.h"

using namespace boost;
using namespace boost::asio;
using namespace std;

CModMuonline::CModMuonline()
{
}

CModMuonline::~CModMuonline()
{
}

void CModMuonline::init(boost::property_tree::ptree &cfg)
{
	string temp;

	_serial = cfg.get<string>("MuOnline.serial");
	if (_serial.length() != 16)
		throw std::exception("incorrect serial length");

	temp = cfg.get<string>("MuOnline.enc_key");
	if (!_encr.LoadEncryptionKey(const_cast<char*>(temp.c_str())))
		throw std::exception("can't load encryption keys");

	_gs_addr.reset( 
		new ip::tcp::endpoint(
			ip::address::from_string( cfg.get<string>("MuOnline.gs_addr") ), 
			cfg.get<uint16_t>("MuOnline.gs_port")
		) 
	);

	temp = cfg.get<string>("MuOnline.bux_key");
	if (!parse_binary(temp, _bux_keys))
		throw std::exception("can't load bux keys");

	temp = cfg.get<string>("MuOnline.auth_key");
	if (!parse_binary(temp, _auth_key))
		throw std::exception("can't load auth keys");

	_protocol = cfg.get<uint8_t>("MuOnline.gs_protocol");
	if (_protocol != PROTOCOL_GMO && _protocol != PROTOCOL_KOR)
		throw std::exception("unknown gs protocol");
}

bool CModMuonline::check_login(std::string& login)
{
	return (login.length() <= 10 || login.length() >= 4);
}


bool CModMuonline::try_login(ApproveInfo& work)
{
	enum { MAX_RECONNECTS = 10 };
	for (int i = 0; i < MAX_RECONNECTS; i++) {
		int res = gs_login(work.login, work.passwd);
		if (res == E_FOUND)
			return true;
		else if (res == E_NOT_FOUND)
			return false;
	}
	return false;
}

int CModMuonline::gs_login(string& login, string& passwd)
{
	ip::tcp::socket sock(_service);
	system::error_code err;
	PMSG_JOINRESULT* packet_join;
	PMSG_IDPASS packet_idpass = {};
	PMSG_IDPASS_EX packet_idpass_ex = {};
	PMSG_RESULT* packet_res;
	uint32_t readed, size;
	char data[256];
	uint32_t counter = 0;

	sock.connect(*_gs_addr.get(), err);
	if (err == error::eof)
		return E_CONN_ERROR;

	// recv join info
	readed = read(sock, buffer(data, sizeof(PMSG_JOINRESULT)), err);
	if (readed < sizeof(PMSG_JOINRESULT) || err == error::eof)
		return E_CONN_ERROR;

/*	for (int i = 0; i < readed; i++) // TODEL espada encryption
		data[i] ^= 0xF4;*/

	packet_join = reinterpret_cast<PMSG_JOINRESULT*>(data);

	if (_protocol == PROTOCOL_KOR) {
		size = prepare_login_packet<PMSG_IDPASS>(
			packet_idpass, data, login.c_str(), passwd.c_str(), (char*)packet_join->CliVersion, _serial.c_str());
	} else {
		size = prepare_login_packet<PMSG_IDPASS_EX>(
			packet_idpass_ex, data, login.c_str(), passwd.c_str(), (char*)packet_join->CliVersion, _serial.c_str());
	}

	write(sock, buffer(data, size), err);
	if (err == error::eof)
		return E_CONN_ERROR;

	// recv result
	readed = read(sock, buffer(data, 5), err);
	if (err == error::eof)
		return E_CONN_ERROR; // Connection closed

	if (readed != 5)
		return false;

/*	for (int i = 0; i < readed; i++) // TODEL espada encryption
		data[i] ^= 0xF4;*/

	packet_res = reinterpret_cast<PMSG_RESULT*>(data);
	return packet_res->result == 1 ? E_FOUND : E_NOT_FOUND;
}

void CModMuonline::bux_convert(char* buf, int size)
{
	for (int n=0;n<size;n++)
		buf[n] ^= _bux_keys[n % 3];
}

void CModMuonline::auth_encrypt(void* data, int start, int end, int inc)
{
	uint8_t* buf = reinterpret_cast<uint8_t*>(data);
	for (int i = start; i != end; i += inc)
		buf[i] ^= _auth_key[i % 32] ^ buf[i - 1];
}