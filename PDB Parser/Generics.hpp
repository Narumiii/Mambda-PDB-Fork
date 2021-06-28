#pragma once

#include <Windows.h>
#include <iostream>
#define Log(format, ...) printf_s("[ %-20s ] "##format##"\n", __FUNCTION__, __VA_ARGS__);
