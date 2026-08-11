#!/usr/bin/env python3
"""Report the first deterministic divergence between qualified AC6 traces."""
from __future__ import annotations

import argparse
import hashlib
import json
import re
from pathlib import Path
from typing import Any


ORACLE_SCHEMA = "ac6.recomp-oracle-trace.v1"
NATIVE_SCHEMA = "ac6.native-execution-trace.v1"
REPORT_SCHEMA = "ac6.execution-first-divergence.v1"
ORACLE_COMMIT = "dcd41b7457fcac8242f8ef40de83d1719390d5af"
XEX_SHA256 = "acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde"
SHA256 = re.compile(r"[0-9a-f]{64}")
ADDRESS = re.compile(r"0x[0-9A-F]{8}")
DOMAINS = ("tick", "guest_address", "input", "graphics", "output_hashes")
EVENT_KEYS = {"sequence", *DOMAINS}


class ComparisonError(ValueError):
    pass


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_hash(value: object, name: str) -> None:
    if not isinstance(value, str) or not SHA256.fullmatch(value):
        raise ComparisonError(name)


def validate_qualification(document: dict[str, Any], role: str) -> None:
    qualification = document.get("qualification")
    if not isinstance(qualification, dict):
        raise ComparisonError(f"{role} qualification")
    if document["schema"] == ORACLE_SCHEMA:
        if (qualification.get("oracle_commit") != ORACLE_COMMIT or
                qualification.get("xex_sha256") != XEX_SHA256 or
                not isinstance(qualification.get("manifest"), str) or
                not qualification["manifest"] or not isinstance(qualification.get("probe"), str) or
                not qualification["probe"]):
            raise ComparisonError(f"{role} oracle qualification")
        for name in ("manifest_sha256", "probe_contract_sha256", "raw_sha256"):
            require_hash(qualification.get(name), f"{role} {name}")
    else:
        if qualification.get("producer") != "ac6-native":
            raise ComparisonError(f"{role} native producer")
        require_hash(qualification.get("content_identity"), f"{role} content identity")
        require_hash(qualification.get("replay_sha256"), f"{role} replay identity")


def validate_trace(document: object, role: str, maximum_events: int) -> dict[str, Any]:
    if not isinstance(document, dict) or document.get("schema") not in {ORACLE_SCHEMA, NATIVE_SCHEMA}:
        raise ComparisonError(f"{role} schema")
    validate_qualification(document, role)
    events = document.get("events")
    if (not isinstance(events, list) or not events or len(events) > maximum_events or
            document.get("event_count") != len(events)):
        raise ComparisonError(f"{role} event count")
    previous_tick = 0
    for index, event in enumerate(events):
        if not isinstance(event, dict) or set(event) != EVENT_KEYS:
            raise ComparisonError(f"{role} event {index} shape")
        if event["sequence"] != index:
            raise ComparisonError(f"{role} event {index} sequence")
        tick = event["tick"]
        if not isinstance(tick, int) or isinstance(tick, bool) or tick < previous_tick:
            raise ComparisonError(f"{role} event {index} tick")
        if not isinstance(event["guest_address"], str) or not ADDRESS.fullmatch(
                event["guest_address"]):
            raise ComparisonError(f"{role} event {index} guest address")
        if not isinstance(event["input"], dict) or not isinstance(event["graphics"], dict):
            raise ComparisonError(f"{role} event {index} structured domains")
        hashes = event["output_hashes"]
        if not isinstance(hashes, dict) or not hashes:
            raise ComparisonError(f"{role} event {index} output hashes")
        for name, digest in hashes.items():
            if not isinstance(name, str) or not name:
                raise ComparisonError(f"{role} event {index} output hash name")
            require_hash(digest, f"{role} event {index} output hash")
        previous_tick = tick
    return document


