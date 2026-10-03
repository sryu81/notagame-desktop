#!/usr/bin/env bash
# Run every lab's auto-drive in an ASan/UBSan build; fail on a red check or a sanitizer report.
# Usage: scripts/verify/run_labs.sh [out_dir]   (screenshots + log land in out_dir)
set -uo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build="$root/build-asan"
out="$(mkdir -p "${1:-$build/verify}" && cd "${1:-$build/verify}" && pwd)"
labs=(renderlab animlab fxlab cameralab systemslab unitlab tacticslab levellab ailab)

[[ -f "$build/build.ninja" ]] || cmake -S "$root" -B "$build" -G Ninja -DNAG_SANITIZE=ON >/dev/null
cmake --build "$build" || exit 1

rm -rf "$out"/*.png "$out/data" "$out/log.txt"
mkdir -p "$out/data"
(cd "$out" && ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=print_stacktrace=1 \
    timeout 600 "$build/not_a_game" --data "$out/data" --start hub \
    --script "$root/scripts/verify/labs.txt") > "$out/log.txt" 2>&1
status=$?

fail=0
[[ $status -eq 0 ]] || { echo "FAIL: exit status $status"; fail=1; }
if grep -qE "ERROR: AddressSanitizer|runtime error:" "$out/log.txt"; then
    echo "FAIL: sanitizer report"; grep -m5 -E "ERROR: AddressSanitizer|runtime error:" "$out/log.txt"; fail=1
fi
for lab in "${labs[@]}"; do
    line=$(grep -m1 "\[$lab\] auto-drive done:" "$out/log.txt" || true)
    if [[ -z "$line" ]]; then
        echo "FAIL: $lab did not finish"; fail=1; continue
    fi
    counts=$(sed -E 's/.*done: ([0-9]+)\/([0-9]+).*/\1 \2/' <<<"$line")
    read -r got want <<<"$counts"
    if [[ "$got" == "$want" ]]; then echo "ok   $lab $got/$want"; else echo "FAIL: $lab $got/$want"; fail=1; fi
done
grep -q "\[level\] loaded" "$out/log.txt" || { echo "FAIL: battle level not loaded"; fail=1; }
echo "log + screenshots: $out"
exit $fail
