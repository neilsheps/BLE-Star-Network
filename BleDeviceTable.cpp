#include "BleStar.h"

// Common reset methods

void BleStar::resetBleDeviceTable() {
	for (int i = 0; i < _maxConnectionsAsCentral + 2; i++) {
		BleDeviceTable * bleDevice = &bleDeviceTable[i];
		bleDevice->peerName = NULL;
		bleDevice->isConnected = false;
		bleDevice->index = i;
		bleDevice->tempReceiveBuffer = new MessageBuilder(MAX_BLE_CHUNK_LENGTH + 1);
		resetReceiveMessage(bleDevice);
		resetSendMessage(bleDevice);
	}
	Log.v("bleDeviceTable has been reset");
}

void BleStar::recalculateNumberOfBleConnections() {
	_numberOfBleConnections = 0;
	for (int i = BLE_PERIPHERAL_INDEX; i < _numberOfBleCentralConnections + 2; i++) {
		if (bleDeviceTable[i].isConnected) { _numberOfBleConnections++; }
	}
}

// Common receive methods

void BleStar::resetReceiveMessage(BleDeviceTable * bleDevice) {

	bleDevice->receiveState = AWAITING_NEW_MESSAGE;
	bleDevice->tempReceiveBuffer->reset();
	bleDevice->isUsingTempReceiveBuffer = true;
	bleDevice->receiveBuffer = bleDevice->tempReceiveBuffer;

	if (bleDevice->messageBeingReceived != NULL) {
		bleDevice->messageBeingReceived->setMessageType(MESSAGE_TYPE_NONE);
		bleDevice->messageBeingReceived->getMessageBuilder()->reset();
	}
	bleDevice->messageBeingReceived = NULL;
	bleDevice->receiveChunkInProgress = 0;
	bleDevice->indexWithinReceiveChunk = 0;
	bleDevice->receiveChunksExpected = 0;
	bleDevice->chunkResendIndex = 0;
	bleDevice->receiveAttempts = 0;
	bleDevice->lastreceivedTime = 0;
	bleDevice->lastResendRequestTime = 0;
	for (int j = 0; j < CHUNK_FLAG_TABLE_CAPACITY + 1; j++) { bleDevice->receivedChunkFlags[j] = 0xFF; }
}

boolean BleStar::getChunkReceived(BleDeviceTable * bleDevice, int chunkNumber) {
	return ((bleDevice->receivedChunkFlags[chunkNumber / 8] & bitSetArray[chunkNumber % 8]) > 0);
}

void BleStar::setChunkReceived(BleDeviceTable * bleDevice, int chunkNumber) {
	bleDevice->receivedChunkFlags[chunkNumber / 8] = bleDevice->receivedChunkFlags[chunkNumber / 8] & bitResetArray[chunkNumber % 8];
}

boolean BleStar::getAllChunksReceived(BleDeviceTable * bleDevice) {
	for (int i = 0; i < bleDevice->receiveChunksExpected; i++) {
			if ((bleDevice->receivedChunkFlags[i / 8] & bitSetArray[i % 8]) !=0 ) { return false; }
	}
	return (true);
}



void BleStar::resetSendMessage(BleDeviceTable * bleDevice) {
	bleDevice->sendBuffer = NULL;
	bleDevice->sendChunkResendRequested = false;
	if (bleDevice->messageBeingSent != NULL) {
		bleDevice->messageBeingSent->setMessageType(MESSAGE_TYPE_NONE);
		bleDevice->messageBeingSent = NULL;
	}
	for (int j = 0; j < CHUNK_FLAG_TABLE_CAPACITY + 1; j++) { bleDevice->sentChunkFlags[j] = 0x00; }
}

boolean BleStar::getChunkNeedsToBeSent(BleDeviceTable * bleDevice, int chunkNumber) {
	return ((bleDevice->receivedChunkFlags[chunkNumber / 8] & bitSetArray[chunkNumber % 8]) > 0);
}

void BleStar::setChunkNotSent(BleDeviceTable * bleDevice, int chunkNumber) {
	bleDevice->sentChunkFlags[chunkNumber / 8] = bleDevice->sentChunkFlags[chunkNumber / 8] | bitSetArray[chunkNumber % 8];
}

void BleStar::setChunkSent(BleDeviceTable * bleDevice, int chunkNumber) {
	bleDevice->sentChunkFlags[chunkNumber / 8] = bleDevice->sentChunkFlags[chunkNumber / 8] & bitResetArray[chunkNumber % 8];
}


boolean BleStar::getAllChunksSent(BleDeviceTable * bleDevice) {
	for (int i = 0; i < bleDevice->receiveChunksExpected; i++) {
			if ((bleDevice->sentChunkFlags[i / 8] & bitSetArray[i % 8]) != 0 ) { return false; }
	}
	return (true);
}

int BleStar::getBleDeviceIndex(char * deviceName) {
	int bleDeviceIndex = ERROR_BLE_DEVICE_NA;
	for (int i = BLE_PERIPHERAL_INDEX; i < MAX_CENTRAL_CONNECTIONS + 2; i++) {
		if (bleDeviceTable[i].peerName == deviceName) { bleDeviceIndex = i; break; }
	}
	if (bleDeviceIndex == -1) { return ERROR_BLE_DEVICE_NA; }
	if (bleDeviceIndex == BLE_THIS_DEVICE_INDEX) { return ERROR_BLE_DEVICE_SAME; }
	if (!bleDeviceTable[bleDeviceIndex].isConnected) { return ERROR_BLE_DEVICE_NOT_CONNECTED; }
	return (bleDeviceIndex);
}
