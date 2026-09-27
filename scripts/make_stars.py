#!/usr/bin/env python3
"""
Generate assets/stars.bin from the HYG stellar database.

HYG (Hipparcos + Yale + Gliese) v3 is public domain: 120k+ stars.
Trim to brightest 2000 by magnitude, convert RA/Dec to unit XYZ,
write engine::stars binary (see engine/Stars.cpp for format).

Falls back to an embedded bright-star set + procedural filler when
the network is unavailable, so the asset is always buildable offline.

Engine reads this via engine::stars::loadStars(bytes, size, field).
Same parser handles any HYG trim or replacement catalog — the engine
doesn't care where the data came from.
"""
import csv
import io
import math
import os
import struct
import sys
import urllib.request
import random

HYG_URL = "https://raw.githubusercontent.com/astronexus/HYG-Database/main/hyg/v3/hyg_v34.csv.gz"
TARGET = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "assets", "stars.bin",
)
import struct as _struct
MAGIC = _struct.pack("<I", 0x53545253)
VERSION = 1
MAX_STARS = 2000
MILLIWAYS_FALLBACK = [
    ("Sirius",        6.7525,  -16.7161, -1.46),
    ("Canopus",       6.3992,  -52.6957, -0.74),
    ("Arcturus",     "14h15m", "+19°11'", -0.05),
    ("Vega",          18.6156,  38.7836,  0.03),
    ("Capella",       5.2782,   45.9980,  0.08),
    ("Rigel",         5.2422,   -8.2016,  0.18),
    ("Procyon",       7.6550,    5.2250,  0.40),
    ("Achernar",      1.6286,  -57.2367,  0.45),
    ("Betelgeuse",    5.9195,    7.4070,  0.50),
    ("Hadar",        "14h04m", "-60°22'",  0.61),
    ("Altair",       "19h51m", "+08°52'",  0.77),
    ("Acrux",        "12h27m", "-63°06'",  0.77),
    ("Aldebaran",    "04h36m", "+16°31'",  0.85),
    ("Antares",      "16h29m", "-26°26'",  1.09),
    ("Spica",        "13h25m", "-11°09'",  1.04),
    ("Pollux",       "07h45m", "+28°01'",  1.14),
    ("Fomalhaut",    "22h58m", "-29°37'",  1.16),
    ("Deneb",        "20h41m", "+45°17'",  1.25),
    ("Mimosa",       "12h48m", "-59°41'",  1.25),
    ("Regulus",      "10h08m", "+11°58'",  1.40),
    ("Adhara",       "06h59m", "-28°58'",  1.50),
    ("Castor",       "07h35m", "+31°53'",  1.58),
    ("Gacrux",       "12h31m", "-57°07'",  1.59),
    ("Shaula",       "17h34m", "-37°06'",  1.62),
    ("Bellatrix",    "05h25m", "+06°21'",  1.64),
    ("Elnath",       "05h26m", "+28°36'",  1.65),
    ("Miaplacidus",  "09h13m", "-69°43'",  1.69),
    ("Alnilam",      "05h36m", "-01°12'",  1.69),
    ("Alnair",       "22h08m", "-46°58'",  1.74),
    ("Alnitak",      "05h41m", "-01°57'",  1.74),
    ("Alioth",       "12h54m", "+55°58'",  1.76),
    ("Dubhe",        "11h04m", "+61°45'",  1.79),
    ("Mirfak",       "03h24m", "+49°51'",  1.79),
    ("Wezen",        "07h08m", "-26°23'",  1.83),
    ("Kaus Australis","18h24m", "-34°23'", 1.85),
    ("Avior",        "08h22m", "-59°31'",  1.86),
    ("Alkaid",       "13h47m", "+49°19'",  1.86),
    ("Sargas",       "17h37m", "-42°59'",  1.87),
    ("Menkalinan",   "05h59m", "+44°57'",  1.90),
    ("Atria",        "16h48m", "-69°02'",  1.91),
    ("Alhena",       "06h38m", "+16°24'",  1.93),
    ("Peacock",      "20h26m", "-56°44'",  1.94),
    ("Polaris",      "02h31m", "+89°16'",  1.97),
    ("Mirzam",       "06h23m", "-17°57'",  1.98),
    ("Alphard",      "09h28m", "-08°40'",  1.99),
    ("Algieba",      "10h20m", "+19°50'",  2.00),
    ("Hamal",        "02h07m", "+23°28'",  2.00),
    ("Diphda",       "00h44m", "-17°59'",  2.04),
    ("Nunki",        "18h55m", "-26°18'",  2.05),
    ("Menkent",      "14h07m", "-36°22'",  2.06),
    ("Mirach",       "01h10m", "+35°37'",  2.05),
]


def _parse_ra(ra_str):
    s = str(ra_str).strip()
    if not s: return None
    if "h" in s:
        hms = s.replace("h", " ").replace("m", " ").replace("s", " ").split()
        h = float(hms[0]); m = float(hms[1]) if len(hms) > 1 else 0.0
        sec = float(hms[2]) if len(hms) > 2 else 0.0
        return (h + m / 60.0 + sec / 3600.0) * 15.0
    return float(s) * 15.0


