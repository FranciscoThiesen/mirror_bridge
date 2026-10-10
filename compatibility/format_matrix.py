#!/usr/bin/env python3
"""Render compatibility/results.json as the published matrix.

The point of the table is the last column. A pass rate says how we are doing;
the reason each library fails says what to do next, and groups the corpus into
a work queue instead of a scoreboard.
"""

import argparse
import datetime
import json
import sys

# Rows are ordered by how actionable they are, not alphabetically: a category
# with a known cause belongs above one that is still a mystery.
CATEGORY_ORDER = [
    "ok", "needs-linking", "imports-failed", "export-macro-discovery",
    "duplicate-binding-name", "reflection-tu-failed",
    "unsupported-container", "inaccessible-destructor",
    "private-nested-type", "no-converter-for-parameter",
    "compiler-crash", "incomplete-type",
    "template-depth", "constexpr-limit", "no-classes-found",
    "header-needs-flags", "manifest-wrong", "compile-error", "timeout",
    "fetch-failed", "unknown",
]

STATUS_MARK = {
    "ok": "binds",
    "needs-linking": "builds, needs the library linked",
    "imports-failed": "builds, will not import",
    "failed": "no module",
    "timeout": "timed out",
    "fetch-failed": "not fetched",
}


def fmt_bytes(n):
    if not n:
        return "-"
    if n < 1024 * 1024:
        return "%.0f KB" % (n / 1024.0)
    return "%.1f MB" % (n / (1024.0 * 1024.0))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("results", nargs="?", default="compatibility/results.json")
    ap.add_argument("--commit", default="unknown")
    ap.add_argument("--env", default="unspecified machine")
    ap.add_argument("--fragment", action="store_true",
                    help="emit only the spliced section, without the page around it")
    args = ap.parse_args()

    with open(args.results) as f:
        payload = json.load(f)
    rows = payload["results"]

    rank = {c: i for i, c in enumerate(CATEGORY_ORDER)}
    rows.sort(key=lambda r: (rank.get(r.get("category", "unknown"), 99), r["name"].lower()))

    ok = [r for r in rows if r["status"] == "ok"]
    oo = [r for r in rows if r.get("shape") == "oo"]
    tpl = [r for r in rows if r.get("shape") == "template"]
    oo_ok = [r for r in oo if r["status"] == "ok"]
    tpl_ok = [r for r in tpl if r["status"] == "ok"]

    when = datetime.datetime.utcfromtimestamp(payload["generated_unix"]).strftime("%Y-%m-%d")
    out = []
    w = out.append

    w("_Generated %s at commit `%s` on %s._" % (when, args.commit[:12], args.env))
    w("")
    w("**%d of %d libraries bind end to end** — %d of %d class-based, %d of %d "
      "header-only generic. Every library is pinned to the tag shown and nothing "
      "here is written or patched by us: the command is `mirror_bridge generate "
      "<include dir> --module <name> --lang python`, run against an unmodified "
      "checkout." % (len(ok), len(rows), len(oo_ok), len(oo), len(tpl_ok), len(tpl)))
    w("")
    refl = [r for r in rows if r.get("discovery") == "reflection"]
    w("Discovery ran through reflection on **%d of %d**; the rest fell back to the "
      "text scan, which the CLI reports as it happens. A `text-scan` row means "
      "reflection could not read those headers, so its class list is the old "
      "heuristic's and the failure cause belongs to that, not to binding."
      % (len(refl), len(rows)))
    w("")
    w("| Library | Version | Shape | Discovery | Classes seen | Module | Time | Why not, if not |")
    w("|---|---|---|---|---|---|---|---|")
    for r in rows:
        mark = STATUS_MARK.get(r["status"], r["status"])
        if r["status"] == "ok":
            module = "%s (%s)" % (mark, fmt_bytes(r.get("module_bytes", 0)))
            why = ""
        else:
            module = mark
            why = r.get("reason", "") or r.get("category", "")
            if r.get("category") in ("compile-error", "unknown"):
                why = "**%s** — %s" % (r.get("category"), why)
        w("| [%s](%s) | `%s` | %s | %s | %s | %s | %ss | %s |" % (
            r["name"], r.get("repo", ""), r["ref"], r.get("shape", "?"),
            r.get("discovery", "?"), r.get("classes_found", "-"), module,
            r.get("seconds", "-"), why))
    w("")

    # The work queue. One line per cause, with the libraries it blocks, so the
    # next thing to fix is the row with the most names on it.
    blocked = {}
    for r in rows:
        if r["status"] == "ok":
            continue
        blocked.setdefault(r.get("category", "unknown"), []).append(r["name"])
    if blocked:
        w("### What is in the way")
        w("")
        w("| Cause | Libraries | Count |")
        w("|---|---|---|")
        for cat in sorted(blocked, key=lambda c: (-len(blocked[c]), rank.get(c, 99))):
            w("| `%s` | %s | %d |" % (cat, ", ".join(sorted(blocked[cat])), len(blocked[cat])))
        w("")

    text = "\n".join(out)
    if args.fragment:
        sys.stdout.write(text + "\n")
        return 0

    sys.stdout.write(text + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
