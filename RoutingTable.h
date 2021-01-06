#ifndef RoutingTable_h
#define RoutingTable_h

#include "Common/CommonDefinitions.h"
#include "Utility/Logger.h"
#include "Utility/MessageBuilder.h"
#include "Arduino.h"
#include <stdarg.h>
#include <algorithm>															// sort
#include <vector>																// vector

#define MAX_LOOKUPS_BEFORE_OPTIMIZATION				100


struct RoutingTableStruct {
	char destinationName[MAX_BLE_DEVICE_NAME_LENGTH + 1];
	boolean routeExists[MAX_CENTRAL_CONNECTIONS + 2];
	int timesUsed;
};



class RoutingTable {

public:

	char * getNamePointerFromName(char * destinationName);
	char * getNamePointerFromName(char * destinationName, int peerBleDeviceIndex);

	void initialize(int routingTableCapacity);

	int getIndexFromName(char * destinationName);
	int getIndexFromName(char * destinationName, int peerBleDeviceIndex);

	int getIndexFromNamePointer(char * destinationName);
	int getIndexFromNamePointer(char * destinationName, int peerBleDeviceIndex);

	boolean getDoesRouteExist(int routingTableIndex, int peerBleDeviceIndex);
	boolean getDoesDestinationHaveRoutes(int routingTableIndex);

	void invalidatePeerBleDeviceRoutes(int peerBleDeviceIndex);
	void optimizeRoutingTable();

	char * getAvailableRoutesAsCharArray();
	void setAvailableRoutesFromCharArray(char * availableRoutes, char * peerName);

	void setRouteForDestination(int routingTableIndex, int peerBleDeviceIndex, boolean b);

	boolean getRoutingTableHasBeenChanged();

private:

	Logger Log;
	RoutingTableStruct * _routingTable;
	int _routingTableCapacity;
	int _routingTableSize;
	int _numberOfLookups;
	boolean _routingTableHasBeenChanged;

	int _sendUpstreamIndex;
	int _sendToAllIndex;

	int addDestinationNameToRoutingTable(char * name);


};


#endif
