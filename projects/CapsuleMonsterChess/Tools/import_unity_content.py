#!/usr/bin/env python3

from __future__ import annotations

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
    parser.add_argument("--axasset", type=Path)
    parser.add_argument("--gltfpack", type=Path)
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


def index_name_key_thumbnails(unity_root: Path) -> dict[str, list[Path]]:
    thumbnail_root = unity_root / "CMS_Unity/Assets/Sprites/UnitThumbnails"
    if not thumbnail_root.is_dir():
        raise FileNotFoundError(f"Unity thumbnail directory was not found: {thumbnail_root}")

    thumbnail_index = {}
    for path in thumbnail_root.glob("Unit*/**/*.png"):
        match = re.fullmatch(r"(.+)_([123])", path.stem)
        if match and match.group(2) == "1":
            thumbnail_index.setdefault(normalize(match.group(1)), []).append(path)
    return thumbnail_index


def bind_name_key_assets(
    unity_root: Path,
    output_root: Path,
    table: dict,
    icon_size: int,
    axasset: Path,
    gltfpack: Path | None,
) -> None:
    thumbnail_root = unity_root / "CMS_Unity/Assets/Sprites/UnitThumbnails"
    model_root = unity_root / "CMS_Unity/Assets/Models"
    thumbnail_index = index_name_key_thumbnails(unity_root)

    icon_root = output_root / "Sprites/Units"
    model_output_root = output_root / "Models/Units"
    icon_root.mkdir(parents=True, exist_ok=True)
    model_output_root.mkdir(parents=True, exist_ok=True)
    resolved_icons = []
    resolved_models = []
    unresolved_icons = []
    unresolved_models = []
    ambiguous_icons = []

    for character in table["characters"]:
        name_key = character["nameKey"]
        if not SAFE_NAME_KEY.fullmatch(name_key):
            raise ValueError(f"unsafe NameKey for icon filename: {name_key!r}")

        matches = thumbnail_index.get(normalize(name_key), [])
        if len(matches) == 1:
            source_icon = matches[0]
            destination_dir = icon_root / name_key
            for shot in range(1, 4):
                shot_source = source_icon.with_name(f"{name_key}_{shot}.png")
                if not shot_source.is_file():
                    raise FileNotFoundError(f"Unity thumbnail shot was not found: {shot_source}")
                resize_png(
                    shot_source,
                    destination_dir / f"{name_key}_{shot}.png",
                    icon_size,
                )
            character["icon"] = f"Sprites/Units/{name_key}/{name_key}_1.png"
            resolved_icons.append(name_key)

            relative_icon = source_icon.relative_to(thumbnail_root)
            if len(relative_icon.parts) < 3:
                raise ValueError(f"unexpected Unity thumbnail path: {source_icon}")
            source_model_dir = model_root / relative_icon.parts[0] / relative_icon.parts[1] / "FBX"
            model_sources = preview_fbx_candidates(source_model_dir, relative_icon.parts[1])
            if not model_sources:
                unresolved_models.append(name_key)
                continue

            model_destination = model_output_root / name_key / f"{name_key}.glb"
            model_destination.parent.mkdir(parents=True, exist_ok=True)
            model_source = None
            conversion_errors = []
            for candidate in model_sources:
                command = [
                    str(axasset),
                    "--output",
                    str(model_destination),
                ]
                if gltfpack is not None:
                    command.extend(("--gltfpack", str(gltfpack)))
                command.append(str(candidate))
                result = subprocess.run(
                    command,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True,
                    check=False,
                )
                if result.returncode == 0:
                    model_source = candidate
                    break
                detail = result.stderr.strip() or result.stdout.strip()
                conversion_errors.append(f"{candidate.name}: {detail}")

            if model_source is None:
                raise RuntimeError(
                    f"axasset failed for every {name_key} preview candidate: "
                    + " | ".join(conversion_errors)
                )
            character["modelId"] = f"Models/Units/{name_key}/{name_key}.glb"
            resolved_models.append(name_key)
            print(
                f"[{len(resolved_models):03d}] {name_key}: "
                f"{model_source.name} -> {model_destination.name}"
            )
        elif matches:
            ambiguous_icons.append(name_key)
        else:
            unresolved_icons.append(name_key)

    table["iconBinding"].update(
        {
            "resolved": len(resolved_icons),
            "unresolved": unresolved_icons,
            "ambiguous": ambiguous_icons,
        }
    )
    table["modelBinding"] = {
        "rule": "NameKey thumbnail folder selects the matching Unity FBX folder; preview prefers an idle animation",
        "resolved": len(resolved_models),
        "unresolved": unresolved_models,
    }


