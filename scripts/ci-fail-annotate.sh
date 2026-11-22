#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# CI only: post what went wrong in build logs as one ::error:: annotation. Annotations of a public
# repository are readable through the API without signing in, unlike the job log itself; GitHub cuts
# long ones, so each file gives its first errors (FAILED:/error:/ERROR:) with context, else its tail,
# and the whole message stays under ~3.5 KB. List the most useful log first.
#   ci-fail-annotate.sh TITLE FILE...
title=$1; shift
body=""
for f in "$@"; do
  [ -s "$f" ] || continue
  if grep -qE 'FAILED:|error:|ERROR:|Error:' "$f"; then
    part=$(grep -m 4 -A 10 -E 'FAILED:|error:|ERROR:|Error:' "$f" | cut -c1-240)
  else
    part=$(tail -n 15 "$f" | cut -c1-240)
  fi
  body+="== $(basename "$f")"$'\n'"$part"$'\n'
  [ ${#body} -lt 3500 ] || break
done
body=${body:0:3500}
[ -n "$body" ] || body="no log files: $*"
# Workflow-command escaping: % first, then CR and LF.
body=${body//'%'/'%25'}; body=${body//$'\r'/'%0D'}; body=${body//$'\n'/'%0A'}
echo "::error title=$title::$body"
