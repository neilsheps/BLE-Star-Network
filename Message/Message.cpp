#include "Message.h"

/*
	The send method compiles a message, the message's origin (this node name), and a destination pattern along with various checksums
	and message length information into a uint8_t buffer which can then be sent to whichever adjacent node in the routing tables appears
	to have the required destination pattern.

	An example transmission format is below (this is called a compiledMessage)

	#12345678#originNodeNamedestinationNodePatternHello, this is the message

	# = first character of transmission, always a hash
	1 = CRC8 placeholder for bytes 2-N of the first chunk to be sent (usually each chunk is 20 bytes, so CRC8 on bytes 2-19)
		This checks that the first chunk gets through ok and if the CRC checks out and the hashes are in the right place,
		this denotes the start of a transmission.
	2,3 = CRC16 placeholder for the whole transmission from one byte after the second hash
	4,5 = length of encapsulated message (limit is 4096 in fact, please make buffers large enough for this if you need it)
	6,7 = MessageId if provided by sender.  When an ACK or NACK is received by the originating node, it fires a callback with this MessageId
	[originNodeName] = always this node's name.  up to 20 bytes
	\0 = char array terminator
	[destinationNodePattern] = a pattern of up to 40 bytes.  This doesn't have to match a single node's name, as wildcards are allowed.  See MessageRouting.cpp
	\0 = char array terminator
	[Message] = a uint8_t buffer which can be as long as needed as long as the compiled message is <250 bytes
	\0 = char array terminator

	However, because packets go missing, we also chop this up into discrete "chunks" when being sent.  Ideally these chunks are the same size
	as a BLE PDU (usually 20 bytes), so that if any chunks go missing, the receiving device knows to request the missing chunks.

	A long message will then be split up like this if each chunk is 20 bytes.

	Note:  Where you see a '\' that's actually a '\0' delimiter, and the numbers
	that show up at the start of each chunk are binary numbers 1 through 4 (0x01 - 0x04)

	Chunk 0:  #1234567Origin\Dest
	Chunk 1:  1inationPAttern\Actua
	Chunk 2:  2l message is here an
	Chunk 3:  3d is split up like t
	Chunk 4:  4his\

	If the receiving BLE device receives the following:

	Chunk 0:  #12345678#Origin\Dest
	Chunk 2:  2l message is here an
	Chunk 3:  3d is split up like t

	The receiving station will time out when chunk 4 doesn't show up and then will request chunk 1 and 4.   If a chunk in the middle goes
	missing, then as soon as the last chunk is received, it will request a resend.  If chunk zero goes missing, the receiving device
	has no way to tell where a transmission starts, so it will basically ignore the entire message, then sending station will eventually
	resend, and hopefully chunk zero gets through the next time

	If a resend is required, the device that is missing chunks will send:

	RESEND:14\				-- where the 1 = 0x01, the 4 = 0x04 and the \ is '\0'

	Given a 250 byte message can have ~13 chunks of 19 bytes (20 minus the byte at the front with the chunk number), and resend:\
	uses 8 bytes, there are 12 bytes for requesting which chunk is needed.   If >10 chunks go missing, a NACK\ is sent instead, to
	trigger the entire messge being resent

	Other messages that go back to the sending device would include "ACK\" - basically everything was received OK


*/

void Message::initialize() {
	if (Serial) { Log.initialize(&Serial, "Message:"); }
	_messagesHaveBeenChanged = true;
}


