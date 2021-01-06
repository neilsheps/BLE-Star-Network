#ifndef Logger_h
#define Logger_h

#include <stdarg.h>
#include "Arduino.h"

/*
	Simple logging utility class to log messages related to BLE scanning, connection, or data transmission
*/



class Logger {

public:

	int QUIET = 		0;														// Serial output is suppressed
	int ERROR =  		1;														// only critical errors related to code execution
	int WARN = 			2;														// data transmission or similar errors
	int INFO = 			3;														// non-trivial, default messages
	int VERBOSE = 		4;														// everything with a "log" call
	static constexpr char LOGGING_LEVEL_TEXT[5][8] = { "QUIET", "ERROR", "WARN", "INFO", "VERBOSE" };

	void initialize(Stream * hwSerial, const char * pf);
	void initialize(Stream * hwSerial, const char * pf, int loggingLevel);

	static void setLoggingLevel(int lm);
	static int getLoggingLevel();
	static const char * getLoggingLevelText();

/*
	void e(const __FlashStringHelper * fsh, ...);
	void w(const __FlashStringHelper * fsh, ...);
	void i(const __FlashStringHelper * fsh, ...);
	void v(const __FlashStringHelper * fsh, ...);
*/
	void e(const char * fsh, ...);
	void w(const char * fsh, ...);
	void i(const char * fsh, ...);
	void v(const char * fsh, ...);

	const char * getPrefix();


private:

	void log(int level, const char * fsh, va_list args);
	static int _loggingLevel;
	bool _serialAvailable = false;

	Stream *  _serial;
	const char * _prefix;

};


#endif
