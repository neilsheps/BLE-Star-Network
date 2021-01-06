#include "BleStar.h"

// this method must be called often to collect data from incoming BLE buffers else they will overflow
void BleStar::pollReceivingMessages() {
	for (int i = BLE_PERIPHERAL_INDEX; i < MAX_CENTRAL_CONNECTIONS + 2; i++) {
		pollReceivingMessage(i);
	}
}


void BleStar::pollReceivingMessage(int bleDeviceIndex) {
	BleDeviceTable * bleDevice = &bleDeviceTable[bleDeviceIndex];

	if (bleDeviceIndex == BLE_THIS_DEVICE_INDEX || !bleDevice->isConnected) { return; }

	if (bleDeviceIndex == BLE_PERIPHERAL_INDEX) {
		while (thisDeviceAsPeripheralUart.available()) { parseAndReorderReceivedData(thisDeviceAsPeripheralUart.read(), bleDevice); }
	} else {
		BLEClientUart * bleCentralUart = &bleCentralConnectionTable[bleDeviceIndex - BLE_CENTRAL_INDEX_0].bleCentralUart;
		while (bleCentralUart->available()) { parseAndReorderReceivedData(bleCentralUart->read(), bleDevice); }
	}

	if (millis() - bleDevice->lastreceivedTime > 30 && millis() - bleDevice->lastResendRequestTime > MIN_INTERVAL_BETWEEN_RESEND_REQUESTS) {
		if (bleDevice->sendAttempts>DEFAULT_MAX_HOP_ATTEMPTS) {
			Log.w("Error: attempting to receive message %d from %s, but did not receive all missing chunks after %d attempts",
					bleDevice->messageBeingReceived->getMessageId(),
					bleDevice->peerName,
					DEFAULT_MAX_HOP_ATTEMPTS
				);
			failAndClearAnyMessagesBeingReceived(bleDevice);
			return;
		}
		bleDevice->sendAttempts++;
		sendResendRequestSequence(bleDevice, 0, bleDevice->sendChunksExpected);
	}
}


void BleStar::parseAndReorderReceivedData(uint8_t u, BleDeviceTable * bleDevice) {

	// Methods to be run before anything is written to buffers
	bleDevice->lastreceivedTime = millis();
	if (bleDevice->indexWithinReceiveChunk == 0) {
		switch (bleDevice->receiveState) {
			case AWAITING_NEW_MESSAGE:
				if (u < ' ') { return; }										// reject characters until a # or something >= 0x20 arrives
				bleDevice->receiveState = (u == '#' ? AWAITING_END_OF_FIRST_CHUNK: RECEIVING_UNFORMED_MESSAGE);
				break;
			case AWAITING_START_OF_IN_SEQUENCE_CHUNK: parseExpectedStartOfChunk(u, bleDevice, true); return;
			case AWAITING_START_OF_OUT_OF_SEQUENCE_CHUNK: parseExpectedStartOfChunk(u, bleDevice, false); return;
		}
	}

	if (!bleDevice->receiveBuffer->append(u)) {
		if (bleDevice->isUsingTempReceiveBuffer) {											// create messageTableEntry if > 20 bytes needed
			bleDevice->messageBeingReceived = mTable.reserveSpaceForIncomingMessage(DEFAULT_EXPECTED_MESSAGE_LENGTH, bleDevice->peerName);
			if (bleDevice->messageBeingReceived == NULL) {
				Log.e("Critical error - insufficient buffer or table space to accept new incoming message from %s", bleDevice->peerName);
				return;
			}
			bleDevice->receiveBuffer = bleDevice->messageBeingReceived->getMessageBuilder();
			bleDevice->receiveBuffer->append(bleDevice->tempReceiveBuffer->getBuffer());
			bleDevice->receiveBuffer->append(u);
			bleDevice->isUsingTempReceiveBuffer = false;
		} else {
			Log.w("Error: messages received through device %s caused buffer over-run; resetting buffer", bleDevice->peerName);
			failAndClearAnyMessagesBeingReceived(bleDevice);
			return;
		}
	}
	bleDevice->indexWithinReceiveChunk++;

	switch (bleDevice->receiveState) {
		case RECEIVING_UNFORMED_MESSAGE: checkForDelimiterOnUnformedMessage(bleDevice, u); return;
		case AWAITING_END_OF_FIRST_CHUNK: checkForFirstChunkComplete(bleDevice); return;
		case AWAITING_END_OF_IN_SEQUENCE_CHUNK: checkForEndOfChunk(bleDevice); return;
		case AWAITING_END_OF_OUT_OF_SEQUENCE_CHUNK: checkForEndOfChunk(bleDevice); return;
	}
}

void BleStar::parseExpectedStartOfChunk(uint8_t u, BleDeviceTable * bleDevice, boolean isInSequence) {

	if (isInSequence && u == bleDevice->receiveChunkInProgress) {
		bleDevice->receiveState = (isInSequence ? AWAITING_END_OF_IN_SEQUENCE_CHUNK : AWAITING_END_OF_OUT_OF_SEQUENCE_CHUNK);
		return;
	}

	if (u > bleDevice->receiveChunksExpected) {
		Log.w("Error! incoming chunk first byte > numberOfChunks in message");
		sendNack(bleDevice);
		failAndClearAnyMessagesBeingReceived(bleDevice);
		return;
	}

	if (isInSequence) {
		sendResendRequestSequence(bleDevice, bleDevice->chunkResendIndex, (int)u);
		bleDevice->chunkResendIndex = u;
	}
	bleDevice->receiveChunkInProgress = u;
	bleDevice->receiveBuffer->setReadIndex(getMessageBuilderIndexForChunkNumber(u));

}