def _parse_dec(dec_str):
    s = str(dec_str).strip()
    if not s: return None
    s = s.replace("°", " ").replace("'", " ").replace('"', " ")
    s = s.replace("+", " +").replace("-", " -").strip()
    parts = s.split()
    sign = -1.0 if s.startswith("-") else 1.0
    parts = [p for p in parts if p not in ("+", "-")]
    if not parts: return None
    d = abs(float(parts[0])); m = float(parts[1]) if len(parts) > 1 else 0.0
    sec = float(parts[2]) if len(parts) > 2 else 0.0
    return sign * (d + m / 60.0 + sec / 3600.0)


def _bv_to_rgb(bv):
    bv = max(-0.4, min(2.0, float(bv)))
    if bv < 0.0:
        t = (bv + 0.4) / 0.4
        r = 0.75 + 0.25 * t
        g = 0.78 + 0.10 * t
        b = 1.00
    elif bv < 0.5:
        t = bv / 0.5
        r = 1.00
        g = 0.95 - 0.15 * t
        b = 1.00 - 0.20 * t
    elif bv < 1.5:
        t = (bv - 0.5) / 1.0
        r = 1.00 - 0.10 * t
        g = 0.80 - 0.20 * t
        b = 0.80 - 0.30 * t
    else:
        t = min(1.0, (bv - 1.5) / 0.5)
        r = 0.95 - 0.25 * t
        g = 0.60 - 0.25 * t
        b = 0.50 - 0.20 * t
    return (int(max(0, min(255, r * 255 + 0.5))),
            int(max(0, min(255, g * 255 + 0.5))),
            int(max(0, min(255, b * 255 + 0.5))))


def _equatorial_to_xyz(ra_deg, dec_deg):
    ra = math.radians(ra_deg)
    dec = math.radians(dec_deg)
    return (math.cos(dec) * math.cos(ra),
            math.sin(dec),
            math.cos(dec) * math.sin(ra))


def fetch_hyg():
    print(f"[stars] fetching HYG database from {HYG_URL}", file=sys.stderr)
    import gzip
    req = urllib.request.Request(HYG_URL, headers={"User-Agent": "not-a-game"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        raw = resp.read()
    return io.StringIO(gzip.decompress(raw).decode("utf-8"))


def parse_csv(text, limit):
    reader = csv.DictReader(text)
    rows = []
    for row in reader:
        try:
            mag = float(row.get("mag", "99"))
        except ValueError:
            continue
        if mag > 6.5: continue
        if mag < -10.0: continue
        try:
            ra = _parse_ra(row.get("ra", ""))
            dec = _parse_dec(row.get("dec", ""))
        except (ValueError, IndexError):
            continue
        if ra is None or dec is None: continue
        try:
            bv = float(row.get("ci", "0") or "0")
        except ValueError:
            bv = 0.0
        rows.append((ra, dec, mag, bv))
    rows.sort(key=lambda r: r[2])
    return rows[:limit]


def fallback_stars(limit):
    rng = random.Random(0xC0FFEE)
    real = []
    for name, ra, dec, mag in MILLIWAYS_FALLBACK[:50]:
        ra_deg = _parse_ra(ra)
        dec_deg = _parse_dec(dec) if isinstance(dec, str) else dec
        if ra_deg is None or dec_deg is None: continue
        real.append((ra_deg, dec_deg, mag, 0.5))
    real.sort(key=lambda r: r[2])

    fill = []
    target = limit - len(real)
    for i in range(target):
        band = rng.random() < 0.6
        if band:
            u = rng.uniform(-0.25, 0.25)
            v = rng.uniform(-0.95, 0.95)
            s = math.sqrt(max(0, 1 - u * u))
            ra = math.degrees(math.atan2(v, s * 0.4)) + 90.0
            dec = math.degrees(math.asin(u))
        else:
            u = rng.uniform(-1, 1)
            phi = rng.uniform(0, math.tau)
            s = math.sqrt(max(0, 1 - u * u))
            ra = math.degrees(math.atan2(s * math.sin(phi), s * math.cos(phi)))
            dec = math.degrees(math.asin(u))
        mag = 2.0 + rng.random() * 4.5
        bv = rng.uniform(-0.2, 1.5)
        fill.append((ra, dec, mag, bv))
    return real + fill


def write_bin(stars, out_path):
    with open(out_path, "wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<II", len(stars), VERSION))
        for ra, dec, mag, bv in stars:
            x, y, z = _equatorial_to_xyz(ra, dec)
            r, g, b = _bv_to_rgb(bv)
            f.write(struct.pack("<ffffBBBB",
                                x, y, z,
                                float(mag),
                                r, g, b, 0))


def main():
    target = sys.argv[1] if len(sys.argv) > 1 else TARGET
    os.makedirs(os.path.dirname(os.path.abspath(target)), exist_ok=True)
    rows = None
    try:
        text = fetch_hyg()
        rows = parse_csv(text, MAX_STARS)
        if len(rows) < 100:
            print("[stars] HYG too sparse, falling back", file=sys.stderr)
            rows = None
    except Exception as e:
        print(f"[stars] HYG fetch failed: {e}; using fallback", file=sys.stderr)

    if not rows:
        rows = fallback_stars(MAX_STARS)

    write_bin(rows, target)
    print(f"[stars] wrote {len(rows)} stars to {os.path.normpath(target)}", file=sys.stderr)


if __name__ == "__main__":
    main()
