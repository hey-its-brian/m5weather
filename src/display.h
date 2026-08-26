#pragma once

#include <Arduino.h>

// Initialize the e-ink panel (call after M5.begin()).
void displayInit();

// Full weather dashboard render. Uses the current `config` and `weather`.
void renderWeather();

// Simple centered status screen (setup instructions, errors, boot progress).
// `lines` is a newline-separated string.
void renderStatus(const String &title, const String &lines);
