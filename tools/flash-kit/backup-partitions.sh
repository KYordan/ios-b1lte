#!/bin/bash
# backup-partitions.sh — Sauvegarde les partitions importantes avant toute modification
# Utilise adb depuis TWRP. Sauvegarde dans flash-kit/backup-<date>/
# Ne touche AUCUNE partition en écriture (lecture seule).

set -euo pipefail

ADB="${ADB:-$HOME/Developer/android/platform-tools/adb}${B2L_SERIAL:+ -s $B2L_SERIAL}"
DATE=$(date +%Y%m%d_%H%M%S)
BACKUP_DIR="$HOME/Developer/Port iOS 26 to S10/flash-kit/backup-$DATE"

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
if [ "$MODEL" != "SM-G973F" ]; then
  echo "ERREUR : modèle inattendu ($MODEL)"
  exit 1
fi

echo "Téléphone vérifié : TWRP root, SM-G973F"
echo ""

# Liste des partitions à sauvegarder (nom:bloc:taille_attendue)
# La taille est vérifiée si connue, sinon on lit jusqu'à la fin
declare -a PARTS=(
  "boot:sda14:57671680"
  "system:sda25:7340032000"
  "userdata:sda34:499641200640"
  "recovery:sda15:67633152"
  "dtb:sda12:8388608"
  "dtbo:sda13:8388608"
  "vbmeta:sda11:0"         # taille inconnue, lecture brute
  "efs:sda3:0"
  "sec_efs:sda7:0"
  "cpefs:sdd1:0"
)

for entry in "${PARTS[@]}"; do
  name="${entry%%:*}"
  rest="${entry#*:}"
  blk="${rest%%:*}"
  size="${rest##*:}"

  echo "--- Sauvegarde de $name ($blk) ---"

  # Vérifier que le bloc existe
  dev=$($adb shell readlink -f /dev/block/by-name/$name 2>/dev/null </dev/null | tr -d '\r' || echo "")
  if [ -z "$dev" ]; then
    echo "  SKIP : bloc by-name/$name introuvable"
    continue
  fi
  echo "  Bloc : $dev"

  # Vérifier que ce n'est pas monté
  mounted=$(adb shell "grep -q '^$dev ' /proc/mounts && echo oui || echo non" </dev/null 2>/dev/null | tr -d '\r')
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
  if [ -n "$count" ]; then
    $adb shell "dd if=$dev bs=$bs count=$count 2>/dev/null | sha256sum" </dev/null | tee "$BACKUP_DIR/${name}.sha256.phone"
  else
    $adb shell "dd if=$dev bs=$bs 2>/dev/null | sha256sum" </dev/null | tee "$BACKUP_DIR/${name}.sha256.phone"
  fi

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
