#pragma once
#include <stdbool.h>
#include <stdio.h>

extern bool debug_enabled;

#define DEBUG_PRINT(...) do { if (debug_enabled) printf(__VA_ARGS__); } while (0)