void BleStar::checkForDelimiterOnUnformedMessage(BleDeviceTable * bleDevice, uint8_t u) {
	if (u < 32) {
		Log.i("Unformed Message received from %s: %s", bleDevice->peerName, bleDevice->receiveBuffer);
		if (!isShortIncomingSystemMessage(bleDevice)) {
			fireUnformedMessageReceivedCallback((char *)bleDevice->receiveBuffer->getBuffer(), bleDevice->index, bleDevice->peerName);
		}
		resetReceiveMessage(bleDevice);
	}
	return;
}


void BleStar::checkForFirstChunkComplete(BleDeviceTable * bleDevice) {
	if (bleDevice->indexWithinReceiveChunk < COMPILED_MESSAGE_ORIGIN_NAME_POSITION) {
		return;																	// if we haven't even received enough bytes to read the stored message length, return
	}

	uint16_t messageLength = bleDevice->messageBeingReceived->getStoredMessageLength();

	if (bleDevice->indexWithinReceiveChunk != MAX_BLE_CHUNK_LENGTH && bleDevice->receiveBuffer->getLength() < messageLength) {
		return;																	// if we haven't finished the first chunk or reached end of message
	}

	if (!bleDevice->messageBeingReceived->getIsMessageCrc8Valid()) {
		Log.w("Message Crc8 invalid:");
		failAndClearAnyMessagesBeingReceived(bleDevice);
		return;
	}

	if (bleDevice->messageBeingReceived->getStoredMessageLength() > MAX_BLE_CHUNK_LENGTH) {
		bleDevice->receiveChunksExpected = getNumberOfChunksForMessageLength(messageLength);
		setChunkReceived(bleDevice, 0);
		bleDevice->receiveChunkInProgress = 1;
		bleDevice->indexWithinReceiveChunk = 1;
		bleDevice->receiveState = AWAITING_START_OF_IN_SEQUENCE_CHUNK;
		return;
	}

	if (!bleDevice->messageBeingReceived->getIsMessageCrc16Valid()) {
		Log.w("Message Crc16 invalid");
		failAndClearAnyMessagesBeingReceived(bleDevice);
		return;
	}

	Log.i("Message received from %s:", bleDevice->peerName);
	if (Log.getLoggingLevel() >= Log.INFO) { bleDevice->receiveBuffer->printEntireMessage(); }
	sendAck(bleDevice);
	processRoutedMessage(bleDevice);
	resetReceiveMessage(bleDevice);
}


void BleStar::checkForEndOfChunk(BleDeviceTable * bleDevice) {
	uint16_t messageLengthPerHeaderPreamble = bleDevice->messageBeingReceived->getStoredMessageLength();
	int currentMessageLength = bleDevice->receiveBuffer->getLength();

	if (currentMessageLength < messageLengthPerHeaderPreamble) {				// message not yet complete
		if (bleDevice->indexWithinReceiveChunk < MAX_BLE_CHUNK_LENGTH) { return; }

		setChunkReceived(bleDevice, bleDevice->receiveChunkInProgress);
		bleDevice->indexWithinReceiveChunk = 1;
		bleDevice->receiveChunkInProgress += (bleDevice->receiveState == AWAITING_END_OF_IN_SEQUENCE_CHUNK ? 1 : 0);
		bleDevice->receiveState = (bleDevice->receiveState == AWAITING_END_OF_IN_SEQUENCE_CHUNK ?
			 		AWAITING_START_OF_IN_SEQUENCE_CHUNK
					:
					AWAITING_START_OF_OUT_OF_SEQUENCE_CHUNK
				);
		return;
	}

	if (bleDevice->indexWithinReceiveChunk >= MAX_BLE_CHUNK_LENGTH
			|| bleDevice->receiveBuffer->getWriteIndex() >= messageLengthPerHeaderPreamble) {

		setChunkReceived(bleDevice, bleDevice->receiveChunkInProgress);

		if (!getAllChunksReceived(bleDevice)) {
			bleDevice->receiveState = AWAITING_START_OF_OUT_OF_SEQUENCE_CHUNK;
			bleDevice->indexWithinReceiveChunk = 1;
			return;
		}

		if (bleDevice->messageBeingReceived->getIsMessageCrc16Valid()) {
			Log.i("Complete Message received from %s", bleDevice->peerName);
			if (Log.getLoggingLevel() >= Log.INFO) { bleDevice->receiveBuffer->printEntireMessage(); }

			sendAck(bleDevice);
			processRoutedMessage(bleDevice);									// Success
			resetReceiveMessage(bleDevice);

		} else {
			Log.w("Crc16 failed in message from %s:", bleDevice->peerName);
			if (Log.getLoggingLevel() >= Log.WARN) { bleDevice->receiveBuffer->printEntireMessage(); }
			sendNack(bleDevice);
			failAndClearAnyMessagesBeingReceived(bleDevice);
		}
	}
}




void BleStar::failAndClearAnyMessagesBeingReceived(BleDeviceTable * bleDevice) {
	if (bleDevice->messageBeingReceived != NULL) {
		Log.w("Message currently being received from device %s will be discarded:", bleDevice->peerName);
		if (Log.getLoggingLevel() >= Log.WARN) { bleDevice->receiveBuffer->printEntireMessage(); }
	}
	resetReceiveMessage(bleDevice);
}
