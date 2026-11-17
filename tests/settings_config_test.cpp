#include <cassert>
#include <cstring>
#include <iostream>
#include "settings_config.h"

int main() {
  using namespace dashboard;
  Settings settings;
  assert(validate(settings)); // Blank SSID is never confirmed.
  strcpy(settings.ssid, "Test network");
  assert(!validate(settings)); // Explicitly open network.
  strcpy(settings.password, "test-only-password");
  assert(!validate(settings));
  strcpy(settings.password, "short");
  assert(validate(settings));
  memset(settings.password, 'a', 64); settings.password[64] = 0;
  assert(!validate(settings)); // WPA hex key.
  settings.password[3] = 'z';
  assert(validate(settings));
  strcpy(settings.password, "test-only-password");
  settings.intervalMinutes = 4; assert(validate(settings));
  settings.intervalMinutes = 5; assert(!validate(settings));
  settings.intervalMinutes = 1440; assert(!validate(settings));
  settings.intervalMinutes = 1441; assert(validate(settings));
  settings.intervalMinutes = 15;
  settings.dark = false;
  memset(settings.ssid, 's', 32); settings.ssid[32] = 0;
  assert(!validate(settings));
  settings.ssid[32] = 's'; assert(validate(settings));
  strcpy(settings.ssid, "Test\nnetwork"); assert(validate(settings));
  strcpy(settings.ssid, "Test network");
  uint8_t encoded[kRecordSize];
  encode(settings, encoded);
  Settings restored;
  assert(decode(encoded, sizeof(encoded), restored));
  assert(!strcmp(restored.ssid, settings.ssid));
  assert(!strcmp(restored.password, settings.password));
  assert(restored.intervalMinutes == 15 && !restored.dark);
  assert(!decode(encoded, sizeof(encoded)-1, restored));
  // Every single-byte corruption must fail without replacing the caller's value.
  for (size_t i = 0; i < sizeof(encoded); ++i) {
    encoded[i] ^= 0x40;
    assert(!decode(encoded, sizeof(encoded), restored));
    assert(!strcmp(restored.ssid, settings.ssid));
    encoded[i] ^= 0x40;
  }
  encode(settings, encoded); encoded[4] = 2;
  assert(!decode(encoded, sizeof(encoded), restored));
  assert(!restartDue(false, 6000, 1000, false));
  assert(!restartDue(true, 2999, 1000, false));
  assert(!restartDue(true, 9000, 1000, true)); // Finish the Google/display cycle first.
  assert(restartDue(true, 3000, 1000, false));
  assert(restartDue(true, 1500, UINT32_MAX-999, false));
  assert(!elapsed(10, UINT32_MAX-9, 21));
  assert(elapsed(10, UINT32_MAX-9, 20));
  Settings candidate;
  assert(!fromForm(settings, settings.ssid, "", false, true, 30, candidate));
  assert(!strcmp(candidate.password, settings.password) && candidate.dark && candidate.intervalMinutes == 30);
  assert(fromForm(settings, "Other network", "", false, true, 30, candidate));
  assert(!strcmp(candidate.ssid, settings.ssid)); // Failed form did not partially replace output.
  assert(!fromForm(settings, "Other network", "", true, false, 60, candidate));
  assert(!candidate.password[0] && !strcmp(candidate.ssid, "Other network"));
  assert(fromForm(settings, "", "", true, true, 15, candidate));
  bool confirmed = true, saved = false;
  Settings active = settings;
  assert(!confirmCandidate(candidate, active, confirmed,
    [](const Settings&) { return false; }, [&saved](const Settings&) { saved = true; return true; }));
  assert(!saved && !strcmp(active.ssid, settings.ssid) && confirmed);
  assert(!confirmCandidate(candidate, active, confirmed,
    [](const Settings&) { return true; }, [](const Settings&) { return false; }));
  assert(!strcmp(active.ssid, settings.ssid));
  confirmed = false;
  assert(confirmCandidate(candidate, active, confirmed,
    [](const Settings&) { return true; }, [](const Settings&) { return true; }));
  assert(confirmed && !strcmp(active.ssid, candidate.ssid));
  std::cout << "PASS: validation, record integrity, form/password handling, Wi-Fi confirmation/rollback, deferred restart, timer rollover\n";
}
