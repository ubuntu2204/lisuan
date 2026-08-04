#!/usr/bin/env bash
#
# fix-steam-cef-swrender.sh — make Steam's CEF (steamwebhelper) use software
# rendering so the UI starts on Lisuan (砺算) 7G100, where the CEF GPU process
# crashes (exit_code=139) inside the pressure-vessel container because
# lisuan_dri.so is missing there.
#
# It patches steamwebhelper_sniper_wrap.sh to pass --disable-gpu
# --disable-gpu-compositing --use-gl=swiftshader to the CEF process.
# Steam UI runs via software rendering; games still use the GPU normally.
#
# Re-run this after every Steam client update (it overwrites the file).
# Idempotent — safe to run multiple times.
#
# Usage:  ./fix-steam-cef-swrender.sh
# License: Apache-2.0
set -euo pipefail

WRAP="$HOME/.local/share/Steam/ubuntu12_64/steamwebhelper_sniper_wrap.sh"

[[ -f "$WRAP" ]] || { echo "Not found: $WRAP (is Steam installed?)" >&2; exit 1; }

MARKER='--use-gl=swiftshader'

if grep -qF -- "$MARKER" "$WRAP"; then
  echo "==> Already patched — nothing to do."
  exit 0
fi

cp -a "$WRAP" "$WRAP.bak"
# Replace the final 'exec ./steamwebhelper "$@"' line with a GPU-disabled version.
sed -i 's|^exec \./steamwebhelper "\$@"$|exec ./steamwebhelper --disable-gpu --disable-gpu-compositing --use-gl=swiftshader "$@"|' "$WRAP"

if grep -qF -- "$MARKER" "$WRAP"; then
  echo "==> Patched $WRAP (backup: $WRAP.bak)"
  echo "==> CEF will now use software rendering. Start Steam normally."
else
  echo "!! Patch did not apply — the file format may have changed." >&2
  echo "!! Inspect $WRAP and add --disable-gpu --use-gl=swiftshader to the exec line." >&2
  exit 1
fi
