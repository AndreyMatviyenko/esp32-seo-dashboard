# ESP32 SEO Dashboard

A standalone e-paper dashboard for Google AdSense, Google Analytics 4,
Google Search Console, and mobile Core Web Vitals.

It connects to Google APIs directly from an ESP32-C3 and refreshes a Waveshare 2.9-inch
black-and-white display. The current layout has a black background and white
text by default. A local browser page configures Wi-Fi, display theme, the refresh interval,
and separate Google credentials and resources for each service.

## Current metrics

| Service | Displayed data |
| --- | --- |
| AdSense | Overview: estimated earnings today/month. Detail: yesterday, last 7 completed days, unpaid balance, latest payment and date |
| GA4 | Overview: live/new users. Detail: live users, new/total users, sessions, views, and engagement rate |
| Search Console | Overview: clicks. Detail: clicks, impressions, CTR, and average position over 28 completed Pacific days, including preliminary data |
| Core Web Vitals | Mobile-origin CrUX assessment, LCP/INP/CLS p75, and collection dates |

The default polling interval is 15 minutes after the previous attempt finishes;
it can be set to 5-1440 minutes in the settings page.
CrUX is fetched once per 24 hours after a successful request; failed requests
are retried on subsequent dashboard cycles. The header shows the screen-update
time in Kyiv, rather than a continuously ticking clock. Cached values are kept
across resets and retained when a request fails.

If Wi-Fi disconnects, configured services switch to `WIFI ERROR` and their
cached values gain `*`, including when BOOT redraws a detail page. The header
time still means frame-render time, not the age of the cached report. A
reconnection schedules a fresh polling cycle before services return to `LIVE`.
CrUX may also be fetched again after a connection failure, before its normal
24-hour interval expires.

Every service is optional. Disabled or unconfigured services make no requests
and occupy no space on the display. Enable AdSense, GA4, Search Console, and
CrUX independently in Settings. Enabled services require complete credentials;
a partially filled connection can be saved while disabled.

The screen arranges 1-3 metrics in full-width rows and 4-6 metrics in two
columns, with the final odd metric spanning the row. Values use the largest
font that fits. Temporary API failures keep the same positions and prior
values, marked with `*`; a service with no successful result shows `--`.
A real reported zero remains `0`. The header is `LIVE` when all enabled
services succeed, `PARTIAL` when some succeed, `ERROR` when all fail, and
`SETUP` when none are enabled. CWV `NO DATA` means CrUX has insufficient data,
not a failed network request. GSC uses preliminary data (`dataState: all`).

### BOOT button navigation

The built-in **BOOT button (GPIO9)** needs no additional wiring:

- Short press/release: **Overview → AdSense → GA4 → GSC → CWV → Overview**.
- Hold for **1.2 seconds**: return to Overview.
- Disabled/unconfigured services are skipped. Every boot starts on Overview;
  automatic updates keep the selected page.

Navigation redraws cached values and makes no Google requests. Button edges are
captured during blocking TLS requests and e-paper refreshes, then debounced in
the Arduino task. Several pending clicks select the final page with one redraw.
A click during an ongoing refresh takes effect after that refresh completes;
full e-paper refreshes still take roughly two seconds. A bounded edge-buffer
overflow drops an ambiguous burst and logs it instead of inventing gestures.
Navigation is suspended while the setup hotspot is active so its instructions
and password stay visible. Do not hold BOOT while resetting/powering the board:
GPIO9 is also the ESP32-C3 ROM download-mode strap.

AdSense detail uses account-time-zone reports: today, yesterday, last seven
completed days (excluding today), and month to date. Payments are obtained from
the AdSense payments endpoint with the same read-only OAuth scope; no new scope
is required. Unpaid balance is separate from estimated month earnings. The most
recent dated AdSense payment is selected by date, irrespective of response order;
YouTube balances/payments are excluded. No available payment is shown as `--`.
Formatted payment currencies/separators are preserved; common currency symbols
are mapped to ASCII currency codes for the display fonts. Unsupported formatting
is rejected rather than shown with a guessed amount/currency.

