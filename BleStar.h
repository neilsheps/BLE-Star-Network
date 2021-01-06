#ifndef BleStar_h
#define BleStar_h

#include "Common/CommonDefinitions.h"
#include "Arduino.h"
#include "RoutingTable.h"
#include "Message/Message.h"
#include "MessageTable.h"
#include "Utility/MessageBuilder.h"
#include "Utility/Logger.h"
#include "Utility/CRC.h"
#include "stdarg.h"
#include "functional"


#define DEFAULT_POWER_LEVEL									0					// usually can go up to +8 depending on chipset
#define DELAY_IF_BLE_TX_BUFFER_FULL							3					// guesstimate.  Need this in send so we don't fill up the tx buffer ever
#define MIN_INTERVAL_BETWEEN_RESEND_REQUESTS				100					// 100 ms minimum between adjacent nodes.  This can be tuned once we have data


#define MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_NAME		5
#define MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_UUID		5


const int DEFAULT_MESSAGE_BUFFER_CAPACITY 			= 5000;						// 5000 chars
const int DEFAULT_MESSAGE_TABLE_CAPACITY			= 60;						// Max number of messages any node can track
const int DEFAULT_NAME_TABLE_CAPACITY 				= 100;						// Max number of individual device names or subscriptions to track
const int DEFAULT_ROUTING_TABLE_CAPACITY 			= 100;						// Max number of individual routes each node can maintain

#define ACK_NOT_RECEIVED_BETWEEN_NODES_TIMEOUT				150					// 150 milliseconds

#define GENERATED_UUID_FOR_CONNECTION					"69957c6e-b4de-4cea-a1dc-95ba5b1ffb64"
#define UUID_FOR_SIGNAL_STRENGTH_MONITORING				"09a9b7e8-5cb1-4299-bf78-1213333e6b9f"
#define GATEWAY_UUID									"0c80b78a-b610-4d73-8b20-20a7e480d40b" // unique ID for gateway so that nothing in network tries to connect with it
const uint8_t DEFAULT_UUID_FOR_CONNECTION[16] = { 0x69, 0x95, 0x7c, 0x6e, 0xb4, 0xde, 0x4c, 0xea, 0xa1, 0xdc, 0x95, 0xba, 0x5b, 0x1f, 0xfb, 0x64};
const uint8_t DEFAULT_UUID_FOR_SIGNAL_STRENGTH_MONITORING[16] = { 0x09, 0xa9, 0xb7, 0xe8, 0x5c, 0xb1, 0x42, 0x99, 0xbf, 0x78, 0x12, 0x13, 0x33, 0x3e, 0x6b, 0x9f};
const uint8_t DEFAULT_UUID_FOR_GATEWAY[16] = { 0x0c, 0x80, 0xb7, 0x8a, 0xb6, 0x10, 0x4d, 0x73, 0x8b, 0x20, 0x20, 0xa7, 0xe4, 0x80, 0xd4, 0x0b };

#define ACK						1
#define NACK					2
#define TIMEOUT 				3

const unsigned long EXPONENTIAL_BACKOFF_TABLE_MILLIS[] = { 0, 100, 200, 400, 2000, 6000, 20000 };   // jumps to allow time for complete reconnects
const unsigned long DELAY_FOR_ACK[] = { 0, 50, 100, 200, 500, 1000, 1500 };

#define ERROR_BLE_DEVICE_NA									-1
#define ERROR_BLE_DEVICE_SAME								-2
#define ERROR_BLE_DEVICE_NOT_CONNECTED						-3

#define AWAITING_NEW_MESSAGE								0
#define RECEIVING_UNFORMED_MESSAGE							1
#define AWAITING_END_OF_FIRST_CHUNK							2

#define AWAITING_START_OF_IN_SEQUENCE_CHUNK					3
#define AWAITING_START_OF_OUT_OF_SEQUENCE_CHUNK				4

#define AWAITING_END_OF_IN_SEQUENCE_CHUNK					5
#define AWAITING_END_OF_OUT_OF_SEQUENCE_CHUNK				6

#define SENDING_COMPILED_MESSAGE							10
#define SENDING_UNFORMED_MESSAGE							11


