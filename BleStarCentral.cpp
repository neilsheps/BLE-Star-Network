#include "BleStar.h"

// scanner start and stop or resume not clear here 



void BleStar::centralBegin() {
	Bluefruit.Central.setConnectCallback(connectAsCentralCallbackWrapper);
	Bluefruit.Central.setDisconnectCallback(disconnectAsCentralCallbackWrapper);
}

void BleStar::startScanning() {
	Bluefruit.Scanner.setRxCallback(scanCallbackWrapper);
	Bluefruit.Scanner.restartOnDisconnect(true);
	Bluefruit.Scanner.setInterval(160, 80); // in unit of 0.625 ms
	Bluefruit.Scanner.filterUuid(thisDeviceAsPeripheralUart.uuid);
	Bluefruit.Scanner.useActiveScan(false);
	Bluefruit.Scanner.start(0);                   // 0 = Don't stop scanning after n seconds
	Log.v("Scanning started");
}

void BleStar::stopScanning() {
	Bluefruit.Scanner.stop();
	Log.v("Scanning stopped");
}

void BleStar::resetBleCentralConnectionTable() {
	for (int i = 0; i < MAX_CENTRAL_CONNECTIONS; i++) {
		bleCentralConnectionTable[i].bleConnectionHandle = BLE_CONN_HANDLE_INVALID;
		bleCentralConnectionTable[i].bleCentralUart.begin();
		bleCentralConnectionTable[i].bleCentralUart.setRxCallback(centralModeRxCallbackWrapper);
	}
	recalculateNumberOfBleConnections();
}

void BleStar::scanCallbackWrapper(ble_gap_evt_adv_report_t* report) { _pointerToBleStarClass->scanCallback(report); }
void BleStar::scanCallback(ble_gap_evt_adv_report_t* report) {

	uint8_t buffer[64];
	if (Bluefruit.Scanner.checkReportForUuid(report, BLEUART_UUID_SERVICE)) {		// check that the device identified can connect via UART
		if (Bluefruit.Scanner.checkReportForUuid(report, uuidForConnection)) {		// check that it's also a device that is part of this network, not a random device
			if (Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer))) {
				Log.v("attempting connection to device: %s\n", buffer);
			} else {
				Log.v("attempting connection to unnamed remote device\n");
			}
			Bluefruit.Central.connect(report);
		}
		Bluefruit.Scanner.resume();
		return;
	}

	// Call on UuidListeners, if any
	for (int i = 0; i < _numberOfDevicesListenedByUuid; i++) {
		if (Bluefruit.Scanner.checkReportForUuid(report, uuidListener[i].uuidToListenFor)) {
			fireAdvertisementListenerByUuid(i, report);
		}
	}

	// Call on deviceNameListeners, if any
	if (_numberOfDevicesListenedByName > 0 && Bluefruit.Scanner.parseReportByType(report, BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, buffer, sizeof(buffer))) {
		for (int i = 0; i < _numberOfDevicesListenedByName; i++) {
			if (strncmp((char *)buffer,deviceNameListener[i].deviceNameToListenFor, strlen(deviceNameListener[i].deviceNameToListenFor))) {
				Log.v("found DeviceName: %s to listen for in advertising packet\n", (char *)buffer);
				fireAdvertisementListenerByDeviceName(i, report);
			}
		}
	}
	Bluefruit.Scanner.resume();
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////


void BleStar::connectAsCentralCallbackWrapper(uint16_t connectionHandle) { _pointerToBleStarClass->connectAsCentralCallback(connectionHandle); }
void BleStar::connectAsCentralCallback(uint16_t connectionHandle) {

	// Find an available connection handle to use
	int bleCentralConnectionIndex = findConnectionHandle(BLE_CONN_HANDLE_INVALID);
	if (bleCentralConnectionIndex < 0) { Log.e("Exceeded the maximum number of active connections in Central Mode, aborting"); return; }

	BLEConnection * connection = Bluefruit.Connection(connectionHandle);
	BleDeviceTable * bleDevice = &bleDeviceTable[bleCentralConnectionIndex + 2];
	BleCentralConnectionTable * bleCentralConnection = &bleCentralConnectionTable[bleCentralConnectionIndex];

	char connectedDeviceName[40];
	connection->getPeerName(connectedDeviceName,39);
	bleCentralConnection->bleConnectionHandle = connectionHandle;
	Log.i("Connected to device %s", connectedDeviceName);

	if (bleCentralConnection->bleCentralUart.discover(connectionHandle)) {
		resetReceiveMessage(bleDevice);
		resetSendMessage(bleDevice);
		bleDevice->peerName = rTable.getNamePointerFromName(connectedDeviceName, bleDevice->index);
		bleDevice->isConnected = true;
		bleCentralConnection->bleCentralUart.enableTXD();

		_numberOfBleCentralConnections++;
		recalculateNumberOfBleConnections();
		Log.i("Connected to %s and UART started", bleDevice->peerName);

		Bluefruit.Scanner.start(0);
	} else {
		Bluefruit.disconnect(connectionHandle);
		Log.e("Connected to %s but UART could not be established; disconnecting", bleDevice->peerName);
	}
}


void BleStar::disconnectAsCentralCallbackWrapper(uint16_t connectionHandle, uint8_t reason) { _pointerToBleStarClass->disconnectAsCentralCallback(connectionHandle, reason); }
void BleStar::disconnectAsCentralCallback(uint16_t connectionHandle, uint8_t reason) {
	int bleCentralConnectionIndex = findConnectionHandle(connectionHandle);
	if (bleCentralConnectionIndex < 0) { Log.w("Cannot find device in bleDeviceTable or bleConnectionTable, aborting"); return; }

	BleDeviceTable * bleDevice = &bleDeviceTable[bleCentralConnectionIndex + 2];
	BleCentralConnectionTable * bcct = &bleCentralConnectionTable[bleCentralConnectionIndex];
	bcct->bleConnectionHandle = BLE_CONN_HANDLE_INVALID;

	Log.i("Disconnecting from device %s for reason %d, connections now active = %d",
					bleDevice->peerName,
					reason,
					--_numberOfBleCentralConnections);

	failAndClearAnyMessagesBeingSent(bleDevice);
	failAndClearAnyMessagesBeingReceived(bleDevice);

	bleDevice->peerName = NULL;
	bleDevice->isConnected = false;
	recalculateNumberOfBleConnections();
	rTable.invalidatePeerBleDeviceRoutes(bleDevice->index);
}


void BleStar::centralModeRxCallbackWrapper(BLEClientUart & thisDeviceAsCentralUart) { _pointerToBleStarClass->centralModeRxCallback(thisDeviceAsCentralUart); }
void BleStar::centralModeRxCallback(BLEClientUart & thisDeviceAsCentralUart) {
	uint16_t connectionHandle = thisDeviceAsCentralUart.connHandle();
	int bleDeviceIndex = findConnectionHandle(connectionHandle) + 2;
	pollReceivingMessage(bleDeviceIndex);
}


int BleStar::findConnectionHandle(uint16_t connectionHandle) {
	for (int i = 0; i < MAX_CENTRAL_CONNECTIONS; i++) {
		if (connectionHandle == bleCentralConnectionTable[i].bleConnectionHandle) { return (i); }
	}
	return (-1);
}




/*
configPrphConn(247, 150, 3, BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);
configCentralConn(247, 150, 3, BLE_GATTC_WRITE_CMD_TX_QUEUE_SIZE_DEFAULT);
(uint16_t mtu_max, uint8_t event_len, uint8_t hvn_qsize, uint8_t wrcmd_qsize);
*/
