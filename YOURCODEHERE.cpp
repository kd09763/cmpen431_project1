#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <math.h>
#include <fcntl.h>
#include <vector>
#include <iterator>
 
#include "431project.h"

using namespace std;

/*
 * Enter your PSU IDs here to select the appropriate scanning order.
 */
#define PSU_ID_SUM (912345679+911111111)

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */
bool newDim = true;
unsigned int currentlyExploringDim = 0;
bool currentDimDone = false;
bool isDSEComplete = false;
bool traversalList[15] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
bool finishedState[15] = { true, true, true, true, true, true, true, true, true, true, true, true, true, true, true };
int dimesionOrderMap[15] = { 2,3,4,5,6,7,8,9,10,12,13,14,11,0,1 }; // For Cache -> BP -> FPU -> CORE

// all sizes in bytes
unsigned int getdl1size2(std::string configuration) {
	unsigned int dl1sets = 32 << extractConfigPararm(configuration, 3);
	unsigned int dl1assoc = 1 << extractConfigPararm(configuration, 4);
	unsigned int dl1blocksize = 8
			* (1 << extractConfigPararm(configuration, 2));
	return dl1assoc * dl1sets * dl1blocksize;
}

unsigned int getil1size2(std::string configuration) {
	unsigned int il1sets = 32 << extractConfigPararm(configuration, 5);
	unsigned int il1assoc = 1 << extractConfigPararm(configuration, 6);
	unsigned int il1blocksize = 8
			* (1 << extractConfigPararm(configuration, 2));
	return il1assoc * il1sets * il1blocksize;
}

unsigned int getl2size2(std::string configuration) {
	unsigned int l2sets = 256 << extractConfigPararm(configuration, 7);
	unsigned int l2blocksize = 16 << extractConfigPararm(configuration, 8);
	unsigned int l2assoc = 1 << extractConfigPararm(configuration, 9);
	return l2assoc * l2sets * l2blocksize;
}

