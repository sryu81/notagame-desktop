import math


def knight_clips(mounted=False):
    def mirror(rot):
        return {n.replace(".L", ".X").replace(".R", ".L").replace(".X", ".R"): v for n, v in rot.items()}

    walk0 = {"UpperLeg.L": (-25, 0, 0), "LowerLeg.L": (5, 0, 0), "UpperLeg.R": (20, 0, 0),
             "LowerLeg.R": (15, 0, 0), "UpperArm.R": (-15, 0, 0), "UpperArm.L": (8, 0, 0),
             "Spine": (2, 0, 4)}
    walk1 = {"UpperLeg.L": (0, 0, 0), "LowerLeg.L": (8, 0, 0), "UpperLeg.R": (-5, 0, 0),
             "LowerLeg.R": (50, 0, 0), "Spine": (2, 0, 0)}
    clips = {
        "Idle": (2.0, [
            (0.0, {}, {}),
            (1.0, {"Spine": (-2, 0, 0), "Head": (3, 0, 0), "LowerArm.R": (-5, 0, 0)}, {"Hips": (0, 0, -0.012)}),
            (2.0, {}, {}),
        ]),
        "Walk": (1.0, [
            (0.0, walk0, {}),
            (0.25, walk1, {"Hips": (0, 0, 0.03)}),
            (0.5, mirror(walk0), {}),
            (0.75, mirror(walk1), {"Hips": (0, 0, 0.03)}),
            (1.0, walk0, {}),
        ]),
        "Attack": (0.6, [
            (0.0, {}, {}),
            (0.2, {"UpperArm.R": (-150, 0, 0), "LowerArm.R": (-30, 0, 0), "Spine": (-8, 0, -15),
                   "UpperLeg.L": (-15, 0, 0), "LowerLeg.L": (10, 0, 0)}, {}),
            (0.35, {"UpperArm.R": (-70, 0, 0), "LowerArm.R": (15, 0, 0), "Spine": (12, 0, 20),
                    "UpperLeg.L": (-25, 0, 0), "LowerLeg.L": (15, 0, 0), "UpperLeg.R": (15, 0, 0)},
             {"Hips": (0, -0.05, -0.04)}),
            (0.6, {}, {}),
        ]),
        "Hurt": (0.4, [
            (0.0, {}, {}),
            (0.12, {"Spine": (-15, 0, 0), "Head": (-15, 0, 0), "UpperArm.R": (15, 0, 0),
                    "UpperArm.L": (10, 0, 0)}, {"Hips": (0, 0.05, 0)}),
            (0.4, {}, {}),
        ]),
        "Death": (0.8, [
            (0.0, {}, {}),
            (0.25, {"Spine": (-20, 0, 0), "Head": (-15, 0, 0), "UpperLeg.L": (-20, 0, 0),
                    "UpperLeg.R": (-10, 0, 0), "LowerLeg.L": (30, 0, 0), "LowerLeg.R": (30, 0, 0)},
             {"Hips": (0, 0.05, -0.05)}),
            (0.5, {"Hips": (-50, 0, 0), "Spine": (-10, 0, 0), "UpperLeg.L": (-30, 0, 0),
                   "UpperLeg.R": (-20, 0, 0), "LowerLeg.L": (40, 0, 0), "LowerLeg.R": (40, 0, 0)},
             {"Hips": (0, 0.15, -0.35)}),
            (0.8, {"Hips": (-88, 0, 0), "Spine": (5, 0, 0), "Head": (10, 0, 0),
                   "UpperArm.L": (0, -50, 0), "UpperArm.R": (0, 50, 0),
                   "UpperLeg.L": (5, -8, 0), "UpperLeg.R": (5, 8, 0)},
             {"Hips": (0, 0.13, -0.77)}),
        ]),
    }
    if not mounted:
        return clips
    legs = ("Hips", "UpperLeg", "LowerLeg", "Foot")
    out = {}
    for name, (dur, keys) in clips.items():
        ks = []
        for t, rot, loc in keys:
            r = {n: v for n, v in rot.items() if not n.startswith(legs)}
            ks.append((t, r, {}))
        out[name] = (dur, ks)
    w = out["Walk"][1]
    out["Walk"] = (1.0, [(t, {"Spine": (4, 0, 0), "UpperArm.R": (-6 if i % 2 else 0, 0, 0)}, {})
                         for i, (t, _, _) in enumerate(w)])
    out["Death"] = (0.8, [
        (0.0, {}, {}),
        (0.25, {"Spine": (15, 0, 0), "Head": (10, 0, 0)}, {}),
        (0.5, {"Spine": (35, 0, 8), "Head": (15, 0, 0), "UpperArm.R": (20, 10, 0)}, {}),
        (0.8, {"Spine": (50, 0, 10), "Head": (25, 0, 0), "UpperArm.L": (30, 0, 0),
               "UpperArm.R": (35, 20, 0)}, {}),
    ])
    return out


