#!/bin/bash
set -euo pipefail

repo_root="${1:?pass candidate source-tree root}"
remote_root="${2:?pass isolated remote source root}"
case "$remote_root" in
	/data3/sujie/experiments/caat-optimization/*/source-*) ;;
	*) echo "refusing non-isolated remote source root: $remote_root" >&2; exit 2 ;;
esac

file=genmc/genmc/Execution/EventLabel.hpp
cd "$repo_root"
test -f "$file"
rsync -anR -e "ssh -p 36722" "./$file" "server@frp-arm.com:$remote_root/"
rsync -avR -e "ssh -p 36722" "./$file" "server@frp-arm.com:$remote_root/"
local_hash="$(sha256sum "$file" | cut -d' ' -f1)"
remote_hash="$(ssh -p 36722 server@frp-arm.com "sha256sum '$remote_root/$file'" | cut -d' ' -f1)"
test "$local_hash" = "$remote_hash"
printf 'Verified %s at %s\n' "$file" "$remote_root"
