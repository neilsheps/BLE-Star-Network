#include "Logger.h"
#include "Arduino.h"

void Logger::initialize(Stream * stream, const char * pf) {
	_serial = stream;
	_prefix = pf;
	_serialAvailable = true;
}

void Logger::initialize(Stream * stream, const char * pf, int loggingLevel) {
	_serial = stream;
	_prefix = pf;
	_serialAvailable = true;
	setLoggingLevel(loggingLevel);
}

int Logger::getLoggingLevel() { return _loggingLevel; }
const char * Logger::getLoggingLevelText() { return (LOGGING_LEVEL_TEXT[_loggingLevel]); }

void Logger::setLoggingLevel(int ls) {
	_loggingLevel = ls;
	if (Serial) {
		Serial.printf("logging level = %d (%s)\n", _loggingLevel, LOGGING_LEVEL_TEXT[_loggingLevel]);
	}
}




/*
void Logger::e(const __FlashStringHelper * fsh, ...) { va_list args; log(ERROR, (const char *) fsh, args); }
void Logger::w(const __FlashStringHelper * fsh, ...) { va_list args; log(WARN, (const char *) fsh, args); }
void Logger::i(const __FlashStringHelper * fsh, ...) { va_list args; log(INFO, (const char *) fsh, args); }
void Logger::v(const __FlashStringHelper * fsh, ...) { va_list args; log(VERBOSE, (const char *) fsh, args); }
*/
void Logger::e(const char * fsh, ...) { va_list args; log(ERROR, (const char *) fsh, args); }
void Logger::w(const char * fsh, ...) { va_list args; log(WARN, (const char *) fsh, args); }
void Logger::i(const char * fsh, ...) { va_list args; log(INFO, (const char *) fsh, args); }
void Logger::v(const char * fsh, ...) { va_list args; log(VERBOSE, (const char *) fsh, args); }

void Logger::log(int level, const char * fsh, va_list args) {
	if (level <= _loggingLevel && _serialAvailable) {
		/*
		va_start(args, fsh);
		int i = strlen(fsh);
		char c[i];
		vsnprintf(c, i, fsh, *args);
		_serial->println(c);
		va_end(args);
		*/
	}
}

const char * Logger::getPrefix() { return (_prefix); }
