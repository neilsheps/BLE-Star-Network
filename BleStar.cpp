#include "BleStar.h"


BleStar::BleStar() { Log.initialize(&Serial, "BleStar:"); }

BleStar::BleStar(int messageBufferCapacity, int messageTableCapacity, int routingTableCapacity) {
	mTable.initialize(messageBufferCapacity, messageTableCapacity);
	rTable.initialize(routingTableCapacity);
	//initializeBleStar(messageBufferCapacity, messageTableCapacity, routingTableCapacity);
}

void BleStar::initializeBleStar(int messageBufferCapacity, int messageTableCapacity, int routingTableCapacity) {
	mTable.initialize(messageBufferCapacity, messageTableCapacity);
	rTable.initialize(routingTableCapacity);
}


void BleStar::begin(int maxConnectionsAsPeripheral, int maxConnectionsAsCentral, int power, char * thisDeviceName, boolean isGateway) {

	Log.initialize(&Serial, "BleStar:", Log.VERBOSE);

	_pointerToBleStarClass = this;
	_maxConnectionsAsPeripheral = maxConnectionsAsPeripheral;
	_maxConnectionsAsCentral = min(MAX_CENTRAL_CONNECTIONS, maxConnectionsAsCentral);
	_transmitPowerDbm = power;
	_thisDeviceName = rTable.getNamePointerFromName(thisDeviceName, BLE_THIS_DEVICE_INDEX);

	bleDeviceTable[0].peerName = _thisDeviceName;
	_isGateway = isGateway;

	CRC::initializeTables();
	Message::initialize();

	setUuidForSignalStrengthMonitoring(DEFAULT_UUID_FOR_SIGNAL_STRENGTH_MONITORING);
	setUuidForConnection(isGateway ? DEFAULT_UUID_FOR_CONNECTION : DEFAULT_UUID_FOR_GATEWAY);
	resetBleDeviceTable();

	Log.v("starting BleStar with max %d peripheral, %d central connections\n", maxConnectionsAsPeripheral, maxConnectionsAsCentral);

#ifdef ARCHITECTURE_NRF52
	Bluefruit.begin(maxConnectionsAsPeripheral, maxConnectionsAsCentral);
	Bluefruit.setTxPower(power);
	Bluefruit.setName(thisDeviceName);

	bleDfu.begin();
	centralBegin();
	startScanning();
#else



#endif

	peripheralBegin();
	startAdvertisingForConnection();

}

void BleStar::loop() {

	pollReceivingMessages();
	pollRoutingMessages();
	

	pollSendingMessages();
}