History and payments update independently of today's/month's earnings. A failed
detail group retains its values with `*` and its own last-success time; its page
shows `PARTIAL` even when overview earnings are `LIVE`. Percentage comparisons
are not included. Extra history/payment requests run only during regular polls.

GA4 detail has six tiles: active users over 30 minutes, plus new users, total
users, sessions, views, and engagement rate over 28 completed property days,
ending yesterday. One Realtime request and one historical report fetch these
six values; the historical response is matched by metric names and validated
as a group. Engagement rate is converted from a 0–1 fraction to a percentage
with one decimal place. A validated empty report has zero counts and an unknown
engagement rate (`--`), rather than an invented percentage. A failed response
retains the previous complete GA4 snapshot with stale markers. V1/V2 metric
caches keep live/new users; the extra tiles remain `--` until the first successful
expanded fetch, which writes the V3 cache. No new credentials or OAuth scope
are needed, and the overview keeps its existing two GA4 values.

GSC detail shows clicks, impressions, CTR, and average position in a two-by-two
layout, with the exact Pacific date range and preliminary-data note below.
All four values come from the existing property-wide `web` / `dataState: all`
query, so no extra requests or OAuth scopes are needed. CTR is converted from
a 0–1 fraction to a percentage with one decimal place; average position also
uses one decimal place and remains `--` when there are no impressions. An empty
or malformed response retains the previous complete report, marked stale.
V1–V3 caches retain clicks/dates; the other three values stay `--` until a
successful request writes the V4 cache. The overview continues to show clicks.

CWV detail shows mobile-origin LCP in seconds, INP in milliseconds, CLS as
a ratio, and the CrUX collection dates. Missing percentiles remain `--`; PASS
requires all three good p75 values. Older caches retain overview values; CrUX
is fetched once on upgrade to populate detail fields before daily caching resumes.

Each CWV percentile has a `GOOD`, `NEEDS WORK`, or `POOR` label next to its name.
`NEEDS WORK` is the compact display wording for Google's "needs improvement"
category. Missing/invalid percentiles show `NO DATA`, with `--` as the value.
The label and overall assessment share the same thresholds in `cwv_quality.h`:

| Metric | GOOD | NEEDS WORK | POOR |
| --- | --- | --- | --- |
| LCP | ≤2.5 s | >2.5–4 s | >4 s |
| INP | ≤200 ms | >200–500 ms | >500 ms |
| CLS | ≤0.1 | >0.1–0.25 | >0.25 |

