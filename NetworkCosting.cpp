#include "BleStar.h"

/*
void BleStar::appendCentralModeConnectionPreferences(char * pc, unsigned long duration, int preferenceWeighting) {

	// clean up any old connection preferences that have expired/duration has gone away
	for (int i = 0; i < numberOfConnectionPreferences; i++) {
		if (millis() > connectionPreferences[i].duration + connectionPreferences[i].timestamp) {
			for (int j = i; j < numberOfConnectionPreferences; j++) {
				connectionPreferences[j].duration = connectionPreferences[j+1].duration;
				connectionPreferences[j].timestamp = connectionPreferences[j+1].timestamp;
				connectionPreferences[j].preferredConnectionName = connectionPreferences[j+1].preferredConnectionName;
				connectionPreferences[j].weighting = connectionPreferences[j+1].weighting;
			}
			numberOfConnectionPreferences--;
		}
	}

	connectionPreferences[numberOfConnectionPreferences].duration = duration;
	connectionPreferences[numberOfConnectionPreferences].timestamp = millis();
	connectionPreferences[numberOfConnectionPreferences].preferredConnectionName = pc;
	connectionPreferences[numberOfConnectionPreferences].weighting = preferenceWeighting;
	numberOfConnectionPreferences++;
}
*/
