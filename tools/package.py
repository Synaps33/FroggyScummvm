#!/usr/bin/env python3
import os
import shutil
import zipfile
from PIL import Image

BASE_DIR = "/home/Sajnaps/scummvm-sf2000"
DIST_DIR = os.path.join(BASE_DIR, "dist")
TEST_ROMS = os.path.join(BASE_DIR, "test_roms")
SCREENSHOTS = os.path.join(BASE_DIR, "screenshots")

def make_rgb565(png_path, out_path, size=(160, 160)):
    im = Image.open(png_path).convert("RGB").resize(size, Image.Resampling.BILINEAR)
    raw = bytearray()
    for y in range(size[1]):
        for x in range(size[0]):
            r, g, b = im.getpixel((x, y))
            val = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
            raw.append(val & 0xFF)
            raw.append((val >> 8) & 0xFF)
    with open(out_path, "wb") as f:
        f.write(raw)

def build_package(target_type):  # 'sf2000' or 'gb300'
    pkg_dir = os.path.join(DIST_DIR, f"sd-{target_type}")
    shutil.rmtree(pkg_dir, ignore_errors=True)
    os.makedirs(pkg_dir, exist_ok=True)

    core_ext = "sf2k" if target_type == "sf2000" else "gb3k"
    core_src = os.path.join(DIST_DIR, f"scummvm.{core_ext}")

    # 1. cores/scummvm and cores/
    cores_dir = os.path.join(pkg_dir, "cores", "scummvm")
    os.makedirs(cores_dir, exist_ok=True)
    shutil.copyfile(core_src, os.path.join(cores_dir, "core_87000000"))
    for ext in ("sf2k", "gb3k"):
        shutil.copyfile(core_src, os.path.join(pkg_dir, "cores", f"scummvm.{ext}"))
        shutil.copyfile(core_src, os.path.join(cores_dir, f"scummvm.{ext}"))

    # 2. system/Deimos/cores/
    deimos_dir = os.path.join(pkg_dir, "system", "Deimos", "cores")
    os.makedirs(deimos_dir, exist_ok=True)
    for ext in ("sf2k", "gb3k"):
        shutil.copyfile(core_src, os.path.join(deimos_dir, f"scummvm.{ext}"))

    # 3. configs/custom_cores/scummvm/games.opt
    cfg_dir = os.path.join(pkg_dir, "configs", "custom_cores", "scummvm")
    os.makedirs(cfg_dir, exist_ok=True)
    with open(os.path.join(cfg_dir, "games.opt"), "w") as f:
        f.write(
            "scummvm|scummvm\n"
            "scummvm.gba|scummvm\n"
            "scummvm.gb3k|scummvm\n"
            "scummvm.sf2k|scummvm\n"
            "MonkeyIsland|scummvm\n"
            "MonkeyIsland.gba|scummvm\n"
            "Atlantis|scummvm\n"
            "Atlantis.gba|scummvm\n"
            "TheDig|scummvm\n"
            "TheDig.gba|scummvm\n"
            "SamAndMax|scummvm\n"
            "SamAndMax.gba|scummvm\n"
            "disk01.lec|scummvm\n"
            "PLAYFATE.001|scummvm\n"
            "dig.la0|scummvm\n"
            "snmdemo.sm0|scummvm\n"
            "monkey-demo|scummvm\n"
            "atlantis-demo|scummvm\n"
            "dig-demo|scummvm\n"
            "samnmax-demo|scummvm\n"
        )

    # 4. ROMS/scummvm
    roms_dir = os.path.join(pkg_dir, "ROMS")
    scumm_roms = os.path.join(roms_dir, "scummvm")
    os.makedirs(scumm_roms, exist_ok=True)

    # Copy test games
    for gdir in ("monkey-demo", "atlantis-demo", "samnmax-demo", "dig-demo"):
        src_path = os.path.join(TEST_ROMS, gdir)
        if os.path.isdir(src_path):
            shutil.copytree(src_path, os.path.join(scumm_roms, gdir), dirs_exist_ok=True)

    # Copy core binaries into ROMS/scummvm for frontends that search locally
    for ext in ("sf2k", "gb3k"):
        shutil.copyfile(core_src, os.path.join(scumm_roms, f"scummvm.{ext}"))
    shutil.copyfile(core_src, os.path.join(scumm_roms, "core_87000000"))

    # Launchers: <core>;<folder>;<file>
    games_map = [
        ("MonkeyIsland.gba", "scummvm;scummvm;monkey-demo/disk01.lec.\n", "tour_04_gameplay.png"),
        ("Atlantis.gba", "scummvm;scummvm;atlantis-demo/PLAYFATE.001.\n", "indy_05_final.png"),
        ("TheDig.gba", "scummvm;scummvm;dig-demo/dig.la0.\n", "dig_04_gameplay.png"),
        ("SamAndMax.gba", "scummvm;scummvm;samnmax-demo/snmdemo.sm0.\n", "samnmax_04_gameplay.png"),
        ("scummvm.gba", "scummvm;scummvm;scummvm.ini.\n", "tour_01_boot.png"),
    ]

    for gba_name, content, sshot in games_map:
        with open(os.path.join(scumm_roms, gba_name), "w") as f:
            f.write(content)
        with open(os.path.join(roms_dir, gba_name), "w") as f:
            f.write(content)

        png_path = os.path.join(SCREENSHOTS, sshot)
        rgb_name = gba_name.replace(".gba", ".rgb565")
        if os.path.exists(png_path):
            make_rgb565(png_path, os.path.join(scumm_roms, rgb_name))
            make_rgb565(png_path, os.path.join(roms_dir, rgb_name))

    # Empty scummvm.ini stub so ScummVM direct launcher boots immediately into ScummVM GUI
    with open(os.path.join(scumm_roms, "scummvm.ini"), "w") as f:
        f.write("[scummvm]\nversion=2.8.0\n")

    # Cover art
    monkey_png = os.path.join(SCREENSHOTS, "tour_04_gameplay.png")
    if os.path.exists(monkey_png):
        make_rgb565(monkey_png, os.path.join(scumm_roms, "MonkeyIsland.rgb565"))
        make_rgb565(monkey_png, os.path.join(roms_dir, "MonkeyIsland.rgb565"))

    indy_png = os.path.join(SCREENSHOTS, "indy_05_final.png")
    if os.path.exists(indy_png):
        make_rgb565(indy_png, os.path.join(scumm_roms, "Atlantis.rgb565"))

    splash_png = os.path.join(SCREENSHOTS, "tour_01_boot.png")
    if os.path.exists(splash_png):
        make_rgb565(splash_png, os.path.join(scumm_roms, "scummvm.rgb565"))

    # Bios
    bios_name = f"bisrv_{target_type}.asd"
    bios_src = os.path.join(DIST_DIR, bios_name)
    if os.path.isfile(bios_src):
        bios_dir = os.path.join(pkg_dir, "bios")
        os.makedirs(bios_dir, exist_ok=True)
        shutil.copyfile(bios_src, os.path.join(bios_dir, "bisrv.asd"))

    # Readme
    with open(os.path.join(pkg_dir, "README.txt"), "w") as f:
        f.write(
            f"ScummVM Standalone Port for {target_type.upper()} / FrogUI\n"
            "==========================================================\n\n"
            "Gry w zestawie:\n"
            "- The Secret of Monkey Island (EGA Demo)\n"
            "- Indiana Jones and the Fate of Atlantis (VGA Demo)\n\n"
            "Sterowanie na konsoli:\n"
            "- D-Pad: Ruch kursorem myszy\n"
            "- A / L: Lewy przycisk myszy (Klik / Akcja / Wybór)\n"
            "- B / R: Prawy przycisk myszy\n"
            "- Start: Menu glowne ScummVM (Zapis/Wczytanie/Opcje)\n"
            "- Select / X: Spacja / Pominiecie dialogu / Klawiatura ekranowa\n"
            "- Y: Escape (Pauza / Wyjscie)\n\n"
            "Instalacja:\n"
            "1. Skopiuj zawartosc tego folderu na glowny katalog karty SD konsoli.\n"
            "2. W menu konsoli lub FrogUI wejdz do zakladki ScummVM lub ROMS.\n"
            "3. Uruchom 'MonkeyIsland.gba' lub 'Atlantis.gba'.\n\n"
            "Dodawanie nowych gier:\n"
            "- Utworz folder dla gry w ROMS/scummvm/<nazwa_gry>/\n"
            "- Utworz plik launcher: ROMS/scummvm/<nazwa_gry>.gba z trescia:\n"
            "  scummvm;scummvm/<nazwa_gry>;<plik_glowny_gry>\n"
        )

    # Create zip archive
    zip_path = os.path.join(DIST_DIR, f"scummvm-{target_type}-sd.zip")
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(pkg_dir):
            for file in files:
                full_path = os.path.join(root, file)
                rel_path = os.path.relpath(full_path, pkg_dir)
                zf.write(full_path, rel_path)
    print(f"Created {zip_path} ({os.path.getsize(zip_path)} bytes)")

def main():
    build_package("sf2000")
    build_package("gb300")
    print("Packaging complete!")

if __name__ == "__main__":
    main()
