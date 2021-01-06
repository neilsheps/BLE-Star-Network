#ifndef MessageTable_cpp
#define MessageTable_cpp


#include "MessageTable.h"

void MessageTable::initialize(int messageBufferCapacity, int messageTableCapacity) {
	_messageBufferCapacity = messageBufferCapacity;
	_messageTableCapacity = messageTableCapacity;
	_messageBuffer = new uint8_t[_messageBufferCapacity];
	_messageTable = new Message[_messageTableCapacity];
	_messageTableHasBeenChanged = false;
	if (Serial) { Log.initialize(&Serial, "MessageTable:"); }
}


Message * MessageTable::addNewMessageToSend(									// used to add new messages
	uint8_t * payload,
	int payloadLength,
	char * origin,
	char * destination,
	uint16_t messageId,
	boolean isSystemMessage
	) {

		if (!isSystemMessage && !canAcceptMoreMessagesFromThisDevice(origin)) { return NULL; }

		Message * message = getNewMessageTableEntry(payloadLength + MAX_MESSAGE_BUFFER_PREAMBLE_LENGTH);
		if (message == NULL) { return NULL; }

		message->compileMessage(
			payload,
			payloadLength,
			origin,
			destination,
			messageId,
			isSystemMessage,
			MESSAGE_TYPE_ORIGIN
		);
		_messageTableHasBeenChanged = true;
		return message;
}

Message * MessageTable::getNewMessageTableEntry(int sizeOfBufferNeeded) {

	if (_messageTableSize + MAX_CENTRAL_CONNECTIONS + 2 > _messageTableCapacity) {
		Log.e("Warning!  Not enough messageTable entries available!  capacity = %d, used = %d", _messageTableCapacity, _messageTableSize);
		return NULL;
	}

	if ((_messageTableSize + MAX_CENTRAL_CONNECTIONS + 2 > _messageTableCapacity)
			|| (_messageBufferCapacity - getMessageBufferSize() < sizeOfBufferNeeded)
		) {
		defragmentMessages();
	}

	if (_messageBufferCapacity - getMessageBufferSize() < sizeOfBufferNeeded) {
		Log.e("Warning!  Cannot allocated desired buffer space (%d bytes), only %d bytes available",
					sizeOfBufferNeeded,
					(_messageBufferCapacity - getMessageBufferSize()));
		return NULL;
	}

	Message * newMessage = &_messageTable[_messageTableSize++];
	uint8_t * newMessageBuffer = &_messageBuffer[getMessageBufferSize()];
	newMessage->reset();
	newMessage->setStartOfCompiledMessage(newMessageBuffer);
	newMessage->setEndOfCompiledMessageReservedSpace(&newMessageBuffer[sizeOfBufferNeeded]);
	return (newMessage);

}


Message * MessageTable::reserveSpaceForIncomingMessage(int requiredBufferCapacity, char * fromHop) {
	Message * message = getNewMessageTableEntry(requiredBufferCapacity);
	if (message == NULL) {
		Log.w("Error: cannot reserve space for incoming message from %s; %d bytes required, %d available", fromHop, requiredBufferCapacity, _messageBufferCapacity - getMessageBufferSize());
		return NULL;
	}
	message->setFromHop(fromHop);
	_messageTableHasBeenChanged = true;
	return message;
}

Message * MessageTable::findNextAvailableMessageForHop(char * toHop) {
	for (int i = 0; i < _messageTableSize; i++) {
		if (_messageTable[i].getToHop() == toHop && _messageTable[i].getMessageType() == MESSAGE_TYPE_HOP) {
			return (&_messageTable[i]);
		}
	}
	return NULL;
}



void MessageTable::makeSpaceForRouting(int originPosition, int numberOfMessageTableEntriesRequired) {
	if (numberOfMessageTableEntriesRequired == 0 ) { return; }
	for (int i = _messageTableSize + numberOfMessageTableEntriesRequired - 1;
		 	i > originPosition + numberOfMessageTableEntriesRequired;
			i--) {
		Message * destination = &_messageTable[i];
		Message * source = &_messageTable[i - numberOfMessageTableEntriesRequired];
		destination->copy(source);
	}
}

void MessageTable::cloneMessage(Message * sourceMessage, char * newFromHop, char * newToHop, int destinationMessageTableIndex) {
	Message * destinationMessage = &_messageTable[destinationMessageTableIndex];
	destinationMessage->copy(sourceMessage);
	setHopsInMessage(destinationMessage, newFromHop, newToHop);
}

