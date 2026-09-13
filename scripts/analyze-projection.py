"""Compare saved observations with a candidate SDK raster convention; never changes acceptance."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def unit(a):
    length = math.sqrt(dot(a, a))
    return [x / length for x in a]


def cross(a, b):
    return [a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0]]


def analyze(path):
    raw = path.read_bytes()
    report = json.loads(raw)
    state = report.get("host", report)
    observations = iter(state["observations"])
    rows = []
    for measurement in state["measurements"]:
        rw, rh = measurement["render_size"]
        pw, ph = measurement["physical_size"]
        if min(rw, rh) <= 2:
            raise ValueError("Viewport is too small")
        errors, precise_errors = [], []
        for diagnostic in measurement["projection_diagnostics"]:
            observation = next(observations)
            camera = observation["camera"]
            forward = unit(camera["direction"])
            right = unit(cross(forward, camera["up"]))
            up = unit(cross(right, forward))
            delta = [p-c for p, c in zip(observation["world"], camera["position"])]
            depth = dot(delta, forward)
            if depth <= 0:
                raise ValueError("Reference point is behind the camera")
            focal = min(rw-1, rh-1) / (2*math.tan(camera["field_sdk"]/2))
            # Hypothesis from landscape samples, checked on other poses/FOVs/aspects.
            expected = [((rw-1)/2 + focal*dot(delta, right)/depth)*(rw-2)/(rw-1),
                        ((rh-1)/2 - focal*dot(delta, up)/depth)*(rh-2)/(rh-1)]
            physical = [math.trunc(expected[0])*pw/rw, math.trunc(expected[1])*ph/rh]
            errors.append(math.dist(physical, observation["actual_physical_px"]))
            if "model_double_px" in diagnostic:
                precise_errors.append(math.hypot((expected[0]-diagnostic["model_double_px"][0])*pw/rw,
                                                (expected[1]-diagnostic["model_double_px"][1])*ph/rh))
        if not errors:
            raise ValueError("Report has no precise diagnostics; measure using the updated DLL")
        rows.append({"dpi": measurement["dpi"], "render_size": [rw, rh], "points": len(errors),
                     "candidate_world_integer_max_physical_px": max(errors),
                     "model_double_comparison_max_physical_px": max(precise_errors) if precise_errors else None})
    if not rows or next(observations, None) is not None:
        raise ValueError("Observation and diagnostic counts do not match")
    return {"source_sha256": hashlib.sha256(raw).hexdigest(), "hypothesis_only": True,
            "candidate": "minimum_radians_full; aspect=(rw-1)/(rh-1); pixel spans=(rw-2,rh-2); truncate toward zero",
            "limitation": "Model-to-view doubles are diagnostic, not independently verified world coordinates. Physical photo alignment and other DPI settings remain unverified.",
            "host_acceptance_unchanged": state["fov_resolved"], "measurements": rows}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = json.dumps(analyze(args.report), indent=2, allow_nan=False)
    if args.output:
        args.output.write_text(result, encoding="utf-8")
    print(result)