void Message::compileMessage(
	uint8_t * payload,
	int payloadLength,
	char * origin,
	char * destination,
	uint16_t messageId,
	boolean isSystemMessage,
	int messageType

) {

	_messageType = messageType;
	_maxSendAttempts = DEFAULT_MAX_SEND_ATTEMPTS;
	_sendAttempts = 0;

	_messageBuilder->initialize(getStartOfCompiledMessage(), getEndOfCompiledMessageReservedSpace() - getStartOfCompiledMessage());

	_messageBuilder->reset();
	_messageBuilder->append('#');												// delimiter
	_messageBuilder->append('1');												// CRC8 placeholder for first chunk bytes starting from position #2 (usually 18 bytes, 20-2)
	_messageBuilder->append("23");												// CRC16 placeholder for entire message starting after second '#'
	_messageBuilder->append("45");												// placeholder for length of message sent by client (total compiled message length = )
	_messageBuilder->append((uint8_t)((messageId / 256) & 0xFF));																	// MesssageId MSB
	_messageBuilder->append((uint8_t) ((messageId) & 0xFF));					// MessageId LSB

	setFromHop(origin);
	_origin = (char *)&_startOfCompiledMessage[_messageBuilder->getLength()];
	_messageBuilder->append(origin);															// crc16 checksum calculation starts here
	_messageBuilder->append('\0');

	_toHop = NULL;
	_destination = (char *)&_startOfCompiledMessage[_messageBuilder->getLength()];
	_messageBuilder->append(destination);
	_messageBuilder->append('\0');

	_messagePayload = &_startOfCompiledMessage[_messageBuilder->getLength()];
	_messageBuilder->append(payload, payloadLength);
	_messageBuilder->append('\0');

	clearMessageLength();
	setStoredMessageLength(_messageBuilder->getLength());
	setIsSystemMessage(isSystemMessage);

	setStoredMessageCrc8(getCalculatedMessageCrc8());
	setStoredMessageCrc16(getCalculatedMessageCrc16());

	_requiresRouting = true;
}

void Message::copy(Message * m) {

	_messageBuilder = m->getMessageBuilder();
	_messageType = m->getMessageType();
 	_requiresRouting = m->getRequiresRouting();
	_sendAttempts = m->getSendAttempts();
	_maxSendAttempts = m->getMaxSendAttempts();

	_firstSendAttemptTimestamp = m->getFirstSendAttemptTimestamp();
	_lastSendAttemptTimestamp = m->getLastSendAttemptTimestamp();

	_startOfCompiledMessage = m->getStartOfCompiledMessage();
	_endOfCompiledMessageReservedSpace = m->getEndOfCompiledMessageReservedSpace();

	_origin = m->getOrigin();
	_destination = m->getDestination();
	_messagePayload = m->getPayload();

	_fromHop = m->getFromHop();
	_toHop = m->getToHop();

	_messagesHaveBeenChanged = true;
}

// need to fix this completely   ...

//
//

void Message::importMessage(uint8_t * messageBufferPointer, uint8_t * messageToImport, char * fromNode) {

	_messageType = MESSAGE_TYPE_INCOMING;
	_maxSendAttempts = DEFAULT_MAX_SEND_ATTEMPTS;
	_sendAttempts = 0;
	_fromHop = fromNode;
	_toHop = NULL;
	_firstSendAttemptTimestamp = 0;
	_lastSendAttemptTimestamp = 0;
	_requiresRouting = true;


	_startOfCompiledMessage = messageBufferPointer;
	_origin = (char *)&messageBufferPointer[COMPILED_MESSAGE_ORIGIN_NAME_POSITION];

	for (int i = 0; i < COMPILED_MESSAGE_ORIGIN_NAME_POSITION; i++ ) { _startOfCompiledMessage[i] = messageToImport[i]; }

	uint16_t messageLength = getStoredMessageLength();
	_endOfCompiledMessageReservedSpace = &messageBufferPointer[messageLength];

	int zerosFound = 0;
	for (int i = COMPILED_MESSAGE_ORIGIN_NAME_POSITION; i < messageLength; i++) {
		uint8_t u = messageToImport[i];
		if (u == 0) {
			zerosFound++;
			if (zerosFound == 1) { _destination = (char *)&messageBufferPointer[i]; }
			else if (zerosFound == 2) { _messagePayload = &messageBufferPointer[i]; }
		}
		_startOfCompiledMessage[i] = u;
	}
}


uint16_t Message::getPayloadLength() { return (uint16_t)(_messageBuilder->getLength() - (_messagePayload - _startOfCompiledMessage)); }
uint16_t Message::getCompiledMessageLength() { return ((uint16_t)(_messageBuilder->getLength())); }

uint16_t Message::getStoredMessageLength() {
	return (_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] * 256
			+ _startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION + 1]);
}