/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBakedConfig) {

	std::string latencySettings;
	int il1Lat;
	int dl1Lat;
	int ul2Lat;

	string string1;
	string string2;
	string string3;


	int IL1CacheSize = getil1size2(halfBakedConfig);
	int DL1CacheSize = getdl1size2(halfBakedConfig);
	int UL2CacheSize = getl2size2(halfBakedConfig);

	int il1Assoc = extractConfigPararm(halfBakedConfig, 6);
	int dl1Assoc = extractConfigPararm(halfBakedConfig, 4);
	int ul2Assoc = extractConfigPararm(halfBakedConfig, 9);

	switch(il1Assoc) {
		case 0: 
			il1Lat += 0;
		case 1: 
			il1Lat += 1;
		case 2: 
			il1Lat += 2;
	}

	switch(dl1Assoc) {
		case 0: 
			il1Lat += 0;
		case 1: 
			il1Lat += 1;
		case 2: 
			il1Lat += 2;
	}

	switch(il1Assoc) {
		case 0: 
			il1Lat += 0;
		case 1: 
			il1Lat += 1;
		case 2: 
			il1Lat += 2;
		case 3: 
			il1Lat += 3;
		case 4: 
			il1Lat += 4;
	}

	switch(IL1CacheSize) {
		case 2: 
			il1Lat += 1;
		case 4: 
			il1Lat += 2;
		case 8: 
			il1Lat += 3;
		case 16: 
			il1Lat += 4;
		case 32: 
			il1Lat += 5;
		case 64: 
			il1Lat += 6;
	}

	switch(DL1CacheSize) {
		case 0: 
			il1Lat += 0;
		case 1: 
			il1Lat += 1;
		case 2: 
			il1Lat += 2;
		case 3: 
			il1Lat += 3;
		case 4: 
			il1Lat += 4;
	}

	switch(UL2CacheSize) {
		case 0: 
			il1Lat += 0;
		case 1: 
			il1Lat += 1;
		case 2: 
			il1Lat += 2;
		case 3: 
			il1Lat += 3;
		case 4: 
			il1Lat += 4;
		case 5: 
			il1Lat += 5;
	}

	stringstream ss;
	ss << il1Lat;
	ss >> string1;
	ss << dl1Lat;
	ss >> string2;
	ss << ul2Lat;
	ss >> string3;


	// This is a dumb implementation.
	latencySettings = string1 + " " + string2 + " " + string3;
	cout << string1;
	//latencySettings = "1 1 1";

	return latencySettings;
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {
	int valid = 1;
	int ifq = extractConfigPararm(configuration, 0);
	int il1BlockSize = extractConfigPararm(configuration, 2);
	int ul2BlockSize = extractConfigPararm(configuration, 8);
	int IL1CacheSize = getil1size2(configuration);
	int DL1CacheSize = getdl1size2(configuration);
	int UL2CacheSize = getl2size2(configuration);

	bool check1 = il1BlockSize >= ifq;

	bool check2 = (2*ul2BlockSize) >= il1BlockSize;

	bool check3 = 2048 <= IL1CacheSize <= 65536;

	bool check4 = 2048 <= DL1CacheSize <= 65536;

	bool check5 = 32768 <= UL2CacheSize <= 1048576;
	
	return(isNumDimConfiguration(configuration) && check1 && check2 && check3 && check4);
}

/*
 * Given the current best known configuration, the current configuration,
 * and the globally visible map of all previously investigated configurations,
 * suggest a previously unexplored design point. You will only be allowed to
 * investigate 1000 design points in a particular run, so choose wisely.
 *
 * In the current implementation, we start from the leftmost dimension and
 * explore all possible options for this dimension and then go to the next
 * dimension until the rightmost dimension.
 */
std::string generateNextConfigurationProposal(std::string currentconfiguration,
		std::string bestEXECconfiguration, std::string bestEDPconfiguration,
		int optimizeforEXEC, int optimizeforEDP) {

	//
	// Some interesting variables in 431project.h include:
	//
	// 1. GLOB_dimensioncardinality
	// 2. GLOB_baseline
	// 3. NUM_DIMS
	// 4. NUM_DIMS_DEPENDENT
	// 5. GLOB_seen_configurations

	std::string nextconfiguration = currentconfiguration;
	// Continue if proposed configuration is invalid or has been seen/checked before.
	while (!validateConfiguration(nextconfiguration) || GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete == true) {
			return currentconfiguration;
		}

		std::stringstream ss;

		string bestConfig;
		if (optimizeforEXEC == 1)
			bestConfig = bestEXECconfiguration;

		if (optimizeforEDP == 1)
			bestConfig = bestEDPconfiguration;

		// Fill in the dimensions already-scanned with the already-selected best
		// value.
		for (int dim = 0; dim < dimesionOrderMap[currentlyExploringDim]; ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}

		// Handling for currently exploring dimension. This is a very dumb
		// implementation.
		int nextValue;
		if(newDim == true){
			nextValue = 0;
			newDim = false;
		}
		else{
			nextValue = extractConfigPararm(nextconfiguration, dimesionOrderMap[currentlyExploringDim]) + 1;
		}		



		if (nextValue >= GLOB_dimensioncardinality[dimesionOrderMap[currentlyExploringDim]]) {
			nextValue = GLOB_dimensioncardinality[dimesionOrderMap[currentlyExploringDim]] - 1;
			//currentDimDone = true;
			traversalList[dimesionOrderMap[currentlyExploringDim]] = true;
			newDim = true;
		}

		ss << nextValue << " ";

		// // Fill in remaining independent params with 0.
		// for (int dim = (currentlyExploringDim + 1);
		// 		dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
		// 	ss << "0 ";
		// }

		// Fill in the dimensions already-scanned with the already-selected best value.
		for (int dim = dimesionOrderMap[currentlyExploringDim] + 1; dim < NUM_DIMS - NUM_DIMS_DEPENDENT; ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}

		//
		// Last NUM_DIMS_DEPENDENT3 configuration parameters are not independent.
		// They depend on one or more parameters already set. Determine the
		// remaining parameters based on already decided independent ones.
		//
		string configSoFar = ss.str();

		// Populate this object using corresponding parameters from config.
		ss << generateCacheLatencyParams(configSoFar);

		// Configuration is ready now.
		nextconfiguration = ss.str();

		// Make sure we start exploring next dimension in next iteration.
		if (traversalList[dimesionOrderMap[currentlyExploringDim]]) {
		 	currentlyExploringDim++;
		 }

		isDSEComplete = true;
		for(int i = 0; i < 15; i++){
			if(isDSEComplete == true && traversalList[i] != finishedState[i]){
				isDSEComplete = false;
			}
		}
	}
	return nextconfiguration;
}