Classification uses the original p75 before display rounding. Thresholds follow
[Google's Core Web Vitals definitions](https://web.dev/articles/defining-core-web-vitals-thresholds).
Near a boundary, the display adds decimal places so the number agrees with
its quality label: for example, `2501 ms` becomes `2.501 s / NEEDS WORK`.
Values too close to distinguish within six decimal places use a boundary
notation, such as `>2.50 s`.
A cached label is marked with `*` along with its value after a failed request.
Labels are computed locally, including for existing caches; no extra API calls,
credentials, or cache migration are needed for this change. The overview's
PASS/FAIL assessment and the daily CrUX polling interval remain the same.

Cached metrics are bound to the credentials and resource using a SHA-256
fingerprint. Changing an account, token, property, or origin prevents the old
metrics from being loaded. Each provider's values, timestamp, and fingerprint
are saved together in one versioned NVS record. Existing bound caches migrate
on upgrade; an unbound legacy cache is ignored. Disabling a service keeps its
cache for later use with the same connection. A cache write failure is logged
and leaves the current report available in RAM.

## Hardware

- ESP32-C3 SuperMini.
- Waveshare 2.9-inch e-Paper Module V2 / Rev 2.1, SSD1680, 296 x 128 pixels.
- USB power.

| Waveshare | ESP32-C3 |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| DIN | GPIO6 (MOSI) |
| CLK | GPIO4 (SCK) |
| CS | GPIO7 |
| DC | GPIO3 |
| RST | GPIO2 |
| BUSY | GPIO1 |

Display RST connects to GPIO2, not to the ESP32 reset pin. Disconnect power
before changing the wiring.

## Firmware setup

1. Install the ESP32 board package in Arduino IDE. The baseline uses
   **Espressif Arduino core 3.3.12**.
2. No local credentials file is required. The sketch uses placeholders when
   `secrets.h` is absent; first boot starts the setup hotspot.
3. Obtain OAuth client details and refresh tokens on your computer for the
   services you plan to use. Each Google service can use a different account.
   Enter these values on the device's Settings page after uploading. Google
   authorization itself is completed separately in the computer browser.
   A CrUX API key is needed only for Core Web Vitals.
4. Open [SEO_Dashboard.ino](firmware/SEO_Dashboard/SEO_Dashboard.ino).
   All required display driver and font files are beside the sketch; no
   additional Arduino libraries are required.
5. Select **ESP32C3 Dev Module**, **USB CDC On Boot: Enabled**,
   **Flash Size: 4MB**, **Flash Mode: DIO**, **Flash Frequency: 40MHz**,
   **Partition Scheme: Huge APP (3MB No OTA/1MB SPIFFS)**, and **Core Debug Level: None**.
   Upload, then open Serial Monitor at **115200 baud**.

The web server exceeds the original 1.25 MiB application partition; select
Huge APP for this revision. It does not support OTA firmware updates. The NVS
offset and size match the original default layout, preserving saved settings.
Unbound legacy metric caches are ignored as described above. The
filesystem partition moves, and this project does not
store data in it. No full-flash erase is required for the application change.

Only placeholders are included. Wi-Fi setup and an empty dashboard work without
Google credentials. For an existing installation, a local `secrets.h` copied
from `secrets.example.h` can supply migration defaults. A confirmed browser
configuration takes precedence over these compiled defaults. Existing V1 Wi-Fi
records are retained. Compiled defaults require explicit resource identifiers;
no property, domain, or website is assumed.
`secrets.h`, tokens, and compiled firmware must stay out of Git; these paths
are covered by `.gitignore`.

## Browser settings

1. On the first boot, connect your phone or computer to **seo-dashboard-setup**.
   Its fixed factory password is **`seodashboard`**, also displayed on e-paper.
2. Open **http://192.168.4.1/** if a portal window does not open automatically.
   At the browser login prompt use **`admin`** and password **`seodashboard`**.
   On iPhone, open this address in Safari if the automatic Wi-Fi login window
   does not show the browser authentication dialog.
3. Enter the exact 2.4 GHz Wi-Fi name (SSID) and password. This first revision
   uses manual SSID entry. Set a display title, choose Light or Dark, and set a refresh interval.
   Enable the Google services you need, enter their credentials, and set the
   GA4 Property ID (digits), GSC resource (such as `sc-domain:example.com`),
   and CrUX origin (such as `https://example.com`) as applicable.
4. Click **Save settings**. The device finishes any running Google/display
   cycle before restarting. New settings are staged, then promoted to confirmed
   settings at boot. A new or changed Wi-Fi configuration must connect first;
   a failed confirmation keeps the old settings. If Wi-Fi credentials are
   unchanged, a temporary connection failure does not roll back changes to
   Google connections, theme, or interval. A failed storage write always keeps
   the old settings.
5. Reconnect to your home network and open **http://seo-dashboard.local/** or
   the device IP printed in Serial Monitor. The page remains available during
   Google requests and e-paper refreshes.

For the same protected SSID, a blank password field keeps the existing password.
Changing to a different SSID requires a password, or explicit selection of
**Open network**. The saved home Wi-Fi password is never returned by the
settings API. Client IDs and resource identifiers are returned for editing;
client secrets, refresh tokens, and API keys are represented only by presence
flags. Leave secret fields blank to keep saved values for the same Client ID.
Changing Client ID clears the previous client's secret and token; an enabled
connection requires both matching replacements. **Clear saved credentials**
marks that connection's credentials for deletion and turns it off; deletion is
applied after Save and boot-time application. Turning a service
off alone keeps its credentials. The combined saved configuration is limited
to 4096 bytes to fit the existing NVS partition; the page reports an error
instead of applying an oversized configuration. The public factory password
stays the same across restarts and replaces the previous randomly generated
access code on upgrade. Existing Wi-Fi and Google settings are retained.

The display title defaults to `SEO DASHBOARD` and accepts 1–20 printable ASCII
characters (Latin letters, digits, punctuation, and spaces). Leading/trailing
spaces are trimmed; an empty or all-space title is rejected. The limit is
checked by the page and firmware and fits beside `PARTIAL`, time, and date.
V1/V2 settings records without a title use the default and keep their existing
Wi-Fi and Google configuration; new saves use the V3 format.

To display the setup instructions again or reopen setup, send **setup** followed by
a newline from Serial Monitor. The command is handled after any running update
finishes. The setup screen remains while the hotspot is active. When connected
to the home network, a manually opened hotspot closes after five minutes once
no hotspot client remains. Automatic recovery hotspots close after 30 seconds
of a working home connection once no hotspot client remains. Dashboard polling
resumes after setup closes. A confirmed network that remains unavailable triggers
setup/reconnection attempts at 60-second intervals.

The HTTP handler runs in a separate, lower-priority FreeRTOS task, so blocking
Google calls do not block page processing and the handler cannot preempt the
Arduino task's short display reset pulse. SPI/display access remains in the
Arduino task. Settings use a separate versioned NVS namespace from metric caches.

The page uses Digest authentication, per-boot CSRF tokens, and local assets
without a CDN. The Digest challenge stays stable for one boot so parallel
requests do not invalidate each other's login. Static CSS and JavaScript
contain no configuration and load without separate authentication challenges;
the settings page and settings API still require login. Save is disabled in
the HTML until JavaScript has loaded the configuration successfully. The form
declares POST as a defensive fallback, but saving requires JavaScript and a
JSON API request; credentials never belong in navigation query strings.
An open page detects a device restart, drops its old CSRF token, and locks
editing until the page is reloaded. A rejected CSRF submission also locks Save.
Serial messages report receipt, NVS staging, Wi-Fi confirmation, and rollback
without logging submitted values. HTTP is not encrypted: use it on a trusted local network or the
password-protected setup hotspot, and do not expose it through router port
forwarding. NVS secrets are not encrypted by this firmware. Google login and
per-metric selection are not included in this revision. The page shows each
service's current state; hover over its state to see the last successful request.

Settings requests are bounded before the core WebServer buffers a body:
headers are limited to 4 KiB with a five-second deadline. Only JSON POSTs to
`/api/settings` are accepted, with a single explicit Content-Length of at most
8 KiB; multipart and transfer-encoded requests are rejected. Digest, host, and
CSRF checks precede body allocation. The body is read within a ten-second
total deadline and its buffer is released after handling or aborting it.
The `settings_http` adapter uses the baseline Arduino core 3.3.12 socket and
raw-handler interfaces; review those interfaces when upgrading the core.

The Display section reports hardware refresh status independently of Google
requests: `WAITING`, `UPDATING`, `READY`, or `ERROR`. It identifies whether the
attempt concerns the dashboard or setup screen, shows a diagnostic identifier
on failure, and shows the last confirmed refresh time when the clock is valid.
`READY` means the driver observed BUSY HIGH → LOW; it is not pixel readback.
The last confirmed timestamp stays unchanged after a failed update and resets
on reboot. A setup refresh before clock synchronization can be confirmed without
a timestamp. Google services may remain `LIVE` while the display reports an
error; the separate display status is the place to check screen-update failures.

## Current firmware scope

The sketch name, pin assignments, driver/LUT, and display recovery logic remain
unchanged. Wi-Fi, theme, interval, credentials, and resources come from the
confirmed browser configuration. The heading is configurable in Display settings.
The existing API report definitions are preserved: GA4 active users in realtime
and new users for 28 completed days, account-wide AdSense earnings, GSC web/all,
and mobile-origin CrUX. OAuth setup tools, additional examples, and enclosure
files will be added separately.

The display driver records BUSY transitions, bounds waits, and makes at most
one guarded SPI recovery attempt for `REFRESH_NOT_STARTED`. A successful
Google request does not confirm that the physical screen refreshed; inspect
the `DISPLAY` messages and the screen itself.

### Source organization

| File | Responsibility |
| --- | --- |
| `SEO_Dashboard.ino` | Startup, polling schedule, setup command, and coordination |
| `dashboard_button.*` | GPIO9 edge capture, debounce, gestures, and page selection |
| `dashboard_pages.*` | Explicit page order, titles, and service mapping |
| `cwv_quality.h` | Shared p75 thresholds and per-metric quality classification |
| `dashboard_data.*` | Metric values, typed provider states, and overall status |
| `google_client.*` | TLS, HTTP, OAuth refresh, and allowlisted error diagnostics |
| `google_reports.*` | AdSense, GA4, GSC, and CrUX requests and report validation |
| `report_dates.*` | Shared Gregorian date validation for AdSense and CrUX |
| `dashboard_display.*` | Separate page renderers, shared metric grid, theme, and guarded refresh |
| `metrics_cache.*` | Complete per-provider NVS records and old-cache migration |
| `device_config.*` | Connection validation and public/private serialization |
| `settings_store.*` | Active/pending configuration records and V1 migration |
| `settings_portal.*` | Wi-Fi setup, local HTTP routes, and status snapshots |
| `settings_http.*` | Bounded incoming requests and stable Digest challenge |
| `legacy_config.*` | Optional compiled defaults for existing installations |

Google access tokens are local to each provider fetch. JSON API responses use
scoped ownership, and failed report validation leaves the previous values
intact. Only the Arduino task owns metric state and display/SPI operations;
the HTTP worker receives copied statuses and stages changes for reboot.
Diagnostics never print request URLs, credentials, or complete error responses;
recognized Google error identifiers are logged to help distinguish permissions,
disabled APIs, and OAuth failures.

Google response payloads are bounded while streaming: 8 KiB for OAuth and error
details, and 32 KiB for report responses. An oversized declared Content-Length
is rejected before allocation, and the same byte limits apply to chunked or
unknown-length bodies. Transfer completion and byte counts are checked before
strict JSON parsing. A rejected successful response reports `RESPONSE TOO LARGE`,
`TRANSFER ERROR`, or `MEMORY ERROR`, retaining the previous metric values. Failed
HTTP responses keep their HTTP-based status even if their optional error details
cannot be read completely. No raw response body is logged.

## Driver checks

The existing host tests exercise the actual display driver and dashboard retry
policy with mock GPIO/SPI, without Wi-Fi or credentials. With Python 3 and a
C++ compiler supporting AddressSanitizer and UndefinedBehaviorSanitizer:

```sh
python3 tests/run_epd_driver_tests.py
python3 tests/run_settings_tests.py
```

The settings tests exercise input validation, password retention/replacement,
versioned record integrity, Wi-Fi confirmation/rollback decisions, deferred
restart, and timer rollover. They do not simulate the radio or NVS hardware.

### UI development without a device

The HTML/CSS/JS in `firmware/SEO_Dashboard/web/` is the working device
page. After editing it, regenerate the PROGMEM header used by the firmware:

```sh
python3 tools/embed_settings_web.py
python3 tools/preview_settings.py
```

Open **http://127.0.0.1:8765/** for a preview using synthetic in-memory values.
It does not contact the ESP32, load credentials, or store submitted values.
The asset generator also supports `--check` to detect a stale generated header.

The modular firmware compiles for ESP32-C3 with core 3.3.12 and Huge APP, using
placeholders only. JavaScript syntax and embedded-asset freshness were checked.
No test cases were added or run for this revision; the existing display runner
was pointed at the extracted display module. Existing host checks predate the
connection configuration and automatic layout. The display driver files remain
unchanged. Physical Wi-Fi provisioning, NVS migration/rollback, API calls using
saved credentials, and e-paper refreshes still require device verification;
no firmware upload was performed.

## License

Project code is licensed under [MIT](LICENSE). Imported Waveshare driver,
painting, and font sources retain their original notices; see
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
