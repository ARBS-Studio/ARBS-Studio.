#!/bin/bash
# =============================================================================
# ARBS Studio - instaluesi i vetem per macOS (.pkg)
# Perdorimi: Installer/macOS/build_pkg.sh <dist-dir> <version>
#   ku <dist-dir>/AC28/ARBS_Studio.bundle, AC29/..., AC30/...
# Paketa kopjon bundle-in ne "Add-Ons" te cdo Archicad-i te gjetur ne /Applications/Graphisoft.
# =============================================================================
set -euo pipefail
DIST="${1:-dist}"
VERSION="${2:-0.1.0}"
WORK="$(mktemp -d)"
PAYLOAD="$WORK/payload/Library/Application Support/ARBS Studio"
SCRIPTS="$WORK/scripts"
mkdir -p "$PAYLOAD" "$SCRIPTS"

for v in 28 29 30; do
  if [ -d "$DIST/AC$v/ARBS_Studio.bundle" ]; then
    mkdir -p "$PAYLOAD/Archicad $v"
    cp -R "$DIST/AC$v/ARBS_Studio.bundle" "$PAYLOAD/Archicad $v/"
  fi
done

cat > "$SCRIPTS/postinstall" <<'POST'
#!/bin/bash
SRC="/Library/Application Support/ARBS Studio"
for v in 28 29 30; do
  [ -d "$SRC/Archicad $v/ARBS_Studio.bundle" ] || continue
  for app in "/Applications/Graphisoft/Archicad $v" "/Applications/GRAPHISOFT/Archicad $v" "/Applications/Graphisoft/ARCHICAD $v"; do
    if [ -d "$app/Add-Ons" ]; then
      mkdir -p "$app/Add-Ons/ARBS Studio"
      rm -rf "$app/Add-Ons/ARBS Studio/ARBS_Studio.bundle"
      cp -R "$SRC/Archicad $v/ARBS_Studio.bundle" "$app/Add-Ons/ARBS Studio/"
      xattr -dr com.apple.quarantine "$app/Add-Ons/ARBS Studio/ARBS_Studio.bundle" 2>/dev/null || true
    fi
  done
done
exit 0
POST
chmod +x "$SCRIPTS/postinstall"

pkgbuild --root "$WORK/payload" --scripts "$SCRIPTS" --identifier "com.arbsstudio.archicad.addon" \
         --version "$VERSION" --install-location "/" "$DIST/ARBS_Studio_Setup_${VERSION}.pkg"
echo "U krijua: $DIST/ARBS_Studio_Setup_${VERSION}.pkg"
