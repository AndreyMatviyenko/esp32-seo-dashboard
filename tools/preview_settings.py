"""Local UI preview with synthetic in-memory settings; does not connect to an ESP32."""
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import argparse
import json
import re
from embed_settings_web import content_security_policy

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', type=int, default=8765)
args = parser.parse_args()
web = Path(__file__).resolve().parents[1] / 'firmware/SEO_Dashboard/web'
zones_source = (web.parent / 'display_options.h').read_text()
time_zones = [dict(id=identifier, label=label) for identifier, label in
              re.findall(r'\{\s*"([^"]+)",\s*"([^"]+)",', zones_source)]
state = dict(title='SEO DASHBOARD', networks=[dict(ssid='Studio Wi-Fi', passwordSet=True)],
             dark=True, rotate180=False, timeZone='Europe/Kyiv', timeZones=time_zones,
             intervalMinutes=15, connected=True, connectedSsid='Studio Wi-Fi',
             ip='192.0.2.10', setupActive=False, updating=False,
             restartPending=False, message='', csrfToken='preview-only', preview=True,
             display=dict(state='WAITING', frame='none', error='', lastSuccess=0),
             services={name: dict(enabled=False, resource='', clientId='', clientSecretSet=False,
                                 refreshTokenSet=False, apiKeySet=False, state='DISABLED', lastSuccess=0)
                       for name in ('adsense', 'ga4', 'gsc', 'crux')})

class Preview(BaseHTTPRequestHandler):
    def respond(self, status, content_type, body, policy=None):
        body = body if isinstance(body, bytes) else body.encode()
        self.send_response(status)
        self.send_header('Content-Type', content_type)
        self.send_header('Content-Length', str(len(body)))
        self.send_header('Cache-Control', 'no-store')
        if policy:
            self.send_header('Content-Security-Policy', policy)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == '/api/settings':
            self.respond(200, 'application/json', json.dumps(state))
            return
        if self.path == '/':
            html = (web / 'index.html').read_text()
            self.respond(200, 'text/html; charset=utf-8', html, content_security_policy(html))
        else:
            self.respond(404, 'text/plain', 'Not found')

    def do_POST(self):
        if self.path != '/api/settings':
            self.respond(404, 'application/json', '{}')
            return
        if self.headers.get('X-CSRF-Token') != state['csrfToken']:
            self.respond(403, 'application/json', '{"error":"Reload before saving."}')
            return
        length = int(self.headers.get('Content-Length', '0'))
        if length > 8192:
            self.respond(400, 'application/json', '{"error":"Request too large."}')
            return
        try:
            data = json.loads(self.rfile.read(length))
            title = data['title']
            if (not isinstance(title, str) or not 1 <= len(title) <= 20 or not title.strip() or
                    any(ord(character) < 32 or ord(character) > 126 for character in title) or
                    not isinstance(data['intervalMinutes'], int) or
                    not 5 <= data['intervalMinutes'] <= 1440 or
                    not isinstance(data['rotate180'], bool) or
                    data['timeZone'] not in {zone['id'] for zone in time_zones}):
                raise ValueError
            networks = data['networks']
            if not isinstance(networks, list) or not 1 <= len(networks) <= 3:
                raise ValueError
            seen = set()
            for network in networks:
                ssid, password, opened = network['ssid'], network['password'], network['openNetwork']
                if (not isinstance(ssid, str) or not ssid or len(ssid.encode()) > 32 or ssid in seen or
                        not isinstance(password, str) or len(password) > 64 or not isinstance(opened, bool)):
                    raise ValueError
                saved = next((item for item in state['networks'] if item['ssid'] == ssid), {})
                if not opened and not password and not saved.get('passwordSet'):
                    raise ValueError
                if password and (len(password) < 8 or any(ord(char) < 32 or ord(char) > 126 for char in password) or
                                 len(password) == 64 and any(char not in '0123456789abcdefABCDEF' for char in password)):
                    raise ValueError
                seen.add(ssid)
        except (KeyError, TypeError, ValueError):
            self.respond(400, 'application/json', '{"error":"Check the settings."}')
            return
        # Discard all submitted values, including OAuth fields. No input is stored or logged.
        self.respond(202, 'application/json', '{"saved":true,"message":"Preview only: settings accepted. No device was changed or restarted."}')

    def log_message(self, format, *args):
        pass

print(f'UI preview only: http://127.0.0.1:{args.port}/', flush=True)
ThreadingHTTPServer(('127.0.0.1', args.port), Preview).serve_forever()
