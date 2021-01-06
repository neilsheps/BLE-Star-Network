#include "BleStar.h"


void BleStar::peripheralBegin() {
	Bluefruit.Periph.setConnectCallback(connectAsPeripheralCallbackWrapper);
	Bluefruit.Periph.setDisconnectCallback(disconnectAsPeripheralCallbackWrapper);
	thisDeviceAsPeripheralUart.begin();
	thisDeviceAsPeripheralUart.setRxCallback(peripheralRxCallbackWrapper);
}

void BleStar::setUuidForConnection(BLEUuid uuid) { uuidForConnection = uuid; }
void BleStar::setUuidForConnection(uint8_t uuidArray[16]) {
	BLEUuid uuid = BLEUuid(uuidArray);
	setUuidForConnection(uuid);
}

void BleStar::setUuidForSignalStrengthMonitoring(BLEUuid uuid) { uuidForSignalStrengthMonitoring = uuid; }
void BleStar::setUuidForSignalStrengthMonitoring(uint8_t uuidArray[16]) {
	BLEUuid uuid = BLEUuid(uuidArray);
	setUuidForSignalStrengthMonitoring(uuid);
}


void BleStar::startAdvertisingForConnection() {
	Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);
    Bluefruit.Advertising.addTxPower();
    Bluefruit.Advertising.addService(thisDeviceAsPeripheralUart);
	Bluefruit.Advertising.addUuid(uuidForConnection);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
    Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
	Log.v("Advertising for connection started");
}

void BleStar::startAdvertisingForRoutingOnly() {
	Bluefruit.Advertising.addFlags(BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE);		// change this
    Bluefruit.Advertising.addTxPower();
	Bluefruit.Advertising.addUuid(uuidForSignalStrengthMonitoring);
    Bluefruit.ScanResponse.addName();
    Bluefruit.Advertising.restartOnDisconnect(true);
    Bluefruit.Advertising.setInterval(32, 244);    // in unit of 0.625 ms
    Bluefruit.Advertising.setFastTimeout(30);      // number of seconds in fast mode
    Bluefruit.Advertising.start(0);                // 0 = Don't stop advertising after n seconds
	Log.v("Advertising for routing costing started");
}


void BleStar::connectAsPeripheralCallbackWrapper(uint16_t connectionHandle) { _pointerToBleStarClass->connectAsPeripheralCallback(connectionHandle); }
void BleStar::connectAsPeripheralCallback(uint16_t connectionHandle) {
	BLEConnection * connection = Bluefruit.Connection(connectionHandle);

	char name[MAX_BLE_DEVICE_NAME_LENGTH + 1];
	connection->getPeerName(name, MAX_BLE_DEVICE_NAME_LENGTH);

	bleDeviceTable[BLE_PERIPHERAL_INDEX].peerName = rTable.getNamePointerFromName(name, BLE_PERIPHERAL_INDEX);;
	Log.v("Connected to device %s\n", bleDeviceTable[BLE_PERIPHERAL_INDEX].peerName);
	bleDeviceTable[BLE_PERIPHERAL_INDEX].isConnected = true;

	recalculateNumberOfBleConnections();
	sendAllRoutingInformationUpstream();   // send all devices and subscriptions findable through this device up the chain to populate their routing tables
}


void BleStar::disconnectAsPeripheralCallbackWrapper(uint16_t connectionHandle, uint8_t reason) { _pointerToBleStarClass->disconnectAsPeripheralCallback(connectionHandle, reason); }
void BleStar::disconnectAsPeripheralCallback(uint16_t connectionHandle, uint8_t reason) {
	BLEConnection * connection = Bluefruit.Connection(connectionHandle);
	char remoteDeviceName[32] = { 0 };
	connection->getPeerName(remoteDeviceName,sizeof(remoteDeviceName));
	Log.v("Disconnected from device %s for reason code %d\n",remoteDeviceName, reason);

	failAndClearAnyMessagesBeingReceived(&bleDeviceTable[BLE_PERIPHERAL_INDEX]);
	failAndClearAnyMessagesBeingSent(&bleDeviceTable[BLE_PERIPHERAL_INDEX]);

	rTable.invalidatePeerBleDeviceRoutes(BLE_PERIPHERAL_INDEX);
	bleDeviceTable[BLE_PERIPHERAL_INDEX].isConnected = false;
	recalculateNumberOfBleConnections();
}

void BleStar::peripheralRxCallbackWrapper(uint16_t connectionHandle) { _pointerToBleStarClass->peripheralRxCallback(connectionHandle); }
void BleStar::peripheralRxCallback(uint16_t connectionHandle) {
	(void) connectionHandle;
	//pollReceivingMessage(BLE_PERIPHERAL_INDEX);
}
