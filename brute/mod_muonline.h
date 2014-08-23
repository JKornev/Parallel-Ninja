#pragma once

#include "shared.h"
#include "mu_utils.h"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

#include <memory>

class CModMuonline : public IModule {
public:

	enum {
		E_FOUND,
		E_NOT_FOUND,
		E_CONN_ERROR,
	};

	enum {
		PROTOCOL_V1 = 0,
		PROTOCOL_V2 = 1,
		PROTOCOL_V3 = 2,
	};

	CModMuonline();
	~CModMuonline();

	virtual void init(boost::property_tree::ptree &cfg);
	virtual bool check_login(std::string& login);
	virtual bool try_login(ApproveInfo& work);

private:
	std::string _serial;
	CSimpleModulus _encr;
	std::auto_ptr<boost::asio::ip::tcp::endpoint> _gs_addr;
	boost::asio::io_service _service;

	unsigned char _bux_keys[3];
	uint8_t _auth_key[32];
	uint8_t _protocol;

private:
	int gs_login(std::string& login, std::string& passwd);

	void bux_convert(char* buf, int size);
	void auth_encrypt(void* data, int start, int end, int inc);

	template<typename T>
	uint32_t prepare_login_packet(T& packet, char* data, 
		const char* login, const char* passwd, const char* vers, const char* serial)
	{
		build_login_packet<T>(&packet, login, passwd, vers, serial);

		uint32_t size = _encr.Encrypt(0, &packet.h.size, sizeof(T) - 1);
		_encr.Encrypt(&data[2], &packet.h.size, sizeof(T) - 1);

		size += 2;
		data[0] = (char)0xC3;
		data[1] = (char)size;
		return size;
	}

	template<typename T>
	void build_login_packet(T* packet, const char* login, const char* passwd, const char* version, const char* serial)
	{
		memset(packet, 0, sizeof(T));

		packet->h.c = 0xC1;
		packet->h.size = 0;
		packet->h.headcode = 0xF1;

		packet->subcode = 1;

		uint32_t len = strlen(login);
		memcpy(packet->Id, login, len <= sizeof(packet->Id) ? len : sizeof(packet->Pass));
		len = strlen(passwd);
		memcpy(packet->Pass, passwd, len <= sizeof(packet->Pass) ? len : sizeof(packet->Pass));

		packet->TickCount = GetTickCount();

		memcpy(packet->CliVersion, version, sizeof(packet->CliVersion));
		memcpy(packet->CliSerial, serial, sizeof(packet->CliSerial));

		bux_convert(packet->Id, sizeof(packet->Id));
		bux_convert(packet->Pass, sizeof(packet->Pass));

		auth_encrypt(packet, sizeof(PBMSG_HEAD), sizeof(T), 1);
	}

	template<typename T>
	bool parse_binary(std::string& src, T& dest)
	{
		const char *str = src.c_str();
		uint32_t offset = 0, value, a = 0;

		for (uint32_t len = src.size(); a < sizeof(T) && offset < len 
		&& sscanf(str + offset, "%x,", &value) == 1; offset++) {
			if (value > 255)
				return false;

			dest[a++] = value;
			for (; offset < len && str[offset] != ','; offset++);
		}

		return a == sizeof(T);
	}
};