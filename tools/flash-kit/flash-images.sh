#!/bin/bash
# flash-images.sh — Flashe boot.img, system.img et userdata.img dans les partitions existantes
# Téléphone cible : celui de B2L_BOARD (../b1l/board.sh) — S10 SM-G973F par défaut,
# S10e SM-G970F avec B2L_BOARD=beyond0lte — en TWRP
# ⚠️ Écriture réelle sur les partitions. Gardes obligatoires avant chaque écriture.

set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../b1l/board.sh"

adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
IMAGES_DIR="${IMAGES_DIR:-$HOME/Developer/Port iOS 26 to S10/flash-kit/images}"

echo "=== Flash des images iOS (flash-images.sh) ==="
echo ""

# 1. Vérification préalable du téléphone
STATE=$($adb get-state 2>/dev/null </dev/null || echo "unknown")
if [ "$STATE" != "recovery" ]; then
  echo "ERREUR : téléphone pas en TWRP. État : $STATE"
  exit 1
fi
MODEL=$($adb shell getprop ro.boot.em.model 2>/dev/null </dev/null | tr -d '\r')
if [ "$MODEL" != "$B2L_MODEL" ]; then
  echo "ERREUR : modèle inattendu ($MODEL != $B2L_MODEL)"
  exit 1
fi
DEVICE_UID=$($adb shell id -u 2>/dev/null </dev/null | tr -d '\r')
if [ "$DEVICE_UID" != "0" ]; then
  echo "ERREUR : pas root (uid=$DEVICE_UID)"
  exit 1
fi
echo "Téléphone vérifié : TWRP root, $B2L_MODEL ($B2L_BOARD)"
echo ""

# 2. Vérifier que les images existent
for img in boot.img system.img userdata.img; do
  if [ ! -f "$IMAGES_DIR/$img" ]; then
    echo "ERREUR : $IMAGES_DIR/$img introuvable."
    echo "   Génère l'image avec la chaîne de build, puis place-la ici."
    exit 1
  fi
  echo "Trouvé : $img ($(stat -f %z "$IMAGES_DIR/$img" 2>/dev/null || stat -c %s "$IMAGES_DIR/$img") octets)"
done

# 3. Fonction de garde pour une écriture sur une partition
guard_write() {
  local name=$1 blk=$2 expected_size=$3
  local dev actual mounted
  dev=$($adb shell readlink -f /dev/block/by-name/$name 2>/dev/null </dev/null | tr -d '\r')
  if [ "$dev" != "$blk" ]; then
    echo "  GARDE ÉCHEC : $name -> $dev (attendu $blk)"
    return 1
  fi
  actual=$($adb shell blockdev --getsize64 $dev 2>/dev/null </dev/null | tr -d '\r')
  if [ "$actual" != "$expected_size" ]; then
    echo "  GARDE ÉCHEC : taille $name = $actual (attendu $expected_size)"
    return 1
  fi
  mounted=$($adb shell "grep -q '^$dev ' /proc/mounts && echo oui || echo non" </dev/null 2>/dev/null | tr -d '\r')
  if [ "$mounted" = "oui" ]; then
    echo "  GARDE ÉCHEC : $name est monté"
    return 1
  fi
  return 0
}

# 4. Flash d'une image avec readback SHA-256
flash_one() {
  local name=$1 blk=$2 expected_size=$3 img=$4
  echo "=== Flash de $name ($blk) ==="
  if ! guard_write "$name" "$blk" "$expected_size"; then
    echo "  ABANDON : garde échouée pour $name"
    return 1
  fi

  # Hash hôte
  HOST_HASH=$(shasum -a 256 "$img" | cut -d' ' -f1)
  echo "  Hash hôte : $HOST_HASH"

  # Push vers /tmp sur le téléphone
  $adb push "$img" /tmp/b1l-flash.img </dev/null
  PUSH_HASH=$($adb shell sha256sum /tmp/b1l-flash.img </dev/null | cut -d' ' -f1)
  echo "  Hash après push : $PUSH_HASH"
  if [ "$PUSH_HASH" != "$HOST_HASH" ]; then
    echo "  ERREUR : hash différent après push"
    $adb shell rm -f /tmp/b1l-flash.img </dev/null
    return 1
  fi

  # Écriture dd
  echo "  Écriture dd vers $blk..."
  $adb shell "dd if=/tmp/b1l-flash.img of=$blk bs=4096 conv=fsync && sync && rm -f /tmp/b1l-flash.img" </dev/null

  # Readback + comparaison
  echo "  Readback et vérification SHA-256..."
  # Relire exactement la longueur de l'image (BOOT est plus petit que la partition).
  local img_size
  img_size=$(stat -f %z "$img" 2>/dev/null || stat -c %s "$img")
  READBACK_HASH=$($adb shell "echo 3 > /proc/sys/vm/drop_caches; dd if=$blk bs=$img_size count=1 2>/dev/null | sha256sum" </dev/null | cut -d' ' -f1)
  echo "  Hash relu : $READBACK_HASH"
  if [ "$READBACK_HASH" = "$HOST_HASH" ]; then
    echo "  OK : $name écrit et vérifié (BOOT/SYSTEM/USERDATA-OK)"
  else
    echo "  ERREUR CRITIQUE : readback échoué pour $name !"
    return 1
  fi
  echo ""
}

# 5. Flash boot
flash_one "boot" "$B2L_BOOT_DEV" "$B2L_BOOT_BYTES" "$IMAGES_DIR/boot.img"

# 6. Flash system — grosse image ; si le téléphone stalle, utiliser flash-chunked.sh à la place.
flash_one "system" "$B2L_SYSTEM_DEV" "$B2L_SYSTEM_BYTES" "$IMAGES_DIR/system.img"

# 7. Flash userdata — TRÈS grosse partition : par chunks vérifiés uniquement.
echo "=== Flash de userdata ($B2L_USERDATA_DEV) ==="
echo "Utilise b1l-tools/flash-chunked.sh pour une écriture par chunks vérifiés :"
echo "  B2L_BOARD=$B2L_BOARD b1l-tools/flash-chunked.sh $IMAGES_DIR/userdata.img userdata $B2L_USERDATA_DEV $B2L_USERDATA_BYTES"

echo ""
echo "=== Flash terminé ==="
echo "Redémarre le téléphone avec : adb reboot"
