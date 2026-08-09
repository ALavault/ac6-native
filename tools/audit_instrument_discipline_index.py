#!/usr/bin/env python3
"""Check that every shape in INSTRUMENT_DISCIPLINE.md is reachable from its
symptom index.

The file is only useful mid-investigation, and mid-investigation nobody reads
seventeen sections in order -- they scan the symptom table at the top for the
thing that just happened to them. A shape that is written but not indexed is,
for that reader, not written.

Why this is a tool and not a one-off check: it was written four times in one
session as a throwaway, with three distinct bugs, each of which reported false
gaps that would have led to duplicate index rows:

  1. matched only heading words of five letters or more, so *half a rule*
     -- whose words are all four letters -- was reported missing;
  2. matched substrings against the raw markdown, so a phrase the 80-column
     wrap had split across two lines ("a correct\\nmeasurement, over-read")
     did not match itself;
  3. required the heading's full tail, while the index deliberately points by
     the short name ("the displacement collision"), not the full title.

Each bug made the instrument report a gap that was not there -- the failure
mode this file's own eighth shape is about. The rule the tool encodes: an
index entry must contain the shape's short name, which is the heading text
after the colon and before the first comma, compared with whitespace
normalised on both sides.

usage: audit_instrument_discipline_index.py [PATH]
exit 0 when every shape is indexed, 1 otherwise.
"""

import re
import sys

DEFAULT_PATH = "INSTRUMENT_DISCIPLINE.md"

# The index is everything before the first section; "## The pattern" is the
# first heading in the file that is not part of the front matter.
INDEX_END = "## The pattern"

# STRUCTURAL sections, which carry no shape and are not expected in the index.
# Everything else after the boundary IS a shape and must be indexed.
#
# The filter here used to be `"shape" in heading.lower()`, which is not a test of
# whether a section is a shape -- it is a test of whether its author happened to
# write the word. At cycle 1455 the file held 39 second-level headings and the
# checker counted 31; of the eight it never looked at, three were shapes ("What
# made the eighth different", "The Xenon project has no reference database", "And
# a corollary about call sites") and one was the section being added at the time.
# A checker that exists to catch an unindexed shape was blind to exactly the
# shapes whose headings read naturally, and its own "shapes=31" was reporting a
# word count. Naming the exemptions makes the default correct.
STRUCTURAL = {
    "the pattern",
    "the specific traps in this repository",
    "two checkers that exist because of this",
    "the audit this file owes itself",
}


def short_name(heading: str) -> str:
    """The name the index is expected to point by.

    "The fifteenth shape: the displacement collision, and the four lines that
    settle it" -> "the displacement collision"
    """
    tail = heading.split(":")[-1]
    name = normalise(tail.split(",")[0])
    # A heading may open with a conjunction ("And a corollary about call sites")
    # where the index entry naturally does not. That is a difference in prose,
    # not a missing entry, and it hid a real shape until cycle 1455.
    return name[4:] if name.startswith("and ") else name


def normalise(text: str) -> str:
    return re.sub(r"\s+", " ", text).strip().lower()


def main() -> int:
    path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_PATH
    try:
        text = open(path, encoding="utf-8").read()
    except OSError as exc:
        print(f"instrument_discipline_index=error {exc}")
        return 1

    if INDEX_END not in text:
        print(f"instrument_discipline_index=error no {INDEX_END!r} heading; "
              "the index boundary is not where this tool expects it")
        return 1

    index = normalise(text[: text.index(INDEX_END)])
    after = text[text.index(INDEX_END):]
    headings = [h for h in re.findall(r"^## (.+)$", after, re.M)
                if normalise(h) not in STRUCTURAL]

    if not headings:
        print("instrument_discipline_index=error no shape headings found")
        return 1

    missing = [h for h in headings if short_name(h) not in index]
    for h in missing:
        print(f"  UNINDEXED  {short_name(h)!r}  (from {h!r})")

    status = "pass" if not missing else "fail"
    print(f"instrument_discipline_index={status} "
          f"shapes={len(headings)} unindexed={len(missing)}")
    return 0 if not missing else 1


if __name__ == "__main__":
    sys.exit(main())
