#pragma once

// Starts the HTTP config server (and captive-portal DNS when in AP mode).
void webServerStart(bool captivePortal);

// Call from loop(). Handles HTTP + DNS traffic.
void webServerLoop();
