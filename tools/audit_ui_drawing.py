"""Record static Ui drawing dependencies; this is not GPU/control acceptance."""
import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

PATTERNS = {
    "native_drawing": r"\b(?:BeginNative|EndNative|GetDC|GetHDC|HDC|SystemDraw|CreateCompatibleDC)\b",
    "cpu_raster": r"\b(?:BufferPainter|ImagePainter|ImageDraw|UiRasterCache)\b",
    "painter_path": r"\b(?:Painter|UiPainterShapePath|UiPaintCircularArc)\b",
    "text": r"\b(?:DrawText|GetTextSize|GetFontInfo|StdFont|GetTextWidth)\b",
    "image": r"\b(?:DrawImage|CropImage|CachedSetColorKeepAlpha)\b",
    "drawing_helpers": r"\b(?:UiDraw\w*|UiPaint\w*|UiRender\w*)\b",
}

def sha(data):
    return hashlib.sha256(data).hexdigest()

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ui", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    root = args.ui.resolve()
    inventory = root / "tests/ui_release_inventory.json"
    data = inventory.read_bytes()
    inventory_data = json.loads(data.decode("utf-8-sig"))
    controls = inventory_data["controls"]
    rows = []
    support = []
    entries = [(control, rows) for control in controls]
    entries += [({"type": Path(header).stem, "header": header}, support)
                for header in inventory_data.get("support_headers", [])]
    for control, destination in entries:
        header = (root / control["header"]).resolve()
        header.relative_to(root)
        paths = sorted(set([header, *header.parent.glob(header.stem + "*.cpp")]))
        observed = []
        files = {}
        for path in paths:
            path.resolve().relative_to(root)
            if not path.is_file():
                continue
            raw = path.read_bytes()
            relative = path.relative_to(root).as_posix()
            files[relative] = sha(raw)
            for line_number, line in enumerate(raw.decode("utf-8-sig").splitlines(), 1):
                # Comments are retained as source observations, never proof a path executes.
                for category, pattern in PATTERNS.items():
                    if re.search(pattern, line):
                        observed.append({"category": category, "file": relative,
                                         "line": line_number, "text": line.strip()})
        destination.append({"type": control["type"], "header": control["header"],
                     "family": control.get("family"), "files": files,
                     "observations": observed, "gpu_acceptance": "unverified"})
    git = ["git", "-c", "safe.directory=" + root.as_posix(), "-C", str(root)]
    report = {
        "schema": 1,
        "scope": "Static source observations, not call-graph completeness, replacement parity or native GPU acceptance",
        "ui_head": subprocess.check_output(git + ["rev-parse", "HEAD"]).decode().strip(),
        "ui_dirty": bool(subprocess.check_output(git + ["status", "--porcelain"]).strip()),
        "inventory_sha256": sha(data),
        "control_count": len(rows),
        "category_control_counts": {category: sum(any(o["category"] == category for o in row["observations"]) for row in rows) for category in PATTERNS},
        "controls": rows,
        "support": support,
        "support_count": len(support),
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k not in ("controls", "support")}, indent=2))

if __name__ == "__main__":
    main()
