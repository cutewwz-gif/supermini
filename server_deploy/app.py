#!/usr/bin/env python3
"""SuperMini device companion API + image host under /supermini."""

from __future__ import annotations

import hashlib
import hmac
import io
import json
import re
import secrets
import time
from pathlib import Path
from typing import Any, Callable

from fastapi import FastAPI, File, Form, HTTPException, Request, UploadFile
from fastapi.responses import FileResponse, HTMLResponse, JSONResponse, RedirectResponse, Response
from fastapi.staticfiles import StaticFiles
from PIL import Image
from starlette.middleware.base import BaseHTTPMiddleware

import hypixel_svc
import weather_svc

ROOT = Path(__file__).resolve().parent
DATA = ROOT / "data"
UPLOADS = DATA / "uploads"
PREVIEWS = DATA / "previews"
RGB565 = DATA / "rgb565"
META = DATA / "meta.json"

# Device content area (landscape): full width x (height - status bar)
IMG_W = 160
IMG_H = 116

# Web + device shared secret (keep out of public git; override via env if needed)
import os

SITE_PASSWORD = os.environ.get("SUPERMINI_PASSWORD", "")
COOKIE_NAME = "supermini_session"
COOKIE_MAX_AGE = 60 * 60 * 24 * 30  # 30 days

for d in (UPLOADS, PREVIEWS, RGB565):
    d.mkdir(parents=True, exist_ok=True)

app = FastAPI(title="SuperMini Hub", docs_url=None, openapi_url=None, redoc_url=None)


def _session_token() -> str:
    return hashlib.sha256(f"supermini|{SITE_PASSWORD}".encode("utf-8")).hexdigest()


def _is_authed(request: Request) -> bool:
    cookie = request.cookies.get(COOKIE_NAME, "")
    if cookie and hmac.compare_digest(cookie, _session_token()):
        return True
    key = request.headers.get("X-SuperMini-Key", "")
    if key and hmac.compare_digest(key, SITE_PASSWORD):
        return True
    return False


class AuthMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next: Callable) -> Response:
        path = request.url.path
        if not path.startswith("/supermini"):
            return await call_next(request)

        # Public: login page + login API only
        if path in ("/supermini/login", "/supermini/api/login") or path == "/supermini/api/login/":
            return await call_next(request)

        if _is_authed(request):
            return await call_next(request)

        if path.startswith("/supermini/api/"):
            return JSONResponse({"ok": False, "error": "unauthorized"}, status_code=401)
        return RedirectResponse(url="/supermini/login", status_code=302)


app.add_middleware(AuthMiddleware)


def _load_meta() -> list[dict[str, Any]]:
    if not META.exists():
        return []
    try:
        data = json.loads(META.read_text(encoding="utf-8"))
        return data if isinstance(data, list) else []
    except Exception:
        return []


def _save_meta(items: list[dict[str, Any]]) -> None:
    META.write_text(json.dumps(items, ensure_ascii=False, indent=2), encoding="utf-8")


def _safe_name(name: str) -> str:
    base = Path(name or "image").name
    base = re.sub(r"[^\w.\-]+", "_", base)[:80]
    return base or "image.bin"


def _rgb565_bytes(img: Image.Image) -> bytes:
    """Convert RGB image to little-endian RGB565, row-major."""
    rgb = img.convert("RGB")
    w, h = rgb.size
    px = rgb.load()
    out = bytearray(w * h * 2)
    i = 0
    for y in range(h):
        for x in range(w):
            r, g, b = px[x, y]
            val = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            out[i] = val & 0xFF
            out[i + 1] = (val >> 8) & 0xFF
            i += 2
    return bytes(out)


def _fit_cover(img: Image.Image, tw: int, th: int) -> Image.Image:
    """Center-crop to aspect then resize (cover). Fallback for non-cropped uploads."""
    src = img.convert("RGB")
    sw, sh = src.size
    if sw <= 0 or sh <= 0:
        return Image.new("RGB", (tw, th), (0, 0, 0))
    scale = max(tw / sw, th / sh)
    nw = max(1, int(round(sw * scale)))
    nh = max(1, int(round(sh * scale)))
    resized = src.resize((nw, nh), Image.Resampling.LANCZOS)
    left = (nw - tw) // 2
    top = (nh - th) // 2
    return resized.crop((left, top, left + tw, top + th))


def _prepare_device_image(img: Image.Image, pre_cropped: bool) -> Image.Image:
    """Client already framed the shot → only scale to 160x116; else cover-crop."""
    src = img.convert("RGB")
    if pre_cropped:
        if src.size == (IMG_W, IMG_H):
            return src
        return src.resize((IMG_W, IMG_H), Image.Resampling.LANCZOS)
    return _fit_cover(src, IMG_W, IMG_H)


