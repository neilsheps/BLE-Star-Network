/*
	BleStar.   A library to allow BLE devices to create a star network using peripheral and central modes
	and transmit data to named devices or subscriptions with a reasonable expectation of guaranteed delivery

	Copyright (C) 2021 Neil Shepherd

	This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
	This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
	You should have received a copy of the GNU General Public License along with this program. If not, see <https://www.gnu.org/licenses/>.
*/

#ifndef MessageTable_h
#define MessageTable_h

#include "Common/CommonDefinitions.h"
#include "Utility/Logger.h"
#include "Message/Message.h"


/**
	MessageTable is a class for storing and retrieving routable messages that are stored in class "Message"
	Every message must have a payload, an origin, a destination (which can be a subscription or a device).\n\n

	This class also handles cloning messages (e.g. when they need to be routed/fanned out to child devices),
	and table optimization / garbage collection.\n\n

	This class contains a limited number of messageTable entries, and also a large uint8_t buffer for storing
	the message data that is actually sent (some message preamble, CRCs etc., the origin, destination, payload).
	Both the messageTable array and the messageBuffer buffer are FIFO, and eventually fill up even if old messages
	are deleted.  When this happens, the messageTable and messageBuffer are defragmented, which will cause a
	slight CPU hit.  Larger messageTables or messageBuffers will reduce but not eliminate this.
*/



class MessageTable {
public:

	/** Adds a new message into messageTable, and assigns it messageType "MESSAGE_TYPE_ORIGIN", which means that
	the message will stay in the messageTable until an ACK or NACK has been received from the destination devices.

	Returns NULL if a message cannot be added for any reason, otherwise the compiled, ready-to-send message itself.
	*/
	Message * addNewMessageToSend(
		uint8_t * payload,
		int payloadLength,
		char * originName,
		char * destinationName,
		uint16_t messageId,
		boolean isSystemMessage			/**< boolean to indicate if is a system message, in which case a routed ACK/NACK does not need to be sent on receipt */
	);

	/// Initializes the messageTable and messageBuffer when the BleStar variable is declared in the main program (before setup)
	///
	void initialize(int messageBufferCapacity, int messageTableCapacity);

	/// Finds the next messageTable entry for a given messageBuffer size needed.  The messageBuffer and/or messageTable is
	/// defragmented first if getting full and this is required
	Message * getNewMessageTableEntry(int sizeOfBufferNeeded);

	/// Creates a message and messageTable, messageBuffer entry for a message that's being received by a BLE device.  Once
	/// the message has been completely received, it will be routed further if required, or a callback fired to alert the
	/// main app that a message has arrived.
	Message * reserveSpaceForIncomingMessage(int payloadLength, char * fromHopName);

	/// A utility method that inserts entries into the FIFO messageTable for when a message is routed and fanned out to
	/// multiple intermediate devices.  This is done because the order of messages and messageBuffer entries should remain
	/// in the same order, else defragmentation needs a sort and becomes more computationally intensive.
	void makeSpaceForRouting(int originPosition, int numberOfMessageTableEntriesRequired);

	/// Clones a message into a space in messageTable created by "makeSpaceForRouting", but changes the fromHop/toHop
	/// based on routing required.
	void cloneMessage(Message * sourceMessage, char * newFromHop, char * newToHop, int destinationMessageTableIndex);

	/// Changes the from and to hops in a given message.  used as part of the cloneMessage method
	///
	void setHopsInMessage(Message * message, char * newFromHop, char * newToHop);

	/// Returns a given message stored at a given entry in messageTable
	///
	Message * getMessage(int i) { return &_messageTable[i]; }

	/// Used to find which message should next be sent to or through a connected device.  This is usually requested
	/// once a message has been completely sent to a connected device, and now BleStar is looking for other messages
	/// that need to take the same path
	Message * findNextAvailableMessageForHop(char * toHop);

	/// The messageBuffer that stores all messages (BleStar generated preamble, origin, destination, payload.
	/// a single large uint8_t array is used rather than creating and deleting uint8_t arrays to minimize the
	/// possiblity of memory leaks.   The messageBuffer is defragmented as required when it gets full.
	uint8_t * _messageBuffer;
	/// Variable used to stored the maximum capacity of messageBuffer.  Set at runtime during BleStar declaration.
	///
	int _messageBufferCapacity;
	/// Returns the top of the FIFO buffer.  Usually called to check if there is space to store a new message.
	///
	int getMessageBufferSize();

	/// Checks whether more messages from the current device can be added to messageTable.  Currently the code only
	/// allows a single message at any time, but this may change in future to allow queueing of multiple messages.
	boolean canAcceptMoreMessagesFromThisDevice(char * deviceName);
	/// Defragments both messageTable and messageBuffer if required.
	///
	void defragmentMessages();

	/** Defragments the messageTable.  The lifecycle of a message is based on its messageType, as follows:

		MESSAGE_TYPE_ORIGIN  --- the message was created by this device and stays here until ACK/NACK received\n
		MESSAGE_TYPE_INCOMING --- the message was received by this device (or is in process)\n
		MESSAGE_TYPE_HOP --- the message has been fanned/routed.  This "HOP" delineation lasts only as long as it takes
		 					for the message to successfully make it to the next device it needs to get to\n
		MESSAGE_TYPE_NONE --- once a message does not need to be stored any longer, it's given this type.\n\n

		Defragmentation removes all the MESSAGE_TYPE_NONES and compresses the table, that's all.\n\n

		Example:\n
		_messageTableSize = 10; entries 0-5 are NONE, and 6,7,8,9 are ORIGIN, HOP, HOP, INCOMING\n
		after defragmentation, _messageTableSize = 4, and 0,1,2,3 are ORIGIN, HOP, HOP, INCOMING\n

	*/
	boolean defragmentMessageTable();
	/// Kept here for debugging.  In theory the table will never need to be sorted if "makeSpaceForRouting" works
	///
	void sortMessageTable();
	/// TBD
	///
	boolean getMessageBufferNeedsDefragmentation();
	/// Defragments the messageBuffer.   Each message stores a messageStart and messageEnd pointer.  defragmentation
	/// finds gaps in the table where no messageTable entry claims space, and moves the buffer data so that once done,
	/// each contiguous message entry in messageTable also has contiguous entries in messageBuffer.
	void defragmentMessageBuffer();

	/// Boolean result for optimizing routing only.   Returns true (and then clears it to false) if the messageTable has
	/// been changed, and if so, the messageTable is scanned to see if new messages can be sent
	boolean getMessageTableHasBeenChanged();

	int getCapacity() { return _messageTableCapacity; }
	int getSize() { return _messageTableSize; }

private:
	Logger Log;
	Message * _messageTable;
	int _messageTableSize;
	int _messageTableCapacity;
	int _messageTableIndex;														// used to index through messageTable to see what needs to be sent
	boolean _messageTableHasBeenChanged;

};

#endif
