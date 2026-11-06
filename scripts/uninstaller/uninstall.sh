#!/bin/bash
# Uninstall Squelchmoth - (c) 2026 Jamar Stewart. All rights reserved.
# Removes Squelchmoth.app, this uninstaller, the installer receipt, preferences, caches, logs and the plug-in list.
# Your sessions, recordings, bounces, presets and backups are KEPT unless you choose "Remove Everything"
# (then they go to the Trash, never straight to deletion, so they can still be recovered).
#
#   --dry-run [Uninstall|"Remove Everything"]   lists what would be removed and changes nothing (for testing)

TITLE="Uninstall Squelchmoth"
DRY=""
[ "$1" = "--dry-run" ] && DRY=1
run () { if [ -n "$DRY" ]; then echo "would run: $*"; else "$@"; fi; }
ask () { osascript -e "display dialog \"$1\" buttons {$2} default button \"$3\" cancel button \"Cancel\" with title \"$TITLE\" with icon caution" -e 'button returned of result' 2>/dev/null; }
say_done () { [ -n "$DRY" ] && { echo "message: $1"; return; }; osascript -e "display dialog \"$1\" buttons {\"OK\"} default button \"OK\" with title \"$TITLE\"" >/dev/null 2>&1; }

if [ -n "$DRY" ]; then
    CHOICE="${2:-Uninstall}"
else
    CHOICE=$(ask "This removes Squelchmoth from this Mac: the app, this uninstaller, its settings, caches and logs.\n\nYour sessions, recordings, bounces, presets and backups in Documents are KEPT unless you choose Remove Everything (they then go to the Trash).\n\nSquelchmoth will quit first." '"Cancel", "Remove Everything", "Uninstall"' "Uninstall") || exit 0
    [ -z "$CHOICE" ] && exit 0
    # quit Squelchmoth politely (it offers to save), then make sure
    osascript -e 'tell application id "com.jamarstewart.squelchmoth" to quit' >/dev/null 2>&1
    for i in 1 2 3 4 5 6 7 8 9 10; do pgrep -x Squelchmoth >/dev/null || break; sleep 1; done
    pkill -x Squelchmoth 2>/dev/null
fi

# every copy of the app the Installer or the user put in Applications
APPS=()
for a in "/Applications/Squelchmoth.app" "$HOME/Applications/Squelchmoth.app" "/Applications/Squelchmoth/Squelchmoth.app" \
         "/Applications/Uninstall Squelchmoth.app" "$HOME/Applications/Uninstall Squelchmoth.app"; do
    [ -d "$a" ] && APPS+=("$a")
done
[ -n "$DRY" ] && for a in "/Applications/Squelchmoth.app" "/Applications/Uninstall Squelchmoth.app"; do echo "app: $a $( [ -d "$a" ] && echo '(present)' || echo '(not installed here)')"; done

STAMP=$(date +%Y-%m-%d-%H%M%S)

# 1. user files (no password needed): settings and licence, plug-in list, link helper, caches, logs
SUP="$HOME/Library/Application Support/Squelchmoth"
if [ "$CHOICE" = "Remove Everything" ]; then
    if [ -d "$SUP" ] || [ -n "$DRY" ]; then run mv "$SUP" "$HOME/.Trash/Squelchmoth Application Support $STAMP"; fi
else
    # keep the system-drive copies of session backups, remove everything else
    if [ -d "$SUP" ]; then
        for f in "$SUP"/* "$SUP"/.[!.]*; do
            [ -e "$f" ] || continue
            [ "$(basename "$f")" = "Session Backups" ] && continue
            run rm -rf "$f"
        done
    elif [ -n "$DRY" ]; then echo "would run: rm -rf (contents of) $SUP except Session Backups"; fi
fi
for p in "$HOME/Library/Squelchmoth" "$HOME/Library/Application Support/SquelchmothSelfTest" \
         "$HOME/Library/Caches/com.jamarstewart.squelchmoth" "$HOME/Library/Logs/Squelchmoth" \
         "$HOME/Library/Saved Application State/com.jamarstewart.squelchmoth.savedState" \
         "$HOME/Library/Preferences/com.jamarstewart.squelchmoth.plist" "$HOME/Library/HTTPStorages/com.jamarstewart.squelchmoth"; do
    if [ -e "$p" ] || [ -n "$DRY" ]; then run rm -rf "$p"; fi
done

if [ "$CHOICE" = "Remove Everything" ]; then
    for d in "$HOME/Documents/Squelchmoth Sessions" "$HOME/Documents/Squelchmoth"; do
        if [ -d "$d" ] || [ -n "$DRY" ]; then run mv "$d" "$HOME/.Trash/$(basename "$d") $STAMP"; fi
    done
else
    [ -n "$DRY" ] && echo "kept: $HOME/Documents/Squelchmoth Sessions, $HOME/Documents/Squelchmoth, $SUP/Session Backups"
fi

# 2. the apps and the installer receipts (an administrator password when the Installer put them there)
CMD=""
for a in "${APPS[@]}"; do CMD="$CMD rm -rf '$a';"; done
CMD="$CMD /usr/sbin/pkgutil --forget com.jamarstewart.squelchmoth.app >/dev/null 2>&1; true"
NEED_ADMIN=0
/usr/sbin/pkgutil --pkg-info com.jamarstewart.squelchmoth.app >/dev/null 2>&1 && NEED_ADMIN=1
for a in "${APPS[@]}"; do { [ -w "$(dirname "$a")" ] && [ -O "$a" ]; } || NEED_ADMIN=1; done
if [ -n "$DRY" ]; then
    echo "would run$( [ $NEED_ADMIN = 1 ] && echo ' (with administrator password)' ): $CMD"
elif [ $NEED_ADMIN = 0 ]; then
    /bin/bash -c "$CMD" 2>/dev/null
else
    osascript -e "do shell script \"$CMD\" with administrator privileges with prompt \"Squelchmoth needs your password to remove itself from Applications.\"" >/dev/null 2>&1 || {
        say_done "Squelchmoth was not removed from Applications (no password given). Your settings were removed; open Uninstall Squelchmoth again to finish."
        exit 1
    }
fi

# the trial start marker (~/Library/.squelchmoth-first-run) is deliberately kept so reinstalling doesn't restart the trial

if [ "$CHOICE" = "Remove Everything" ]; then
    say_done "Squelchmoth has been uninstalled. Your sessions and presets were moved to the Trash - empty it to delete them for good."
else
    say_done "Squelchmoth has been uninstalled. Your sessions, recordings and presets are still in Documents."
fi
exit 0
