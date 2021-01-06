#ifndef CRC_h
#define CRC_h

#include "Arduino.h"
#include <stdarg.h>


#define CRC_START_MODBUS			0xFFFF
#define	CRC_POLY_16					0xA001
#define CRC_POLY_CCITT				0x1021


class CRC {

public:
	static void initializeTables();
	static void initializeCrc8Table();
	static void initializeCrc16Table();

	static uint8_t getCrc8(uint8_t * messageToCreateChecksum, int messageLength);
	static uint16_t getCrc16(uint8_t * messageToCreateChecksum, int messageLength);

private:
	static boolean crcTablesInitialized;
	static uint8_t crc8Table[256];
	static uint16_t crc16Table[256];
};

#endif