def _process_upload(raw: bytes, original_name: str, pre_cropped: bool = False) -> dict[str, Any]:
    try:
        img = Image.open(io.BytesIO(raw))
        img.load()
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"Invalid image: {exc}") from exc

    fitted = _prepare_device_image(img, pre_cropped)
    img_id = secrets.token_hex(6)
    safe = _safe_name(original_name)
    stem = f"{img_id}_{Path(safe).stem}"

    upload_path = UPLOADS / f"{stem}.bin"
    upload_path.write_bytes(raw)

    preview_path = PREVIEWS / f"{stem}.jpg"
    fitted.save(preview_path, format="JPEG", quality=85, optimize=True)

    rgb_path = RGB565 / f"{stem}.rgb565"
    rgb_path.write_bytes(_rgb565_bytes(fitted))

    item = {
        "id": img_id,
        "name": safe,
        "stem": stem,
        "w": IMG_W,
        "h": IMG_H,
        "bytes": IMG_W * IMG_H * 2,
        "created": int(time.time()),
        "preview": f"/supermini/api/images/{img_id}/preview.jpg",
        "rgb565": f"/supermini/api/images/{img_id}/rgb565",
    }
    items = _load_meta()
    items.insert(0, item)
    _save_meta(items)
    return item


def _find(img_id: str) -> dict[str, Any]:
    for item in _load_meta():
        if item.get("id") == img_id:
            return item
    raise HTTPException(status_code=404, detail="Image not found")


@app.get("/supermini/login", response_class=HTMLResponse)
def login_page() -> HTMLResponse:
    return HTMLResponse((ROOT / "static" / "login.html").read_text(encoding="utf-8"))


@app.post("/supermini/api/login")
async def api_login(password: str = Form(...)) -> Response:
    if not hmac.compare_digest(password, SITE_PASSWORD):
        return JSONResponse({"ok": False, "error": "bad password"}, status_code=401)
    resp = JSONResponse({"ok": True})
    resp.set_cookie(
        key=COOKIE_NAME,
        value=_session_token(),
        max_age=COOKIE_MAX_AGE,
        httponly=True,
        samesite="lax",
        path="/",
    )
    return resp


@app.post("/supermini/api/logout")
async def api_logout() -> Response:
    resp = JSONResponse({"ok": True})
    resp.delete_cookie(COOKIE_NAME, path="/")
    return resp


@app.get("/supermini", response_class=HTMLResponse)
@app.get("/supermini/", response_class=HTMLResponse)
def home() -> HTMLResponse:
    return HTMLResponse((ROOT / "static" / "index.html").read_text(encoding="utf-8"))


@app.get("/supermini/api/health")
def health() -> dict[str, Any]:
    return {"ok": True, "service": "supermini", "images": len(_load_meta())}


@app.get("/supermini/api/images")
def list_images() -> dict[str, Any]:
    items = _load_meta()
    return {
        "ok": True,
        "count": len(items),
        "w": IMG_W,
        "h": IMG_H,
        "images": [
            {
                "id": it["id"],
                "name": it.get("name", it["id"]),
                "w": it.get("w", IMG_W),
                "h": it.get("h", IMG_H),
                "bytes": it.get("bytes", IMG_W * IMG_H * 2),
                "created": it.get("created", 0),
                "preview": it.get("preview"),
                "rgb565": it.get("rgb565"),
            }
            for it in items
        ],
    }


@app.post("/supermini/api/images")
async def upload_image(
    file: UploadFile = File(...),
    pre_cropped: str = Form("0"),
) -> dict[str, Any]:
    raw = await file.read()
    if not raw:
        raise HTTPException(status_code=400, detail="Empty file")
    if len(raw) > 3 * 1024 * 1024:
        raise HTTPException(status_code=400, detail="File too large (max 3MB)")
    cropped = pre_cropped.strip().lower() in ("1", "true", "yes", "on")
    item = _process_upload(raw, file.filename or "upload.jpg", pre_cropped=cropped)
    return {"ok": True, "image": item}


@app.get("/supermini/api/images/{img_id}/preview.jpg")
def preview(img_id: str) -> FileResponse:
    item = _find(img_id)
    path = PREVIEWS / f"{item['stem']}.jpg"
    if not path.exists():
        raise HTTPException(status_code=404, detail="Preview missing")
    return FileResponse(
        path,
        media_type="image/jpeg",
        headers={"Cache-Control": "private, no-store"},
    )


@app.get("/supermini/api/images/{img_id}/rgb565")
def rgb565(img_id: str) -> Response:
    item = _find(img_id)
    path = RGB565 / f"{item['stem']}.rgb565"
    if not path.exists():
        raise HTTPException(status_code=404, detail="RGB565 missing")
    data = path.read_bytes()
    return Response(
        content=data,
        media_type="application/octet-stream",
        headers={
            "X-Image-Width": str(item.get("w", IMG_W)),
            "X-Image-Height": str(item.get("h", IMG_H)),
            "Cache-Control": "private, max-age=300",
        },
    )


