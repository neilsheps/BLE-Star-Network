#include "BleStar.h"

// Message transmission success/fail callbacks

void BleStar::setTransmissionSucceededCallback(TransmissionSucceededCallback tsc) {
	transmissionSucceededCallback = tsc;
	Log.v("setTransmissionSucceededCallback called");
}
void BleStar::fireTransmissionSucceededCallback(uint16_t messageID) { transmissionSucceededCallback(messageID); }

void BleStar::setTransmissionFailedCallback(TransmissionFailedCallback tfc) {
	transmissionFailedCallback = tfc;
	Log.v("setTransmissionFailedCallback called");
}
void BleStar::fireTransmissionFailedCallback(uint16_t messageID) { transmissionFailedCallback(messageID); }


// Unformed message callback.  Fires is message starts with a '$' and ends with a char <32.
// So, only suited for text messages really, as it cannot support uin8_t / bytes
void BleStar::setUnformedMessageReceivedCallback(UnformedMessageReceivedCallback umrc) {
	unformedMessageReceivedCallback = umrc;
	Log.v("setUnformedMessageReceivedCallback called");
}

void BleStar::fireUnformedMessageReceivedCallback(char * receiveBuffer, int bleDeviceIndex, char * fromName) {
	if (unformedMessageReceivedCallback != NULL) {
		unformedMessageReceivedCallback(receiveBuffer, bleDeviceIndex, fromName);
	}
}

// Routed message received callback.  If a message is routed to this device, then this callback fires
// when a message is received
void BleStar::setRoutedMessageReceivedCallback(RoutedMessageReceivedCallback rmrc) {
	routedMessageReceivedCallback = rmrc;
	Log.v("setRoutedMessageReceivedCallback called");
}

void BleStar::fireRoutedMessageReceivedCallback(Message * m) {
	if (routedMessageReceivedCallback != NULL) {
		routedMessageReceivedCallback(m);
	}
}



// advertisement listeners.  these work by name or by UUID.  up to 5 of each are supported.  In this way,
// the nRF52 can listen to sensors that advertise.  It's up to the callback function to interpret the
// returned BLE reports

void BleStar::addAdvertisementListenerByDeviceName(char * name, listenerFunctionCallback pointerToFunction) {

	if (_numberOfDevicesListenedByName >= MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_NAME) {
		Log.e("Cannot listen for device name: %s; already listening for max # devices (%d)", name, _numberOfDevicesListenedByName);
		return;
	}

	for (int i = 0; i < _numberOfDevicesListenedByName; i++) {
		if (strcmp(name,deviceNameListener[i].deviceNameToListenFor)) {
			Log.w("Already listening to device %s by name", name);
			return;
		}
	}
	strcpy(deviceNameListener[_numberOfDevicesListenedByName].deviceNameToListenFor, name);
	deviceNameListener[_numberOfDevicesListenedByName].pointerToListenerFunction = pointerToFunction;
	_numberOfDevicesListenedByName++;
	Log.v("Added device name to listen to: %s\n", name);
}

void BleStar::fireAdvertisementListenerByDeviceName(int number, ble_gap_evt_adv_report_t * report) {
	(*deviceNameListener[number].pointerToListenerFunction)(report);	// Do something
}





void BleStar::addAdvertisementListenerByUuid(uint8_t uuidArray[16], listenerFunctionCallback ptr) {
	BLEUuid uuid = BLEUuid(uuidArray);
	addAdvertisementListenerByUuid(uuid, ptr);
}

void BleStar::addAdvertisementListenerByUuid(BLEUuid uuid, listenerFunctionCallback ptr) {
	if (_numberOfDevicesListenedByUuid >= MAX_NUMBER_OF_BLE_DEVICES_TO_LISTEN_FOR_BY_UUID) {
		Log.w("Cannot listen for UUID; already listening for max # devices (%d)\n", _numberOfDevicesListenedByUuid);
		return;
	}

	boolean found = true;
	for (int i = 0; i < _numberOfDevicesListenedByUuid; i++) {
		found = true;
		for (int j = 0; j < 16; j++) {
			if (uuid._uuid128[j] != uuidListener[i].uuidToListenFor._uuid128[j]) {
				found = false;
				continue;
			}
			if (found) {
				Log.w("Already listening for UUID provided\n");
				return;;
			}
		}

	uuidListener[_numberOfDevicesListenedByUuid].uuidToListenFor = uuid;
	uuidListener[_numberOfDevicesListenedByUuid].pointerToListenerFunction = ptr;
	_numberOfDevicesListenedByUuid++;

	Log.v("Added device to listen for by UUID\n");
	}
}

void BleStar::fireAdvertisementListenerByUuid(int number, ble_gap_evt_adv_report_t * report) {
	(*uuidListener[number].pointerToListenerFunction)(report);	// Do something
}
