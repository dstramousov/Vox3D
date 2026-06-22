#!/usr/bin/env bash
set -euo pipefail

case "${1:-}" in
  ""|--clean)
    make clean
    ;;
  --cmake|--cmake-clean)
    make cmake-clean
    ;;
  --dist|--distclean)
    make distclean
    rm -f ./*.zip
    ;;
  --deps|--depsclean)
    make depsclean
    ;;
  *)
    echo "usage: ./c [--clean|--cmake|--dist|--deps]" >&2
    exit 2
    ;;
esac
