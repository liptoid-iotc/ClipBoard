#!/usr/bin/env bash
# Installs clipboard-manager for the current user only (no root needed).
set -e

BIN_DIR="$HOME/.local/bin"
AUTOSTART_DIR="$HOME/.config/autostart"

echo "Building..."
make clean && make

mkdir -p "$BIN_DIR" "$AUTOSTART_DIR"
cp -f clipboard-manager "$BIN_DIR/clipboard-manager"

sed "s#/usr/local/bin/clipboard-manager#$BIN_DIR/clipboard-manager#" \
    clipboard-manager.desktop > "$AUTOSTART_DIR/clipboard-manager.desktop"

echo ""
echo "Installed to: $BIN_DIR/clipboard-manager"
echo "Autostart entry: $AUTOSTART_DIR/clipboard-manager.desktop"
echo ""
if ! echo "$PATH" | grep -q "$HOME/.local/bin"; then
    echo "NOTE: $BIN_DIR is not in your PATH. Add this to your ~/.bashrc:"
    echo "  export PATH=\"\$HOME/.local/bin:\$PATH\""
fi
echo ""
echo "The app will start automatically next login, or run it now with:"
echo "  $BIN_DIR/clipboard-manager &"
