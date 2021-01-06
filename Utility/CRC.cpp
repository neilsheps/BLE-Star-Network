#include "CRC.h"


void CRC::initializeTables() {
	initializeCrc8Table();
	initializeCrc16Table();
	crcTablesInitialized = true;
}

void CRC::initializeCrc8Table() {
	const uint8_t generator = 0x1D;

	for (int i = 0; i < 256; i++) {
		uint8_t thisByte = (uint8_t) i;
		for (uint8_t bit =0; bit < 8; bit++) {
			if ((thisByte & 0x80) != 0) {
				thisByte <<= 1;
				thisByte ^= generator;
			} else {
				thisByte <<= 1;
			}
		}
		crc8Table[i] = thisByte;
	}
}

void CRC::initializeCrc16Table() {
    uint16_t i;
    uint16_t j;
    uint16_t crc;
    uint16_t c;
    	for (i = 0; i < 256; i++) {
    		crc = 0;
    		c = i;
    		for (j = 0; j < 8; j++) {
    			if ( (crc ^ c) & 0x0001 ) {
                    crc = ( crc >> 1 ) ^ CRC_POLY_16;
                } else {
                    crc =   crc >> 1;
                }
    			c = c >> 1;
    		}
    		crc16Table[i] = crc;
    	}
}


uint8_t CRC::getCrc8(uint8_t * messageToValidate, int messageLength) {
	uint8_t crc8 = 0;
	for (int i = 0; i < messageLength; i++) {
		uint8_t data = (uint8_t)(messageToValidate[i] ^ crc8);
		crc8 = (uint8_t)(crc8Table[data]);
	}
	return crc8;
}


uint16_t CRC::getCrc16(uint8_t * messageToValidate, int messageLength) {
    uint16_t crc16 = CRC_START_MODBUS;
    if (!crcTablesInitialized) { Serial.println("Checksum: Checksum table not initialized; getChecksum() called"); }
    for (int i = 0; i < messageLength; i++) {
		crc16 = (crc16 >> 8) ^ crc16Table[ (crc16 ^ (uint16_t) messageToValidate[i]) & 0x00FF ];
	}
    return (crc16);
}