def preview_fbx_candidates(source_dir: Path, character_folder: str) -> list[Path]:
    if not source_dir.is_dir():
        return []

    candidates = sorted(
        path for path in source_dir.iterdir() if path.is_file() and path.suffix.lower() == ".fbx"
    )
    if not candidates:
        return []

    normalized_subject = normalize(character_folder)

    def subject(path: Path) -> str:
        return path.stem.split("@", 1)[0]

    matching_subject = [
        path for path in candidates if normalize(subject(path)) == normalized_subject
    ]
    pool = matching_subject or candidates

    def rank(path: Path) -> tuple[int, str]:
        stem = path.stem.lower()
        is_transition = " to " in stem or "spawn" in stem
        if stem.endswith("@idle"):
            priority = 0
        elif not is_transition and stem.endswith("fly idle"):
            priority = 1
        elif not is_transition and stem.endswith("ground idle"):
            priority = 2
        elif "idle" in stem and not is_transition:
            priority = 3
        elif "@" not in stem:
            priority = 4
        else:
            priority = 5
        return priority, path.name.lower()

    return sorted(pool, key=rank)


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
    staged_icons = staging_root / "Sprites/Units"
    final_icons = output_root / "Sprites/Units"
    backup_icons = output_root / "Sprites/.Units.previous"
    staged_models = staging_root / "Models/Units"
    final_models = output_root / "Models/Units"
    backup_models = output_root / "Models/.Units.previous"
    staged_table = staging_root / "Data/Tables/monster_unit_table.json"
    final_table = output_root / "Data/Tables/monster_unit_table.json"
    backup_table = output_root / "Data/Tables/.monster_unit_table.previous"

    final_icons.parent.mkdir(parents=True, exist_ok=True)
    final_models.parent.mkdir(parents=True, exist_ok=True)
    final_table.parent.mkdir(parents=True, exist_ok=True)
    if backup_icons.exists():
        shutil.rmtree(backup_icons)
    if backup_models.exists():
        shutil.rmtree(backup_models)
    if backup_table.exists():
        backup_table.unlink()

    had_previous_icons = final_icons.exists()
    had_previous_models = final_models.exists()
    had_previous_table = final_table.exists()
    if had_previous_icons:
        os.replace(final_icons, backup_icons)
    if had_previous_models:
        os.replace(final_models, backup_models)
    if had_previous_table:
        os.replace(final_table, backup_table)

    try:
        os.replace(staged_icons, final_icons)
        os.replace(staged_models, final_models)
        os.replace(staged_table, final_table)
    except Exception:
        if final_icons.exists():
            shutil.rmtree(final_icons)
        if final_models.exists():
            shutil.rmtree(final_models)
        if final_table.exists():
            final_table.unlink()
        if had_previous_icons and backup_icons.exists():
            os.replace(backup_icons, final_icons)
        if had_previous_models and backup_models.exists():
            os.replace(backup_models, final_models)
        if had_previous_table and backup_table.exists():
            os.replace(backup_table, final_table)
        raise

    if backup_icons.exists():
        shutil.rmtree(backup_icons)
    if backup_models.exists():
        shutil.rmtree(backup_models)
    if backup_table.exists():
        backup_table.unlink()


def main() -> int:
    args = parse_args()
    if args.icon_size <= 0:
        raise ValueError("--icon-size must be greater than zero")

    project_root = Path(__file__).resolve().parents[1]
    engine_root = project_root.parents[1]
    output_root = project_root / "Content"

    if args.assets_only:
        output_root.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(prefix=".unity-import-", dir=output_root) as staging:
            staging_root = Path(staging)
            copy_lobby_feature_assets(args.unity_root.resolve(), staging_root)
            publish_lobby_feature_assets(staging_root, output_root)
        print(f"Imported {len(LOBBY_FEATURE_ASSETS)} lobby feature assets.")
        return 0

    axasset = (
        args.axasset.resolve()
        if args.axasset
        else engine_root / "build-axasset/tools/axasset/axasset"
    )
    if not axasset.is_file():
        raise FileNotFoundError(f"axasset executable was not found: {axasset}")
    gltfpack = args.gltfpack.resolve() if args.gltfpack else None
    if gltfpack is not None and not gltfpack.is_file():
        raise FileNotFoundError(f"gltfpack executable was not found: {gltfpack}")

    unit_csv = read_url(args.unit_table_url)
    unit_table = build_unit_table(unit_csv, args.unit_table_url)
    output_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".unity-import-", dir=output_root) as staging:
        staging_root = Path(staging)
        bind_name_key_assets(
            args.unity_root.resolve(),
            staging_root,
            unit_table,
            args.icon_size,
            axasset,
            gltfpack,
        )
        copy_lobby_feature_assets(args.unity_root.resolve(), staging_root)
        stamp_table_identity(unit_table)
        write_json(staging_root / "Data/Tables/monster_unit_table.json", unit_table)
        publish_import(staging_root, output_root)
        publish_lobby_feature_assets(staging_root, output_root)

    binding = unit_table["iconBinding"]
    print(
        f"Imported {len(unit_table['characters'])} units and "
        f"resolved {binding['resolved']} icons and "
        f"{unit_table['modelBinding']['resolved']} models by NameKey."
    )
    for warning in unit_table["warnings"]:
        print(f"warning: {warning}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