/* 	Holds an array of devices connected to this device, and a reference to a message if one is actively receiving data
	Each device number has a meaning:
		index # 0 = THIS device
		index # 1 = PERIPHERAL device
		index #2 - (MAX_CENTRAL_CONNECTIONS + 2) = CENTRAL DEVICES -- IF this is a nRF52
*/
struct BleDeviceTable {
	int index;																	// index 0 to _numberOfBleCentralConnections + 2
	boolean isConnected;														// true if device is connected and UART available
	char * peerName;															// name of remote device that's connected

	boolean isUsingTempReceiveBuffer = true;									// a 21 byte receive buffer is kept to receive short messages
	MessageBuilder * tempReceiveBuffer;											// without the overhead of creating a message in messageTable
																				// if messages are > 20 bytes, a message in messageTable is created

	//int sendState = 0;
	Message * messageBeingSent;													// pointer to message that's in process of being sent
	MessageBuilder * sendBuffer;												// pointer to the underlying MessageBuilder
	uint8_t sentChunkFlags[(CHUNK_FLAG_TABLE_CAPACITY+1)];						// an array of bits that holds whether a chunk needs to be resent or not.  1 = needs resent
	int sendChunksExpected = 0;
	int sendChunkInProgress = 0;
	int indexWithinSendChunk = 0;
	int sendAttempts = 0;
	uint32_t lastSendTime;														// last time a chunk was sent
	boolean sendChunkResendRequested = false;									// if a resend request is received, this goes true


	int receiveState = AWAITING_NEW_MESSAGE;									// flag for how to process incoming bytes to reconstruct a message
	Message * messageBeingReceived;												// pointer to message once it's clear that a message needs to be created
	MessageBuilder * receiveBuffer;												// MessageBuilder that can point to the tempReceiveBuffer or the receiveBuffer in a Message
	uint8_t receivedChunkFlags[(CHUNK_FLAG_TABLE_CAPACITY+1)];					// each bit == 0 if has been received successfully, 1 not yet and/or resend needed
	int receiveChunksExpected = 0;
	int receiveChunkInProgress = 0;
	int indexWithinReceiveChunk = 0;
	int chunkResendIndex = 0;
	int receiveAttempts = 0;
	uint32_t lastreceivedTime;
	uint32_t lastResendRequestTime;

};


#if defined(ARDUINO_ARCH_NRF52)

struct BleCentralConnectionTable {		// index 0 - MAX_CENTRAL_CONNECTIONS = central devices. Subtract 2 from BleDeviceTableIndex to find relevant central connection handles and UARTs
	uint16_t bleConnectionHandle;
	BLEClientUart bleCentralUart;
};



#endif




class BleStar {
public:

	// initialization methods
	BleStar();
	BleStar(int messageBufferCapacity, int messageTableCapacity, int routingTableCapacity);
	void initializeBleStar(int messageBufferCapacity, int messageTableCapacity, int routingTableCapacity);
	void begin(int connectionsAsPeripheral, int connectionsAsCentral, int power, char * thisDeviceName, boolean isGateway);


	// Peripheral public methods
	BLEUart thisDeviceAsPeripheralUart;													// nRF52 device behaves as a Peripheral UART
	void startAdvertisingForConnection();
	void startAdvertisingForRoutingOnly();
	void setUuidForConnection(BLEUuid uuid);
	void setUuidForConnection(uint8_t uuidArray[16]);
	void setUuidForSignalStrengthMonitoring(BLEUuid uuid);
	void setUuidForSignalStrengthMonitoring(uint8_t uuidArray[16]);
	boolean send(uint8_t * payload, int payloadLength, char * destination, uint16_t messageId, boolean isSystemMessage);

	// User facing callbacks
	typedef void (*listenerFunctionCallback) (ble_gap_evt_adv_report_t*);
	struct DeviceNameListener { char deviceNameToListenFor[MAX_BLE_DEVICE_NAME_LENGTH+1]; listenerFunctionCallback pointerToListenerFunction; };
	DeviceNameListener deviceNameListener[MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_NAME];
	int _numberOfDevicesListenedByName;
	void addAdvertisementListenerByDeviceName(char * name, listenerFunctionCallback pointerToFunction);

	struct UuidListener { BLEUuid uuidToListenFor; listenerFunctionCallback pointerToListenerFunction; };
	UuidListener uuidListener[MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_UUID];
	int _numberOfDevicesListenedByUuid;
	void addAdvertisementListenerByUuid(uint8_t uuidArray[16], listenerFunctionCallback ptr);
	void addAdvertisementListenerByUuid(BLEUuid uuid, listenerFunctionCallback ptr);

