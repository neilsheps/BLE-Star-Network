#ifndef CommonDefinitions_h
#define CommonDefinitions_h

#include "Arduino.h"

#define ARCH_NRF51				0
#define ARCH_NRF52				1
#define ARCH_UNKNOWN			-1

#if defined(ARDUINO_ARCH_SAM)
	#define MAX_CENTRAL_CONNECTIONS				0
	#define ARCHITECTURE_NRF51
	const int ARCHITECTURE = ARCH_NRF51;

#elif defined(ARDUINO_ARCH_SAMD)
	#define MAX_CENTRAL_CONNECTIONS				0
	#define ARCHITECTURE_NRF51
	const int ARCHITECTURE = ARCH_NRF51;

#elif defined(ARDUINO_ARCH_NRF52)
	#define MAX_CENTRAL_CONNECTIONS				6
	#define ARCHITECTURE_NRF52
	#include "Bluefruit.h"
	const int ARCHITECTURE = ARCH_NRF52;

#else
	#error "This library only supports Adafruit boards based on nRF52 or Feather M0 Bluefruit boards (it has not been tested beyond that)"
#endif


#define MAX_BLE_CHUNK_LENGTH								20					// must send 20 byte chunks at most unf.
#define DEFAULT_MAX_SEND_ATTEMPTS							3
#define DEFAULT_MAX_HOP_ATTEMPTS							5
#define DEFAULT_EXPECTED_MESSAGE_LENGTH						250					// if messageTable needs to allocate space for an incoming message of unknown length, pick this
#define MAX_COMPILED_MESSAGE_LENGTH							4000				// enough for most messages
#define MAX_UNFORMED_MESSAGE_LENGTH							255					// packet loss will mean trying for more unlikely to work
#define MAX_BLE_CHUNKS										(MAX_COMPILED_MESSAGE_LENGTH) / (MAX_BLE_CHUNK_LENGTH - 1) + 1
#define DEFAULT_MAX_SEND_ATTEMPTS							3
#define MAX_BLE_DEVICE_NAME_LENGTH							20

#define MAX_MESSAGE_BUFFER_PREAMBLE_LENGTH					(MAX_BLE_DEVICE_NAME_LENGTH + 1) * 2 + 8 + 1		// max size of origins, destinations etc and preamble of routed message

#define CHUNK_FLAG_TABLE_CAPACITY							(MAX_BLE_CHUNKS) / 8 + 1

const uint8_t bitSetArray[8] = { 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80 };
const uint8_t bitResetArray[8] = { 0xFE, 0xFD, 0xFB, 0xF7, 0xEF, 0xDF, 0xBF, 0x7F };

#define BLE_THIS_DEVICE_INDEX								0
#define BLE_PERIPHERAL_INDEX								1
#define BLE_CENTRAL_INDEX_0									2
#define BLE_CENTRAL_INDEX_1									3 // etc.

#define MESSAGE_TYPE_NONE					0
#define MESSAGE_TYPE_ORIGIN					1
#define MESSAGE_TYPE_INCOMING				2
#define MESSAGE_TYPE_HOP					3
#define MESSAGE_TYPE_SUCCESS				4

#define STRING_ROUTES						"$ROUTES:"
#define STRING_ACK							"$ACK"
#define STRING_NACK							"$NACK"
#define STRING_RESEND						"$RS:"

#endif
