#!/usr/bin/env bash
set -euo pipefail

make run ARGS="${*} --config=config/app.json --log-level=debug"
