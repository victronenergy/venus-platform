#!/usr/bin/env python3

"""
Finds tr() strings in the source (discovered via `lupdate`, so Qt's own
context/location resolution is used rather than a hand-rolled string search)
that are not yet present in POEditor, and uploads them.

This closes the gap `scripts/download_from_poeditor.py` doesn't cover: that
script only ever pulls translations *from* POEditor. Nothing previously
pushed newly-added or newly-reworded tr() strings *to* POEditor, so they
silently went untranslated.

Requires `lupdate` (ships with Qt, e.g. via `brew install qt`) on PATH.

Usage:
    POEDITOR_TOKEN=... python3 scripts/upload_new_terms.py [--dry-run] [--out report.json]
"""

import argparse
import json
import os
import subprocess
import sys
import xml.etree.ElementTree as ET

from poeditor import POEditorAPI

PROJECT_ID = "669797"
CHUNK_SIZE = 100


def get_git_root():
    return subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], stdout=subprocess.PIPE, check=True
    ).stdout.rstrip().decode("utf-8")


def run_lupdate(repo_root):
    pro_dir = os.path.join(repo_root, "venus-platform")
    print("Running lupdate (refreshes every translations/*.ts with current source strings)...")
    result = subprocess.run(
        ["lupdate", "venus-platform.pro"], cwd=pro_dir, capture_output=True, text=True
    )
    if result.returncode != 0:
        print(result.stdout)
        print(result.stderr, file=sys.stderr)
        raise SystemExit("lupdate failed")


def parse_ts(ts_path):
    """Returns a list of (context, source_text, location) for every live
    (non-obsolete) message in a .ts file. Any one language's .ts works here:
    <source>/<location> data is identical across languages after lupdate,
    only <translation> differs."""
    tree = ET.parse(ts_path)
    entries = []
    for context_el in tree.getroot().findall("context"):
        ctx = (context_el.find("name").text or "").strip()
        for message_el in context_el.findall("message"):
            translation_el = message_el.find("translation")
            if translation_el is not None and translation_el.attrib.get("type") == "obsolete":
                continue
            source_el = message_el.find("source")
            if source_el is None or source_el.text is None:
                continue
            location_el = message_el.find("location")
            location = (
                f"{location_el.attrib.get('filename')}:{location_el.attrib.get('line')}"
                if location_el is not None
                else ""
            )
            entries.append((ctx, source_el.text, location))
    return entries


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--repo-root", default=get_git_root(), help="venus-platform checkout root"
    )
    parser.add_argument(
        "--ts",
        default="translations/venus_de.ts",
        help="which .ts file to read source strings from after lupdate "
        "(any language works, source/context/location data is shared)",
    )
    parser.add_argument("--dry-run", action="store_true", help="don't upload, just report")
    parser.add_argument("--out", help="also write the new-terms payload to this JSON file")
    args = parser.parse_args()

    token = os.environ.get("POEDITOR_TOKEN")
    if not token:
        raise SystemExit("Please set the POEDITOR_TOKEN environment variable")
    client = POEditorAPI(token)

    run_lupdate(args.repo_root)

    ts_path = os.path.join(args.repo_root, args.ts)
    entries = parse_ts(ts_path)
    print(f"lupdate found {len(entries)} live (context, source) pairs in {args.ts}")

    print("Fetching current POEditor terms...")
    existing_terms = client.view_project_terms(PROJECT_ID)
    existing_pairs = {(t.get("context", ""), t["term"]) for t in existing_terms}
    print(f"POEditor currently has {len(existing_pairs)} (context, term) pairs")

    missing = []
    seen = set()
    for ctx, source, location in entries:
        key = (ctx, source)
        if key in existing_pairs or key in seen:
            continue
        seen.add(key)
        missing.append(
            {"term": source, "context": ctx, "plural": "", "reference": location, "comment": ""}
        )

    print(f"{len(missing)} new term(s) to add")
    for m in missing:
        print(f"  + [{m['context']}] {m['term']!r}  ({m['reference']})")

    if args.out:
        with open(args.out, "w") as f:
            json.dump(missing, f, ensure_ascii=False, indent=2)
        print(f"Wrote payload to {args.out}")

    if not missing:
        return
    if args.dry_run:
        print("Dry run - not uploading.")
        return

    added_total = 0
    for i in range(0, len(missing), CHUNK_SIZE):
        chunk = missing[i : i + CHUNK_SIZE]
        result = client.add_terms(PROJECT_ID, chunk)
        added_total += len(result.get("added", []))
        print(f"  uploaded batch {i // CHUNK_SIZE + 1}: {result}")

    print(f"Done. {added_total} term(s) added to POEditor.")


if __name__ == "__main__":
    sys.exit(main())
