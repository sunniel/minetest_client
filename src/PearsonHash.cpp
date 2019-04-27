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

#include "PearsonHash.h"

uint8_t pearsonLookupTable[256] = { 98, 6, 85, 150, 36, 23, 112, 164, 135, 207,
        169, 5, 26, 64, 165, 219, 61, 20, 68, 89, 130, 63, 52, 102, 24, 229,
        132, 245, 80, 216, 195, 115, 90, 168, 156, 203, 177, 120, 2, 190, 188,
        7, 100, 185, 174, 243, 162, 10, 237, 18, 253, 225, 8, 208, 172, 244,
        255, 126, 101, 79, 145, 235, 228, 121, 123, 251, 67, 250, 161, 0, 107,
        97, 241, 111, 181, 82, 249, 33, 69, 55, 59, 153, 29, 9, 213, 167, 84,
        93, 30, 46, 94, 75, 151, 114, 73, 222, 197, 96, 210, 45, 16, 227, 248,
        202, 51, 152, 252, 125, 81, 206, 215, 186, 39, 158, 178, 187, 131, 136,
        1, 49, 50, 17, 141, 91, 47, 129, 60, 99, 154, 35, 86, 171, 105, 34, 38,
        200, 147, 58, 77, 118, 173, 246, 76, 254, 133, 232, 196, 144, 98, 124,
        53, 4, 108, 74, 223, 234, 134, 230, 157, 139, 189, 205, 199, 128, 176,
        19, 211, 236, 127, 192, 231, 70, 233, 88, 146, 44, 183, 201, 22, 83, 13,
        214, 116, 109, 159, 32, 95, 226, 140, 220, 57, 12, 221, 31, 209, 182,
        143, 92, 149, 184, 148, 62, 113, 65, 37, 27, 106, 166, 3, 14, 204, 72,
        21, 41, 56, 66, 28, 193, 40, 217, 25, 54, 179, 117, 238, 87, 240, 155,
        180, 170, 242, 212, 191, 163, 78, 218, 137, 194, 175, 110, 43, 119, 224,
        71, 122, 142, 42, 160, 104, 48, 247, 103, 15, 11, 138, 239 };

// i.e., unsigned char
uint8_t pearsonHash(std::string message, uint8_t* lookupTable) {
    uint8_t result = 0;
    for (auto byte : message) {
        result = lookupTable[result ^ byte];
    }
    return result;
}

// i.e., unsigned short
uint16_t pearsonHash16(std::string message, uint8_t* lookupTable) {
    uint16_t result = 0;
    uint8_t correctedFirstMessageByte = message.at(0);
    for (int iteration = 0; iteration < 2; ++iteration) {
        uint8_t iterationResult = pearsonHash(message, lookupTable);
        result += (iterationResult << (iteration << 3));
        message.at(0) += 1;
    }
    message.at(0) = correctedFirstMessageByte;
    return result;
}

#endif /* PEARSONHASH_H_ */
