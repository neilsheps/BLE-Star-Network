#ifndef MessageBuilder_h
#define MessageBuilder_h

#include "Arduino.h"
#include "Utility/Logger.h"
#include <stdarg.h>

class MessageBuilder

{
public:

	MessageBuilder(int requestedSize);
	MessageBuilder(uint8_t * buf, int capacity);
	void initialize(uint8_t * buf, int capacity);

	void reset();

	boolean append(const __FlashStringHelper * fsh, ...);
	boolean append(const char * fsh, ...) ;
	boolean append(uint8_t a);
	boolean append(char a);
	boolean append (uint8_t * a);
	boolean append(char * a);
	boolean append(uint8_t * tempbuf, int length);
	boolean innerAppend(uint8_t * buf, int bufferSize, const char * fmt, va_list args);
	void nullTerminateBuffer();


	// R/W access methods
	int getCapacity() { return _capacity; }
	int getLength() { return _length; }
	void setLength(int i);

	uint8_t * getReadPointer() { return &buffer[_readIndex]; }
	int getReadIndex() { return _readIndex; }
	void setReadIndex(int i) { _readIndex = max(0, min(_capacity, i)); }

	uint8_t * getWritePointer() { return &buffer[_writeIndex]; }
	int getWriteIndex() { return _writeIndex; }
	void setWriteIndex(int i) { _writeIndex = max(0, min(_capacity, i)); }

	uint8_t * getBuffer() { return buffer;}
	boolean available();
	uint8_t read();
	uint8_t peek();


	// Debug methods
	void printEntireMessage();
	void printIndex();
	void printChars();
	void printChars(uint8_t * buf, uint16_t length);
	void printHex();
	void printHex(uint8_t * buf, uint16_t length);


private:

	uint8_t * buffer;
	int _capacity;
	int _writeIndex;
	int _readIndex;
	int _length;

};
#endif
