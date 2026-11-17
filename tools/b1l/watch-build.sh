#!/bin/bash
# watch-build.sh -- the newest build of b1l-build.sh, live (every 2 s): each stage with its state and time, and for the
# running one what it is doing (Mesa's [n/m], the kernel's last compiled file, the SYSTEM flash's step, BOOT's guards
# and read-back). Leave it open in a terminal: it follows the next build by itself.
P=${S10_PORT:-$HOME/Developer/s10-port}/progress
L=${B2L_ROOT:-$HOME/Developer/s10-port/beyond2lte-ios}/out/live/b1l-system-live.img
fmt() { local s=$1; printf '%d:%02d' $((s / 60)) $((s % 60)); }
names() {
	case $1 in
	mesa) echo "Mesa (compilateur GPU)" ;; libs) echo "Bibliothèques Metal" ;; kernel) echo "Noyau" ;;
	package) echo "Image BOOT" ;; system) echo "Flash SYSTEM" ;; boot) echo "Flash BOOT" ;; *) echo "$1" ;;
	esac
}
detail() {   # stage log
	local log=$2
	[ -f "$log" ] || { echo ""; return; }
	case $1 in
	mesa)
		local last=$(grep -o '^\[[0-9]*/[0-9]*\]' ${S10_PORT:-$HOME/Developer/s10-port}/build-mesa/ninja.log 2>/dev/null | tail -1)
		echo "${last:-préparation}" ;;
	libs) echo "$(grep -c 'unsign-branches' "$log") bibliothèque(s) patchée(s)" ;;
	kernel)
		local f=$(grep -E '^\s*(CC|C\+\+|LD|AS)\s' "$log" | tail -1 | sed 's/^ *//')
		echo "${f:-préparation}" ;;
	package) echo "$(tail -1 "$log" | cut -c1-70)" ;;
	system)
		if grep -q SYSTEM-OK "$log"; then echo "SYSTEM écrit"
		elif grep -q '^run ' "$log"; then echo "écriture : $(grep '^run ' "$log" | tail -1 | cut -c1-60)"
		elif grep -q 'differing blocks' "$log"; then echo "$(grep 'differing blocks' "$log" | tail -1)"
		elif grep -q 'hashing .* blocks on host' "$log"; then echo "hachage de la nouvelle image sur le Mac"
		elif grep -q 'full pull' "$log"; then echo "lecture complète : $(( $(stat -f %z "$L" 2>/dev/null || echo 0) / 1048576 )) / 7000 Mo"
		elif grep -q 'pulling only' "$log"; then echo "récupération des blocs modifiés par le téléphone"
		elif grep -q 'phone: hashing' "$log"; then echo "le téléphone hache SYSTEM (~30 s)"
		else echo "démarrage"; fi ;;
	boot)
		if grep -q BOOT-OK "$log"; then echo "BOOT-OK"
		elif grep -q 'boot readback' "$log"; then echo "relecture"
		elif grep -q 'guards ok' "$log"; then echo "gardes OK, envoi et écriture"
		else echo "vérification des gardes"; fi ;;
	esac
}
while true; do
	ST=$(readlink "$P/current" 2>/dev/null)
	clear
	if [ -z "$ST" ] || [ ! -f "$ST" ]; then
		echo "Aucun build en cours (en attente de b1l-build.sh)…"; sleep 2; continue
	fi
	N=$(basename "$ST" .status)
	META=$(cat "$P/$N.meta" 2>/dev/null); SET=$(echo "$META" | cut -d'|' -f3); T0=$(echo "$META" | cut -d'|' -f4)
	now=$(date +%s)
	echo "B1L-$N   ensemble $SET   $(date '+%H:%M:%S')   total $(fmt $((now - ${T0:-now})))"
	echo
	running=""
	while IFS='|' read -r name state start end log; do
		label=$(names "$name")
		case $state in
		done) printf '  [x] %-24s %s\n' "$label" "$(fmt $((end - start)))" ;;
		running) printf '  [>] %-24s %s   %s\n' "$label" "$(fmt $((now - start)))" "$(detail "$name" "$log")"; running=$name ;;
		failed) printf '  [!] %-24s ÉCHEC   %s\n' "$label" "$log" ;;
		skipped) printf '  [-] %-24s (sautée)\n' "$label" ;;
		*) printf '  [ ] %-24s\n' "$label" ;;
		esac
	done < "$ST"
	echo
	if grep -q '^boot|done' "$ST"; then echo ">>> Terminé : BOOT-OK. Tu peux redémarrer le téléphone."
	elif grep -q '|failed|' "$ST"; then echo ">>> Une étape a échoué (voir le journal indiqué)."
	elif [ -n "$running" ]; then
		log=$(grep "^$running|" "$ST" | cut -d'|' -f5)
		[ -f "$log" ] && { echo "--- fin du journal ---"; tail -4 "$log" | cut -c1-110; }
	fi
	sleep 2
done
