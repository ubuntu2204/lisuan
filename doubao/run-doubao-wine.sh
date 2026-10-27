#!/usr/bin/env bash
# 豆包专用启动器：先修复前缀，再仅对本进程使用 Mesa 软件渲染。
set -euo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
"$SCRIPT_DIR/fix-doubao-wine.sh"

export __GLX_VENDOR_LIBRARY_NAME=mesa
export LIBGL_ALWAYS_SOFTWARE=1
export GBM_BACKEND=dri
export VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.json

# 原桌面入口在普通启动时也会留下裸 --uri，令客户端进入修复流程。
if [[ $# -eq 1 && ${1:-} == '--uri' ]]; then
    set --
fi

exec /opt/apps/doubao-wine/files/run.sh "$@"
