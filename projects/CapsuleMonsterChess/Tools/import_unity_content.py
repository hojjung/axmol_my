#!/usr/bin/env python3

import argparse
import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import urllib.request
from pathlib import Path


DEFAULT_UNIT_TABLE_URL = (
    "https://docs.google.com/spreadsheets/d/"
    "1uAVHJJWSTMEC_sdhsYeV87275gmubhGLyJF-yZ2fgLc/export?format=csv&gid=0"
)
SAFE_NAME_KEY = re.compile(r"^[A-Za-z0-9][A-Za-z0-9_-]*$")
LOBBY_FEATURE_ASSETS = (
    "Lobby/lobby_submenu_bg.png",
    "Lobby/lobby_sub_icon_friends.png",
    "Lobby/lobby_sub_icon_inbox.png",
    "Lobby/lobby_sub_icon_mission.png",
    "Lobby/lobby_sub_icon_news-.png",
    "Lobby/lobby_sub_icon_reward.png",
    "Lobby/lobby_sub_icon_trophy.png",
    "Lobby/lobby_pass_icon.png",
    "Lobby/lobby_bottom_bg.png",
    "Lobby/lobby_menu_btn_blue.png",
    "Lobby/lobby_menu_btn_purple.png",
    "Lobby/lobby_menu_shop.png",
    "Lobby/lobby_menu_cards.png",
    "Lobby/lobby_play_icon_stage.png",
    "Lobby/lobby_menu_clan_dim.png",
    "Mission/mission_icon_book.png",
    "Mission/mission_icon_crown.png",
    "Mission/mission_icon_trophy.png",
    "Mission/mission_prg_bg.png",
    "Mission/mission_prg_bar.png",
    "Mission/mission_reward_icon_energy.png",
    "Mission/mission_reward_icon_gem.png",
    "Mission/mission_reward_icon_gold.png",
    "Pass/icon_golden_pass.png",
    "Pass/icon_normal_pass.png",
    "Pass/pass_icon_lock.png",
    "Pass/pass_reward_frame_blue.png",
    "Pass/pass_reward_frame_purple.png",
    "Pass/pass_reward_icon_chest_0.png",
    "Pass/pass_reward_icon_chest_1.png",
    "Pass/pass_reward_icon_gem.png",
    "Pass/pass_reward_icon_gold.png",
    "Play/play_character_hp_bar_front.png",
    "Play/play_character_hp_bar_green.png",
    "Play/play_character_hp_bar_red.png",
    "Play/play_bottom_bg.png",
    "Play/play_bottom_character_ bgframe.png",
    "Play/play_btn_icon_auto.png",
    "Play_Continue_Pause_Result/result_star_large.png",
    "Common/button_blue.png",
    "Common/reward_icon_gold.png",
    "Common/reward_icon_chest.png",
    "Ranking/ranking_medal_gold.png",
    "Ranking/ranking_medal_silver.png",
    "Ranking/ranking_medal_bronze.png",
    "Ranking/ranking_icon_trophy.png",
    "_Icons/ItemIcons/256/common_icon_ranking.png",
    "_Icons/Pictoicons/256/btn_icon_arrow_backward.png",
    "_Icons/Pictoicons/256/btn_icon_close.png",
    "_Icons/Pictoicons/256/btn_icon_lock.png",
    "_Icons/Pictoicons/256/btn_icon_menu_0.png",
    "_Icons/Pictoicons/256/btn_icon_setting.png",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Import Capsule Monster Chess Unity content.")
    parser.add_argument("--unity-root", required=True, type=Path)
    parser.add_argument("--unit-table-url", default=DEFAULT_UNIT_TABLE_URL)
    parser.add_argument("--icon-size", default=192, type=int)
    parser.add_argument("--assets-only", action="store_true")
    return parser.parse_args()


def normalize(value: str) -> str:
    return re.sub(r"[^a-z0-9]", "", value.lower())


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def stamp_table_identity(table: dict) -> None:
    canonical_payload = {
        "schemaVersion": table["schemaVersion"],
        "characters": table["characters"],
    }
    canonical_json = json.dumps(
        canonical_payload,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")
    content_hash = sha256(canonical_json)
    table["tableVersion"] = f"monster.v1.{content_hash[:12]}"
    table["contentHash"] = content_hash


def read_url(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "CapsuleMonsterChessImporter/1"})
    with urllib.request.urlopen(request, timeout=30) as response:
        return response.read()


def build_unit_table(csv_bytes: bytes, source_url: str) -> dict:
    rows = list(csv.DictReader(csv_bytes.decode("utf-8-sig").splitlines()))
    characters = []
    name_keys = set()

    for row in rows:
        name_key = row.get("NameKey", "").strip()
        if not name_key:
            continue

        if name_key in name_keys:
            raise ValueError(f"duplicate NameKey in live unit table: {name_key}")
        name_keys.add(name_key)

        characters.append(
            {
                "unitId": int(row["아이디"]),
                "nameKey": name_key,
                "modelId": row.get("그래픽", "").strip(),
                "icon": "",
                "element": row["ElementType"].strip(),
                "role": row["UnitType"].strip(),
                "hp": int(row["HP"]),
                "ad": int(row["AD"]),
                "ap": int(row["AP"]),
                "adDefense": int(row["ADDEF"]),
                "apDefense": int(row["APDEF"]),
                "range": int(row["RNG"]),
                "evolutionKey": row.get("EvoKey", "").strip(),
                "skills": [
                    value.strip()
                    for value in (row.get("스킬1", ""), row.get("스킬2", ""), row.get("스킬3", ""))
                    if value.strip()
                ],
            }
        )

    warnings = []
    for character in characters:
        evolution_key = character["evolutionKey"]
        if evolution_key and evolution_key not in name_keys:
            warnings.append(
                f"{character['nameKey']} references missing evolutionKey {evolution_key}"
            )

    return {
        "schemaVersion": 1,
        "source": {"url": source_url, "sha256": sha256(csv_bytes)},
        "warnings": warnings,
        "iconBinding": {"rule": "normalized NameKey equals normalized <NameKey>_1 thumbnail filename"},
        "characters": characters,
    }


def resize_png(source: Path, destination: Path, size: int) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    result = subprocess.run(
        ["sips", "-z", str(size), str(size), str(source), "--out", str(destination)],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if result.returncode != 0:
        raise RuntimeError(f"sips failed for {source}: {result.stderr.strip()}")


def bind_name_key_icons(unity_root: Path, output_root: Path, table: dict, icon_size: int) -> None:
    thumbnail_root = unity_root / "CMS_Unity/Assets/Sprites/UnitThumbnails"
    if not thumbnail_root.is_dir():
        raise FileNotFoundError(f"Unity thumbnail directory was not found: {thumbnail_root}")

    thumbnail_index = {}
    for path in thumbnail_root.glob("Unit*/**/*.png"):
        match = re.fullmatch(r"(.+)_([123])", path.stem)
        if match and match.group(2) == "1":
            thumbnail_index.setdefault(normalize(match.group(1)), []).append(path)

    icon_root = output_root / "UI/MonsterIcons"
    icon_root.mkdir(parents=True, exist_ok=True)
    generated_icon_names = set()
    unresolved = []
    ambiguous = []

    for character in table["characters"]:
        name_key = character["nameKey"]
        if not SAFE_NAME_KEY.fullmatch(name_key):
            raise ValueError(f"unsafe NameKey for icon filename: {name_key!r}")

        matches = thumbnail_index.get(normalize(name_key), [])
        if len(matches) == 1:
            icon_name = f"{name_key}.png"
            resize_png(matches[0], icon_root / icon_name, icon_size)
            generated_icon_names.add(icon_name)
            character["icon"] = f"UI/MonsterIcons/{icon_name}"
        elif matches:
            ambiguous.append(name_key)
        else:
            unresolved.append(name_key)

    table["iconBinding"].update(
        {
            "resolved": len(generated_icon_names),
            "unresolved": unresolved,
            "ambiguous": ambiguous,
        }
    )


def copy_lobby_feature_assets(unity_root: Path, staging_root: Path) -> None:
    sprite_root = (
        unity_root
        / "CMS_Unity/Assets/Sprites/GUI Kit Casual Game/ResourcesData/Sprite"
    )
    output_root = staging_root / "UI/LobbyFeatures"
    for relative_path in LOBBY_FEATURE_ASSETS:
        source = sprite_root / relative_path
        if not source.is_file():
            raise FileNotFoundError(f"Lobby feature asset was not found: {source}")
        destination = output_root / relative_path
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)


def publish_lobby_feature_assets(staging_root: Path, output_root: Path) -> None:
    staged = staging_root / "UI/LobbyFeatures"
    final = output_root / "UI/LobbyFeatures"
    backup = output_root / "UI/.LobbyFeatures.previous"

    final.parent.mkdir(parents=True, exist_ok=True)
    if backup.exists():
        shutil.rmtree(backup)
    had_previous = final.exists()
    if had_previous:
        os.replace(final, backup)

    try:
        os.replace(staged, final)
    except Exception:
        if final.exists():
            shutil.rmtree(final)
        if had_previous and backup.exists():
            os.replace(backup, final)
        raise

    if backup.exists():
        shutil.rmtree(backup)


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def publish_import(staging_root: Path, output_root: Path) -> None:
    staged_icons = staging_root / "UI/MonsterIcons"
    final_icons = output_root / "UI/MonsterIcons"
    backup_icons = output_root / "UI/.MonsterIcons.previous"
    staged_table = staging_root / "Data/Tables/monster_unit_table.json"
    final_table = output_root / "Data/Tables/monster_unit_table.json"

    final_icons.parent.mkdir(parents=True, exist_ok=True)
    final_table.parent.mkdir(parents=True, exist_ok=True)
    if backup_icons.exists():
        shutil.rmtree(backup_icons)

    had_previous_icons = final_icons.exists()
    if had_previous_icons:
        os.replace(final_icons, backup_icons)

    try:
        os.replace(staged_icons, final_icons)
        os.replace(staged_table, final_table)
    except Exception:
        if final_icons.exists():
            shutil.rmtree(final_icons)
        if had_previous_icons and backup_icons.exists():
            os.replace(backup_icons, final_icons)
        raise

    if backup_icons.exists():
        shutil.rmtree(backup_icons)


def main() -> int:
    args = parse_args()
    if args.icon_size <= 0:
        raise ValueError("--icon-size must be greater than zero")

    project_root = Path(__file__).resolve().parents[1]
    output_root = project_root / "Content"

    if args.assets_only:
        output_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".unity-import-", dir=output_root) as staging:
            staging_root = Path(staging)
            copy_lobby_feature_assets(args.unity_root.resolve(), staging_root)
            publish_lobby_feature_assets(staging_root, output_root)
        print(f"Imported {len(LOBBY_FEATURE_ASSETS)} lobby feature assets.")
        return 0

    unit_csv = read_url(args.unit_table_url)
    unit_table = build_unit_table(unit_csv, args.unit_table_url)
    output_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".unity-import-", dir=output_root) as staging:
        staging_root = Path(staging)
        bind_name_key_icons(args.unity_root.resolve(), staging_root, unit_table, args.icon_size)
        copy_lobby_feature_assets(args.unity_root.resolve(), staging_root)
        stamp_table_identity(unit_table)
        write_json(staging_root / "Data/Tables/monster_unit_table.json", unit_table)
        publish_import(staging_root, output_root)
        publish_lobby_feature_assets(staging_root, output_root)

    binding = unit_table["iconBinding"]
    print(
        f"Imported {len(unit_table['characters'])} units and "
        f"resolved {binding['resolved']} icons by NameKey."
    )
    for warning in unit_table["warnings"]:
        print(f"warning: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
