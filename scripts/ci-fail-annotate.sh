#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-2-Clause
# CI only: post the last lines of build logs as one ::error:: annotation. Annotations of a public
# repository are readable through the API without signing in, unlike the job log itself.
#   ci-fail-annotate.sh TITLE FILE...
title=$1; shift
body=""
for f in "$@"; do
  [ -s "$f" ] || continue
  body+="== $(basename "$f")"$'\n'"$(tail -n 40 "$f" | cut -c1-300)"$'\n'
done
[ -n "$body" ] || body="no log files: $*"
# Workflow-command escaping: % first, then CR and LF.
body=${body//'%'/'%25'}; body=${body//$'\r'/'%0D'}; body=${body//$'\n'/'%0A'}
echo "::error title=$title::$body"
