#include "readfile.h"

// Function to read file content into a String
String readFileToString(const char *path)
{
    File file = SPIFFS.open(path, FILE_READ);
    if (!file || file.isDirectory())
    {
        Serial.println("Failed to open file for reading");
        return String();
    }

    String content;
    while (file.available())
    {
        content += (char)file.read();
    }
    file.close();
    return content;
}

bool readFileInit()
{
    return SPIFFS.begin(true);
}