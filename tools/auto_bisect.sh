#!/usr/bin/env bash
# tools/auto_bisect.sh <good> <bad>
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "Usage: $0 <good_commit> <bad_commit>" >&2
  exit 2
fi

good="$1"
bad="$2"

cat > .git-bisect-runner.sh <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
make clean
make uart_test.img || exit 125
./tools/perf_ci.sh
exit $?
EOF
chmod +x .git-bisect-runner.sh

git bisect start "$bad" "$good"
git bisect run ./.git-bisect-runner.sh
git bisect reset