def horse_clips(mounted=False):
    def legs(fl, fr, bl, br):
        r = {}
        for n, (up, low) in (("FL", fl), ("FR", fr), ("BL", bl), ("BR", br)):
            r[f"{n}_Up"] = (up, 0, 0)
            r[f"{n}_Low"] = (low, 0, 0)
        return r

    a = dict(legs((-25, 5), (20, 10), (20, -5), (-20, -5)), Neck=(4, 0, 0), Tail=(0, 8, 0))
    b = dict(legs((0, 0), (-5, 60), (-5, -40), (0, 0)), Neck=(-2, 0, 0), Tail=(0, 0, 0))
    c = dict(legs((20, 10), (-25, 5), (-20, -5), (20, -5)), Neck=(4, 0, 0), Tail=(0, -8, 0))
    d = dict(legs((-5, 60), (0, 0), (0, 0), (-5, -40)), Neck=(-2, 0, 0), Tail=(0, 0, 0))
    rear = dict(legs((-60, 70), (-45, 70), (25, 0), (25, 0)), Body=(-25, 0, 0), Neck=(-15, 0, 0),
                HorseHead=(-10, 0, 0))
    strike = dict(legs((-30, 0), (-25, 10), (-5, 0), (-5, 0)), Body=(5, 0, 0), Neck=(12, 0, 0))
    fold = legs((-10, 30), (-10, 30), (10, -20), (10, -20))
    down = legs((-30, 60), (-15, 40), (-20, -40), (-35, -50))
    kneel = (0.8, [
        (0.0, {}, {}),
        (0.25, dict(legs((-15, 60), (-10, 50), (10, -20), (10, -20)), Body=(10, 0, 0), Neck=(10, 0, 0)),
         {"Body": (0, 0, -0.15)}),
        (0.5, dict(legs((-30, 100), (-30, 100), (30, -60), (30, -60)), Body=(6, 0, 0), Neck=(25, 0, 0)),
         {"Body": (0, 0, -0.45)}),
        (0.8, dict(legs((-35, 120), (-35, 120), (45, -100), (45, -100)), Body=(2, 0, 4), Neck=(35, 0, 0),
                   HorseHead=(15, 0, 0)), {"Body": (0, 0, -0.6)}),
    ])
    clips = {
        "Idle": (2.0, [
            (0.0, {}, {}),
            (1.0, {"Neck": (6, 0, 0), "HorseHead": (5, 0, 0), "Tail": (0, 15, 0)}, {"Body": (0, 0, -0.01)}),
            (2.0, {}, {}),
        ]),
        "Walk": (1.0, [
            (0.0, a, {}),
            (0.25, b, {"Body": (0, 0, 0.03)}),
            (0.5, c, {}),
            (0.75, d, {"Body": (0, 0, 0.03)}),
            (1.0, a, {}),
        ]),
        "Attack": (0.6, [
            (0.0, {}, {}),
            (0.2, rear, {}),
            (0.35, strike, {"Body": (0, -0.06, -0.02)}),
            (0.6, {}, {}),
        ]),
        "Hurt": (0.4, [
            (0.0, {}, {}),
            (0.12, {"Neck": (-20, 0, 0), "HorseHead": (-20, 0, 0), "Body": (-4, 0, 0)}, {"Body": (0, 0.05, 0)}),
            (0.4, {}, {}),
        ]),
        "Death": (0.8, [
            (0.0, {}, {}),
            (0.25, dict(fold, Body=(0, 10, 0), Neck=(10, 0, 0)), {"Body": (0, 0, -0.08)}),
            (0.5, dict(down, Body=(0, 45, 0), Neck=(20, 0, 0)), {"Body": (0.12, 0, -0.35)}),
            (0.8, dict(down, Body=(0, 85, 0), Neck=(25, 0, 0), HorseHead=(10, 0, 0)), {"Body": (0.3, 0, -0.75)}),
        ]),
    }
    if mounted:
        clips["Death"] = kneel
    return clips


def merge_clips(a, b):
    out = {}
    for name, (dur, ka) in a.items():
        kb = b[name][1]
        assert [k[0] for k in ka] == [k[0] for k in kb], name
        out[name] = (dur, [(t, {**ra, **rb}, {**la, **lb}) for (t, ra, la), (_, rb, lb) in zip(ka, kb)])
    return out


def archer_clips():
    c = knight_clips()
    draw = {"UpperArm.L": (-85, 0, 0), "LowerArm.L": (20, 0, 0), "Hand.L": (65, 0, 0),
            "UpperArm.R": (-80, 0, 0), "LowerArm.R": (-70, 0, 0), "Spine": (0, 0, -25), "Head": (0, 0, 20)}
    loose = dict(draw, **{"UpperArm.R": (-75, 0, -10), "LowerArm.R": (-30, 0, 0)})
    c["Attack"] = (0.6, [(0.0, {}, {}), (0.2, draw, {}), (0.35, loose, {}), (0.6, {}, {})])
    return c


def mage_clips():
    c = knight_clips()
    c["Attack"] = (0.6, [
        (0.0, {}, {}),
        (0.2, {"UpperArm.R": (-130, 0, 0), "Hand.R": (110, 0, 0), "UpperArm.L": (-60, 0, 0), "Spine": (-8, 0, 0)}, {}),
        (0.35, {"UpperArm.R": (-60, 0, 0), "LowerArm.R": (20, 0, 0), "Hand.R": (90, 0, 0), "UpperArm.L": (-80, 0, 0),
                "LowerArm.L": (20, 0, 0), "Spine": (10, 0, 0)}, {}),
        (0.6, {}, {}),
    ])
    return c


def slinger_clips():
    c = knight_clips()
    c["Attack"] = (0.6, [
        (0.0, {}, {}),
        (0.12, {"UpperArm.R": (-150, 0, 20), "LowerArm.R": (-20, 0, 0), "Spine": (-6, 0, -15)}, {}),
        (0.24, {"UpperArm.R": (-170, 0, -25), "LowerArm.R": (-10, 0, 0), "Spine": (-8, 0, -20),
                "UpperArm.L": (-40, 0, 0)}, {}),
        (0.35, {"UpperArm.R": (-75, 0, 0), "LowerArm.R": (10, 0, 0), "Spine": (12, 0, 18),
                "UpperArm.L": (10, 0, 0), "UpperLeg.L": (-15, 0, 0), "LowerLeg.L": (10, 0, 0)}, {}),
        (0.6, {}, {}),
    ])
    return c
