#ifndef READFILE_H
#define READFILE_H

#include <Arduino.h>
#include <SPIFFS.h>

bool readFileInit();

String readFileToString(const char *path);

#endif
