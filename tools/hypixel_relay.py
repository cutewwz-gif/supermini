"""Fetch Hypixel on a PC that can reach api.hypixel.net, then push to the SuperMini hub.

Use this when the hub itself cannot complete TLS to Cloudflare (api.hypixel.net).

Usage:
  set SUPERMINI_HUB=http://YOUR_HUB
  set SUPERMINI_PASSWORD=your-hub-password
  python tools/hypixel_relay.py              # once
  python tools/hypixel_relay.py --loop 45    # every 45s

Auth: hub password (X-SuperMini-Key). Hypixel API key is read from the hub
config page (only when the request uses that header).
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from typing import Any

HUB = os.environ.get("SUPERMINI_HUB", "").rstrip("/")
HUB_KEY = os.environ.get("SUPERMINI_PASSWORD", "")
UA = "SuperMiniHypixelRelay/1.0"


def _req(url: str, *, data: bytes | None = None, headers: dict[str, str] | None = None, timeout: float = 20.0) -> Any:
    hdrs = {"User-Agent": UA, "Accept": "application/json"}
    if headers:
        hdrs.update(headers)
    request = urllib.request.Request(url, data=data, headers=hdrs, method="POST" if data is not None else "GET")
    with urllib.request.urlopen(request, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8", errors="replace"))


def hub_headers() -> dict[str, str]:
    return {"X-SuperMini-Key": HUB_KEY}


def load_hub_config() -> dict[str, Any]:
    if not HUB or not HUB_KEY:
        raise RuntimeError("set SUPERMINI_HUB and SUPERMINI_PASSWORD")
    return _req(f"{HUB}/supermini/api/hypixel/config", headers=hub_headers())


def hypixel_get(path_qs: str, api_key: str) -> dict[str, Any]:
    url = "https://api.hypixel.net" + path_qs
    return _req(url, headers={"API-Key": api_key, "User-Agent": UA}, timeout=20.0)


def resolve_uuid(username: str) -> str:
    name = username.strip()
    compact = "".join(ch for ch in name if ch in "0123456789abcdefABCDEF")
    if len(compact) == 32:
        return compact.lower()
    url = "https://api.mojang.com/users/profiles/minecraft/" + urllib.parse.quote(name)
    data = _req(url, timeout=15.0)
    uid = str(data.get("id") or "")
    if len(uid) != 32:
        raise RuntimeError("bad uuid from mojang")
    return uid.lower()


def push_ingest(player: dict[str, Any], session: dict[str, Any] | None, uuid: str) -> dict[str, Any]:
    body = json.dumps({"player": player, "session": session, "uuid": uuid}, ensure_ascii=False).encode("utf-8")
    return _req(
        f"{HUB}/supermini/api/hypixel/ingest",
        data=body,
        headers={**hub_headers(), "Content-Type": "application/json"},
        timeout=30.0,
    )


def once() -> dict[str, Any]:
    cfg = load_hub_config()
    api_key = (cfg.get("api_key") or os.environ.get("HYPIXEL_API_KEY") or "").strip()
    if not api_key:
        raise RuntimeError("no Hypixel API key on hub (save it on /supermini/hypixel) and no HYPIXEL_API_KEY env")
    username = (cfg.get("username") or "").strip()
    uuid = (cfg.get("uuid") or "").strip()
    if not uuid:
        if not username:
            raise RuntimeError("username/uuid missing on hub")
        print("resolve uuid for", username)
        uuid = resolve_uuid(username)

    print("fetch player", uuid)
    player_raw = hypixel_get("/v2/player?" + urllib.parse.urlencode({"uuid": uuid}), api_key)
    if not player_raw.get("success"):
        raise RuntimeError(player_raw.get("cause") or "player_failed")
    player = player_raw.get("player")
    if not isinstance(player, dict):
        raise RuntimeError("player_null")

    session = None
    try:
        status_raw = hypixel_get("/v2/status?" + urllib.parse.urlencode({"uuid": uuid}), api_key)
        if status_raw.get("success") and isinstance(status_raw.get("session"), dict):
            session = status_raw["session"]
    except Exception as exc:
        print("status skipped:", exc)

    summary = push_ingest(player, session, uuid)
    if summary.get("ok"):
        bw = summary.get("bedwars") or {}
        print(
            "pushed ok:",
            summary.get("name"),
            "stars",
            bw.get("stars"),
            "fkdr",
            bw.get("fkdr"),
            "online",
            summary.get("online"),
        )
    else:
        print("push failed:", summary)
    return summary


def main() -> int:
    ap = argparse.ArgumentParser(description="Relay Hypixel API -> SuperMini hub")
    ap.add_argument("--loop", type=float, default=0.0, help="repeat every N seconds (0 = once)")
    args = ap.parse_args()
    print("hub", HUB or "(set SUPERMINI_HUB)")
    try:
        if args.loop and args.loop > 0:
            while True:
                try:
                    once()
                except Exception as exc:
                    print("error:", exc)
                time.sleep(args.loop)
        else:
            once()
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        print("HTTP", exc.code, body[:300], file=sys.stderr)
        return 1
    except Exception as exc:
        print("error:", exc, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
