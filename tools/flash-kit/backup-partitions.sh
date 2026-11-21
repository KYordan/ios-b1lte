#!/bin/bash
# backup-partitions.sh — Sauvegarde les partitions importantes avant toute modification
# Utilise adb depuis TWRP. Sauvegarde dans flash-kit/backup-<date>/
# Ne touche AUCUNE partition en écriture (lecture seule).

set -euo pipefail
. "$(dirname "${BASH_SOURCE[0]}")/../b1l/board.sh"   # B2L_BOARD=beyond0lte pour le S10e

adb="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="${BACKUP_ROOT:-$HOME/Developer/Port iOS 26 to S10/flash-kit}/backup-$B2L_BOARD-$DATE"

echo "=== Sauvegarde des partitions importantes ==="
echo "Répertoire de destination : $BACKUP_DIR"
mkdir -p "$BACKUP_DIR"

# Vérification préalable
STATE=$($adb get-state 2>/dev/null </dev/null || echo "unknown")
if [ "$STATE" != "recovery" ]; then
  echo "ERREUR : le téléphone n'est pas en TWRP. État : $STATE"
  exit 1
fi
MODEL=$($adb shell getprop ro.boot.em.model 2>/dev/null </dev/null | tr -d '\r')
if [ "$MODEL" != "$B2L_MODEL" ]; then
  echo "ERREUR : modèle inattendu ($MODEL != $B2L_MODEL)"
  exit 1
fi

echo "Téléphone vérifié : TWRP root, $B2L_MODEL ($B2L_BOARD)"
echo ""

# Liste des partitions à sauvegarder (nom:taille_attendue). Le bloc est
# résolu par by-name (vbmeta est sda23 sur S10/S10e, pas sda11 = uhcfg).
# La taille est vérifiée si connue (0 = lecture brute jusqu'à la fin).
# userdata n'est pas sauvé : trop gros, et l'installation iOS l'efface.
declare -a PARTS=(
  "boot:$B2L_BOOT_BYTES"
  "system:$B2L_SYSTEM_BYTES"
  "recovery:0"
  "dtb:8388608"
  "dtbo:8388608"
  "vbmeta:0"
  "efs:0"
  "sec_efs:0"
  "cpefs:0"
  "param:0"
  "up_param:0"
  "keystorage:0"
  "steady:0"
  "persistent:0"
  "misc:0"
  "uh:0"
  "uhcfg:0"
)

for entry in "${PARTS[@]}"; do
  name="${entry%%:*}"
  size="${entry##*:}"

  echo "--- Sauvegarde de $name ---"

  # Vérifier que le bloc existe
  dev=$($adb shell readlink -f /dev/block/by-name/$name 2>/dev/null </dev/null | tr -d '\r' || echo "")
  if [ -z "$dev" ]; then
    echo "  SKIP : bloc by-name/$name introuvable"
    continue
  fi
  echo "  Bloc : $dev"

  # Vérifier que ce n'est pas monté
  mounted=$($adb shell "grep -q '^$dev ' /proc/mounts && echo oui || echo non" </dev/null 2>/dev/null | tr -d '\r')
  if [ "$mounted" = "oui" ]; then
    echo "  SKIP : $name est monté, ne peut pas être sauvé à chaud"
    continue
  fi

  # Déterminer la taille à copier
  if [ "$size" != "0" ]; then
    actual=$($adb shell blockdev --getsize64 $dev 2>/dev/null </dev/null | tr -d '\r')
    if [ "$actual" != "$size" ]; then
      echo "  ERREUR : taille inattendue pour $name ($actual != $size)"
      continue
    fi
    count=$(( size / (4*1024*1024) ))
    bs=$((4*1024*1024))
  else
    # Taille inconnue : lire tout en 4 Mio chunks
    bs=$((4*1024*1024))
    count=""
  fi

  # Sauvegarde via dd + hash sur le téléphone, puis pull
  echo "  Lecture de $name vers le Mac..."
  # Toute la partition (taille déjà vérifiée) : le pull ci-dessous copie aussi
  # tout, y compris un reste qui n'est pas un multiple de 4 Mio (boot).
  $adb shell "dd if=$dev bs=$bs 2>/dev/null | sha256sum" </dev/null | tee "$BACKUP_DIR/${name}.sha256.phone"

  $adb pull /dev/block/by-name/$name "$BACKUP_DIR/${name}.img" 2>/dev/null || {
    echo "  ERREUR : échec du pull de $name"
    continue
  }

  # Vérifier le hash côté Mac
  MAC_HASH=$(shasum -a 256 "$BACKUP_DIR/${name}.img" | cut -d' ' -f1)
  PHONE_HASH=$(head -c 64 "$BACKUP_DIR/${name}.sha256.phone")
  echo "  Hash téléphone : $PHONE_HASH"
  echo "  Hash Mac       : $MAC_HASH"
  if [ "$MAC_HASH" = "$PHONE_HASH" ]; then
    echo "  OK : hash identique"
  else
    echo "  ERREUR : hash différent ! Le fichier peut être corrompu."
  fi
  echo ""
done

echo "=== Sauvegarde terminée dans $BACKUP_DIR ==="
echo "Conserve ce dossier en sécurité. Il contient l'IMEI et des données sensibles."
