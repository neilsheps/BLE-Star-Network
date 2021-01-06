#include "BleStar.h"


// ----------------------------- Short hop messages ----------------------------
/*
	These are messages that only go between adjacent, directly connected devices
	They are usually short, and there is no guaranteed delivery or follow-up
	Assured when they are sent

*/

// Intercepts unformed messages and checks whether they are system messages
// (ACK, NACK, RESEND......)

boolean BleStar::isShortIncomingSystemMessage(BleDeviceTable * bleDevice) {
	if (strstr((char *)bleDevice->receiveBuffer->getBuffer(), STRING_ACK) != NULL) {
		bleDevice->messageBeingReceived->setMessageType(MESSAGE_TYPE_NONE);
		return true;
	}

	if (strstr((char *)bleDevice->receiveBuffer->getBuffer(), STRING_NACK) != NULL) {
		if (bleDevice->messageBeingSent->getSendAttempts() >= bleDevice->messageBeingSent->getMaxSendAttempts()) {
			Log.w("Max send attempts for messageID %d hit, aborting", bleDevice->messageBeingSent->getMessageId());
			fireTransmissionFailedCallback(bleDevice->messageBeingSent->getMessageId());
			failAndClearAnyMessagesBeingSent(bleDevice);
			return true;
		}
		// resend message
	}
	if (strstr((char *)bleDevice->receiveBuffer->getBuffer(), STRING_RESEND) != NULL) {
		char * b = strstr((char *)bleDevice->receiveBuffer->getBuffer(), ":") + 1;
		while (* b >= 48) { setChunkNotSent(bleDevice, *b++ - 48); }
		bleDevice->sendChunkResendRequested = true;
		return true;
	}
	return false;
}

// Methods that send short, raw, unrouted messages between directly connected
// devices (ACK, NACK, RESEND......)

void BleStar::sendAck(BleDeviceTable * bleDevice) {
	MessageBuilder mb(MAX_BLE_CHUNK_LENGTH);
	mb.append(STRING_ACK);
	sendRawToBleDevice(mb.getBuffer(), mb.getLength(), bleDevice->index);
	return;
}

void BleStar::sendNack(BleDeviceTable * bleDevice) {
	MessageBuilder mb(MAX_BLE_CHUNK_LENGTH);
	mb.append(STRING_NACK);
	sendRawToBleDevice(mb.getBuffer(), mb.getLength(), bleDevice->index);
	return;
}

boolean BleStar::sendResendRequestSequence(BleDeviceTable * bleDevice, int fromChunk, int toChunkInclusive) {
	int resendsRequested = 0;
	MessageBuilder mb(MAX_BLE_CHUNK_LENGTH);
	mb.append(STRING_RESEND);
	for (uint8_t i = fromChunk; i < toChunkInclusive; i++) {
		if (!getChunkReceived(bleDevice, i)) {
			mb.append((uint8_t)(48+i));
			bleDevice->lastResendRequestTime = millis();
			resendsRequested++;
		}
	}
	if (resendsRequested > 10) {
		mb.reset();
		mb.append(STRING_NACK);
	}
	sendRawToBleDevice(mb.getBuffer(), mb.getLength(), bleDevice->index);
	return (resendsRequested > 0);
}

void BleStar::sendResendRequest(BleDeviceTable * bleDevice, int chunkResendIndex) {
	MessageBuilder mb(MAX_BLE_CHUNK_LENGTH);
	mb.append(STRING_RESEND);
	mb.append((uint8_t)(48+chunkResendIndex));
	sendRawToBleDevice(mb.getBuffer(), mb.getLength(), bleDevice->index);
	bleDevice->lastResendRequestTime = millis();
}



// --------------------------- Routed system messages --------------------------
/*
	These system messages are designed for guaranteed delivery and can reach
	devices that are not directly connected.
	Examples include:
	 - sending notice that a device (and anything that it was connected to
 	   downstream) has been connected or disconnected and updating the routing
	   table on upstream devices
	 - sending information on rssi etc. for network configuration
	 - accepting instructions from the upstream gateway on any reconfiguration
	   that needs to happen (e.g. a device has poor connectivity the way it's
   	   connected right now, and once it's disconnected a specified device has
   	   priority connecting to it

*/

void BleStar::processRoutedSystemMessage(Message * m) {
	char * payload = (char *)m->getPayload();

	if (strstr(payload, STRING_ROUTES) == payload) {
		char * colonPosition = strstr(payload, ":");
		rTable.setAvailableRoutesFromCharArray(&colonPosition[1], m->getFromHop());
		return;
	}

	Log.w("Error: routed system message found from %s, but not executed: %s", m->getOrigin(), (char *)m->getPayload());
}



void BleStar::sendRoutingInformationUpstream(char * routingInformation) {

	if (!bleDeviceTable[BLE_PERIPHERAL_INDEX].isConnected) { return; }

	MessageBuilder mb(1000);													// large buffer, as nodes closer to gateway may have dozens of connections
	mb.append(STRING_ROUTES);
	mb.append(routingInformation);
	int len = mb.getLength();

	char destination[2] = "\0";
	mTable.addNewMessageToSend(
		(uint8_t *)mb.getBuffer(),
		len,
		_thisDeviceName,
		destination,															// null, so it just goes upstream
		0,																		// messageId
		true																	// isSystemMessage
	);
	Log.i("New routing changes message: %s, added to message table", (char*)mb.getBuffer());
}

void BleStar::sendAllRoutingInformationUpstream() {
	sendRoutingInformationUpstream(rTable.getAvailableRoutesAsCharArray());
}

void BleStar::subscribe(char * subscriptionName) {
	rTable.getIndexFromName(subscriptionName, BLE_THIS_DEVICE_INDEX);
	sendRoutingInformationUpstream(subscriptionName);
}

void BleStar::unsubscribe(char * subscriptionName) {
	int routingTableIndex = rTable.getIndexFromName(subscriptionName);
	rTable.setRouteForDestination(routingTableIndex, BLE_THIS_DEVICE_INDEX, false);
	if (rTable.getDoesDestinationHaveRoutes(routingTableIndex)) {
		Log.i("Unsubscribe requested for %s, but subscription has routes to other destinations; no changes to send upsteam", subscriptionName);
	} else {
		MessageBuilder mb(40);
		mb.append('-');
		mb.append(subscriptionName);
		sendRoutingInformationUpstream((char *)mb.getBuffer());
		Log.i("Unsubscribe requested for %s; sending changes upstream", subscriptionName);
	}
}