@app.delete("/supermini/api/images/{img_id}")
def delete_image(img_id: str) -> dict[str, Any]:
    items = _load_meta()
    keep: list[dict[str, Any]] = []
    removed = None
    for it in items:
        if it.get("id") == img_id:
            removed = it
        else:
            keep.append(it)
    if not removed:
        raise HTTPException(status_code=404, detail="Image not found")
    stem = removed["stem"]
    for p in (
        UPLOADS / f"{stem}.bin",
        PREVIEWS / f"{stem}.jpg",
        RGB565 / f"{stem}.rgb565",
    ):
        try:
            p.unlink(missing_ok=True)
        except Exception:
            pass
    _save_meta(keep)
    return {"ok": True, "deleted": img_id}


@app.get("/supermini/hypixel", response_class=HTMLResponse)
def hypixel_page() -> HTMLResponse:
    return HTMLResponse((ROOT / "static" / "hypixel.html").read_text(encoding="utf-8"))


@app.get("/supermini/api/hypixel/config")
def hypixel_config_get(request: Request) -> dict[str, Any]:
    cfg = hypixel_svc.load_config()
    key = cfg.get("api_key") or ""
    masked = ""
    if key:
        masked = (key[:4] + "…" + key[-4:]) if len(key) > 10 else "****"
    out: dict[str, Any] = {
        "ok": True,
        "username": cfg.get("username") or "",
        "uuid": cfg.get("uuid") or "",
        "api_key_set": bool(key),
        "api_key_masked": masked,
        "updated_at": cfg.get("updated_at") or 0,
        "vps_direct_fetch": False,
        "edge": os.environ.get("HYPIXEL_EDGE_URL", ""),
    }
    # Full key only for device/script auth header (not browser cookie sessions).
    hdr = request.headers.get("X-SuperMini-Key", "")
    if key and hdr and hmac.compare_digest(hdr, SITE_PASSWORD):
        out["api_key"] = key
    return out


@app.post("/supermini/api/hypixel/config")
async def hypixel_config_set(
    username: str = Form(""),
    api_key: str = Form(""),
) -> dict[str, Any]:
    cfg = hypixel_svc.load_config()
    username = (username or "").strip()
    api_key = (api_key or "").strip()
    if username:
        cfg["username"] = username
        cfg["uuid"] = ""  # re-resolve on next fetch
    if api_key:
        cfg["api_key"] = api_key
    if not cfg.get("username") and not cfg.get("uuid"):
        raise HTTPException(status_code=400, detail="username required")
    if not cfg.get("api_key"):
        raise HTTPException(status_code=400, detail="api_key required")
    cfg["updated_at"] = int(time.time())
    hypixel_svc.save_config(cfg)
    summary = hypixel_svc.fetch_hypixel(force=True)
    return {
        "ok": True,
        "config": {
            "username": cfg.get("username") or "",
            "uuid": cfg.get("uuid") or "",
            "api_key_set": True,
        },
        "summary": summary,
    }


@app.get("/supermini/api/hypixel")
def hypixel_summary(force: int = 0) -> dict[str, Any]:
    """Compact payload for ESP32 module #3 (and web preview)."""
    return hypixel_svc.fetch_hypixel(force=bool(force))


@app.post("/supermini/api/hypixel/refresh")
def hypixel_refresh() -> dict[str, Any]:
    return hypixel_svc.fetch_hypixel(force=True)


@app.post("/supermini/api/hypixel/ingest")
async def hypixel_ingest(request: Request) -> dict[str, Any]:
    """Push Hypixel player/status JSON fetched on a machine that can reach api.hypixel.net."""
    try:
        body = await request.json()
    except Exception as exc:
        raise HTTPException(status_code=400, detail=f"bad json: {exc}") from exc
    if not isinstance(body, dict):
        raise HTTPException(status_code=400, detail="object required")
    player = body.get("player")
    if not isinstance(player, dict):
        # Allow posting an already-built summary
        if body.get("ok") and isinstance(body.get("rank"), dict):
            body = dict(body)
            body["source"] = "relay"
            body["fetched_at"] = int(body.get("fetched_at") or time.time())
            hypixel_svc._save_cache(body)
            return body
        raise HTTPException(status_code=400, detail="player object required")
    session = body.get("session") if isinstance(body.get("session"), dict) else None
    uuid = str(body.get("uuid") or "")
    return hypixel_svc.ingest_player(player, session, uuid=uuid)


@app.get("/supermini/api/weather")
def weather_bundle(force: int = 0) -> dict[str, Any]:
    """Compact weather+forecast+astro for ESP32 (Clock / Forecast modules)."""
    return weather_svc.get_weather(force=bool(force))


@app.post("/supermini/api/weather/refresh")
def weather_refresh() -> dict[str, Any]:
    return weather_svc.get_weather(force=True)


# Warm cache in background (low QPS; safe on tiny VPS)
weather_svc.start_background_refresh()


# Static assets for the web UI (also behind auth middleware)
static_dir = ROOT / "static"
if static_dir.exists():
    app.mount("/supermini/static", StaticFiles(directory=str(static_dir)), name="static")
