#!/usr/bin/env python3
"""Build an external Mission 01 developer manifest from bounded NDXR slices."""
from __future__ import annotations

import argparse
import hashlib
import importlib.util
import re
import shutil
import sys
from pathlib import Path


def load_extractor():
    source = Path(__file__).with_name("extract_ndxr_native_slices.py")
    spec = importlib.util.spec_from_file_location("ac6_extractor", source)
    if spec is None or spec.loader is None:
        raise RuntimeError("cannot load NDXR contract helper")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def identity(path: Path) -> tuple[int, int, str]:
    data = path.read_bytes()
    value = 1469598103934665603
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return len(data), value, hashlib.sha256(data).hexdigest()


def fnv64(data: bytes) -> int:
    value = 1469598103934665603
    for byte in data:
        value ^= byte
        value = (value * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return value


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--f16", type=Path, required=True)
    parser.add_argument("--terrain", type=Path, action="append", required=True)
    parser.add_argument(
        "--extra",
        action="append",
        default=[],
        metavar="ASSET:PATH:KIND:STABLE",
        help="additional qualified NDXR slice and asset metadata",
    )
    parser.add_argument("--camera", type=Path)
    parser.add_argument("--controls", type=Path,
                        help="optional qualified SDL controls TSV")
    parser.add_argument("--texture", action="append", default=[],
                        metavar="STABLE:PPM",
                        help="optional external PPM source for a stable drawable")
    args = parser.parse_args()
    output = args.output_dir.resolve()
    output.mkdir(parents=True, exist_ok=True)
    extractor = load_extractor()
    texture_sources = {}
    for spec in args.texture:
        fields = spec.split(":")
        if len(fields) != 2 or not fields[0] or not fields[1]:
            parser.error(f"--texture must be STABLE:PPM, got {spec!r}")
        if fields[0] in texture_sources:
            parser.error(f"duplicate --texture stable id: {fields[0]!r}")
        source = Path(fields[1]).resolve()
        if not source.is_file():
            parser.error(f"texture source does not exist: {source}")
        data = source.read_bytes()
        if not data.startswith(b"P6\n"):
            parser.error(f"texture source is not a binary PPM: {source}")
        texture_sources[fields[0]] = (source, len(data), fnv64(data))
    source_rows = [("f16", args.f16.resolve(), 9, "aircraft", "f16")]
    for index, terrain in enumerate(args.terrain):
        source_rows.append(("terrain" if index == 0 else f"terrain{index + 1}",
                            terrain.resolve(), 119, "terrain",
                            "terrain" if index == 0 else f"terrain{index + 1}"))
    for spec in args.extra:
        fields = spec.split(":")
        if len(fields) != 4:
            parser.error(f"--extra must be ASSET:PATH:KIND:STABLE, got {spec!r}")
        try:
            asset = int(fields[0], 0)
        except ValueError:
            parser.error(f"invalid --extra asset id: {fields[0]!r}")
        if asset == 0 or not fields[1] or not fields[2] or not fields[3]:
            parser.error(f"invalid --extra metadata: {spec!r}")
        source_rows.append((fields[3], Path(fields[1]).resolve(), asset, fields[2], fields[3]))
    rows = []
    for buffer_id, source, asset, kind, stable in source_rows:
        payload = source.read_bytes()
        vertices, indices, primitives, stride = extractor.contract(payload)
        destination = output / f"{buffer_id}.ndxr"
        shutil.copyfile(source, destination)
        size, digest, sha = identity(destination)
        rows.append((buffer_id, destination.name, asset, kind, stable, vertices, indices,
                     primitives, size, digest, sha, stride))
    asset_ids = sorted({row[2] for row in rows})
    (output / "catalog.tsv").write_text(
        f"1\tair_intercept\t{','.join(str(asset) for asset in asset_ids)}\n")
    unique_assets = {}
    for row in rows:
        unique_assets.setdefault(row[2], (row[1], row[8], row[10]))
    (output / "assets.tsv").write_text("\n".join(
        f"{asset}\t{name}\t{sha}\t{size}\t-"
        for asset, (name, size, sha) in sorted(unique_assets.items())) + "\n")
    launch_assets = ",".join(f"{4096 + index}:1:{asset}" for index, asset in enumerate(asset_ids, 1))
    (output / "launches.tsv").write_text(f"1\t4097\t{launch_assets}\n")
    (output / "input.tsv").write_text("1\tstart_mission\n2\tpause\n4\tresume\n8\tabort\n")
    (output / "render.tsv").write_text(
        f"1\t{','.join(str(asset) for asset in asset_ids)}\n")
    (output / "drawables.tsv").write_text("# mission stable kind asset primitive buffer vertices indices hash\n" +
        "\n".join(f"1\t{stable}\t{kind}\t{asset}\t{primitives}\t{buffer_id}\t{vertices}\t{indices}\t{sha}"
                  for buffer_id, _, asset, kind, stable, vertices, indices, primitives, _, _, sha, _ in rows) + "\n")
    (output / "transforms.tsv").write_text("\n".join(
        f"1\t{row[4]}\t0\t0\t0\t1\t1\t1" for row in rows) + "\n")
    (output / "materials.tsv").write_text("\n".join(
        f"1\t{row[4]}\tD5B4.{row[4]}\t1\t1\topaque\t0x{('FFFFFFFF' if row[4] == 'f16' else 'FF4060A0')}"
        for row in rows) + "\n")
    texture_rows = []
    for row in rows:
        stable = row[4]
        if stable in texture_sources:
            source, size, digest = texture_sources[stable]
            destination = output / f"{stable}.ppm"
            shutil.copyfile(source, destination)
            texture_rows.append(f"1\t{stable}\tretail.{stable}\tlinear\twrap\t0x{digest:016X}\t{destination.name}\t{size}")
        else:
            texture_rows.append(
                f"1\t{stable}\tretail.{stable}\tlinear\twrap\t0x{('0102030405060708' if stable == 'f16' else '1112131415161718')}"
            )
    (output / "textures.tsv").write_text("\n".join(texture_rows) + "\n")
    (output / "shaders.tsv").write_text("\n".join(
        f"D5B4.{row[4]}\tpos_norm_uv\t1\t{8 if row[4] == 'f16' else 12}\trgba8"
        for row in rows) + "\n")
    (output / "targets.tsv").write_text("1\tworld_color\t1280\t720\t1\trgba8\td24s8\t1\n1\tpresent\t1280\t720\t1\trgba8\tnone\t0\n")
    (output / "passes.tsv").write_text("1\tworld\t1\tworld_color\tmain_depth\t0x00000000\t1\n")
    (output / "resolves.tsv").write_text("1\tworld\tworld_color\tpresent\tcopy\n")
    (output / "buffers.tsv").write_text("\n".join(f"{buffer_id}\t{name}\t{size}\t{digest}" for buffer_id, name, _, _, _, _, _, _, size, digest, _, _ in rows) + "\n")
    manifest_rows = {
        "catalog": "catalog.tsv", "assets": "assets.tsv", "launches": "launches.tsv",
        "input": "input.tsv",
        "render": "render.tsv", "drawables": "drawables.tsv", "transforms": "transforms.tsv",
        "materials": "materials.tsv", "textures": "textures.tsv", "shaders": "shaders.tsv",
        "targets": "targets.tsv", "passes": "passes.tsv", "resolves": "resolves.tsv",
        "buffers": "buffers.tsv"}
    if args.camera:
        destination = output / "camera.tsv"
        shutil.copyfile(args.camera.resolve(), destination)
        manifest_rows["camera"] = destination.name
    if args.controls:
        destination = output / "controls.tsv"
        shutil.copyfile(args.controls.resolve(), destination)
        manifest_rows["controls"] = destination.name
    (output / "manifest.tsv").write_text("\n".join(
        f"{key}\t{value}" for key, value in manifest_rows.items()) + "\n")
    print(f"manifest={output / 'manifest.tsv'}")
    for row in rows:
        print(f"{row[0]} vertices={row[5]} indices={row[6]} primitives={row[7]} stride={row[11]}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
