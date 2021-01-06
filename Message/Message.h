#ifndef Message_h
#define Message_h

#include "Arduino.h"
#include "Common/CommonDefinitions.h"
#include "Utility/CRC.h"
#include "Utility/Logger.h"
#include "Utility/MessageBuilder.h"

#define CRC_START_MODBUS			0xFFFF
#define	CRC_POLY_16					0xA001
#define CRC_POLY_CCITT				0x1021


#define COMPILED_MESSAGE_CRC8_POSITION						1
#define COMPILED_MESSAGE_CRC16_POSITION						2
#define COMPILED_MESSAGE_LENGTH_POSITION					4
#define MESSAGEID_POSITION									6
#define COMPILED_MESSAGE_ORIGIN_NAME_POSITION				8




class Message {

public:

	static void initialize();

	void compileMessage(
		uint8_t * payload,
		int payloadLength,
		char * origin,
		char * destination,
		uint16_t messageId,
		boolean isSystemMessage,
		int messageType

	);

	void copy(Message * m);
	void importMessage(uint8_t * messageBufferPointer, uint8_t * messageToImport, char * fromNodeName);


	// getter setters left in header

	MessageBuilder * getMessageBuilder() { return _messageBuilder; }

	int getMessageType() { return _messageType; }
	void setMessageType(int mt) { _messageType = mt; }

	boolean getRequiresRouting() { return _requiresRouting; }
	void setRequiresRouting(boolean b) { _requiresRouting = b; }

	int getSendAttempts() { return _sendAttempts; }
	void setSendAttempts(int sa) { _sendAttempts = sa; }

	int getMaxSendAttempts() { return _maxSendAttempts; }
	void setMaxSendAttempts(int msa) { _maxSendAttempts = msa; }

	unsigned long getFirstSendAttemptTimestamp() { return _firstSendAttemptTimestamp; }
	void setFirstSendAttemptTimestamp(unsigned long ul) { _firstSendAttemptTimestamp = ul; }

	unsigned long getLastSendAttemptTimestamp() { return _lastSendAttemptTimestamp; }
	void setLastSendAttemptTimestamp(unsigned long ul) { _lastSendAttemptTimestamp = ul; }

	uint8_t * getStartOfCompiledMessage() { return _startOfCompiledMessage; }
	void setStartOfCompiledMessage(uint8_t * u) { _startOfCompiledMessage = u; }

	uint8_t * getEndOfCompiledMessageReservedSpace() { return _endOfCompiledMessageReservedSpace; }
	void setEndOfCompiledMessageReservedSpace(uint8_t * u) { _endOfCompiledMessageReservedSpace = u; }

	boolean getIsSystemMessage();
	void setIsSystemMessage(boolean b);
	void clearMessageLength();

	uint16_t getCapacity() { return (uint16_t)(_endOfCompiledMessageReservedSpace - _startOfCompiledMessage); }

	void reset() { setMessageType(MESSAGE_TYPE_NONE); }

	uint8_t * getPayload() { return _messagePayload; }
	void setMessagePayload(uint8_t * u) { _messagePayload = u; }

	char * getOrigin() { return _origin; }
	void setOrigin(char * c) { _origin = c; }
	char * getDestination() { return _destination; }
	void setDestination(char * c) { _destination = c; }

	char * getFromHop() { return _fromHop; }
	void setFromHop(char * ch) { _fromHop = ch; }

	char * getToHop() { return _toHop; }
	void setToHop(char * ch) { _toHop = ch; }

	// Getter setters requiring code in main .cpp file
	uint16_t getPayloadLength();
	uint16_t getMessageId();
	void setMessageId(uint16_t u);

	uint16_t getCompiledMessageLength();
	uint16_t getStoredMessageLength();
	void setStoredMessageLength(uint16_t u);

	uint8_t getStoredMessageCrc8();
	void setStoredMessageCrc8(uint8_t u);
	uint8_t getCalculatedMessageCrc8();
	boolean getIsMessageCrc8Valid();

	uint16_t getStoredMessageCrc16();
	void setStoredMessageCrc16(uint16_t u);
	uint16_t getCalculatedMessageCrc16();
	boolean getIsMessageCrc16Valid();

	void invalidateMessage();
	static boolean getMessagesHaveBeenChanged();

private:
	static Logger Log;

	MessageBuilder * _messageBuilder;

	int _messageType = 0;
	boolean _requiresRouting = false;
	int _sendAttempts = 0;
	int _maxSendAttempts = 0;

	unsigned long _firstSendAttemptTimestamp = 0;
	unsigned long _lastSendAttemptTimestamp = 0;

	uint8_t * _startOfCompiledMessage;
	uint8_t * _endOfCompiledMessageReservedSpace;

	char * _origin;
	char * _destination;
	uint8_t * _messagePayload;

	char * _fromHop;
	char * _toHop;

	static boolean _messagesHaveBeenChanged;

	uint8_t getMessageCrc8(uint8_t * cm, uint16_t length);
	void setMessageCrc8(uint8_t * cm, uint8_t crc8);

	uint16_t getMessageCrc16(uint8_t * cm, uint16_t length);
	void setMessageCrc16(uint8_t * cm, uint16_t crc16);




};

#endif
