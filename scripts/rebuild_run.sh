#!/usr/bin/env bash
# Clean rebuild + launch. Usage: scripts/rebuild_run.sh [--asan] [--no-run] [-- game args...]
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="$root/build"
cmake_args=()
run=1

while [[ $# -gt 0 ]]; do
    case "$1" in
        --asan)   build_dir="$root/build-asan"; cmake_args+=(-DNAG_SANITIZE=ON); shift ;;
        --no-run) run=0; shift ;;
        --)       shift; break ;;
        *)        break ;;
    esac
done

rm -rf "$build_dir"
cmake -S "$root" -B "$build_dir" -G Ninja "${cmake_args[@]}"
cmake --build "$build_dir"

if [[ $run -eq 1 ]]; then
    exec "$build_dir/not_a_game" "$@"
fi
