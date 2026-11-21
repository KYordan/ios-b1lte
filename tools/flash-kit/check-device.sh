#!/bin/bash
# check-device.sh — Vérifie l'état du téléphone avant toute opération de flash
# Téléphone cible : celui de B2L_BOARD (../b1l/board.sh) — S10 SM-G973F par défaut,
# S10e SM-G970F avec B2L_BOARD=beyond0lte — en TWRP

set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../b1l/board.sh"

adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
EXPECTED_MODEL="$B2L_MODEL"

echo "=== Vérification du téléphone (check-device.sh) ==="
echo ""

# 1. État TWRP
STATE=$($adb get-state 2>/dev/null </dev/null || echo "unknown")
echo "-> État adb : $STATE"
if [ "$STATE" != "recovery" ]; then
  echo "ERREUR : le téléphone n'est pas en TWRP/recovery. État actuel : $STATE"
  echo "   -> Redémarre en recovery (Vol- + Power, puis Vol+ + Power)"
  exit 1
fi
echo "   OK : téléphone en TWRP"

# 2. Root
DEVICE_UID=$($adb shell id -u 2>/dev/null </dev/null | tr -d '\r' || echo "?")
echo "-> UID sur le téléphone : $DEVICE_UID"
if [ "$DEVICE_UID" != "0" ]; then
  echo "ERREUR : le téléphone n'est pas root (uid=$DEVICE_UID)"
  exit 1
fi
echo "   OK : root (uid=0)"

# 3. Modèle (ro.boot.em.model est le seul fiable sur ROM custom)
MODEL=$($adb shell getprop ro.boot.em.model 2>/dev/null </dev/null | tr -d '\r' || echo "?")
echo "-> ro.boot.em.model : $MODEL"
if [ "$MODEL" != "$EXPECTED_MODEL" ]; then
  echo "ERREUR : modèle inattendu ($MODEL != $EXPECTED_MODEL). ro.product.device est falsifiable sur ROM custom."
  exit 1
fi
echo "   OK : modèle $EXPECTED_MODEL confirmé"

# 4. Partitions cibles et leurs blocs
echo ""
echo "=== Partitions cibles ==="
for entry in "boot:$B2L_BOOT_DEV:$B2L_BOOT_BYTES" "system:$B2L_SYSTEM_DEV:$B2L_SYSTEM_BYTES" "userdata:$B2L_USERDATA_DEV:$B2L_USERDATA_BYTES"; do
  name="${entry%%:*}"
  rest="${entry#*:}"
  blk="${rest%%:*}"
  size="${rest##*:}"
  dev=$($adb shell readlink -f /dev/block/by-name/$name 2>/dev/null </dev/null | tr -d '\r' || echo "?")
  actual=$($adb shell blockdev --getsize64 $dev 2>/dev/null </dev/null | tr -d '\r' || echo "?")
  mounted=$($adb shell "grep -q '^$dev ' /proc/mounts && echo MONTÉ || echo libre" </dev/null 2>/dev/null | tr -d '\r')
  echo "  $name -> $dev (attendu $blk) | taille: $actual (attendu $size) | $mounted"
  if [ "$dev" != "$blk" ]; then
    echo "   ERREUR : $name ne pointe pas vers $blk"
    exit 1
  fi
  if [ "$actual" != "$size" ]; then
    echo "   ERREUR : taille de $name incorrecte ($actual != $size)"
    exit 1
  fi
done

echo ""
echo "=== TOUTES LES VÉRIFICATIONS SONT OK ==="
echo "Le téléphone est prêt pour une opération de flash (avec backup d'abord)."
