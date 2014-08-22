#include "parser.h"

#include <exception>
#include <fstream>

using namespace std;

CStrParser::CStrParser()
{
}

CStrParser::~CStrParser()
{
}

void CStrParser::Parse(std::string& path)
{
	fstream file(path, fstream::in | fstream::binary);

	_pointers.clear();
	_buf.clear();

	if (!file.is_open())
		throw std::exception("can't open file");

	file.seekg(0, file.end);
	_buf_size = file.tellg();
	file.seekg(0, file.beg);

	if (_buf_size == 0)
		throw std::exception("file is empty");

	_buf.insert(_buf.begin(), _buf_size + 1, 0);
	file.read(&_buf[0], _buf_size);

	char* entry = &_buf[0];
	for (uint32_t i = 0; i < _buf_size; i++) {
		if (_buf[i] == '\r' || _buf[i] == '\n' || _buf[i] == '\0') {
			_buf[i] = '\0';

			if (entry != 0) {
				_pointers.push_back(entry);
				entry = 0;
			}

		} else if (entry == 0) {
			entry = &_buf[i];
		}
	}
	if (entry != 0)
		_pointers.push_back(entry);
}

uint32_t CStrParser::Count() const
{
	return _pointers.size();
}

char* CStrParser::operator[] (uint32_t inx)
{
	if (_pointers.size() <= inx)
		throw std::exception("index out of range");
	return _pointers[inx];
}
