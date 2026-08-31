#!/usr/bin/env python3
"""Validate the static NTSC-U/J content and campaign contracts for ac6-native."""

from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


TARGET = "ntsc-uj"
ISO_SHA256 = "204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c"
ISO_SIZE = 7_835_492_352
XEX_SHA256 = "6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc"
DATA_TBL_SHA256 = "bad3a157eb75c839d9d6187f69ac061dee6d075d9cd61e0aa73b533473863b2f"
SHA256 = re.compile(r"^[0-9a-f]{64}$")
EXPECTED_FILES = {
    "default.xex": (3_911_897_088, 7_483_392, XEX_SHA256),
    "DATA.TBL": (3_903_617_024, 14_824, DATA_TBL_SHA256),
    "DATA00.PAC": (
        1_637_349_376,
        2_266_267_648,
        "a0a638af761f5a9613a7e0403537dbc2a899d86cbf9dc6405db5deb10f56c783",
    ),
    "DATA01.PAC": (
        5_719_171_072,
        664_141_824,
        "f9479ed38f60e4da9062cddb6b6cbe432d6fb0bec13e8c22b593383e3f19fe3d",
    ),
    "bgmpack.bin": (
        3_919_411_200,
        724_762_624,
        "78db61397696a5c98decc83052a1c36db8f816405b49c9910b689f8ad52c86fa",
    ),
    "demopack_eng.bin": (
        5_484_953_600,
        234_217_472,
        "31aae3a752b01553f42e63d6654ba0e45867e6962360a2fa5dc7cf3c4392c589",
    ),
    "demopack_jpn.bin": (
        4_971_419_648,
        234_455_040,
        "70e8159859662cedc98dcc44740b2e21fd93004a861abda77dcb52f36d410501",
    ),
    "moviepack.bin": (
        6_383_312_896,
        698_417_152,
        "106cdfdc71d9b92239d14e69d56a70ee7f56aea0994c488ebe75f96ffafc78cc",
    ),
    "voicepack_eng.bin": (
        5_205_874_688,
        279_078_912,
        "3e1c358714617337e30aef9ebae0b5bf43a7b84342f9d31093b8ec18296e8726",
    ),
    "voicepack_jpn.bin": (
        4_644_173_824,
        327_245_824,
        "82af039f582d741c574f62060adb1170ee087a2a0fa00fa241d18b846b03adcd",
    ),
}


class ContractError(ValueError):
    pass


def require(condition: bool, reason: str) -> None:
    if not condition:
        raise ContractError(reason)


def load_document(path: Path) -> dict:
    document = json.loads(path.read_text(encoding="utf-8"))
    require(isinstance(document, dict), f"document:{path}")
    return document


def validate_content_identity(document: dict) -> None:
    require(document.get("schema") == "ac6.retail-content-identity.v1", "identity.schema")
    require(document.get("status") == "qualified-metadata-only", "identity.status")
    require(document.get("target") == TARGET, "identity.target")
    media = document.get("source_media")
    require(isinstance(media, dict), "identity.source_media")
    require(media.get("size") == ISO_SIZE, "identity.iso_size")
    require(media.get("sha256") == ISO_SHA256, "identity.iso_sha256")
    require(media.get("retail_bytes_committed") is False, "identity.retail_bytes")
    xdvdfs = document.get("xdvdfs")
    require(
        isinstance(xdvdfs, dict)
        and xdvdfs.get("game_offset") == 265_879_552
        and xdvdfs.get("root_sector") == 1_776_247
        and xdvdfs.get("root_size") == 2_048,
        "identity.xdvdfs",
    )
    files = document.get("files")
    require(isinstance(files, dict) and set(files) == set(EXPECTED_FILES), "identity.files")
    for name, expected in EXPECTED_FILES.items():
        record = files.get(name)
        require(isinstance(record, dict), f"identity.file:{name}")
        actual = (record.get("iso_offset"), record.get("size"), record.get("sha256"))
        require(actual == expected, f"identity.file_identity:{name}")


def validate_campaign(document: dict) -> None:
    require(document.get("schema") == "ac6.retail-campaign-manifest.v1", "campaign.schema")
    require(document.get("target") == TARGET, "campaign.target")
    require(document.get("region") == "NTSC-U/J", "campaign.region")
    identity = document.get("identity")
    require(isinstance(identity, dict), "campaign.identity")
    require(identity.get("xex_sha256") == XEX_SHA256, "campaign.xex_sha256")
    require(identity.get("iso_sha256") == ISO_SHA256, "campaign.iso_sha256")
    require(identity.get("ghidra_project") == "ghidra-projects/ac6-us", "campaign.project")
    require(identity.get("program") == "default.xex", "campaign.program")
    require(identity.get("language") == "PowerPC:BE:64:Xenon", "campaign.language")

    static = document.get("static_evidence")
    require(isinstance(static, dict), "campaign.static_evidence")
    selector = static.get("selector_to_dpl")
    require(isinstance(selector, dict) and selector.get("status") == "qualified", "campaign.selector")
    require(selector.get("function_address") == "0x821B6EE8", "campaign.selector_function")
    require(selector.get("table_address") == "0x820657B0", "campaign.selector_table")
    require(selector.get("mission_count") == 15, "campaign.selector_count")
    dpl = static.get("dpl_to_data_table")
    require(isinstance(dpl, dict) and dpl.get("status") == "qualified", "campaign.dpl")
    require(dpl.get("request_function_address") == "0x821D1190", "campaign.dpl_request")
    require(dpl.get("queue_call_target") == "0x821CD168", "campaign.dpl_queue")
    require(dpl.get("loader_function_address") == "0x821CC288", "campaign.dpl_loader")
    table = dpl.get("data_table")
    require(isinstance(table, dict), "campaign.data_table")
    require(table.get("sha256") == DATA_TBL_SHA256, "campaign.data_table_sha256")
    require(
        table.get("size") == 14_824
        and table.get("entry_count") == 926
        and table.get("pack_count") == 2
        and table.get("record_size") == 16,
        "campaign.data_table_shape",
    )
    payloads = static.get("mission_payloads")
    require(
        isinstance(payloads, dict)
        and payloads.get("status") == "qualified-structural-only"
        and payloads.get("mission_range") == [1, 15],
        "campaign.payload_evidence",
    )
    missions = document.get("missions")
    require(isinstance(missions, list) and len(missions) == 15, "campaign.missions")
    for mission_id, mission in enumerate(missions, 1):
        require(isinstance(mission, dict), f"campaign.mission:{mission_id}")
        route = (
            mission.get("mission_id"),
            mission.get("campaign_selector"),
            mission.get("dpl_resource_id"),
            mission.get("data_table_entry_index"),
        )
        require(route == (mission_id, mission_id, mission_id + 8, mission_id + 8),
                f"campaign.route:{mission_id}")
        require(mission.get("static_route_status") == "qualified",
                f"campaign.route_status:{mission_id}")


