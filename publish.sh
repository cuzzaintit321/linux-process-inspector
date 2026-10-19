#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")"
gh auth status
gh auth setup-git
if [[ -e .git ]]; then echo 'Git checkout exists; review before publishing manually.'; exit 1; fi
if gh repo view cuzzaintit321/linux-process-inspector >/dev/null 2>&1; then
    echo 'Repository exists; review before publishing manually.'; exit 1
fi
git init -b main
git add .
git -c user.name='Jason Edwards' \
    -c user.email='228125969+cuzzaintit321@users.noreply.github.com' \
    commit -m 'Add C process inspector with filtering, JSON, tests, and sanitizers'
gh repo create cuzzaintit321/linux-process-inspector --public --source . --remote origin --push \
    --description 'Read-only Linux process inspector written in C, with /proc parsing, memory sorting, UID/name filters, JSON output, and sanitizer-tested code.'
