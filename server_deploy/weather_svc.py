"""Guangzhou weather via Open-Meteo — cached for SuperMini device (stdlib only)."""

from __future__ import annotations

import json
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from pathlib import Path
from typing import Any

DATA = Path(__file__).resolve().parent / "data"
CACHE_PATH = DATA / "weather_cache.json"

CACHE_TTL_SEC = 10 * 60
LAT = 23.1291
LON = 113.2644
TZ_SH = timezone(timedelta(hours=8))

_lock = threading.Lock()
_bg_started = False

# WMO-ish short English (device also has its own map)
_SUMMARY = {
    0: "Clear",
    1: "Mainly clear",
    2: "P.Cloudy",
    3: "Overcast",
    45: "Fog",
    48: "Fog",
    51: "Drizzle",
    53: "Drizzle",
    55: "Drizzle",
    61: "Rain",
    63: "Rain",
    65: "Rain",
    71: "Snow",
    73: "Snow",
    75: "Snow",
    80: "Rain",
    81: "Rain",
    82: "Rain",
    95: "Thunder",
    96: "Thunder",
    99: "Thunder",
}


def _summary(code: int) -> str:
    if code in _SUMMARY:
        return _SUMMARY[code]
    if 50 <= code <= 59:
        return "Drizzle"
    if 60 <= code <= 69:
        return "Rain"
    if 70 <= code <= 79:
        return "Snow"
    if 80 <= code <= 84:
        return "Rain"
    if 95 <= code <= 99:
        return "Thunder"
    return "N/A"


def _http_get_json(url: str, timeout: float = 15.0) -> dict[str, Any]:
    req = urllib.request.Request(url, headers={"User-Agent": "SuperMiniHub/1.0"})
    with urllib.request.urlopen(req, timeout=timeout) as resp:
        return json.loads(resp.read().decode("utf-8", errors="replace"))


def _parse_hour(iso: str) -> tuple[int, int, int, int] | None:
    # YYYY-MM-DDTHH:MM
    if not iso or len(iso) < 13:
        return None
    try:
        y = int(iso[0:4])
        mo = int(iso[5:7])
        d = int(iso[8:10])
        h = int(iso[11:13])
        return y, mo, d, h
    except ValueError:
        return None


def _parse_dt(iso: str) -> tuple[int, int, int, int, int] | None:
    if not iso or len(iso) < 16:
        return None
    try:
        y = int(iso[0:4])
        mo = int(iso[5:7])
        d = int(iso[8:10])
        h = int(iso[11:13])
        mi = int(iso[14:16])
        return y, mo, d, h, mi
    except ValueError:
        return None


def _epoch_local(y: int, mo: int, d: int, h: int, mi: int = 0) -> int:
    return int(datetime(y, mo, d, h, mi, tzinfo=TZ_SH).timestamp())


def _is_rainy(code: int, pop: int) -> bool:
    if pop >= 40:
        return True
    if code < 0:
        return False
    if 50 <= code <= 69:
        return True
    if 80 <= code <= 99:
        return True
    return False