def validate_payloads(document: dict) -> None:
    require(document.get("schema") == "ac6.retail-us-mission-payload-static.v1", "payload.schema")
    require(document.get("status") == "qualified-structural-only", "payload.status")
    require(document.get("target") == TARGET, "payload.target")
    require(document.get("xex_sha256") == XEX_SHA256, "payload.xex_sha256")
    source = document.get("source_iso")
    require(isinstance(source, dict), "payload.source_iso")
    require(source.get("size") == ISO_SIZE and source.get("sha256") == ISO_SHA256,
            "payload.iso_identity")
    table = document.get("data_tbl")
    require(isinstance(table, dict), "payload.data_tbl")
    require(
        table.get("size") == 14_824
        and table.get("sha256") == DATA_TBL_SHA256
        and table.get("entry_count") == 926
        and table.get("pack_count") == 2,
        "payload.data_tbl_identity",
    )
    require(document.get("mission_range") == [1, 15], "payload.mission_range")
    policy = document.get("policy")
    require(isinstance(policy, dict), "payload.policy")
    require(policy.get("bounded_iso_reads") is True, "payload.bounded_reads")
    require(policy.get("complete_pac_copied") is False, "payload.pac_copy")
    require(policy.get("payloads_retained") is False, "payload.retained")
    require(policy.get("objective_semantics_inferred") is False, "payload.semantics")
    require(policy.get("renderer_or_runtime_claimed") is False, "payload.runtime_claim")
    records = document.get("records")
    require(isinstance(records, list) and len(records) == 15, "payload.records")
    for mission_id, record in enumerate(records, 1):
        require(isinstance(record, dict), f"payload.record:{mission_id}")
        route = (
            record.get("mission_id"),
            record.get("campaign_selector"),
            record.get("dpl_resource_id"),
            record.get("data_table_entry_index"),
        )
        require(route == (mission_id, mission_id, mission_id + 8, mission_id + 8),
                f"payload.route:{mission_id}")
        require(record.get("archive") in {"DATA00.PAC", "DATA01.PAC"},
                f"payload.archive:{mission_id}")
        require(record.get("codec") == "mode1_pi_xor_raw_deflate",
                f"payload.codec:{mission_id}")
        for field in ("archive_relative_offset", "stored_size", "expanded_size"):
            require(type(record.get(field)) is int and record[field] > 0,
                    f"payload.{field}:{mission_id}")
        require(SHA256.fullmatch(record.get("stored_sha256", "")) is not None,
                f"payload.stored_sha256:{mission_id}")
        require(SHA256.fullmatch(record.get("expanded_sha256", "")) is not None,
                f"payload.expanded_sha256:{mission_id}")
        structure = record.get("structure")
        require(isinstance(structure, dict) and structure.get("root") == "FHM",
                f"payload.structure:{mission_id}")
        require(structure.get("parse_failures") == 0, f"payload.parse:{mission_id}")


def validate_documents(identity: dict, campaign: dict, payloads: dict) -> None:
    validate_content_identity(identity)
    validate_campaign(campaign)
    validate_payloads(payloads)
    campaign_missions = campaign["missions"]
    payload_records = payloads["records"]
    for mission_id, (mission, record) in enumerate(
        zip(campaign_missions, payload_records, strict=True), 1
    ):
        route_fields = ("mission_id", "campaign_selector", "dpl_resource_id",
                        "data_table_entry_index")
        require(
            all(mission[field] == record[field] for field in route_fields),
            f"contract.route_cross_check:{mission_id}",
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--identity", type=Path, required=True)
    parser.add_argument("--campaign", type=Path, required=True)
    parser.add_argument("--payloads", type=Path, required=True)
    args = parser.parse_args()
    try:
        identity = load_document(args.identity)
        campaign = load_document(args.campaign)
        payloads = load_document(args.payloads)
        validate_documents(identity, campaign, payloads)
    except (ContractError, OSError, json.JSONDecodeError) as error:
        print(f"ntsc_uj_contract=fail reason={str(error).replace(' ', '_')}")
        return 1
    print(f"ntsc_uj_contract=pass files={len(EXPECTED_FILES)} missions=15 target={TARGET}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
