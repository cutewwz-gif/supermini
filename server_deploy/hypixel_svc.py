"""Hypixel fetch + rank formatting for SuperMini hub (stdlib only)."""

from __future__ import annotations

import json
import os
import re
import time
import urllib.error
import urllib.parse
import urllib.request
from pathlib import Path
from typing import Any

DATA = Path(__file__).resolve().parent / "data"
CONFIG_PATH = DATA / "hypixel_config.json"
CACHE_PATH = DATA / "hypixel_cache.json"

# Device / web cache TTL — keep Hypixel traffic low on small VPS
CACHE_TTL_SEC = 45

# US edge that can TLS to Cloudflare (CN hub cannot). Override via env.
HYPIXEL_EDGE_URL = os.environ.get(
    "HYPIXEL_EDGE_URL", "http://127.0.0.1:19876/fetch"
).rstrip("/")
HYPIXEL_EDGE_KEY = os.environ.get("SUPERMINI_PASSWORD", "")

# Minecraft / Hypixel named colors → RGB565-ish hex for device
COLOR_HEX = {
    "black": "0000",
    "dark_blue": "0011",
    "dark_green": "03E0",
    "dark_aqua": "0451",
    "dark_red": "A800",
    "dark_purple": "9012",
    "gold": "FE00",
    "gray": "8410",
    "dark_gray": "4208",
    "blue": "001F",
    "green": "07E0",
    "aqua": "07FF",
    "red": "F800",
    "light_purple": "F81F",
    "yellow": "FFE0",
    "white": "FFFF",
}

PLUS_COLOR_MAP = {
    "BLACK": "black",
    "DARK_BLUE": "dark_blue",
    "DARK_GREEN": "dark_green",
    "DARK_AQUA": "dark_aqua",
    "DARK_RED": "dark_red",
    "DARK_PURPLE": "dark_purple",
    "GOLD": "gold",
    "GRAY": "gray",
    "DARK_GRAY": "dark_gray",
    "BLUE": "blue",
    "GREEN": "green",
    "AQUA": "aqua",
    "RED": "red",
    "LIGHT_PURPLE": "light_purple",
    "YELLOW": "yellow",
    "WHITE": "white",
}

MC_COLOR_CODE = {
    "0": "black",
    "1": "dark_blue",
    "2": "dark_green",
    "3": "dark_aqua",
    "4": "dark_red",
    "5": "dark_purple",
    "6": "gold",
    "7": "gray",
    "8": "dark_gray",
    "9": "blue",
    "a": "green",
    "b": "aqua",
    "c": "red",
    "d": "light_purple",
    "e": "yellow",
    "f": "white",
}


def _default_config() -> dict[str, Any]:
    return {
        "api_key": "",
        "username": "",
        "uuid": "",
        "updated_at": 0,
    }


def load_config() -> dict[str, Any]:
    DATA.mkdir(parents=True, exist_ok=True)
    if not CONFIG_PATH.exists():
        return _default_config()
    try:
        data = json.loads(CONFIG_PATH.read_text(encoding="utf-8"))
        if isinstance(data, dict):
            base = _default_config()
            base.update({k: data.get(k, base.get(k)) for k in base})
            return base
    except Exception:
        pass
    return _default_config()


def save_config(cfg: dict[str, Any]) -> None:
    DATA.mkdir(parents=True, exist_ok=True)
    CONFIG_PATH.write_text(json.dumps(cfg, ensure_ascii=False, indent=2), encoding="utf-8")


