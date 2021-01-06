
#include "RoutingTable.h"


struct SortRoutingTable {
	boolean operator() (RoutingTableStruct a, RoutingTableStruct b) { return (a.timesUsed < b.timesUsed); }
} routingTableEntryComparator;


void RoutingTable::initialize(int routingTableCapacity) {
	_routingTableCapacity = routingTableCapacity;
	_routingTable = new RoutingTableStruct[_routingTableCapacity];
	_routingTableSize = 0;
	_numberOfLookups = 0;
	_routingTableHasBeenChanged = false;
	_sendUpstreamIndex = getIndexFromName((char *)("\0"), BLE_PERIPHERAL_INDEX);			// sets a null string to point upstream to the gateway
	_sendToAllIndex = getIndexFromName((char *)("*"));									// creates an entry for send to all
	Log.initialize(&Serial, "RoutingTable:");
}

char * RoutingTable::getNamePointerFromName(char * destinationName) { return (getNamePointerFromName(destinationName, -1)); }
char * RoutingTable::getNamePointerFromName(char * destinationName, int peerBleDeviceIndex) {
	int routingTableIndex = getIndexFromName(destinationName, peerBleDeviceIndex);
	return (index >=0 ? _routingTable[routingTableIndex].destinationName : NULL);
}


int RoutingTable::getIndexFromName(char * destinationName) { return (getIndexFromName(destinationName, -1)); }
int RoutingTable::getIndexFromName(char * destinationName, int peerBleDeviceIndex) {
	int routingTableIndex = -1;
	for (int i = 0; i < _routingTableSize; i++) {
		if (_routingTable[i].destinationName[0] == '\0' && destinationName[0] == '\0') { routingTableIndex = i; break; }
		if (strstr(_routingTable[i].destinationName, destinationName)) { routingTableIndex = i; break; }
	}
	routingTableIndex = (routingTableIndex == -1 ? addDestinationNameToRoutingTable(destinationName) : routingTableIndex);
	setRouteForDestination(routingTableIndex, peerBleDeviceIndex, true);
	return (routingTableIndex);
}

int RoutingTable::getIndexFromNamePointer(char * destinationName) { return (getIndexFromNamePointer(destinationName, -1)); }
int RoutingTable::getIndexFromNamePointer(char * destinationName, int peerBleDeviceIndex) {
	int routingTableIndex = -1;
	for (int i = 0; i < _routingTableSize; i++) {
		if (_routingTable[i].destinationName == destinationName) { routingTableIndex = i; break; }
	}
	routingTableIndex = (routingTableIndex == -1 ? addDestinationNameToRoutingTable(destinationName) : routingTableIndex);
	setRouteForDestination(routingTableIndex, peerBleDeviceIndex, true);
	return (routingTableIndex);
}

void RoutingTable::setRouteForDestination(int routingTableIndex, int peerBleDeviceIndex, boolean b) {
	_routingTable[routingTableIndex].timesUsed++;
	_numberOfLookups++;
	if (routingTableIndex == -1 || peerBleDeviceIndex == -1 || peerBleDeviceIndex >= MAX_CENTRAL_CONNECTIONS + 2) { return; }

	if (_routingTable[routingTableIndex].routeExists[peerBleDeviceIndex] == !b) {
		_routingTableHasBeenChanged = true;
		_routingTable[routingTableIndex].routeExists[peerBleDeviceIndex] = b;
	}
	_routingTable[_sendToAllIndex].routeExists[peerBleDeviceIndex] = b;
	if (peerBleDeviceIndex == BLE_PERIPHERAL_INDEX) { 	_routingTable[_sendUpstreamIndex].routeExists[peerBleDeviceIndex] = b;	}
}


int RoutingTable::addDestinationNameToRoutingTable(char * name) {
	if (_routingTableSize >= _routingTableCapacity - 1) {
		Log.e("routingTable is full at %d items, cannot add node name: %s\n", _routingTableSize, name);
		return -1;
	}
	Log.v("Adding name: %s to routingTable, entry (zero based) #%d\n", name, _routingTableSize);

	for (int i = 0; i < MAX_CENTRAL_CONNECTIONS + 2; i++) { _routingTable[_routingTableSize].routeExists[i] = false; }
	_routingTable[_routingTableSize].timesUsed = 0;
	strcpy(_routingTable[_routingTableSize++].destinationName, name);

	_routingTableHasBeenChanged = true;
	return (_routingTableSize - 1);
}