void MessageTable::setHopsInMessage(Message * message, char * newFromHop, char * newToHop) {
	message->setMessageType(MESSAGE_TYPE_HOP);
	message->setSendAttempts(0);
	message->setMaxSendAttempts(DEFAULT_MAX_SEND_ATTEMPTS);

	message->setFromHop(newFromHop);
	message->setToHop(newToHop);

	_messageTableHasBeenChanged  = true;
}



// need methods to determine capacity




// fix this......
boolean MessageTable::defragmentMessageTable() {

	int entriesToDelete = 0;
	int index = 0;
	while (index + entriesToDelete < _messageTableSize) {
		if (_messageTable[index].getMessageType() == MESSAGE_TYPE_SUCCESS && _messageTable[index].getLastSendAttemptTimestamp() + 5000 < millis()) {
			entriesToDelete++;
		}
		if (entriesToDelete > 0) {
			_messageTable[index] = _messageTable[index + entriesToDelete];
		}
		index++;
	}
	if (entriesToDelete > 0) {
		Log.v("defragmented messageTable and deleted %d entries\n", entriesToDelete);
		_messageTableSize -= entriesToDelete;
	}
	return (entriesToDelete > 0);
}

void MessageTable::defragmentMessages() {
	if (!defragmentMessageTable()) {
		return;
	}
	sortMessageTable();
	if (getMessageBufferNeedsDefragmentation()) {
		defragmentMessageBuffer();
	}
}

boolean MessageTable::canAcceptMoreMessagesFromThisDevice(char * device) {
	for (int i = 0; i < _messageTableSize; i++) {
		char * chk = strstr(_messageTable[i].getFromHop(), device);
		if (chk != NULL && _messageTable[i].getMessageType() != MESSAGE_TYPE_ORIGIN) { return false; }
	}
	return true;
}

int MessageTable::getMessageBufferSize() {
	return (_messageTableSize == 0 ?
		0
		:
		_messageTable[_messageTableSize-1].getEndOfCompiledMessageReservedSpace() - _messageTable[0].getStartOfCompiledMessage()
	);
}

boolean MessageTable::getMessageBufferNeedsDefragmentation() {
	int sizeOfMessageBuffer = getMessageBufferSize();
	int bytesUsedInMessageBuffer = 0;
	uint8_t * tempMessageStart = 0;
	int sharedMessageBufferEntries = 0;
	for (int i = 0; i < _messageTableSize; i++) {
		uint8_t * thisMessageStart = _messageTable[i].getStartOfCompiledMessage();
		if (thisMessageStart != tempMessageStart) {								// there might be multiple messageTable entries pointing to the same messageBuffer entries, so discard duplicates
			bytesUsedInMessageBuffer = _messageTable[i].getEndOfCompiledMessageReservedSpace() - _messageTable[i].getStartOfCompiledMessage();
		} else {
			sharedMessageBufferEntries++;
		}
	}
	float utilization = (float)(100 * sizeOfMessageBuffer)/((float)_messageBufferCapacity);
	float fragmentation = 100.0f - (float)(100 * bytesUsedInMessageBuffer)/((float)sizeOfMessageBuffer);

	Log.v("BleStar:: MessageBuffer size = %d (%0.1f \%utilization), fragmentation = %0.1f\n", sizeOfMessageBuffer, utilization, fragmentation);
	return (_messageBufferCapacity - sizeOfMessageBuffer < (MAX_COMPILED_MESSAGE_LENGTH + 5));
}


void MessageTable::defragmentMessageBuffer() {
	unsigned long t = millis();
	uint8_t * moveTo = _messageTable[0].getEndOfCompiledMessageReservedSpace() + 1;

	for (int i = 1; i < _messageTableSize; i++) {
		if (moveTo == _messageTable[i].getStartOfCompiledMessage()) {	// only move memory if required
			moveTo = _messageTable[i].getEndOfCompiledMessageReservedSpace() + 1;
		} else {
			uint8_t * moveFrom = _messageTable[i].getStartOfCompiledMessage();
			uint8_t * stopWhen = _messageTable[i].getEndOfCompiledMessageReservedSpace();
			while (moveFrom <= stopWhen) {
				*moveTo++ = *moveFrom++;
			}
		}

	}
	Log.w("BleStar:: Dragmented messageBuffer in %d millis\n", t);
}

boolean MessageTable::getMessageTableHasBeenChanged() {
	boolean b = _messageTableHasBeenChanged;
	_messageTableHasBeenChanged = false;
	return (b);
}


#endif
