#!/usr/bin/env bash

# NOTE: This script is intended to be **sourced** so that `nvm use` affects
# your current shell:
#   source ./initOkxCli.sh
#   okx market orderbook BTC-USDT --sz 5
#
# You can also execute it, but the Node activation won't persist:
#   ./initOkxCli.sh

__INIT_OKXCLI_SOURCED=0
if [[ "${BASH_SOURCE[0]}" != "$0" ]]; then
  __INIT_OKXCLI_SOURCED=1
  __INIT_OKXCLI_SAVED_OPTS="$(set +o)"
fi

set -euo pipefail

# OKX CLI needs Node.js >= 16. This script activates an nvm-managed Node
# (default: 18) for the current shell and ensures `okx` is available.

OKX_NODE_MAJOR_MIN=16
OKX_NODE_MAJOR_DEFAULT=18

die() {
  echo "[initOkxCli] ERROR: $*" >&2
  if [[ "$__INIT_OKXCLI_SOURCED" -eq 1 ]]; then
    return 1
  fi
  exit 1
}

info() {
  echo "[initOkxCli] $*" >&2
}

load_nvm() {
  export NVM_DIR="${NVM_DIR:-$HOME/.nvm}"
  if [[ -s "$NVM_DIR/nvm.sh" ]]; then
    # shellcheck disable=SC1090
    . "$NVM_DIR/nvm.sh"
    return 0
  fi
  if command -v nvm >/dev/null 2>&1; then
    return 0
  fi
  return 1
}

ensure_node() {
  local want_major="$1"

  load_nvm || die "nvm not found. Install nvm first, then re-run this script. (Expected at $HOME/.nvm/nvm.sh)"

  if ! command -v node >/dev/null 2>&1; then
    info "node not found; installing Node ${want_major} via nvm"
    nvm install "${want_major}"
  fi

  local current_major
  current_major="$(node -p 'process.versions.node.split(".")[0]')" || die "failed to read node version"

  if [[ "$current_major" -lt "$OKX_NODE_MAJOR_MIN" ]]; then
    info "current node major is ${current_major} (<${OKX_NODE_MAJOR_MIN}); switching to Node ${want_major} via nvm"
    nvm install "${want_major}"
    nvm use "${want_major}"
  else
    # Prefer nvm-managed node when available.
    if nvm ls "${want_major}" >/dev/null 2>&1; then
      nvm use "${want_major}" >/dev/null
    fi
  fi

  local final_major
  final_major="$(node -p 'process.versions.node.split(".")[0]')" || die "failed to read node version (post-switch)"
  if [[ "$final_major" -lt "$OKX_NODE_MAJOR_MIN" ]]; then
    die "Node.js >= ${OKX_NODE_MAJOR_MIN} required, but got $(node --version)"
  fi

  info "using node $(node --version) ($(command -v node))"
  info "using npm  $(npm --version) ($(command -v npm))"
}

ensure_okx_cli() {
  if command -v okx >/dev/null 2>&1; then
    info "okx already available: $(command -v okx)"
    return 0
  fi

  info "okx not found; installing: npm install -g @okx_ai/okx-trade-cli"
  npm install -g @okx_ai/okx-trade-cli

  command -v okx >/dev/null 2>&1 || die "okx still not found after install; check npm global bin in PATH"
  info "okx installed: $(command -v okx)"
}

main() {
  ensure_node "${OKX_NODE_MAJOR_DEFAULT}"
  ensure_okx_cli

  # Print a small hint for next commands.
  info "ready. Example: okx market orderbook BTC-USDT --sz 5"
}

main "$@"

if [[ "$__INIT_OKXCLI_SOURCED" -eq 1 ]]; then
  # Restore caller shell options.
  eval "${__INIT_OKXCLI_SAVED_OPTS}"
  unset __INIT_OKXCLI_SAVED_OPTS
fi
unset __INIT_OKXCLI_SOURCED