	typedef void (* TransmissionSucceededCallback) (uint16_t messageID);
	TransmissionSucceededCallback transmissionSucceededCallback;
	void setTransmissionSucceededCallback(TransmissionSucceededCallback tsc);
	typedef void (* TransmissionFailedCallback) (uint16_t messageID);
	TransmissionFailedCallback transmissionFailedCallback;
	void setTransmissionFailedCallback(TransmissionFailedCallback tfc);
	typedef void (* UnformedMessageReceivedCallback) (char * receiveBuffer, int bleDeviceIndex, char * fromName);
	UnformedMessageReceivedCallback unformedMessageReceivedCallback;
	void setUnformedMessageReceivedCallback(UnformedMessageReceivedCallback umrc);
	typedef void (* RoutedMessageReceivedCallback) (Message * m);
	RoutedMessageReceivedCallback routedMessageReceivedCallback;
	void setRoutedMessageReceivedCallback(RoutedMessageReceivedCallback rmrc);

private:
	Logger Log;
	CRC crc;
	BLEDfu bleDfu;

	MessageTable mTable;
	RoutingTable rTable;

	static BLEUuid uuidForConnection;
	static BLEUuid uuidForSignalStrengthMonitoring;
	static BleStar * _pointerToBleStarClass;

	int _maxConnectionsAsCentral;
	int _connectionsAsCentral;
	int _transmitPowerDbm = 0;

	int _maxConnectionsAsPeripheral;
	char * _thisDeviceName;
	boolean _isGateway = false;

	void loop();

	// BleDeviceTable and related methods
	int _numberOfBleConnections = 0;
	int _numberOfBleCentralConnections = 0;
	BleDeviceTable bleDeviceTable[MAX_CENTRAL_CONNECTIONS+2];			// entry #0 = thisDevice, entry #1 = peripheral connection, entries 2++ = central
	BleCentralConnectionTable bleCentralConnectionTable[MAX_CENTRAL_CONNECTIONS];
	void resetBleDeviceTable();
	void resetBleDeviceEntry(int i);
	void recalculateNumberOfBleConnections();




	// Routing methods and variables
	RoutingTable * _routingTable;
	int _routingTableCapacity = DEFAULT_ROUTING_TABLE_CAPACITY;
	int _routingTableSize = 0;


	void addToRoutingTable(char * branchNodeName, char * destinationNodeName);
	boolean routeMessage(Message * m);
	boolean getDestinationNodePattern(char * message, char * target);
	void processRoutedMessage(BleDeviceTable * bleDevice);
	void processUnformedMessage(BleDeviceTable * bleDevice);
	void pollRoutingMessages();





	// Peripheral setup and connection methods
	void peripheralBegin();
	static void connectAsPeripheralCallbackWrapper(uint16_t connectionHandle);
	void connectAsPeripheralCallback(uint16_t connectionHandle);
	static void disconnectAsPeripheralCallbackWrapper(uint16_t connectionHandle, uint8_t reason);
	void disconnectAsPeripheralCallback(uint16_t connectionHandle, uint8_t reason);
	static void peripheralRxCallbackWrapper(uint16_t connectionHandle);
	void peripheralRxCallback(uint16_t connectionHandle);




	// Central setup and connection Methods
	void centralBegin();
	void resetBleCentralConnectionTable();
	int findConnectionHandle(uint16_t connectionHandle);
	static void connectAsCentralCallbackWrapper(uint16_t connectionHandle);
	void connectAsCentralCallback(uint16_t connectionHandle);
	static void disconnectAsCentralCallbackWrapper(uint16_t connectionHandle, uint8_t reason);
	void disconnectAsCentralCallback(uint16_t connectionHandle, uint8_t reason);
	static void centralModeRxCallbackWrapper(BLEClientUart & thisDeviceAsCentralUart);
	void centralModeRxCallback(BLEClientUart & thisDeviceAsCentralUart);

	// Central mode public methods
	void startScanning();
	void stopScanning();
	static void scanCallbackWrapper(ble_gap_evt_adv_report_t* report);
	void scanCallback(ble_gap_evt_adv_report_t* report);





