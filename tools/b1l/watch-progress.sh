#!/bin/bash
# watch-progress.sh [NNNN] -- surveille la progression d'un build b1l-build.sh en temps reel
# Affiche chaque etape avec son etat et le temps ecoule. Rafraichit regulierement.
# Quitte sur Ctrl-C. Si toutes les etapes sont terminees, affiche TERMINE mais continue
# a rafraichir (utile pour verifier qu'aucune etape ne repasse en running/failed).
set -uo pipefail

N=${1:-}
P=${S10_PORT:-$HOME/Developer/s10-port}/progress
if [ -z "$N" ]; then
    if [ -L "$P/current" ]; then
        N=$(basename "$(readlink "$P/current")")
    else
        echo "Usage: $0 NNNN   (ou creez un lien progress/current vers le fichier .status)"
        exit 1
    fi
fi

# si N se termine deja par .status, ne pas l'ajouter deux fois
case "$N" in
    *.status) ST=$P/$N ;;
    *)        ST=$P/$N.status ;;
esac
if [ ! -f "$ST" ]; then
    echo "Fichier de statut introuvable: $ST"
    exit 1
fi

nom_etape() {
    case $1 in
        mesa)   echo "Mesa (compilateur GPU)" ;;
        libs)   echo "Bibliotheques Metal" ;;
        kernel) echo "Noyau" ;;
        package) echo "Image BOOT" ;;
        system) echo "Flash SYSTEM" ;;
        boot)   echo "Flash BOOT" ;;
        *)      echo "$1" ;;
    esac
}

etat_symbole() {
    case $2 in
        skipped) echo "[-]" ;;
        running) echo "[>]" ;;
        done)    echo "[x]" ;;
        failed)  echo "[!]" ;;
        pending) echo "[ ]" ;;
        *)       echo "[?]" ;;
    esac
}

fmt() {
    local s=$1
    if [ "$s" -eq 0 ] 2>/dev/null; then echo "-:-"; return; fi
    printf '%d:%02d' $((s / 60)) $((s % 60))
}

clear
echo "B1L-$N   suivi de progression   (rafraichissement toutes les 2 s, Ctrl-C pour arreter)"
echo "Le suivi continue meme une fois toutes les etapes terminees."
echo

while true; do
    t=$(date +%s)
    heure=$(date '+%H:%M:%S')
    tout_fini=1
    echo "----------------------------------------"
    echo "  $heure"
    echo "----------------------------------------"
    while IFS='|' read -r name st start end detail; do
        [ -z "$name" ] && continue
        sym=$(etat_symbole "$name" "$st")
        started=""
        elapsed=""
        if [ "$start" -gt 0 ] 2>/dev/null; then
            if [ "$end" -gt 0 ] 2>/dev/null; then
                elapsed=" $(fmt $((end - start)))"
            else
                elapsed=" +$(fmt $((t - start)))"
            fi
            started=" $(fmt $start)"
        fi
        detail_txt=""
        [ -n "$detail" ] && detail_txt="  $detail"
        echo "$sym $(nom_etape "$name")   $st$elapsed$detail_txt"
        if [ "$st" = "running" ] || [ "$st" = "pending" ]; then
            tout_fini=0
        fi
    done < "$ST"
    echo "----------------------------------------"
    if [ $tout_fini -eq 1 ]; then
        echo ">>> TERMINE : toutes les etapes sont traitees (pas de running/pending)."
    else
        echo ">>> En cours : une etape est en running ou pending."
    fi
    echo "   (Ctrl-C pour arreter le suivi)"
    sleep 2
    printf '\033[2J\033[H'
    echo "B1L-$N   suivi de progression   (rafraichissement toutes les 2 s, Ctrl-C pour arreter)"
    echo "Le suivi continue meme une fois toutes les etapes terminees."
    echo
done
