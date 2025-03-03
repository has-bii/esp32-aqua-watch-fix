#ifndef USERCONFIG_H
#define USERCONFIG_H

#include <Arduino.h>

class UserConfig
{
public:
    String email;
    String password;
    String access_token;
    String refresh_token;

public:
    // Constructor
    UserConfig();
};

#endif
