"""Hypixel edge fetch for SuperMini — run on a host that can TLS to Cloudflare.

POST /fetch  JSON: {"api_key": "...", "uuid": "..."}
Header: X-SuperMini-Key: <shared secret>
Returns: {"ok": true, "player": {...}, "session": {...}|null}
"""

from __future__ import annotations

import json
import os
import urllib.error
import urllib.parse
import urllib.request
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any

HOST = os.environ.get("HYPIXEL_EDGE_HOST", "0.0.0.0")
PORT = int(os.environ.get("HYPIXEL_EDGE_PORT", "18766"))
SITE_PASSWORD = os.environ.get("SUPERMINI_PASSWORD", "")
UA = "SuperMiniHypixelEdge/1.0"


def _http_get_json(url: str, headers: dict[str, str], timeout: float = 20.0) -> dict[str, Any]:
    req = urllib.request.Request(url, headers=headers)
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8", errors="replace"))


def fetch_hypixel(api_key: str, uuid: str) -> dict[str, Any]:
    headers = {"API-Key": api_key, "User-Agent": UA}
    player_url = "https://api.hypixel.net/v2/player?" + urllib.parse.urlencode({"uuid": uuid})
    status_url = "https://api.hypixel.net/v2/status?" + urllib.parse.urlencode({"uuid": uuid})
    player_raw = _http_get_json(player_url, headers=headers, timeout=20.0)
    if not player_raw.get("success"):
        return {"ok": False, "error": player_raw.get("cause") or "player_failed"}
    player = player_raw.get("player")
    if not isinstance(player, dict):
        return {"ok": False, "error": "player_null"}
    session = None
    try:
        status_raw = _http_get_json(status_url, headers=headers, timeout=12.0)
        if status_raw.get("success") and isinstance(status_raw.get("session"), dict):
            session = status_raw["session"]
    except Exception:
        session = None
    return {"ok": True, "player": player, "session": session, "uuid": uuid}


class Handler(BaseHTTPRequestHandler):
    server_version = "SuperMiniHypixelEdge/1.0"

    def log_message(self, fmt: str, *args: Any) -> None:
        print("[%s] %s" % (self.log_date_time_string(), fmt % args))

    def _send(self, code: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path in ("/", "/health"):
            self._send(200, {"ok": True, "service": "hypixel-edge"})
            return
        self._send(404, {"ok": False, "error": "not_found"})

    def do_POST(self) -> None:  # noqa: N802
        if self.path.rstrip("/") != "/fetch":
            self._send(404, {"ok": False, "error": "not_found"})
            return
        key = self.headers.get("X-SuperMini-Key", "")
        if key != SITE_PASSWORD:
            self._send(401, {"ok": False, "error": "unauthorized"})
            return
        try:
            n = int(self.headers.get("Content-Length") or "0")
        except ValueError:
            n = 0
        raw = self.rfile.read(n) if n > 0 else b"{}"
        try:
            body = json.loads(raw.decode("utf-8"))
        except Exception:
            self._send(400, {"ok": False, "error": "bad_json"})
            return
        api_key = str((body or {}).get("api_key") or "").strip()
        uuid = str((body or {}).get("uuid") or "").strip().lower()
        uuid = "".join(ch for ch in uuid if ch in "0123456789abcdef")
        if not api_key or len(uuid) != 32:
            self._send(400, {"ok": False, "error": "api_key_and_uuid_required"})
            return
        try:
            result = fetch_hypixel(api_key, uuid)
            self._send(200 if result.get("ok") else 502, result)
        except Exception as exc:
            self._send(502, {"ok": False, "error": str(exc)[:200]})


def main() -> None:
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"hypixel-edge listening on {HOST}:{PORT}")
    httpd.serve_forever()


if __name__ == "__main__":
    main()
