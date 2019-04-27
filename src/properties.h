/*
 * properties.h
 *
 *  Created on: Apr 22, 2019
 *      Author: user
 */

#ifndef PROPERTIES_H_
#define PROPERTIES_H_

#include <iostream>
#include "INIReader.h"

using namespace std;

class properties {
private:
    static INIReader reader;
public:
    static unsigned int getUInteger(string section, string property,
            unsigned int default_value = 0) {
        return reader.GetInteger(section, property, default_value);
    }

    static signed short getShort(string section, string property,
            signed short default_value = 0) {
        return reader.GetInteger(section, property, default_value);
    }

    static string getString(string section, string property,
            string default_value = 0) {

        if (reader.ParseError() != 0) {
            std::cout << "Can't load 'minetest.ini'\n";
        }

        return reader.Get(section, property, default_value);
    }

    static double getDouble(string section, string property,
            double default_value = 0) {

        if (reader.ParseError() != 0) {
            std::cout << "Can't load 'minetest.ini'\n";
        }

        return reader.GetReal(section, property, default_value);
    }

    static bool getBool(string section, string property,
            bool default_value = 0) {

        if (reader.ParseError() != 0) {
            std::cout << "Can't load 'minetest.ini'\n";
        }

        return reader.GetBoolean(section, property, default_value);
    }
};

#endif /* PROPERTIES_H_ */
