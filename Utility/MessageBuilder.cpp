#include "MessageBuilder.h"


// ------------ initiation and allocation methods ------------------------------

MessageBuilder::MessageBuilder(int capacity) {
	buffer = new uint8_t[capacity + 1];
	initialize(buffer, capacity);
}

MessageBuilder::MessageBuilder(uint8_t * buf, int capacity) {
	initialize(buf, capacity);
}

void MessageBuilder::initialize(uint8_t * buf, int capacity) {
	buffer = buf;
	_capacity = capacity;
	reset();
}

// --------------- Append methods using vsnprintf functions --------------------

boolean MessageBuilder::append(const __FlashStringHelper * fsh, ...) {
	va_list args;
	va_start(args, fsh);
	boolean ret = innerAppend((&buffer[_writeIndex]), _capacity - _writeIndex, (const char *) fsh, args);
	va_end(args);
	return (ret);
}

boolean MessageBuilder::append(const char * fmt, ...) {
	va_list args;
	va_start(args, fmt);
	boolean ret = innerAppend((&buffer[_writeIndex]), _capacity - _writeIndex, fmt, args);
	va_end(args);
	return (ret);
}

boolean MessageBuilder::innerAppend(uint8_t * buf, int bufferSize, const char * fmt, va_list args) {
	int bytesWritten = vsnprintf((char *)(&buffer[_writeIndex]), bufferSize, fmt, args);
	_writeIndex = min(_writeIndex + (bytesWritten >= 0 ? bytesWritten: 0), _capacity);
	_length = max(_length, _writeIndex);
	nullTerminateBuffer();

	if (bytesWritten < 0) { Serial.printf("Formatting error trying to append char array: %s\n", buf); }
	return (bytesWritten >= 0);
}

// --------------- utility and getter/setter classes ---------------------------

void MessageBuilder::nullTerminateBuffer() { buffer[_writeIndex] = '\0'; }
void MessageBuilder::reset() {
	_length = 0;
	_readIndex = 0;
	_writeIndex = 0;
}

void MessageBuilder::setLength(int i) {
	_writeIndex = min(_capacity - 1, i);
	_length = max(_length, _writeIndex);
	nullTerminateBuffer();
}

boolean MessageBuilder::append(char c) {
	boolean b = append ((uint8_t)c);
	nullTerminateBuffer();
	return (b);
}
boolean MessageBuilder::append(char * c) {
	while (*c != '\0') {
		if(!append(*c++)) {
			return false;
		}
	}
	return true;
}
boolean MessageBuilder::append(uint8_t * tempbuf, int length) {
	while (length-- > 0) {
		if (!append(*tempbuf++)) {
			return false;
		}
	}
	return true;
}

boolean MessageBuilder::append(uint8_t i) {
	if(_writeIndex >= _capacity) {
		return false;
	}
	buffer[_writeIndex++] = i;
	_length = max(_length, _writeIndex);
	return true;
}


boolean MessageBuilder::available() { return (_readIndex < _writeIndex); }
uint8_t MessageBuilder::peek() { return (buffer[_readIndex]); }
uint8_t MessageBuilder::read() {
	if (_readIndex >= _capacity - 1) { Serial.println("MessageBuilder: Buffer overflow on read"); return '\0'; }
	return (buffer[_readIndex++]);
}



// ------------------------- Debug methods -------------------------------------

void MessageBuilder::printEntireMessage() {
	printIndex();
	printChars();
	printHex();
}

void MessageBuilder::printIndex() {
	for (int i = 0; i < _writeIndex; i++) {
		Serial.printf("%3d",i);
	}
	Serial.print('\n');
}

void MessageBuilder::printChars() { printChars(buffer, _writeIndex); }
void MessageBuilder::printChars(uint8_t * buf, uint16_t length) {
	for (int i = 0; i < _writeIndex; i++) {
		Serial.print("  ");
		if (buf[i] >= 32 && buf[i] < 127) {
			Serial.print(buf[i]);
		} else {
			Serial.print('.');
		}
	}
	Serial.print('\n');
}

void MessageBuilder::printHex() { printHex(buffer, _writeIndex); }
void MessageBuilder::printHex(uint8_t * buf, uint16_t length) {
	for (int i = 0; i < _writeIndex; i++) {
		Serial.printf(" %02X", buf[i]);
	}
	Serial.print('\n');
}