	// User facing Callbacks
	void fireTransmissionSucceededCallback(uint16_t messageID);
	void fireTransmissionFailedCallback(uint16_t messageID);
	void fireUnformedMessageReceivedCallback(char * receiveBuffer, int bleDeviceIndex, char * fromName);
	void fireRoutedMessageReceivedCallback(Message * m);
	void fireAdvertisementListenerByDeviceName(int i, ble_gap_evt_adv_report_t * report);
	void fireAdvertisementListenerByUuid(int number, ble_gap_evt_adv_report_t * report);


	// Abstracted send methods (work regardless of whether peripheral or central connection)
	int bleWriteDeviceIndex;
	BleCentralConnectionTable * bcct;

	void pollSendingMessages();
	void assignMessageToBleDevice(Message * m, BleDeviceTable * bleDevice);
	void pollSendingMessage(BleDeviceTable * bleDevice);
	int writeToBleDevice(uint8_t u);

	boolean sendRawToBleDevice(uint8_t * buffer, int bufferLength, char * destinationDevice);
	boolean sendRawToBleDevice(uint8_t * buffer, int bufferLength, int bleDeviceIndex);
	int getBleDeviceIndex(char * deviceName);
	void setBleWriteDevice(int bleDeviceIndex);

	int getNumberOfChunksForMessageLength(int messageLength);
	int getMessageBuilderIndexForChunkNumber(int chunkNumber);
	void resetSendMessage(BleDeviceTable * bleDevice);

	boolean getChunkNeedsToBeSent(BleDeviceTable * bleDevice, int chunkNumber);
	void setChunkNotSent(BleDeviceTable * bleDevice, int chunkNumber);
	void setChunkSent(BleDeviceTable * bleDevice, int chunkNumber);
	boolean getAllChunksSent(BleDeviceTable * bleDevice);
	void failAndClearAnyMessagesBeingSent(BleDeviceTable * bleDevice);

	// Abstracted receive methods (work regardless of whether peripheral or central connection)
	void pollReceivingMessages();
	void pollReceivingMessage(int bleDeviceIndex);
	void parseAndReorderReceivedData(uint8_t u, BleDeviceTable * bleDevice);

	boolean sendResendRequestSequence(BleDeviceTable * bleDevice, int fromChunk, int toChunkInclusive);
	void sendResendRequest(BleDeviceTable * bleDevice, int upToAndIncludingThisChunkNumber);

	void parseExpectedStartOfChunk(uint8_t u, BleDeviceTable * bleDevice, boolean isInSequence);
	void resetReceiveMessage(BleDeviceTable * bleDevice);

	boolean getChunkReceived(BleDeviceTable * bleDevice, int chunkNumber);
	void setChunkReceived(BleDeviceTable * bleDevice, int chunkNumber);
	boolean getAllChunksReceived(BleDeviceTable * bleDevice);

	void sendAck(BleDeviceTable * bleDevice);
	void sendNack(BleDeviceTable * bleDevice);

	void checkForDelimiterOnUnformedMessage(BleDeviceTable * bleDevice, uint8_t u);
	void checkForFirstChunkComplete(BleDeviceTable * bleDevice);
	void checkForEndOfChunk(BleDeviceTable * bleDevice);
	void printEntireMessage(BleDeviceTable * bleDevice);

	boolean isShortIncomingSystemMessage(BleDeviceTable * bleDevice);
	void failAndClearAnyMessagesBeingReceived(BleDeviceTable * bleDevice);

	void processRoutedSystemMessage(Message * m);
	void sendRoutingInformationUpstream(char * routingChangesList);
	void sendAllRoutingInformationUpstream();
	void subscribe(char * subscriptionName);
	void unsubscribe(char * subscriptionName);



};





#endif


/*
#if defined(ARDUINO_ARCH_AVR)
#include "avr/ServoTimers.h"
#elif defined(ARDUINO_ARCH_SAM)
#include "sam/ServoTimers.h"
#elif defined(ARDUINO_ARCH_SAMD)
#include "samd/ServoTimers.h"
#elif defined(ARDUINO_ARCH_STM32F4)
#include "stm32f4/ServoTimers.h"
#elif defined(ARDUINO_ARCH_NRF52)
#include "nrf52/ServoTimers.h"
#elif defined(ARDUINO_ARCH_MEGAAVR)
#include "megaavr/ServoTimers.h"
#else
#error "This library only supports boards with an AVR, SAM, SAMD, NRF52 or STM32F4 processor."
#endif


*/