def _http_get_json(url: str, headers: dict[str, str] | None = None, timeout: float = 12.0) -> dict[str, Any]:
    req = urllib.request.Request(url, headers=headers or {"User-Agent": "SuperMiniHub/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        body = resp.read().decode("utf-8", errors="replace")
    return json.loads(body)


def resolve_uuid(username: str) -> str:
    name = username.strip()
    if not name:
        raise ValueError("username empty")
    # Already a dashed/undashed UUID?
    compact = re.sub(r"[^0-9a-fA-F]", "", name)
    if len(compact) == 32:
        return compact.lower()
    url = "https://api.mojang.com/users/profiles/minecraft/" + urllib.parse.quote(name)
    try:
        data = _http_get_json(url, timeout=8.0)
    except urllib.error.HTTPError as exc:
        if exc.code == 404:
            raise ValueError(f"Minecraft user not found: {name}") from exc
        raise
    uid = str(data.get("id") or "")
    if len(uid) != 32:
        raise ValueError("bad uuid from mojang")
    return uid.lower()


def _fmt_uuid(compact: str) -> str:
    u = compact.lower()
    return f"{u[0:8]}-{u[8:12]}-{u[12:16]}-{u[16:20]}-{u[20:32]}"


def bedwars_star_from_exp(exp: int) -> int:
    """Hypixel Bed Wars level from Experience (approx official curve)."""
    if exp <= 0:
        return 0
    # First 4 levels: 500, 1000, 2000, 3500 then +5000 each
    levels = 0
    thresholds = [500, 1000, 2000, 3500]
    rem = exp
    for t in thresholds:
        if rem < t:
            return levels
        rem -= t
        levels += 1
    levels += rem // 5000
    return int(levels)


def _rank_info(player: dict[str, Any]) -> dict[str, Any]:
    """Build colored rank tag + name segments for device rendering."""
    name = str(player.get("displayname") or player.get("playername") or "?")
    plus_key = str(player.get("rankPlusColor") or "RED")
    plus_color = PLUS_COLOR_MAP.get(plus_key, "red")
    monthly_color_key = str(player.get("monthlyRankColor") or "GOLD")
    monthly_color = PLUS_COLOR_MAP.get(monthly_color_key, "gold")

    prefix = player.get("prefix")
    if isinstance(prefix, str) and prefix:
        # Custom prefix like §c[OWNER] — strip codes into segments roughly
        segments = _parse_mc_prefix(prefix)
        # append name in last color or white
        last_c = segments[-1]["c"] if segments else "white"
        segments.append({"t": name, "c": last_c})
        tag = re.sub(r"§.", "", prefix)
        return {
            "tag": tag,
            "name": name,
            "segments": segments,
        }

    rank = player.get("rank")
    monthly = player.get("monthlyPackageRank")
    new_pkg = player.get("newPackageRank")
    pkg = player.get("packageRank")

    def ok(v: Any) -> bool:
        return bool(v) and v not in ("NONE", "NORMAL")

    kind = None
    if ok(rank):
        kind = str(rank)
    elif ok(monthly):
        kind = str(monthly)
    elif ok(new_pkg):
        kind = str(new_pkg)
    elif ok(pkg):
        kind = str(pkg)

    # Defaults for non-ranked
    if not kind:
        return {
            "tag": "",
            "name": name,
            "segments": [{"t": name, "c": "gray"}],
        }

    if kind == "SUPERSTAR":  # MVP++
        # [MVP++] name — tag gold/monthly, pluses use plus_color
        segs = [
            {"t": "[MVP", "c": monthly_color},
            {"t": "++", "c": plus_color},
            {"t": "]", "c": monthly_color},
            {"t": name, "c": monthly_color},
        ]
        return {"tag": "[MVP++]", "name": name, "segments": segs}

    if kind == "MVP_PLUS":
        segs = [
            {"t": "[MVP", "c": "aqua"},
            {"t": "+", "c": plus_color},
            {"t": "]", "c": "aqua"},
            {"t": name, "c": "aqua"},
        ]
        return {"tag": "[MVP+]", "name": name, "segments": segs}

    if kind == "MVP":
        segs = [{"t": "[MVP]", "c": "aqua"}, {"t": name, "c": "aqua"}]
        return {"tag": "[MVP]", "name": name, "segments": segs}

    if kind == "VIP_PLUS":
        segs = [
            {"t": "[VIP", "c": "green"},
            {"t": "+", "c": "gold"},
            {"t": "]", "c": "green"},
            {"t": name, "c": "green"},
        ]
        return {"tag": "[VIP+]", "name": name, "segments": segs}

    if kind == "VIP":
        segs = [{"t": "[VIP]", "c": "green"}, {"t": name, "c": "green"}]
        return {"tag": "[VIP]", "name": name, "segments": segs}

    if kind == "YOUTUBER":
        segs = [
            {"t": "[", "c": "red"},
            {"t": "YOUTUBE", "c": "white"},
            {"t": "]", "c": "red"},
            {"t": name, "c": "red"},
        ]
        return {"tag": "[YOUTUBE]", "name": name, "segments": segs}

    if kind in ("ADMIN", "MODERATOR", "HELPER", "GAME_MASTER"):
        label = {"ADMIN": "ADMIN", "MODERATOR": "MOD", "HELPER": "HELPER", "GAME_MASTER": "GM"}[kind]
        segs = [{"t": f"[{label}]", "c": "red"}, {"t": name, "c": "red"}]
        return {"tag": f"[{label}]", "name": name, "segments": segs}

    # Fallback
    segs = [{"t": f"[{kind}]", "c": "white"}, {"t": name, "c": "white"}]
    return {"tag": f"[{kind}]", "name": name, "segments": segs}


def _parse_mc_prefix(prefix: str) -> list[dict[str, str]]:
    segs: list[dict[str, str]] = []
    color = "white"
    i = 0
    buf = ""
    while i < len(prefix):
        if prefix[i] == "§" and i + 1 < len(prefix):
            if buf:
                segs.append({"t": buf, "c": color})
                buf = ""
            code = prefix[i + 1].lower()
            color = MC_COLOR_CODE.get(code, color)
            i += 2
            continue
        buf += prefix[i]
        i += 1
    if buf:
        segs.append({"t": buf, "c": color})
    return segs


def _ratio(a: float, b: float) -> float:
    if b <= 0:
        return float(a) if a > 0 else 0.0
    return round(a / b, 2)


def _activity_text(session: dict[str, Any] | None) -> tuple[bool, str]:
    if not session:
        return False, "Unknown"
    online = bool(session.get("online"))
    if not online:
        return False, "Offline"
    gt = str(session.get("gameType") or "")
    mode = str(session.get("mode") or "")
    map_name = str(session.get("map") or "")
    nice_game = {
        "BEDWARS": "BedWars",
        "SKYWARS": "SkyWars",
        "SKYBLOCK": "SkyBlock",
        "DUELS": "Duels",
        "HOUSING": "Housing",
        "LOBBY": "Lobby",
        "MAIN": "Main",
        "PROTOTYPE": "Prototype",
        "MURDER_MYSTERY": "Murder",
        "BUILD_BATTLE": "BuildBattle",
        "PIT": "Pit",
        "ARCADE": "Arcade",
        "TNTGAMES": "TNT",
        "UHC": "UHC",
        "SPEED_UHC": "SpeedUHC",
        "WALLS3": "MegaWalls",
        "SUPER_SMASH": "Smash",
        "BATTLEGROUND": "Warlords",
        "MCGO": "C&C",
        "SURVIVAL_GAMES": "Blitz",
        "PAINTBALL": "Paintball",
        "QUAKECRAFT": "Quake",
        "VAMPIREZ": "VampireZ",
        "WALLS": "Walls",
        "ARENA": "Arena",
        "WOOL_GAMES": "WoolWars",
        "SMP": "SMP",
        "REPLAY": "Replay",
    }.get(gt, gt or "Online")
    mode_nice = mode.replace("BEDWARS_", "").replace("_", " ").strip()
    if mode.upper() == "LOBBY" or mode.upper().endswith("LOBBY"):
        detail = f"{nice_game} Lobby"
    elif mode_nice and map_name:
        detail = f"{nice_game} {mode_nice}"
    elif mode_nice:
        detail = f"{nice_game} {mode_nice}"
    else:
        detail = nice_game
    return True, detail[:28]


def build_summary(player: dict[str, Any], session: dict[str, Any] | None) -> dict[str, Any]:
    rank = _rank_info(player)
    online, activity = _activity_text(session)

    ach = player.get("achievements") or {}
    bw_stats = ((player.get("stats") or {}).get("Bedwars")) or {}
    stars = ach.get("bedwars_level")
    if stars is None:
        stars = bedwars_star_from_exp(int(bw_stats.get("Experience") or 0))

    kills = int(bw_stats.get("kills_bedwars") or 0)
    deaths = int(bw_stats.get("deaths_bedwars") or 0)
    fk = int(bw_stats.get("final_kills_bedwars") or 0)
    fd = int(bw_stats.get("final_deaths_bedwars") or 0)
    wins = int(bw_stats.get("wins_bedwars") or 0)
    losses = int(bw_stats.get("losses_bedwars") or 0)
    beds = int(bw_stats.get("beds_broken_bedwars") or 0)

    # Attach RGB565 hex map for device convenience
    color_map = {k: COLOR_HEX[k] for k in COLOR_HEX}

    return {
        "ok": True,
        "name": rank["name"],
        "rank": rank,
        "colors": color_map,
        "online": online,
        "activity": activity,
        "bedwars": {
            "stars": int(stars or 0),
            "kills": kills,
            "deaths": deaths,
            "kdr": _ratio(kills, deaths),
            "final_kills": fk,
            "final_deaths": fd,
            "fkdr": _ratio(fk, fd),
            "wins": wins,
            "losses": losses,
            "beds_broken": beds,
        },
        "fetched_at": int(time.time()),
    }


def _load_cache() -> dict[str, Any] | None:
    if not CACHE_PATH.exists():
        return None
    try:
        data = json.loads(CACHE_PATH.read_text(encoding="utf-8"))
        return data if isinstance(data, dict) else None
    except Exception:
        return None


def _save_cache(payload: dict[str, Any]) -> None:
    DATA.mkdir(parents=True, exist_ok=True)
    CACHE_PATH.write_text(json.dumps(payload, ensure_ascii=False), encoding="utf-8")


def ingest_player(player: dict[str, Any], session: dict[str, Any] | None = None, uuid: str = "") -> dict[str, Any]:
    """Accept Hypixel player(+session) JSON fetched elsewhere (e.g. Windows relay)."""
    if not isinstance(player, dict) or not player:
        return {"ok": False, "error": "player_missing"}
    summary = build_summary(player, session)
    uid = (uuid or "").strip().lower()
    if not uid:
        raw = str(player.get("uuid") or "")
        uid = re.sub(r"[^0-9a-fA-F]", "", raw).lower()
    if uid:
        summary["uuid"] = uid
        cfg = load_config()
        if cfg.get("uuid") != uid:
            cfg["uuid"] = uid
            save_config(cfg)
    summary["source"] = "relay"
    _save_cache(summary)
    return summary


def _network_error_hint(exc: BaseException) -> str:
    msg = str(exc)
    low = msg.lower()
    if "handshake" in low or "timed out" in low or "timeout" in low:
        return (
            "hypixel_tls_blocked "
            "(hub cannot HTTPS to Cloudflare; configure HYPIXEL_EDGE_URL)"
        )
    return msg[:160]


def _fetch_via_edge(api_key: str, uuid: str) -> dict[str, Any]:
    """Ask US edge to call api.hypixel.net; returns player/session payload."""
    if not HYPIXEL_EDGE_URL:
        raise RuntimeError("edge_url_missing")
    body = json.dumps({"api_key": api_key, "uuid": uuid}, ensure_ascii=False).encode("utf-8")
    req = urllib.request.Request(
        HYPIXEL_EDGE_URL if HYPIXEL_EDGE_URL.endswith("/fetch") else (HYPIXEL_EDGE_URL + "/fetch"),
        data=body,
        headers={
            "Content-Type": "application/json",
            "User-Agent": "SuperMiniHub/1.0",
            "X-SuperMini-Key": HYPIXEL_EDGE_KEY,
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=25.0) as resp:
        data = json.loads(resp.read().decode("utf-8", errors="replace"))
    if not isinstance(data, dict):
        raise RuntimeError("edge_bad_payload")
    return data


def _fetch_direct(api_key: str, uuid: str) -> dict[str, Any]:
    headers = {"API-Key": api_key, "User-Agent": "SuperMiniHub/1.0"}
    player_url = "https://api.hypixel.net/v2/player?" + urllib.parse.urlencode({"uuid": uuid})
    status_url = "https://api.hypixel.net/v2/status?" + urllib.parse.urlencode({"uuid": uuid})
    player_raw = _http_get_json(player_url, headers=headers, timeout=12.0)
    if not player_raw.get("success"):
        return {"ok": False, "error": player_raw.get("cause") or "player_failed"}
    player = player_raw.get("player")
    if not player:
        return {"ok": False, "error": "player_null"}
    session = None
    try:
        status_raw = _http_get_json(status_url, headers=headers, timeout=8.0)
        if status_raw.get("success"):
            session = status_raw.get("session")
    except Exception:
        session = None
    return {"ok": True, "player": player, "session": session, "uuid": uuid}


def fetch_hypixel(force: bool = False) -> dict[str, Any]:
    cfg = load_config()
    api_key = (cfg.get("api_key") or "").strip()
    username = (cfg.get("username") or "").strip()
    uuid = (cfg.get("uuid") or "").strip()

    if not api_key:
        return {"ok": False, "error": "api_key_missing"}
    if not username and not uuid:
        return {"ok": False, "error": "username_missing"}

    cache = _load_cache()
    if (
        not force
        and cache
        and cache.get("ok")
        and (time.time() - int(cache.get("fetched_at") or 0)) < CACHE_TTL_SEC
    ):
        return cache

    try:
        if not uuid or (force and username):
            # Mojang works from CN; refresh uuid when forcing with a username set.
            if username:
                uuid = resolve_uuid(username)
                cfg["uuid"] = uuid
                save_config(cfg)
            elif not uuid:
                uuid = resolve_uuid(username or uuid)
                cfg["uuid"] = uuid
                save_config(cfg)

        edge_err = None
        raw: dict[str, Any] | None = None
        source = "edge"
        try:
            raw = _fetch_via_edge(api_key, uuid)
            if not raw.get("ok"):
                edge_err = raw.get("error") or "edge_failed"
                raw = None
        except Exception as exc:
            edge_err = _network_error_hint(exc)
            raw = None

        if raw is None:
            source = "direct"
            try:
                raw = _fetch_direct(api_key, uuid)
            except Exception as exc:
                if cache and cache.get("ok"):
                    out = dict(cache)
                    out["stale"] = True
                    out["fetch_error"] = f"edge:{edge_err}; direct:{_network_error_hint(exc)}"
                    return out
                return {
                    "ok": False,
                    "error": f"edge:{edge_err}; direct:{_network_error_hint(exc)}",
                }

        if not raw.get("ok"):
            return {"ok": False, "error": raw.get("error") or "fetch_failed"}
        player = raw.get("player")
        if not isinstance(player, dict):
            return {"ok": False, "error": "player_null"}
        session = raw.get("session") if isinstance(raw.get("session"), dict) else None
        summary = build_summary(player, session)
        summary["uuid"] = uuid
        summary["source"] = source
        _save_cache(summary)
        return summary
    except urllib.error.HTTPError as exc:
        detail = f"http_{exc.code}"
        try:
            body = exc.read().decode("utf-8", errors="replace")
            j = json.loads(body)
            detail = j.get("cause") or detail
        except Exception:
            pass
        return {"ok": False, "error": detail}
    except Exception as exc:
        if cache and cache.get("ok"):
            out = dict(cache)
            out["stale"] = True
            out["fetch_error"] = _network_error_hint(exc)
            return out
        return {"ok": False, "error": _network_error_hint(exc)}
