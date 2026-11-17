# Connecting Google services

[Back to README](../README.md) · [Device setup](getting-started.md) · [Troubleshooting](troubleshooting.md)

AdSense, GA4, and Search Console use user OAuth: you authorize on your computer,
then enter a **Client ID**, **Client secret**, and **Refresh token** on the ESP32.
The device obtains its own short-lived access tokens afterward. Core Web Vitals
uses a **CrUX API key**, without an OAuth login.

Each builder uses their own Google Cloud project. The Cloud project owner and
the accounts owning the reports can be different. Authorize each service with
the account that has access to its data; the three services can use different
emails and different OAuth clients.

All services are optional. Complete one connection at a time and leave the
others disabled until ready.

## 1. Create a Cloud project and enable APIs

1. Open [Google Cloud Console](https://console.cloud.google.com/).
2. Open the project selector at the top, choose **New project**, and name it,
   for example, **SEO Dashboard**. Create it and select it.
3. Open **APIs & Services → Library**. Enable only the APIs you need:

   | Dashboard service | API to enable |
   | --- | --- |
   | AdSense | [AdSense Management API](https://console.cloud.google.com/apis/library/adsense.googleapis.com) |
   | GA4 | [Google Analytics Data API](https://console.cloud.google.com/apis/library/analyticsdata.googleapis.com) |
   | Search Console | [Google Search Console API](https://console.cloud.google.com/apis/library/searchconsole.googleapis.com) |
   | Core Web Vitals | [Chrome UX Report API](https://console.cloud.google.com/apis/library/chromeuxreport.googleapis.com) |

An API key or OAuth client from another project uses that other project's API
configuration. Check the selected project when enabling an API.

## 2. Configure Google Auth Platform

For AdSense, GA4, or Search Console:

1. Use the Cloud Console search bar to open **Google Auth Platform**. On a new
   project, choose **Get started** and complete the app name and contact emails.
2. Under **Audience**, choose **External** for personal Google accounts. An
   organization's Internal app is limited to that organization.
3. While the app is **Testing**, add every Google account you will use under
   **Audience → Test users**. These are the report-owning accounts, not just
   the Cloud project owner's email.
4. Under **Data Access → Add or remove scopes**, add the read-only scopes for
   the services you need:

   | Service | Scope |
   | --- | --- |
   | AdSense | `https://www.googleapis.com/auth/adsense.readonly` |
   | GA4 | `https://www.googleapis.com/auth/analytics.readonly` |
   | Search Console | `https://www.googleapis.com/auth/webmasters.readonly` |

Google documents these scopes for
[AdSense](https://developers.google.com/adsense/management/direct_requests#authorizing_requests),
[Analytics](https://developers.google.com/analytics/devguides/reporting/data/v1/quickstart),
and [Search Console](https://developers.google.com/webmaster-tools/v1/how-tos/authorizing).
Adding a scope to the consent configuration does not grant access by itself;
the user still approves it in the browser.

## 3. Create a Desktop OAuth client

Open **Google Auth Platform → Clients → Create client**. Choose
**Desktop app**, give it a name, and download its JSON file.

Use the installed-application flow below. AdSense does not support service
accounts; the firmware also expects user refresh tokens for GA4 and GSC.
See [AdSense authorization](https://developers.google.com/adsense/management/direct_requests#authorizing_requests)
and Google's [installed application guide](https://googleapis.github.io/google-api-python-client/docs/oauth-installed.html).

Keep the downloaded file private. A working existing Desktop client can be
reused. If you replace the client, generate tokens belonging to the new client
and update all three OAuth fields together on the device.

## 4. Authorize on your computer

The following local recipe uses Google's `google-auth-oauthlib` library and a
temporary loopback callback. The computer running Python must also open the
browser; there is no hosted callback service. The callback listener closes
after authorization. Google's library documents
[InstalledAppFlow and offline access](https://googleapis.dev/python/google-auth-oauthlib/latest/reference/google_auth_oauthlib.flow.html).

### Prepare Python

Install [Python 3](https://www.python.org/downloads/) if needed. Open a terminal
in the repository root. On macOS/Linux:

```sh
python3 -m venv .venv
source .venv/bin/activate
python -m pip install google-auth-oauthlib
python -c "from pathlib import Path; Path('.private').mkdir(exist_ok=True)"
```

On Windows PowerShell:

```powershell
py -m venv .venv
.\.venv\Scripts\Activate.ps1
python -m pip install google-auth-oauthlib
python -c "from pathlib import Path; Path('.private').mkdir(exist_ok=True)"
```

If PowerShell blocks activation, use `.\.venv\Scripts\python.exe` instead of
`python` in the remaining commands; activation is not required in that case.

Move the downloaded Desktop client JSON into `.private/` and name it
`google-client.json`. Both `.private/` and `.venv/` are ignored by Git.
The downloaded JSON must contain an `installed` client, not a service-account key.

### Save and run the authorization recipe

Create `.private/authorize_google.py` with this code. It writes a new private
JSON file for each run and prints only its path, not the resulting credentials:

```python
import json
import os
import time
from pathlib import Path

from google_auth_oauthlib.flow import InstalledAppFlow

scopes = {
    "adsense": "https://www.googleapis.com/auth/adsense.readonly",
    "ga4": "https://www.googleapis.com/auth/analytics.readonly",
    "gsc": "https://www.googleapis.com/auth/webmasters.readonly",
}
service = input("Service (adsense / ga4 / gsc): ").strip().lower()
if service not in scopes:
    raise SystemExit("Choose adsense, ga4, or gsc.")

client_file = Path(input("Client JSON [.private/google-client.json]: ").strip()
                   or ".private/google-client.json").expanduser()
flow = InstalledAppFlow.from_client_secrets_file(
    str(client_file), scopes=[scopes[service]]
)
credentials = flow.run_local_server(
    host="127.0.0.1",
    port=0,
    access_type="offline",
    prompt="consent select_account",
    timeout_seconds=180,
    authorization_prompt_message="Complete authorization in the browser.",
)
if not credentials or not credentials.refresh_token:
    raise SystemExit("No refresh token received. Repeat browser consent.")

folder = Path(".private")
folder.mkdir(exist_ok=True)
output = folder / f"{service}-oauth-{time.time_ns()}.json"
descriptor = os.open(output, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
with os.fdopen(descriptor, "w", encoding="utf-8") as handle:
    json.dump({
        "client_id": credentials.client_id,
        "client_secret": credentials.client_secret,
        "refresh_token": credentials.refresh_token,
    }, handle, indent=2)
    handle.write("\n")
print(f"Saved credentials to {output}. Keep this file private.")
```

Run it from the repository root with the Python environment above:

```sh
python .private/authorize_google.py
```

1. Choose `adsense`, `ga4`, or `gsc`.
2. Accept the default client file, or enter the path to that service's client JSON.
3. In the browser, select the Google account with access to that service.
4. Review and approve the requested read-only permission. For your own unverified
   app, Google may show a warning; confirm that its name and account match your
   project before continuing. Organization policies can block authorization.
5. Keep the terminal running until the callback finishes and the file is saved.
6. Open the generated file locally. Copy `client_id`, `client_secret`, and
   `refresh_token` into the corresponding service fields on the ESP32.

Repeat for the other services, selecting the appropriate account each time.
Use plain values without JSON quotes, C/C++ escapes such as `\x35`, or an
`Authorization: Bearer` prefix. The ESP32 needs the **refresh token**, not the
temporary access token. This step obtains consent; API/resource access is
verified by the device's first requests after Save.

## 5. Enter the service resource

### AdSense

Enable **Display this service** and enter the three OAuth values. No account ID
field is needed: the firmware discovers the accessible AdSense account. This
version requires **exactly one** account from the account-list response; it
reports `ACCOUNT ERROR` rather than guessing if the response is ambiguous.

Reports use account-wide totals and the AdSense account's reporting time zone.
Estimated earnings, unpaid balance, and paid amounts are different figures.
YouTube payments are excluded from the payment detail view.

### Google Analytics 4

Enable the service and enter its OAuth values and numeric **Property ID**, for
example `123456789`. The account used for consent must have access to that GA4
property. The Data API must be enabled in the OAuth client's Cloud project.

To find the ID, open the desired property in Analytics, then **Admin → Property
settings → Property details**; Google may group these labels differently as its
UI changes. Use the numeric property identifier. A web stream's `G-…`
[Measurement ID](https://support.google.com/analytics/answer/12270356?hl=en)
is a different identifier and cannot be used here.

### Google Search Console

Enable the service, enter its OAuth values, and copy the **exact property** to
the resource field:

| Property type | Example |
| --- | --- |
| Domain | `sc-domain:example.com` |
| URL prefix | `https://example.com/` |

The authorizing Google account needs access to that property. Domain and URL
prefix properties are distinct, as are HTTP/HTTPS and www/non-www prefixes.
A Cloud project owner's role alone does not provide Search Console access.
The dashboard requests Web results with preliminary data included (`all`).

### Core Web Vitals / CrUX

1. Enable **Chrome UX Report API** in the project that will own the key.
2. Open **APIs & Services → Credentials → Create credentials → API key**.
3. Restrict the key to **Chrome UX Report API**. A browser HTTP-referrer
   restriction does not match requests made directly by an ESP32.
4. In the device's CWV settings, enter the key and an origin such as
   `https://example.com`, without a path, query, or trailing slash.

This queries aggregated mobile field data for one origin, not a crawl of every
page and not a PageSpeed lab test. An origin with insufficient CrUX coverage
can legitimately show `NO DATA`. See the [CrUX API guide](https://developer.chrome.com/docs/crux/api/).

## 6. Save and check

Click **Save settings**, wait for the restart, and reload the page. Confirm each
enabled service's state and the display's last confirmed refresh. Secret fields
stay blank after reload; presence indicators mean saved values are retained.

An authorization error usually means a mismatched, expired, or revoked token.
An access-denied error can mean a missing scope, a disabled API, key restriction,
or lack of access to the property. See [troubleshooting](troubleshooting.md#google-requests-fail).

## Testing, Production, and token lifetime

For these scopes, an External app left in **Testing** issues refresh tokens
that expire after seven days. For ongoing personal use, open
**Google Auth Platform → Audience → Publish app** and confirm the status is
**In production**. Then repeat authorization to obtain fresh tokens and replace
them on the ESP32. Google's [audience documentation](https://support.google.com/cloud/answer/15549945?hl=en)
describes both states.

Production status and OAuth verification are separate. Google documents a
[personal-use verification exception](https://support.google.com/cloud/answer/13464323?hl=en)
for fewer than 100 users; an unverified warning and applicable user limits can
remain. A publicly distributed shared OAuth client has different obligations.
This project does not ship a shared client.

Production refresh tokens can still be revoked or expire under Google's
[token-lifetime rules](https://developers.google.com/identity/protocols/oauth2#expiration).
Changing to Production is not a promise that a token will work forever. Keep the
client JSON and resulting token files private, and replace authorization if
Google reports `invalid_grant`.
