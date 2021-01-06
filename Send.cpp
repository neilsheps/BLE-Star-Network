#include "Arduino.h"
#include "BleStar.h"

// "send" only adds a message to the routing table, the sending happens later, but we keep this nomenclature for user convenience
boolean BleStar::send(uint8_t * payload, int payloadLength, char * destination, uint16_t messageId, boolean isSystemMessage) {
	return (mTable.addNewMessageToSend(
		payload,
		payloadLength,
		_thisDeviceName,
		rTable.getNamePointerFromName(destination),
		messageId,
		isSystemMessage
		)
		!= NULL
	);
}


void BleStar::pollSendingMessages() {
	Bluefruit.Scanner.stop();

	// First find if there are any bleDevices not currently sending a message, and if so, try to find a message that needs to be sent
	for (int i = BLE_PERIPHERAL_INDEX; i < _numberOfBleCentralConnections + BLE_CENTRAL_INDEX_0; i++) {
		BleDeviceTable * bleDevice = &bleDeviceTable[i];
		if (bleDevice->isConnected && bleDevice->messageBeingSent == NULL) {
			Message * m = mTable.findNextAvailableMessageForHop(bleDevice->peerName);
			if (m != NULL) { assignMessageToBleDevice(m, bleDevice); }
		}
		if (bleDevice->messageBeingSent != NULL) {
			pollSendingMessage(bleDevice);
		}
	}
	Bluefruit.Scanner.start(0);
}

void BleStar::assignMessageToBleDevice(Message * m, BleDeviceTable * bleDevice) {
	resetSendMessage(bleDevice);
	bleDevice->messageBeingSent = m;
	bleDevice->sendBuffer = m->getMessageBuilder();
	bleDevice->sendBuffer->setReadIndex(0);
	bleDevice->sendChunkInProgress = 0;
	bleDevice->sendChunksExpected = getNumberOfChunksForMessageLength(bleDevice->sendBuffer->getLength());
	bleDevice->indexWithinSendChunk = 0;
	bleDevice->sendAttempts = 0;
	bleDevice->lastSendTime = 0;
	bleDevice->sendChunkResendRequested = false;
}

void BleStar::pollSendingMessage(BleDeviceTable * bleDevice) {

	setBleWriteDevice(bleDevice->index);

	if (bleDevice->sendChunkInProgress == bleDevice->sendChunksExpected) {
		if (bleDevice->sendChunkResendRequested) {
			bleDevice->sendChunkInProgress = 0;
			bleDevice->indexWithinSendChunk = 0;
			bleDevice->sendChunkResendRequested = false;
		} else {
			// Check timeout
			if ((millis() - bleDevice->lastSendTime > DELAY_FOR_ACK[bleDevice->sendAttempts]) || (bleDevice->lastSendTime > millis())) {
				bleDevice->sendChunkInProgress = 0;
				bleDevice->indexWithinSendChunk = 0;
				if (bleDevice->sendAttempts++ > DEFAULT_MAX_HOP_ATTEMPTS) {
					Log.w("Max send attempts for messageID %d hit, aborting", bleDevice->messageBeingSent->getMessageId());
					failAndClearAnyMessagesBeingSent(bleDevice);
					return;
				}
			}
		}
	}

	while (bleDevice->sendChunkInProgress < bleDevice->sendChunksExpected) {
		if (getChunkNeedsToBeSent(bleDevice, bleDevice->sendChunkInProgress)) {

			int readIndex = (getMessageBuilderIndexForChunkNumber(bleDevice->sendChunkInProgress)
							+ (bleDevice->indexWithinSendChunk > 0 ? bleDevice->indexWithinSendChunk - 1 : 0)
						);
			bleDevice->sendBuffer->setReadIndex(readIndex);

			while(bleDevice->indexWithinSendChunk < MAX_BLE_CHUNK_LENGTH) {
				if (writeToBleDevice(bleDevice->indexWithinSendChunk == 0 ? bleDevice->sendChunkInProgress : bleDevice->sendBuffer->read()) == 1) {
					bleDevice->indexWithinSendChunk++;
				} else {
					// change rate limiter when i have one
					break;
				}
			}

			if (bleDevice->indexWithinSendChunk == MAX_BLE_CHUNK_LENGTH) {					// then entire chunk was sent
				setChunkSent(bleDevice, bleDevice->sendChunkInProgress);
				bleDevice->indexWithinSendChunk = 0;
				bleDevice->sendChunkInProgress++;

			} else {
				// the send buffer is full
				break;
			}
			bleDevice->lastSendTime = millis();
		} else {
			bleDevice->sendChunkInProgress++;
		}
	}

	if (bleDevice->sendChunkInProgress == bleDevice->sendChunksExpected) {
		bleDevice->sendAttempts++;
	}
}


