# Troubleshooting

[Back to README](../README.md) · [First setup](getting-started.md) · [Wiring](hardware.md)

Start with Serial Monitor at **115200 baud** and the settings page. They report
Google requests and display refreshes separately. An image remaining on e-paper
after power loss is normal and does not prove that the firmware is running.

## Compilation or uploading fails

| Symptom | Check |
| --- | --- |
| `Sketch too big` / `text section exceeds available space` | Select **Huge APP (3MB No OTA/1MB SPIFFS)**, 4 MB flash, and ESP32C3 Dev Module |
| Missing core headers or incompatible methods | Use **esp32 by Espressif Systems 3.3.12**; this project needs no additional Arduino libraries |
| `lookup … no such host` while installing a package | The computer could not resolve the download host. Check its Internet/DNS connection and retry; this is not an ESP32 flash error |
| `Failed to connect` / `No serial data received` | Check the selected port, data cable, direct USB connection, and manual BOOT/RST upload procedure |
| MD5/hash verification mismatch | Upload failed. Try a direct USB connection and lower upload speed, verify power, and repeat before diagnosing application behavior |
| `No changed sectors found` followed by verification | The identical data is already in flash; verification succeeded |

For manual download mode: close Serial Monitor, hold BOOT, press and release
RST, then release BOOT. Select the enumerated port and upload. Press RST without
BOOT afterward to run the sketch. See [uploading](getting-started.md#4-upload).

## Serial Monitor is empty

1. Confirm the last upload actually succeeded.
2. Confirm **USB CDC On Boot: Enabled**, the current port, and **115200 baud**.
3. Close other tools reading that port. USB can re-enumerate on reset; reselect
   the port if its name changes.
4. Press RST without BOOT and wait for startup. The app has a two-second initial
   delay, and unavailable saved Wi-Fi profiles can add connection delays.
5. If the device is still silent, test it with only USB connected and inspect
   the unpowered wiring before reconnecting the display.

A reported flash size or small sketch size does not prove the application has
started. Repeated full-flash erases also remove configuration and are not needed
for a normal upload.

## The settings page will not open

- On first setup, join **seo-dashboard-setup** with password `seodashboard`.
  Stay connected when the phone warns that there is no Internet.
- Open **http://192.168.4.1/** explicitly, including `http://`. The browser
  login is **admin / seodashboard**.
- If an iPhone's Wi-Fi login sheet does not offer authentication, open Safari.
- On your home network, use **http://seo-dashboard.local/** or the IP following
  `SETTINGS:` in Serial Monitor. The phone/computer must be able to reach that
  device on the local network. Try the IP if mDNS or guest-network isolation
  prevents `.local` access.
- To reopen setup over USB, send **setup** plus a newline. It is handled after
  the current update. BOOT changes pages; it is not a setup shortcut.

While setup is active, the display keeps the setup instructions and dashboard
polling is paused. Leave the hotspot after saving; see
[hotspot closing behavior](getting-started.md#7-open-settings-again).

## The page is unstyled, stuck, or Save is unavailable

The firmware serves one local HTML document with inline CSS and JavaScript;
it needs no Internet/CDN assets. Reload after a device restart and open the
address in a full browser rather than a captive-portal sheet.

Save stays disabled until the page has loaded the current configuration. A tab
from a previous boot has an expired CSRF token and must be reloaded. If you
edited the web source, regenerate `settings_assets.h` as described in
[UI development](development.md#editing-the-settings-page), then rebuild/upload.

## Settings seem not to save

Wait for the success message and restart, reconnect, then reload. Expected
Serial messages include:

```text
SETTINGS SAVE: authenticated request received
SETTINGS SAVE: pending configuration written to NVS; restart scheduled
SETTINGS APPLY: configuration saved as active
```

`authentication required; nothing written` means that particular request did
not save. A browser can retry after its authentication challenge; check the
subsequent messages rather than just the first line.

A changed Wi-Fi list is confirmed at boot. If none of its networks connects,
the entire pending configuration is rejected and the old one remains. Check
SSID spelling, 2.4 GHz availability, and the password. Google access is checked
later and does not decide Wi-Fi confirmation.

Blank password, secret, refresh-token, and API-key inputs after reload are
intentional. Their presence indicators show whether values are saved. Client
IDs and resources are returned for editing. Also check for a form validation
error, a 4096-byte total-size error, or an NVS write error in the page/Serial log.

## Google requests fail

| Status or safe error identifier | Meaning and next step |
| --- | --- |
| `WIFI ERROR` | No active home-network connection; check saved profiles and signal |
| `CLOCK ERROR` | Network time could not be established; check Internet access and NTP reachability |
| `AUTH REQUIRED`, `invalid_grant`, `invalid_client` | Check the matching client ID/secret/token; reauthorize if expired or revoked |
| `ACCESS DENIED` / HTTP 403 | Check report-account permissions, the enabled API, OAuth scope, and key restrictions |
| `SERVICE_DISABLED`, `accessNotConfigured` | Enable the API in the project belonging to the actual OAuth client or key |
| `ACCESS_TOKEN_SCOPE_INSUFFICIENT`, `insufficientPermissions` | Reauthorize with that service's required read-only scope |
| `ACCOUNT ERROR` | AdSense discovery did not return exactly one supported account |
| Negative HTTP code / `HTTPS ERROR` | Transport/TLS failure; inspect Wi-Fi, network time, and the accompanying diagnostic |
| `API WARNING` | The response included a warning, sampling, thresholding, or data loss the firmware does not silently accept |
| `JSON ERROR`, `TRANSFER ERROR`, `RESPONSE TOO LARGE` | The response was malformed, incomplete, or exceeded its limit; the previous report is retained |
| `NO DATA`, `DATE MISMATCH`, `CWV DATE ERROR` | The report had no usable data or inconsistent dates; inspect the selected resource and period |

Verify the exact GA4 numeric Property ID and GSC property identifier. Google
Cloud project membership does not itself grant access to Analytics or Search
Console. If different services use different accounts, reauthorize each with
the correct account. See [Google setup](google-setup.md).

External OAuth apps in Testing issue seven-day refresh tokens for these scopes.
See [Production and token lifetime](google-setup.md#testing-production-and-token-lifetime).

## CWV says FAIL or NO DATA

`FAIL` is a site-performance assessment, not an authorization error. A successful
request can show `LIVE` and `FAIL` together. Open the CWV page to see which p75
metric is `NEEDS WORK` or `POOR`.

`NO DATA` can be a valid CrUX result for insufficient mobile-origin coverage.
Use an origin such as `https://example.com`; it does not represent every
subdomain or a single page. CrUX has its own collection window and normally
updates daily, so its data-end date can lag the current date. An API HTTP 404
for CrUX is handled as a valid no-field-data result by this firmware.

## The display does not change

`LIVE` reports Google request health. In **Display** on the settings page, look
for **READY** and its last confirmed refresh. In Serial Monitor, look for
`DISPLAY REFRESH CONFIRMED` rather than just `INIT RETURNED` or `IMAGE WRITTEN`.
SPI writes have no controller readback.

- `REFRESH_NOT_STARTED` means the driver did not observe BUSY starting. Check
  the exact panel driver, SPI signals, common ground, display RST to GPIO2,
  and BUSY to GPIO1. Disconnect power before touching wiring or the ribbon.
- The firmware makes at most one guarded SPI retry for that condition. It
  skips the display sleep command after an unconfirmed refresh.
- `READY` means BUSY HIGH → LOW was observed, not that pixels were read back.
  Compare the physical screen with the expected page/time as well.
- Page changes and theme/orientation changes take a full refresh. A click during
  API requests or a refresh is applied when the current blocking work finishes.

E-paper retains its previous image without power. A stale image alone does not
identify whether the cause is firmware startup, networking, or display transfer.

## Reporting a problem

Include the ESP32 board/display revision, Arduino core version, selected board
options, what you expected, and the relevant startup/request/display logs.
For enclosure problems include the part filename, print settings, measurements,
and a photograph. Remove tokens, keys, passwords, personal account details,
and private network names before posting. Do not attach `secrets.h`, OAuth JSON,
private credential files, or firmware binaries containing credentials.
