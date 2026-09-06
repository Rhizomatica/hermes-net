#!/bin/bash
#
# uuxcomp header/parsing test harness.
#
# Builds uuxcomp, then runs it in UUXCOMP_DRY_RUN mode (no daemon, no uux, no
# compression: stdin is transformed and the result printed to stdout) over a
# set of fixtures and checks the result.
#
set -u

here="$(cd "$(dirname "$0")" && pwd)"
root="$(dirname "$here")"
fix="$here/fixtures"
out="$here/out"
bin="$root/uuxcomp"

mkdir -p "$out"

make -C "$root" uuxcomp >/dev/null || { echo "build failed"; exit 1; }

# headers uuxcomp is allowed to keep (lowercase); "chat-*" handled separately
ALLOW="from to cc bcc reply-to subject date message-id in-reply-to references \
mime-version content-type content-transfer-encoding content-disposition \
content-id content-description"

fail=0
note() { printf '  %s\n' "$*"; }
bad()  { printf '  FAIL: %s\n' "$*"; fail=1; }

# body of a message file = everything after the first blank line
body_of() { awk 'seen{print} /^\r?$/{seen=1}' "$1"; }

# header field names present in a message file (up to the first blank line)
header_names() {
    awk '/^\r?$/{exit} /^[!-9;-~]+:/{ sub(/:.*/,""); print tolower($0) }' "$1"
}

for f in "$fix"/*.eml; do
    name="$(basename "$f")"
    echo "== $name =="
    UUXCOMP_DRY_RUN=1 "$bin" x < "$f" > "$out/$name" 2>/dev/null

    if [ ! -s "$out/$name" ]; then
        bad "empty output"
        continue
    fi

    # --- SMS fixture: output is "From: <addr>\n" + verbatim body ---
    if grep -qi '^To: *sms@hermes.radio' "$f"; then
        got_from="$(head -1 "$out/$name")"
        note "sms forward first line: $got_from"
        case "$got_from" in
            "From: "*@*) : ;;
            *) bad "SMS forward From line malformed" ;;
        esac
        if ! diff <(body_of "$f") <(tail -n +2 "$out/$name") >/dev/null; then
            bad "SMS body not preserved verbatim"
        else
            note "SMS body preserved"
        fi
        continue
    fi

    # --- header allowlist ---
    kept="$(header_names "$out/$name" | sort -u | tr '\n' ' ')"
    note "headers kept: $kept"
    for h in $(header_names "$out/$name"); do
        case " $ALLOW " in
            *" $h "*) continue ;;
        esac
        case "$h" in
            chat-*) continue ;;
        esac
        bad "disallowed header survived: $h"
    done

    # --- Chat-Version must survive when present in the input ---
    if grep -qi '^Chat-Version:' "$f" && ! grep -qi '^Chat-Version:' "$out/$name"; then
        bad "Chat-Version dropped"
    fi

    # --- mbox 'From ' envelope line preserved verbatim when present ---
    if head -1 "$f" | grep -q '^From '; then
        if [ "$(head -1 "$f")" != "$(head -1 "$out/$name")" ]; then
            bad "mbox envelope line not preserved"
        else
            note "mbox envelope line preserved"
        fi
    fi

    # --- body preserved byte-for-byte (modulo trailing whitespace) ---
    if ! diff <(body_of "$f" | sed -e :a -e '/^[[:space:]]*$/{$d;N;ba}') \
              <(body_of "$out/$name" | sed -e :a -e '/^[[:space:]]*$/{$d;N;ba}') >/dev/null; then
        bad "body changed"
    else
        note "body preserved"
    fi

    # --- size report ---
    note "size: $(wc -c < "$f") -> $(wc -c < "$out/$name") bytes"
done

# --- CRLF variant of the plain fixture: line endings must be respected ---
echo "== crlf(plain_mbox) =="
crlf_in="$out/plain_mbox.crlf.eml"
sed 's/$/\r/' "$fix/plain_mbox.eml" > "$crlf_in"
UUXCOMP_DRY_RUN=1 "$bin" x < "$crlf_in" > "$out/plain_mbox.crlf.out" 2>/dev/null
crlf_lines=$(grep -c $'\r$' "$out/plain_mbox.crlf.out")
lf_only=$(grep -c $'[^\r]$' "$out/plain_mbox.crlf.out")
if [ "$crlf_lines" -gt 5 ] && [ "$lf_only" -eq 0 ]; then
    note "CRLF preserved ($crlf_lines lines)"
else
    bad "CRLF line endings lost (crlf=$crlf_lines lf_only=$lf_only)"
fi
if grep -qa '^DKIM-Signature' "$out/plain_mbox.crlf.out"; then
    bad "cruft survived in CRLF message"
fi

echo
if [ "$fail" -eq 0 ]; then echo "ALL TESTS PASSED"; else echo "TESTS FAILED"; fi
exit "$fail"