def first_value_difference(reference: Any, candidate: Any, path: str) -> dict[str, Any] | None:
    if type(reference) is not type(candidate):
        return {"path": path, "reference": reference, "candidate": candidate}
    if isinstance(reference, dict):
        for key in sorted(set(reference) | set(candidate)):
            child = f"{path}.{key}"
            if key not in reference:
                return {"path": child, "reference": "<missing>", "candidate": candidate[key]}
            if key not in candidate:
                return {"path": child, "reference": reference[key], "candidate": "<missing>"}
            difference = first_value_difference(reference[key], candidate[key], child)
            if difference is not None:
                return difference
        return None
    if isinstance(reference, list):
        for index, (expected, actual) in enumerate(zip(reference, candidate)):
            difference = first_value_difference(expected, actual, f"{path}[{index}]")
            if difference is not None:
                return difference
        if len(reference) != len(candidate):
            return {"path": f"{path}.length", "reference": len(reference),
                    "candidate": len(candidate)}
        return None
    if reference != candidate:
        return {"path": path, "reference": reference, "candidate": candidate}
    return None


def compare_documents(reference: dict[str, Any], candidate: dict[str, Any],
                      domains: tuple[str, ...] = DOMAINS,
                      maximum_events: int = 600_000) -> dict[str, Any]:
    reference = validate_trace(reference, "reference", maximum_events)
    candidate = validate_trace(candidate, "candidate", maximum_events)
    if not domains or len(domains) != len(set(domains)) or any(
            domain not in DOMAINS for domain in domains):
        raise ComparisonError("comparison domains")

    reference_events = reference["events"]
    candidate_events = candidate["events"]
    first: dict[str, Any] | None = None
    inspected = 0
    for index, (expected, actual) in enumerate(zip(reference_events, candidate_events)):
        inspected = index + 1
        for domain in domains:
            first = first_value_difference(expected[domain], actual[domain],
                                            f"events[{index}].{domain}")
            if first is not None:
                first["sequence"] = index
                first["tick"] = {"reference": expected["tick"], "candidate": actual["tick"]}
                break
        if first is not None:
            break
    if first is None and len(reference_events) != len(candidate_events):
        index = min(len(reference_events), len(candidate_events))
        first = {"path": "event_count", "reference": len(reference_events),
                 "candidate": len(candidate_events), "sequence": index,
                 "tick": {"reference": reference_events[-1]["tick"],
                          "candidate": candidate_events[-1]["tick"]}}

    return {
        "schema": REPORT_SCHEMA,
        "equal": first is None,
        "domains": list(domains),
        "compared_events": inspected,
        "reference": {"schema": reference["schema"], "event_count": len(reference_events)},
        "candidate": {"schema": candidate["schema"], "event_count": len(candidate_events)},
        "first_divergence": first,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("candidate", type=Path)
    parser.add_argument("--domains", default=",".join(DOMAINS))
    parser.add_argument("--max-events", type=int, default=600_000)
    parser.add_argument("--report", type=Path)
    arguments = parser.parse_args()
    try:
        if arguments.max_events <= 0:
            raise ComparisonError("max-events")
        domains = tuple(part.strip() for part in arguments.domains.split(",") if part.strip())
        reference = json.loads(arguments.reference.read_text(encoding="utf-8"))
        candidate = json.loads(arguments.candidate.read_text(encoding="utf-8"))
        report = compare_documents(reference, candidate, domains, arguments.max_events)
        report["reference"].update(
            {"path": str(arguments.reference), "sha256": sha256(arguments.reference)})
        report["candidate"].update(
            {"path": str(arguments.candidate), "sha256": sha256(arguments.candidate)})
        if arguments.report is not None:
            arguments.report.write_text(json.dumps(report, indent=2, sort_keys=True) + "\n",
                                        encoding="utf-8")
    except (ComparisonError, OSError, json.JSONDecodeError) as error:
        print(f"execution_trace_compare=fail reason={error}")
        return 2
    if report["equal"]:
        print(f"execution_trace_compare=equal events={report['compared_events']}")
        return 0
    divergence = report["first_divergence"]
    print(f"execution_trace_compare=diverged sequence={divergence['sequence']} "
          f"path={divergence['path']}")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
