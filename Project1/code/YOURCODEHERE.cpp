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

 //modulo 15: 15 has Core Cache FPU BP order
#define PSU_ID_SUM (921604930 + 971725829)

/*
 * Some global variables to track heuristic progress.
 * 
 * Feel free to create more global variables to track progress of your
 * heuristic.
 */
unsigned int currentlyExploringDim = 0;
bool currentDimDone = false;
bool isDSEComplete = false;
int configIterationCounter = 0;

/*
 * Given a half-baked configuration containing cache properties, generate
 * latency parameters in configuration string. You will need information about
 * how different cache paramters affect access latency.
 * 
 * Returns a string similar to "1 1 1"
 */
std::string generateCacheLatencyParams(string halfBackedConfig) {

	string latencySettings;

	//
	//YOUR CODE BEGINS HERE
	//
	
	int l1block, dl1sets, dl1assoc, il1sets, il1assoc, ul2assoc, ul2sets, ul2block, il1sizeKB, dl1sizeKB, ul2sizeKB;
	//determines values of fields needed to calculate latency
	for (int fieldnum = 2; fieldnum < 10; ++fieldnum) {
			int j = 2 * fieldnum;
			std::string field = halfBackedConfig.substr(j, 1);
			int fieldvalue = atoi(field.c_str());
			if (fieldnum == 2){
				l1block = pow(2,fieldvalue)*8;
			}
			else if (fieldnum == 3){
				dl1sets = 32*pow(2,fieldvalue);
			}
			else if (fieldnum == 4){
				dl1assoc = pow(2,fieldvalue);
			}
			else if (fieldnum == 5){
				il1sets = 32*pow(2,fieldvalue);
			}
			else if (fieldnum == 6){
				il1assoc = pow(2,fieldvalue);
			}
			else if (fieldnum == 7){
				ul2sets = 256*pow(2,fieldvalue);
			}
			else if(fieldnum == 8){
				ul2block = 16*pow(2,fieldvalue);
			}
			else if(fieldnum == 9){
				ul2assoc = pow(2,fieldvalue);
				il1sizeKB = l1block*il1sets*il1assoc/1024;
				dl1sizeKB = l1block*dl1sets*dl1assoc/1024;
				ul2sizeKB = ul2sets*ul2block*ul2assoc/1024; 
			}
	
	}

	int il1lat = log2(il1sizeKB) + log2(il1assoc) - 1;
	int dl1lat = log2(dl1sizeKB) + log2(dl1assoc) - 1;
	int ul2lat = log2(ul2sizeKB) + log2(ul2assoc) - 5;
	std::stringstream ss;
    ss << dl1lat << " " << il1lat << " " << ul2lat;
	latencySettings = ss.str();
 
	//
	//YOUR CODE ENDS HERE
	//

	return latencySettings;
}

/*
 * Returns 1 if configuration is valid, else 0
 */
int validateConfiguration(std::string configuration) {

	// The below is a necessary, but insufficient condition for validating a
	// configuration.
	if (isNumDimConfiguration(configuration)){
		int l1block, dl1sets, dl1assoc, il1sets, il1assoc, ul2assoc, ul2sets, ul2block, width;
		//taken from project files, iterates through config values that need to be validated
		for (int fieldnum = 0; fieldnum < 10; ++fieldnum) {
			int j = 2 * fieldnum;
			std::string field = configuration.substr(j, 1);
			int fieldvalue = atoi(field.c_str());
			//stores width since needed later
			if (fieldnum == 0){
				width = fieldvalue;
			}
			else if (fieldnum == 2){
				if (width>fieldvalue){
					return 0;
				}
				l1block = pow(2,fieldvalue)*8;
			}
			else if (fieldnum == 3){
				dl1sets = 32*pow(2,fieldvalue);
			}
			else if (fieldnum == 4){
				dl1assoc = pow(2,fieldvalue);
			}
			else if (fieldnum == 5){
				il1sets = 32*pow(2,fieldvalue);
			}
			else if (fieldnum == 6){
				il1assoc = pow(2,fieldvalue);
			}
			else if (fieldnum == 7){
				ul2sets = 256*pow(2,fieldvalue);
			}
			else if(fieldnum == 8){
				ul2block = 16*pow(2,fieldvalue);
				//ul2 blocksize must be atleast double l1blocksize;
				if (ul2block < 2*l1block){
					return 0;
				}
			}
			else if(fieldnum == 9){
				ul2assoc = pow(2,fieldvalue);
				//all values have been converted to bits
				int il1size = l1block*il1sets*il1assoc;
				int dl1size = l1block*dl1sets*dl1assoc;
				int ul2size = ul2sets*ul2block*ul2assoc;
				//size contstraints check;
			    if(ul2size < 2 *(dl1size+il1size) || il1size < 2048 || il1size > 65536 ||
				dl1size < 2048 || dl1size > 65536 || ul2size < 32768 || ul2size > 1024000){
					return 0;
				}
				else{
					return 1;
				}
			}
			
		}
	}
	return 0;
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
	while (!validateConfiguration(nextconfiguration) ||
		GLOB_seen_configurations[nextconfiguration]) {

		// Check if DSE has been completed before and return current
		// configuration.
		if(isDSEComplete) {
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
		for (int dim = 0; dim < currentlyExploringDim; ++dim) {
			ss << extractConfigPararm(bestConfig, dim) << " ";
		}

		// Handling for currently exploring dimension. This is a very dumb
		// implementation.
		int nextValue = extractConfigPararm(nextconfiguration,
				currentlyExploringDim) + 1;
		configIterationCounter+=1;

		if (/*nextValue >= GLOB_dimensioncardinality[currentlyExploringDim]*/configIterationCounter >= GLOB_dimensioncardinality[currentlyExploringDim]) {
			nextValue = GLOB_dimensioncardinality[currentlyExploringDim] - 1;
			configIterationCounter = 0;
			currentDimDone = true;
		}

		else if(nextValue >= GLOB_dimensioncardinality[currentlyExploringDim]){
			nextValue = 0;
		}

		ss << nextValue << " ";

		// Fill in remaining independent params with 0.
		for (int dim = (currentlyExploringDim + 1);
				dim < (NUM_DIMS - NUM_DIMS_DEPENDENT); ++dim) {
			ss << "0 "; //NEEDS TO FILL WITH BASELINE CONFIG
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
		if (currentDimDone) {
			currentlyExploringDim++;
			currentDimDone = false;
		}

		// Signal that DSE is complete after this configuration.
		if (currentlyExploringDim == (NUM_DIMS - NUM_DIMS_DEPENDENT))
			isDSEComplete = true;
	}
	return nextconfiguration;
}

