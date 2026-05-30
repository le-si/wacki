#!/usr/bin/env bash
# T47 — depack regression harness.
#
# Re-extracts every file from DANE_02.DTA into a temp dir, compares SHA-256
# of every file against the baseline manifest stored in this repo. Any
# divergence indicates the depacker has regressed (or the archive itself
# changed — unlikely since we ship from the 1997 master DTA).
#
# Usage:
#   ./tools/dta-validate.sh                  # default ./data/DANE_02.DTA
#   ./tools/dta-validate.sh /path/to/X.dta   # explicit archive
#
# Exit codes:
#   0 = all 1782 files match baseline
#   1 = build artifact missing
#   2 = extraction failed (depack returned errors)
#   3 = checksum mismatch (regression)
set -u

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
EXTRACTOR="$REPO_ROOT/dta-extract"
BASELINE="$REPO_ROOT/tools/dta-baseline.sha256"
ARCHIVE="${1:-$REPO_ROOT/data/DANE_02.DTA}"

if [[ ! -x "$EXTRACTOR" ]]; then
  echo "[validate] missing $EXTRACTOR — run 'make tools' first" >&2
  exit 1
fi
if [[ ! -r "$ARCHIVE" ]]; then
  echo "[validate] cannot read archive: $ARCHIVE" >&2
  exit 1
fi
if [[ ! -r "$BASELINE" ]]; then
  echo "[validate] missing baseline: $BASELINE" >&2
  exit 1
fi

TMP="$(mktemp -d "${TMPDIR:-/tmp}/wacki-dta-validate.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

echo "[validate] extracting $ARCHIVE → $TMP"
log="$TMP/_extract.log"
"$EXTRACTOR" "$ARCHIVE" "$TMP" >"$log" 2>&1
if grep -qE "depack|underflow|bail|runaway|skip " "$log"; then
  echo "[validate] depack reported warnings:" >&2
  grep -E "depack|underflow|bail|runaway|skip " "$log" >&2
  exit 2
fi

n_extracted=$(grep -c "bytes)" "$log" || true)
n_baseline=$(wc -l < "$BASELINE" | tr -d ' ')
if [[ "$n_extracted" != "$n_baseline" ]]; then
  echo "[validate] count mismatch: extracted=$n_extracted baseline=$n_baseline" >&2
  exit 3
fi

echo "[validate] checking $n_extracted file checksums against baseline"
cd "$TMP"
rm -f _extract.log
if shasum -a 256 -c "$BASELINE" --quiet >"$TMP/check.log" 2>&1; then
  echo "[validate] PASS — all $n_extracted files match baseline"
  exit 0
fi

echo "[validate] FAIL — checksum mismatches:" >&2
cat "$TMP/check.log" >&2 | head -20
exit 3
