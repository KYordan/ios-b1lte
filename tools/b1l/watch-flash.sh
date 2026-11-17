#!/bin/bash
# Live view of the chunked flashes (USERDATA, SYSTEM, BOOT). Ctrl-C to quit.
logs="${S10_PORT:-$HOME/Developer/s10-port}"
while true; do
  clear
  echo "Installation iOS 26 sur le S10 - $(date '+%H:%M:%S')"
  echo
  for part in userdata system boot; do
    log="$logs/flash-$part.log"
    [[ -f "$log" ]] || { printf '%-9s en attente\n' "$part"; continue; }
    line=$(grep -o 'chunk [0-9]*/[0-9]*' "$log" | tail -1)
    done_n=${line#chunk }; done_n=${done_n%/*}; total=${line#*/}
    if grep -q '^rc=0' "$log"; then state="termine et verifie"
    elif grep -q '^rc=' "$log"; then state="ERREUR (voir $log)"
    else state="en cours"; fi
    if [[ -n "$total" ]]; then
      pct=$(( done_n * 100 / total )); fill=$(( pct / 2 ))
      bar=$(printf '%*s' "$fill" '' | tr ' ' '#')$(printf '%*s' $((50 - fill)) '' | tr ' ' '.')
      printf '%-9s [%s] %3d%%  %s/%s  %s\n' "$part" "$bar" "$pct" "$done_n" "$total" "$state"
    else
      printf '%-9s %s\n' "$part" "$state"
    fi
  done
  echo
  last=$(ls -t "$logs"/flash-*.log 2>/dev/null | head -1)
  [[ -n "$last" ]] && { echo "--- $(basename "$last")"; grep -v 'pushed' "$last" | tail -6; }
  sleep 2
done
