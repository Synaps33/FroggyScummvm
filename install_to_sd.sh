#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
DIST="$PROJECT_DIR/dist"
SD_ROOT="${1:-}"

if [ -z "$SD_ROOT" ]; then
    for p in /media/Sajnaps/GB300 /media/Sajnaps/SF2000 /media/* /mnt/*; do
        if [ -d "$p/system/Deimos/cores" ] || [ -d "$p/cores" ]; then
            SD_ROOT="$p"
            break
        fi
    done
fi

if [ -z "$SD_ROOT" ] || [ ! -d "$SD_ROOT" ]; then
    echo "Nie wykryto karty SD. Uzycie: $0 <sciezka_do_karty>"
    exit 1
fi

IS_GB300=0
if [ "${FORCE_GB300:-0}" = "1" ] || [[ "$SD_ROOT" =~ GB300 ]] || \
   [ -f "$SD_ROOT/cores/sonicmania.gb3k" ] || \
   [ -f "$SD_ROOT/system/Deimos/cores/sonicmania.gb3k" ]; then
    IS_GB300=1
fi

if [ "$IS_GB300" -eq 1 ]; then
    CORE="$DIST/scummvm.gb3k"
    BIOS="$DIST/bisrv_gb300.asd"
    PKG="$DIST/sd-gb300"
    echo "Wykryto GB300 / GB300V2"
else
    CORE="$DIST/scummvm.sf2k"
    BIOS="$DIST/bisrv_sf2000.asd"
    PKG="$DIST/sd-sf2000"
    echo "Wykryto Data Frog SF2000"
fi

if [ ! -f "$CORE" ]; then
    echo "Brak $CORE - uruchom najpierw pakowanie"
    exit 1
fi

echo "Instalacja ScummVM w: $SD_ROOT"
cp -a "$PKG"/. "$SD_ROOT"/

# Kopiowanie rdzenia do wszystkich wymaganych lokalizacji frontendu
mkdir -p "$SD_ROOT/system/Deimos/cores" "$SD_ROOT/cores/scummvm" "$SD_ROOT/ROMS/scummvm" "$SD_ROOT/configs/custom_cores/scummvm"
for ext in sf2k gb3k; do
    cp -f "$CORE" "$SD_ROOT/system/Deimos/cores/scummvm.$ext"
    cp -f "$CORE" "$SD_ROOT/cores/scummvm.$ext"
    cp -f "$CORE" "$SD_ROOT/ROMS/scummvm/scummvm.$ext"
    cp -f "$CORE" "$SD_ROOT/cores/scummvm/scummvm.$ext"
done
cp -f "$CORE" "$SD_ROOT/cores/scummvm/core_87000000"
cp -f "$CORE" "$SD_ROOT/ROMS/scummvm/core_87000000"

# Opcjonalna instalacja multicore BIOS
if [ -f "$BIOS" ]; then
    if [ "${INSTALL_BIOS:-0}" = "1" ]; then
        mkdir -p "$SD_ROOT/bios"
        [ -f "$SD_ROOT/bios/bisrv.asd" ] && cp -f "$SD_ROOT/bios/bisrv.asd" "$SD_ROOT/bios/bisrv.asd.bak" || true
        cp -f "$BIOS" "$SD_ROOT/bios/bisrv.asd"
        echo "Zaktualizowano bios/bisrv.asd (patched multicore)"
    elif [ ! -f "$SD_ROOT/bios/bisrv.asd" ]; then
        mkdir -p "$SD_ROOT/bios"
        cp -f "$BIOS" "$SD_ROOT/bios/bisrv.asd"
        echo "Skopiowano brakujacy bios/bisrv.asd (patched multicore)"
    fi
fi

sync
echo "=========================================================="
echo " ScummVM zainstalowany pomyslnie na karcie SD!"
echo " D-Pad: ruch myszy, A / L: lewy klik, B / R: prawy klik"
echo " Start: Menu glowne, Select / X: Spacja / dialog"
echo " Uruchom z menu: ROMS/MonkeyIsland.gba lub Atlantis.gba"
echo "=========================================================="
