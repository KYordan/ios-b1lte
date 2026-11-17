#!/bin/bash
# Progress of the current SYSTEM flash, refreshed every 2 s: the newest flash log (re-chosen every time), its stage,
# and the size of a full pull when one is running.
L=${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}/out/live/b1l-system-live.img
while true; do
  LOG=$(ls -t ${S10_PORT:-$HOME/Developer/s10-port}/flash-*-system.log 2>/dev/null | head -1)
  clear
  echo "$(date '+%H:%M:%S')   $(basename "$LOG")   (modified $(( $(date +%s) - $(stat -f %m "$LOG") )) s ago)"
  echo
  if grep -q SYSTEM-OK "$LOG"; then stage="SYSTEM written: BOOT follows in a few seconds (wait for BOOT-OK)"
  elif grep -q '^run ' "$LOG"; then stage="writing the changed blocks: $(grep -c '^run ' "$LOG") run(s) written and read back"
  elif grep -q 'differing blocks' "$LOG"; then stage="comparing done: $(grep 'differing blocks' "$LOG" | tail -1)"
  elif grep -q 'hashing .* blocks on host' "$LOG"; then stage="hashing the new image on the Mac (about 30 s)"
  elif grep -q 'full pull' "$LOG"; then
    s=$(stat -f %z "$L" 2>/dev/null || echo 0); stage="full read of SYSTEM: $((s/1048576)) / 7000 MB"
  elif grep -q 'live copy' "$LOG"; then stage="live copy ready; adding the files to the image"
  elif grep -q 'phone: hashing' "$LOG"; then stage="the phone is hashing its SYSTEM (about 30 s)"
  else stage="starting"; fi
  echo ">>> $stage"
  echo
  tail -8 "$LOG" | cut -c1-110
  sleep 2
done
