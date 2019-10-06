#ifndef LOGGER_HPP_
#define LOGGER_HPP_

#define ENABLE_DEBUG

#include <iostream>
#include <cstring>

class Logger {
public:
	static void debug(const std::string &str) {
	#ifdef ENABLE_DEBUG
		std::cerr << "DEBUG: " << str << "\n";
	#else
		(void)str;
	#endif
	}
	
	static void error(const std::string &str) {
		std::cerr << "ERROR: " << str << "\n";
	}
};

#endif