boolean RoutingTable::getDoesDestinationHaveRoutes(int routingTableIndex) {
	for (int i = 0; i < MAX_CENTRAL_CONNECTIONS + 2; i++) {
		if (_routingTable[routingTableIndex].routeExists[i]) { return true; }
	}
	return false;
}

boolean RoutingTable::getDoesRouteExist(int routingTableIndex, int peerBleDeviceIndex) {
	return (_routingTable[routingTableIndex].routeExists[peerBleDeviceIndex]);
}


void RoutingTable::invalidatePeerBleDeviceRoutes(int peerBleDeviceIndex) {
	int destinationsInvalidated = 0;
	for (int i = 0; i < _routingTableSize; i++) {
		_routingTable[i].routeExists[peerBleDeviceIndex] = false;

		boolean destinationHasRoute = false;
		for (int j = 0; j < MAX_CENTRAL_CONNECTIONS + 2; j ++) {
			if (_routingTable[i].routeExists[j]) { destinationHasRoute = true; break; }
		}

		if (!destinationHasRoute) {
			_routingTable[i].timesUsed = -1;
			destinationsInvalidated++;
			Log.i("Destination name %s removed from routing table; no route exists", _routingTable[i].destinationName);
		}
	}

	Log.i("%d destinations removed from Routing Table", destinationsInvalidated);
	optimizeRoutingTable();
	_routingTableHasBeenChanged = true;
}

void RoutingTable::optimizeRoutingTable() {
	unsigned long t = millis();
	std::vector<RoutingTableStruct> myVector (_routingTable, _routingTable + _routingTableSize);
	std::sort(myVector.begin(), myVector.end(), routingTableEntryComparator);
	Log.i("Sorted %d entries in routingTable in %d milliseconds\n", _routingTableSize, millis() - t);

	int entriesToDelete = 0;
	for (int i = 0; i < _routingTableSize; i++) {
		Log.v("RoutingTableIndex = %d, destinationName = %s, timesUsed = %d", i, _routingTable[i].destinationName, _routingTable[i].timesUsed);
		if (_routingTable[i].timesUsed < 0) { entriesToDelete++; }
		_routingTable[i].timesUsed = 0;
	}
	_routingTableSize -= entriesToDelete;
}

char * RoutingTable::getAvailableRoutesAsCharArray() {
	int totalCharArrayLength = 0;
	for (int i = 0; i < _routingTableSize; i++) { totalCharArrayLength += strlen(_routingTable[i].destinationName) + 1; }

	MessageBuilder mb(totalCharArrayLength + 10);
	mb.append(STRING_ROUTES);
	for (int i = 0; i < _routingTableSize; i++) {
		if (i > 0) { mb.append(","); }
		mb.append(_routingTable[i].destinationName);
	}
	return ((char *) mb.getBuffer());
}


void RoutingTable::setAvailableRoutesFromCharArray(char * availableRoutes, char * peerName) {

	int bleDeviceIndex = getIndexFromNamePointer(peerName);
	if (bleDeviceIndex < 0) {
		Log.e("Error: Cannot update routing table from incoming char Array; device or subscription name %s not found in table", peerName);
		return;
	}

	char routeToAdd[MAX_BLE_DEVICE_NAME_LENGTH + 1];
	char * routeToImportPointer = &availableRoutes[strlen(STRING_ROUTES)];
	char a;
	int routeToAddCounter = 0;
	int routeToAddLength = 0;

	do {
		a = *routeToImportPointer++;
		if (a == ',' || a == '\0')  {
			routeToAdd[routeToAddLength] = '\0';
			getIndexFromName(routeToAdd, bleDeviceIndex);
			routeToAddCounter++;
			routeToAddLength = 0;
		} else {
			routeToAdd[routeToAddLength++] = a;
		}
	} while (a != '\0');
	Log.i("Added from device %s routes: %s (%d total)", peerName, availableRoutes, routeToAddCounter);
}

boolean RoutingTable::getRoutingTableHasBeenChanged() {
	boolean b = _routingTableHasBeenChanged;
	_routingTableHasBeenChanged = false;
	return (b);
}