void Message::clearMessageLength() {
	_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] = 0;
	_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION + 1] = 0;
}

void Message::setStoredMessageLength(uint16_t u) {
	uint8_t systemMessageBit = _startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] & 0x80;
	_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] = (uint8_t)((u / 256 & 0xFF) + systemMessageBit);
	_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION + 1] = (uint8_t)(u & 0xFF);
}

boolean Message::getIsSystemMessage() {
	return ((_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] & 0x80) > 0);
}

void Message::setIsSystemMessage(boolean b) {
	_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] =
				(_startOfCompiledMessage[COMPILED_MESSAGE_LENGTH_POSITION] & 0x7F)
			+	(b ? 0x80 : 0x00);
}

uint16_t Message::getMessageId() {
	return (uint16_t)((256 * _startOfCompiledMessage[MESSAGEID_POSITION] + _startOfCompiledMessage[MESSAGEID_POSITION + 1]) & 0xFFFF);
}
void Message::setMessageId(uint16_t u) {
	_startOfCompiledMessage[MESSAGEID_POSITION] = (uint8_t)(u / 256 & 0xFF);
	_startOfCompiledMessage[MESSAGEID_POSITION + 1] = (uint8_t)(u & 0xFF);
}

uint8_t Message::getStoredMessageCrc8() { return (_startOfCompiledMessage[COMPILED_MESSAGE_CRC8_POSITION]); }
void Message::setStoredMessageCrc8(uint8_t u) { _startOfCompiledMessage[COMPILED_MESSAGE_CRC8_POSITION] = u; }
uint8_t Message::getCalculatedMessageCrc8() {
	return (
		(uint8_t)CRC::getCrc8(
					&_startOfCompiledMessage[COMPILED_MESSAGE_CRC8_POSITION + 1],
					min(MAX_BLE_CHUNK_LENGTH, getStoredMessageLength()) - (COMPILED_MESSAGE_CRC8_POSITION + 1) )
	);
}
boolean Message::getIsMessageCrc8Valid() {
	uint8_t calculatedCrc8 = getCalculatedMessageCrc8();
	if (getStoredMessageCrc8() == calculatedCrc8) { return true; }
	Log.w("Crc8 Checksum error!  Expected %02X calculated %02X in message", getStoredMessageCrc8(), calculatedCrc8);
	return false;
}

uint16_t Message::getStoredMessageCrc16() { return (256 * _startOfCompiledMessage[COMPILED_MESSAGE_CRC16_POSITION] + _startOfCompiledMessage[COMPILED_MESSAGE_CRC16_POSITION + 1]); }
void Message::setStoredMessageCrc16(uint16_t u) {
	_startOfCompiledMessage[COMPILED_MESSAGE_CRC16_POSITION + 1] = (uint8_t)((u / 256) & 0xFF);
	_startOfCompiledMessage[COMPILED_MESSAGE_CRC16_POSITION] = (uint8_t)((u) & 0xFF);
}
uint16_t Message::getCalculatedMessageCrc16() {
	return (
			(uint16_t)(CRC::getCrc16(&_startOfCompiledMessage[COMPILED_MESSAGE_ORIGIN_NAME_POSITION],
			getStoredMessageLength() - (COMPILED_MESSAGE_ORIGIN_NAME_POSITION))  & 0xFFFF)
		);
}
boolean Message::getIsMessageCrc16Valid() {
	uint16_t calculatedCrc16 = getCalculatedMessageCrc16();
	if (getStoredMessageCrc16() == calculatedCrc16) { return true; }
	Log.w("Crc16 Checksum error!  Expected %04X calculated %04X in message", getStoredMessageCrc16(), calculatedCrc16);
	return false;
}

void Message::invalidateMessage() {
	_messageType = MESSAGE_TYPE_NONE;
	_requiresRouting = false;
	_messagesHaveBeenChanged = true;
}

boolean Message::getMessagesHaveBeenChanged() {
	boolean b = _messagesHaveBeenChanged;
	_messagesHaveBeenChanged = false;
	return b;
}