int BleStar::getNumberOfChunksForMessageLength(int messageLength) {
	return (messageLength > MAX_BLE_CHUNK_LENGTH ? (messageLength - MAX_BLE_CHUNK_LENGTH) / (MAX_BLE_CHUNK_LENGTH - 1) + 1 : 1);
}
int BleStar::getMessageBuilderIndexForChunkNumber(int chunkNumber) {
	return (chunkNumber > 0 ? MAX_BLE_CHUNK_LENGTH + (chunkNumber - 1) * (MAX_BLE_CHUNK_LENGTH - 1) : 0 );
}

boolean BleStar::sendRawToBleDevice(uint8_t * buffer, int bufferLength, char * destinationDevice) {
	int bleDeviceIndex = getBleDeviceIndex(destinationDevice);
	switch (bleDeviceIndex) {
		case ERROR_BLE_DEVICE_NA: Log.e("Cannot send to device %s; not in bleDeviceTable\n", destinationDevice); return false;
		case ERROR_BLE_DEVICE_SAME: Log.e("Cannot send to self %s\n", destinationDevice); return false;
		case ERROR_BLE_DEVICE_NOT_CONNECTED: Log.e("Cannot send to device %s; not connected", destinationDevice); return false;
	}
	return (sendRawToBleDevice(buffer, bufferLength, bleDeviceIndex));
}

boolean BleStar::sendRawToBleDevice(uint8_t * buffer, int bufferLength, int bleDeviceIndex) {
	setBleWriteDevice(bleDeviceIndex);
	Bluefruit.Scanner.stop();

	for (int i = 0; i < bufferLength; i++) {
		int sendFailures = 0;
		while (writeToBleDevice(*buffer) == 0) {
			delay(1);
			if (sendFailures++ > 5) {
				Bluefruit.Scanner.start(0);
				return false;
			}
		}
		buffer++;
	}
	Bluefruit.Scanner.start(0);
	return (true);
}


void BleStar::setBleWriteDevice(int bleDeviceIndex) {
	bleWriteDeviceIndex = bleDeviceIndex;
	if (bleDeviceIndex > BLE_PERIPHERAL_INDEX) {
		bcct = &bleCentralConnectionTable[bleDeviceIndex - BLE_CENTRAL_INDEX_0];
	}
}

int BleStar::writeToBleDevice(uint8_t u) {
	return (bleWriteDeviceIndex == BLE_PERIPHERAL_INDEX ? thisDeviceAsPeripheralUart.write(u) : bcct->bleCentralUart.write(u));
}

void BleStar::failAndClearAnyMessagesBeingSent(BleDeviceTable * bleDevice) {
	if (bleDevice->messageBeingSent != NULL) {
		Log.w("Message currently being sent by device %s will be discarded:", bleDevice->peerName);
		if (Log.getLoggingLevel() >= Log.WARN) { bleDevice->sendBuffer->printEntireMessage(); }
		fireTransmissionFailedCallback(bleDevice->messageBeingSent->getMessageId());
	}
	resetSendMessage(bleDevice);
}
