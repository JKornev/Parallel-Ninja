#pragma once

#include <stdint.h>
#include <vector>
#include <memory>

class CStrParser {
public:
	CStrParser();
	~CStrParser();

	void Parse(std::string& path);

	uint32_t Count() const;
	char* operator[] (uint32_t inx);

public:
	std::vector<char> _buf;
	uint32_t _buf_size;

	std::vector<char*> _pointers;

};