def _compute_tips(
    current: dict[str, Any],
    hourly: list[dict[str, Any]],
    daily: list[dict[str, Any]],
    astro: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Rest-of-today advice for the Clock home screen (Guangzhou local).

    Hat = sun protection only (UV / strong sun while daylight remains).
    Night heat alone never triggers hat.
    """
    now_dt = datetime.now(TZ_SH)
    today_mo, today_dy = now_dt.month, now_dt.day
    now_h = now_dt.hour
    now_ts = now_dt.timestamp()

    umbrella = False
    hat = False

    if _is_rainy(int(current.get("code") or -1), -1):
        umbrella = True

    # Today's sunrise / sunset (epoch seconds, Asia/Shanghai)
    rise_ts: float | None = None
    set_ts: float | None = None
    for a in astro or []:
        if int(a.get("mo") or 0) == today_mo and int(a.get("dy") or 0) == today_dy:
            if a.get("riseEpoch"):
                rise_ts = float(a["riseEpoch"])
            if a.get("setEpoch"):
                set_ts = float(a["setEpoch"])
            break

    # Past sunset → no point wearing a hat for sun
    after_sunset = set_ts is not None and now_ts >= set_ts
    # Fallback if no astro: treat 19:00+ as night
    if set_ts is None and now_h >= 19:
        after_sunset = True

    for h in hourly:
        if int(h.get("mo") or 0) != today_mo or int(h.get("dy") or 0) != today_dy:
            continue
        hh = int(h.get("h") or 0)
        if hh < now_h:
            continue
        code = int(h.get("c") or -1)
        pop = int(h.get("p") or -1)
        temp = int(h.get("t") or 0)
        uv = float(h.get("u") if h.get("u") is not None else -1)
        slot_ep = _epoch_local(now_dt.year, today_mo, today_dy, hh, 0)

        if _is_rainy(code, pop):
            umbrella = True

        if after_sunset:
            continue
        # Only sun-risk hours: between sunrise and sunset
        if rise_ts is not None and slot_ep + 1800 < rise_ts:
            continue
        if set_ts is not None and slot_ep >= set_ts:
            continue
        if set_ts is None and hh >= 19:
            continue
        if rise_ts is None and hh < 6:
            continue

        # Strong UV (WHO high+)
        if uv >= 6.0:
            hat = True
        # Noticeable sun with clear / partly cloudy sky
        elif uv >= 4.0 and code in (0, 1, 2):
            hat = True
        # Hot only matters if the sun is actually out
        elif temp >= 30 and uv >= 3.0:
            hat = True
        # Midday clear, moderate UV still warrants a hat
        elif 10 <= hh <= 16 and code in (0, 1) and uv >= 3.0:
            hat = True

    if daily:
        d0 = daily[0]
        if int(d0.get("mo") or 0) == today_mo and int(d0.get("dy") or 0) == today_dy:
            if not after_sunset:
                uv_max = float(d0.get("uvMax") if d0.get("uvMax") is not None else -1)
                # Only if meaningful daylight remains (before ~1h to sunset)
                daylight_left = set_ts is None or (set_ts - now_ts) > 3600
                if uv_max >= 6.0 and daylight_left and now_h < 17:
                    hat = True
            if _is_rainy(int(d0.get("c") or -1), int(d0.get("p") or -1)) and now_h < 22:
                if int(d0.get("p") or -1) >= 50:
                    umbrella = True

    return {"umbrella": umbrella, "hat": hat}


def _build_from_open_meteo(raw: dict[str, Any]) -> dict[str, Any]:
    cur = raw.get("current") or {}
    code = int(cur.get("weather_code") if cur.get("weather_code") is not None else -1)
    current = {
        "tempC": float(cur.get("temperature_2m") or 0.0),
        "humidity": float(cur.get("relative_humidity_2m") or 0.0),
        "windMs": float(cur.get("wind_speed_10m") or 0.0),
        "code": code,
        "summary": _summary(code),
    }

    now = time.time()
    # keep next ~48h from "now"
    hourly_out: list[dict[str, Any]] = []
    ht = (raw.get("hourly") or {}).get("time") or []
    htemp = (raw.get("hourly") or {}).get("temperature_2m") or []
    hcode = (raw.get("hourly") or {}).get("weather_code") or []
    hpop = (raw.get("hourly") or {}).get("precipitation_probability") or []
    huv = (raw.get("hourly") or {}).get("uv_index") or []
    end = now + 48 * 3600
    for i, iso in enumerate(ht):
        if len(hourly_out) >= 48:
            break
        parsed = _parse_hour(str(iso))
        if not parsed:
            continue
        y, mo, d, h = parsed
        ep = _epoch_local(y, mo, d, h, 0)
        if ep + 3600 <= now:
            continue
        if ep >= end:
            continue
        pop = -1
        if i < len(hpop) and hpop[i] is not None:
            pop = int(hpop[i])
        uv = -1.0
        if i < len(huv) and huv[i] is not None:
            uv = float(huv[i])
        hourly_out.append(
            {
                "mo": mo,
                "dy": d,
                "h": h,
                "t": int(round(float(htemp[i] if i < len(htemp) else 0))),
                "c": int(hcode[i] if i < len(hcode) and hcode[i] is not None else -1),
                "p": pop,
                "u": round(uv, 1) if uv >= 0 else -1,
            }
        )

    daily_out: list[dict[str, Any]] = []
    dt = (raw.get("daily") or {}).get("time") or []
    dmax = (raw.get("daily") or {}).get("temperature_2m_max") or []
    dmin = (raw.get("daily") or {}).get("temperature_2m_min") or []
    dcode = (raw.get("daily") or {}).get("weather_code") or []
    dpop = (raw.get("daily") or {}).get("precipitation_probability_max") or []
    duv = (raw.get("daily") or {}).get("uv_index_max") or []
    for i, iso in enumerate(dt):
        if len(daily_out) >= 7:
            break
        s = str(iso)
        if len(s) < 10:
            continue
        try:
            mo = int(s[5:7])
            d = int(s[8:10])
        except ValueError:
            continue
        pop = -1
        if i < len(dpop) and dpop[i] is not None:
            pop = int(dpop[i])
        uv_max = -1.0
        if i < len(duv) and duv[i] is not None:
            uv_max = float(duv[i])
        daily_out.append(
            {
                "mo": mo,
                "dy": d,
                "tMin": int(round(float(dmin[i] if i < len(dmin) else 0))),
                "tMax": int(round(float(dmax[i] if i < len(dmax) else 0))),
                "c": int(dcode[i] if i < len(dcode) and dcode[i] is not None else -1),
                "p": pop,
                "uvMax": round(uv_max, 1) if uv_max >= 0 else -1,
            }
        )

    astro_out: list[dict[str, Any]] = []
    drise = (raw.get("daily") or {}).get("sunrise") or []
    dset = (raw.get("daily") or {}).get("sunset") or []
    for i, rise_iso in enumerate(drise):
        if len(astro_out) >= 8:
            break
        if i >= len(dset):
            break
        rp = _parse_dt(str(rise_iso))
        sp = _parse_dt(str(dset[i]))
        if not rp or not sp:
            continue
        y, mo, d, rh, rm = rp
        _, _, _, sh, sm = sp
        astro_out.append(
            {
                "mo": mo,
                "dy": d,
                "riseH": rh,
                "riseM": rm,
                "setH": sh,
                "setM": sm,
                "riseEpoch": _epoch_local(y, mo, d, rh, rm),
                "setEpoch": _epoch_local(y, mo, d, sh, sm),
            }
        )

    tips = _compute_tips(current, hourly_out, daily_out, astro_out)

    return {
        "ok": True,
        "city": "Guangzhou",
        "current": current,
        "hourly": hourly_out,
        "daily": daily_out,
        "astro": astro_out,
        "tips": tips,
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
    CACHE_PATH.write_text(json.dumps(payload, ensure_ascii=False, separators=(",", ":")), encoding="utf-8")


def _pull_open_meteo() -> dict[str, Any]:
    q = urllib.parse.urlencode(
        {
            "latitude": LAT,
            "longitude": LON,
            "current": "temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m",
            "hourly": "temperature_2m,weather_code,precipitation_probability,uv_index",
            "daily": "weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,uv_index_max,sunrise,sunset",
            "forecast_days": 8,
            "timezone": "Asia/Shanghai",
        }
    )
    url = "https://api.open-meteo.com/v1/forecast?" + q
    raw = _http_get_json(url, timeout=12.0)
    return _build_from_open_meteo(raw)


def get_weather(force: bool = False) -> dict[str, Any]:
    with _lock:
        cache = _load_cache()
        if (
            not force
            and cache
            and cache.get("ok")
            and (time.time() - int(cache.get("fetched_at") or 0)) < CACHE_TTL_SEC
        ):
            # Tips depend on "now" — recompute cheaply without upstream call
            out = dict(cache)
            out["tips"] = _compute_tips(
                out.get("current") or {},
                out.get("hourly") or [],
                out.get("daily") or [],
                out.get("astro") or [],
            )
            return out
        try:
            payload = _pull_open_meteo()
            _save_cache(payload)
            return payload
        except Exception as exc:
            if cache and cache.get("ok"):
                cache = dict(cache)
                cache["stale"] = True
                cache["error"] = str(exc)[:80]
                cache["tips"] = _compute_tips(
                    cache.get("current") or {},
                    cache.get("hourly") or [],
                    cache.get("daily") or [],
                    cache.get("astro") or [],
                )
                return cache
            return {"ok": False, "error": str(exc)[:120]}


def start_background_refresh() -> None:
    """Refresh every CACHE_TTL — one cheap thread, no busy loop."""
    global _bg_started
    if _bg_started:
        return
    _bg_started = True

    def _loop() -> None:
        # Initial fill after short delay so boot stays light
        time.sleep(5)
        while True:
            try:
                get_weather(force=True)
            except Exception:
                pass
            time.sleep(CACHE_TTL_SEC)

    threading.Thread(target=_loop, name="wx-cache", daemon=True).start()
