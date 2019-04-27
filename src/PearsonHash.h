/*
 * PearsonHash.h
 *
 *  Created on: Apr 16, 2019
 *      Author: user
 */

#ifndef PEARSONHASH_H_
#define PEARSONHASH_H_

// Required for C++ integer types
#include <cstdint>
// Required for std::string
#include <string>

extern uint8_t pearsonLookupTable[256];

// i.e., unsigned char
uint8_t pearsonHash(std::string message, uint8_t* lookupTable);

// i.e., unsigned short
uint16_t pearsonHash16(std::string message, uint8_t* lookupTable = pearsonLookupTable);

#endif /* PEARSONHASH_H_ */
