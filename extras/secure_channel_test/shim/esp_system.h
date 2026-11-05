#ifndef JL_SHIM_ESP_SYSTEM_H
#define JL_SHIM_ESP_SYSTEM_H

// Exits the test server with a code that tells test_secure_channel.py that it would have rebooted.
[[noreturn]] void esp_restart();

#endif  // JL_SHIM_ESP_SYSTEM_H
