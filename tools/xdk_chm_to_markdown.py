#!/usr/bin/env python3
"""Convert the HTML payload of the Xbox 360 XDK CHM to navigable Markdown.

The converter intentionally keeps one Markdown file per source page.  This
preserves the CHM's stable page names, makes cross-references local, and keeps
the generated corpus searchable without putting the whole SDK in one file.
"""

from __future__ import annotations

import argparse
import hashlib
import html
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from urllib.parse import urldefrag

from bs4 import BeautifulSoup, NavigableString, Tag


LANGUAGE_MAP = {
    "managedcplusplus": "cpp",
    "cplusplus": "cpp",
    "c++": "cpp",
    "c": "c",
    "csharp": "csharp",
    "c#": "csharp",
    "visualbasic": "vbnet",
    "visual basic": "vbnet",
    "javascript": "javascript",
    "jscript": "javascript",
}

DECORATIVE_IMAGES = {
    "collapse_all.gif",
    "collapse_all.png",
    "collapse.gif",
    "expand_all.gif",
    "expand_all.png",
    "expand.gif",
    "collall.gif",
    "expall.gif",
    "drpdown.gif",
    "drpdown_orange.gif",
    "copycode.gif",
    "copycodehighlight.gif",
}

API_MARKERS = re.compile(
    r">(?:Syntax|Parameters|Return Value|Requirements)</", re.IGNORECASE
)


def clean_space(value: str) -> str:
    value = value.replace("\xa0", " ").replace("\uFFFD", "�")
    return re.sub(r"[ \t\r\f\v]+", " ", value).strip()


def clean_block(value: str) -> str:
    lines = [re.sub(r"[ \t]+", " ", line).rstrip() for line in value.splitlines()]
    while lines and not lines[0]:
        lines.pop(0)
    while lines and not lines[-1]:
        lines.pop()
    result: list[str] = []
    blank = False
    for line in lines:
        if not line:
            if not blank:
                result.append("")
            blank = True
        else:
            result.append(line)
            blank = False
    return "\n".join(result)


def inline_text(node: Tag | NavigableString | None) -> str:
    if node is None:
        return ""
    if isinstance(node, NavigableString):
        return clean_space(str(node))
    if not isinstance(node, Tag):
        return ""
    if node.name in {"script", "style", "noscript"}:
        return ""
    if node.name == "br":
        return "\n"
    if node.name == "img":
        return clean_space(node.get("alt", ""))
    if node.name == "a":
        return clean_space("".join(inline_text(child) for child in node.children))
    if node.name in {"b", "strong", "i", "em", "code", "span", "label", "small"}:
        return clean_space("".join(inline_text(child) for child in node.children))
    return clean_space("".join(inline_text(child) for child in node.children))


def escape_table(value: str) -> str:
    return clean_space(value).replace("|", r"\|").replace("\n", " ")


def page_target(href: str, current: str) -> str:
    href = html.unescape(href.strip())
    if not href or href.startswith(("#", "mailto:", "javascript:", "data:")):
        return href
    if re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*://", href):
        return href
    path, fragment = urldefrag(href)
    if not path:
        return f"#{fragment}" if fragment else ""
    path = path.replace("\\", "/")
    if path.lower().startswith("mailto:"):
        return path
    if "@" in path and "/" not in path:
        return f"mailto:{path}"
    if Path(path).suffix.lower() not in {
        ".htm",
        ".html",
        ".jpg",
        ".jpeg",
        ".png",
        ".gif",
        ".bmp",
        ".css",
        ".js",
    }:
        return ""
    if path.startswith("O:"):
        # Compiled-help object links are viewer directives, not filesystem
        # targets. Keep the visible label and drop the unusable destination.
        return ""
    base = Path(path).name
    if base.lower().endswith((".htm", ".html")):
        base = re.sub(r"\.(?:html?)$", ".md", base, flags=re.IGNORECASE)
    if fragment:
        return f"{base}#{fragment}"
    return base


def strip_invalid_literal_links(value: str) -> str:
    """Remove Markdown-looking menu labels embedded literally in CHM text."""
    allowed_suffixes = {
        ".htm",
        ".html",
        ".md",
        ".jpg",
        ".jpeg",
        ".png",
        ".gif",
        ".bmp",
        ".css",
        ".js",
    }
    result: list[str] = []
    cursor = 0
    while cursor < len(value):
        start = value.find("[", cursor)
        if start < 0:
            result.append(value[cursor:])
            break
        close_label = value.find("](", start + 1)
        if close_label < 0:
            result.append(value[cursor:])
            break
        depth = 1
        end = close_label + 2
        while end < len(value) and depth:
            if value[end] == "(":
                depth += 1
            elif value[end] in {")", "）"}:
                depth -= 1
            end += 1
        if depth:
            result.append(value[cursor:])
            break
        destination = value[close_label + 2 : end - 1].strip()
        path, _ = urldefrag(destination)
        valid = (
            destination.startswith(("http://", "https://", "mailto:", "#", "data:"))
            or Path(path).suffix.lower() in allowed_suffixes
        )
        result.append(value[cursor:start])
        result.append(value[start:end] if valid else value[start + 1 : close_label])
        cursor = end
    return "".join(result)


def render_inline(node: Tag | NavigableString | None, current: str, assets: bool) -> str:
    if node is None:
        return ""
    if isinstance(node, NavigableString):
        return strip_invalid_literal_links(str(node).replace("\xa0", " "))
    if not isinstance(node, Tag) or node.name in {"script", "style", "noscript"}:
        return ""
    name = node.name.lower()
    if name == "br":
        return "\n"
    if name == "a":
        label = clean_space(node.get_text(" ", strip=True))
        label = re.sub(r"\[([^\]]+)\]\([^)]*\)", r"\1", label)
        href = page_target(node.get("href", ""), current)
        if not label:
            label = href
        return f"[{label}]({href})" if href else label
    if name == "img":
        src = node.get("src", "")
        lower = Path(src).name.lower()
        alt = clean_space(node.get("alt", ""))
        if lower in DECORATIVE_IMAGES or not src:
            return ""
        target = f"assets/{Path(src).name}" if assets else src
        return f"![{alt}]({target})"
    content = "".join(render_inline(c, current, assets) for c in node.children)
    content = content.replace("\n", " ") if name not in {"code", "pre"} else content
    content = clean_space(content)
    if name in {"b", "strong"} and content:
        return f"**{content}**"
    if name in {"i", "em"} and content:
        return f"*{content}*"
    if name == "code" and content:
        fence = "``" if "`" in content else "`"
        return f"{fence}{content}{fence}"
    return content


def code_language(pre: Tag) -> str:
    for parent in [pre, *pre.parents]:
        raw = parent.get("codeLanguage") if isinstance(parent, Tag) else None
        if raw:
            return LANGUAGE_MAP.get(raw.lower().replace("_", " "), "text")
        if isinstance(parent, Tag) and parent.name == "table":
            header = parent.find("th")
            if header:
                value = clean_space(header.get_text(" ", strip=True)).lower()
                if value in LANGUAGE_MAP:
                    return LANGUAGE_MAP[value]
    return "text"


def render_table(table: Tag, current: str, assets: bool) -> str:
    rows: list[list[str]] = []
    for row in table.find_all("tr"):
        cells = row.find_all(["th", "td"], recursive=False)
        if not cells:
            continue
        rows.append(
            [
                escape_table("".join(render_inline(child, current, assets) for child in cell.children))
                for cell in cells
            ]
        )
    if not rows:
        return ""
    width = max(len(row) for row in rows)
    rows = [row + [""] * (width - len(row)) for row in rows]
    header = rows[0]
    lines = ["| " + " | ".join(header) + " |", "| " + " | ".join("---" for _ in header) + " |"]
    lines.extend("| " + " | ".join(row) + " |" for row in rows[1:])
    return "\n".join(lines)


def render_inline_container(node: Tag, current: str, assets: bool) -> str:
    """Render a paragraph/cell without inserting breaks between inline tags."""
    parts: list[str] = []
    block_names = {"pre", "table", "ul", "ol", "dl", "p", "div", "blockquote"}
    for child in node.children:
        if isinstance(child, Tag) and child.name.lower() in block_names:
            value = render_block(child, current, assets)
        else:
            value = render_inline(child, current, assets)
        if value:
            parts.append(value)
    return clean_block("".join(parts))


def render_dl(dl: Tag, current: str, assets: bool) -> str:
    chunks: list[str] = []
    for child in dl.find_all(["dt", "dd"], recursive=False):
        text = render_inline_container(child, current, assets)
        if not text:
            continue
        if child.name == "dt":
            if child.find(["b", "strong"], recursive=True):
                chunks.append(text)
            else:
                chunks.append(f"**{text}**")
        else:
            if chunks:
                chunks[-1] += f" — {text}"
            else:
                chunks.append(text)
    return "\n\n".join(chunks)


def render_block(node: Tag | NavigableString, current: str, assets: bool, list_depth: int = 0) -> str:
    if isinstance(node, NavigableString):
        return clean_space(strip_invalid_literal_links(str(node)))
    if not isinstance(node, Tag):
        return ""
    name = node.name.lower()
    if name in {"script", "style", "noscript", "head"}:
        return ""
    if name == "pre":
        text = node.get_text("", strip=False).replace("\r\n", "\n").strip("\n")
        language = code_language(node)
        return f"```{language}\n{text}\n```"
    if name == "table":
        if node.find("pre") and node.find_parent(class_="code"):
            return "\n\n".join(
                render_block(pre, current, assets) for pre in node.find_all("pre", recursive=True)
            )
        return render_table(node, current, assets)
    if name == "dl":
        return render_dl(node, current, assets)
    if name in {"ul", "ol"}:
        lines: list[str] = []
        ordered = name == "ol"
        number = 1
        for li in node.find_all("li", recursive=False):
            prefix = f"{number}. " if ordered else "- "
            number += 1
            parts: list[str] = []
            for child in li.children:
                if isinstance(child, Tag) and child.name in {"ul", "ol"}:
                    continue
                rendered = render_block(child, current, assets, list_depth + 1)
                if rendered:
                    parts.append(rendered)
            text = clean_block(" ".join(parts))
            if text:
                lines.append("  " * list_depth + prefix + text)
            for child in li.find_all(["ul", "ol"], recursive=False):
                nested = render_block(child, current, assets, list_depth + 1)
                if nested:
                    lines.append(nested)
        return "\n".join(lines)
    if name in {"h1", "h2", "h3", "h4", "h5", "h6"}:
        level = min(int(name[1]) + 1, 6)
        text = clean_space("".join(render_inline(c, current, assets) for c in node.children))
        nested_anchor = node.find("a", id=True)
        anchor = node.get("id") or (nested_anchor.get("id") if nested_anchor else None)
        prefix = f'<a id="{anchor}"></a>\n' if anchor else ""
        return f"{prefix}{'#' * level} {text}" if text else ""
    if name == "a" and node.get("id") and not node.get("href"):
        return f'<a id="{node["id"]}"></a>'
    if name in {"p", "dt", "dd", "td", "th"}:
        return render_inline_container(node, current, assets)
    if name in {"div", "section", "article", "blockquote", "tr"}:
        parts = [render_block(child, current, assets, list_depth) for child in node.children]
        value = clean_block("\n\n".join(part for part in parts if part))
        if name == "blockquote" and value:
            return "\n".join(f"> {line}" if line else ">" for line in value.splitlines())
        return value
    if name == "hr":
        return "---"
    if name == "span" and node.find(["pre", "table", "ul", "ol", "dl"], recursive=True):
        return clean_block("\n\n".join(render_block(child, current, assets) for child in node.children))
    if name in {"b", "strong", "i", "em", "span", "label", "a", "code", "small"}:
        return render_inline(node, current, assets)
    return clean_block("\n\n".join(render_block(child, current, assets, list_depth) for child in node.children))


def is_api_page(source: Path) -> bool:
    try:
        data = source.read_bytes()
    except OSError:
        return False
    return bool(API_MARKERS.search(data.decode("cp1252", errors="replace")))


def parse_title(soup: BeautifulSoup, fallback: str) -> str:
    title = soup.find(id="nsrTitle")
    if title:
        value = clean_space(title.get_text(" ", strip=True))
        if value:
            return value
    if soup.title:
        value = clean_space(soup.title.get_text(" ", strip=True))
        if value:
            return value
    return fallback


def convert_page(
    source: Path,
    destination: Path,
    source_label: str,
    xdk_version: str,
    assets: bool,
    api: bool,
) -> None:
    soup = BeautifulSoup(source.read_bytes(), "html.parser")
    for node in soup.select("script, style, noscript, #header, #languageSpan, .footer"):
        node.decompose()
    main = soup.find(id="mainBody") or soup.body or soup
    title = parse_title(soup, source.stem)
    rendered = clean_block("\n\n".join(render_block(child, source.name, assets) for child in main.children))
    rendered = strip_invalid_literal_links(rendered)
    if rendered.startswith(f"# {title}"):
        body = rendered
    else:
        body = f"# {title}\n\n{rendered}" if rendered else f"# {title}"
    kind = "api" if api else "guide"
    metadata = (
        "---\n"
        f"source: {source_label}\n"
        f"source_page: {source.name}\n"
        f"xdk_version: {xdk_version}\n"
        f"kind: {kind}\n"
        "---\n\n"
    )
    destination.write_text(metadata + body.rstrip() + "\n", encoding="utf-8")


def extract_chm(chm: Path, destination: Path) -> Path:
    temporary = Path(tempfile.mkdtemp(prefix="xdk-chm-"))
    command = ["7z", "e", "-y", str(chm), "*.htm", "*.hhc", "*.hhk", "*.png", "*.PNG", "*.jpg", "*.JPG", "*.gif", "*.GIF", "*.bmp", "*.BMP", f"-o{temporary}"]
    subprocess.run(command, check=True, stdout=subprocess.DEVNULL)
    return temporary


def copy_assets(raw: Path, output: Path) -> None:
    assets = output / "assets"
    assets.mkdir(parents=True, exist_ok=True)
    for pattern in ("*.png", "*.PNG", "*.jpg", "*.JPG", "*.gif", "*.GIF", "*.bmp", "*.BMP"):
        for source in raw.glob(pattern):
            target = assets / source.name
            if not target.exists():
                shutil.copy2(source, target)
            lowercase = assets / source.name.lower()
            if lowercase != target and not lowercase.exists():
                shutil.copy2(source, lowercase)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def hhc_toc(raw: Path, output: Path, xdk_version: str) -> int:
    files = list(raw.glob("*.hhc"))
    if not files:
        return 0
    soup = BeautifulSoup(files[0].read_bytes(), "html.parser")
    lines = ["# XDK documentation", "", f"Generated from `xbox360sdk.chm` (XDK {xdk_version}).", ""]

    def walk(ul: Tag, depth: int) -> None:
        # HHC files commonly encode a child <ul> as a sibling immediately
        # after the <li> it belongs to, rather than nesting it inside that li.
        for child in ul.find_all(["li", "ul"], recursive=False):
            if child.name == "ul":
                walk(child, depth + 1)
                continue
            obj = child.find("object", recursive=False)
            if obj:
                params = {p.get("name", ""): p.get("value", "") for p in obj.find_all("param")}
                name = clean_space(params.get("Name", ""))
                local = params.get("Local", "")
                if name:
                    target = page_target(local, "") if local else ""
                    link = f"[{name}]({target})" if target else name
                    lines.append(f"{'  ' * depth}- {link}")

    root = soup.find("ul")
    if root:
        walk(root, 0)
    (output / "toc.md").write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")
    return len(lines) - 4


def hhk_index(raw: Path, output: Path, api_pages: set[str], xdk_version: str) -> int:
    files = list(raw.glob("*.hhk"))
    if not files:
        return 0
    soup = BeautifulSoup(files[0].read_bytes(), "html.parser")
    entries: list[tuple[str, str]] = []
    for obj in soup.find_all("object"):
        params = obj.find_all("param")
        values: dict[str, list[str]] = {}
        for p in params:
            values.setdefault(p.get("name", ""), []).append(p.get("value", ""))
        for name, local in zip(values.get("Name", []), values.get("Local", [])):
            if name and local:
                entries.append((clean_space(name), page_target(local, "")))
    entries = sorted(set(entries), key=lambda item: (item[0].casefold(), item[1]))
    lines = ["# XDK API index", "", f"Keyword index extracted from `xbox360sdk.chm` (XDK {xdk_version}).", ""]
    for name, target in entries:
        lines.append(f"- [{name}]({target})")
    (output / "index.md").write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")

    api_lines = [
        "# XDK API pages",
        "",
        "Pages containing the CHM API sections `Syntax`, `Parameters`, `Return Value`, or `Requirements`.",
        "",
    ]
    for page in sorted(api_pages, key=str.casefold):
        api_lines.append(f"- [{Path(page).stem}]({Path(page).with_suffix('.md').name})")
    (output / "api-index.md").write_text("\n".join(api_lines).rstrip() + "\n", encoding="utf-8")
    return len(entries)


def normalize_local_links(output: Path) -> int:
    """Resolve CHM's Windows-case-insensitive links for case-sensitive hosts."""
    files = [path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()]
    by_fold = {path.casefold(): path for path in files}
    link_pattern = re.compile(r"(\[([^\]]*)\]\()([^)]*)(\))")
    changed = 0
    for page in output.glob("*.md"):
        original = page.read_text(encoding="utf-8")

        def replace(match: re.Match[str]) -> str:
            nonlocal changed
            label = match.group(2)
            target = match.group(3)
            if target.startswith(("http://", "https://", "mailto:", "#", "data:")):
                return match.group(0)
            path, fragment = urldefrag(target)
            if not path or (output / path).exists():
                return match.group(0)
            resolved = by_fold.get(path.casefold())
            if not resolved:
                if not Path(path).suffix and "/" not in path and "\\" not in path:
                    changed += 1
                    return label
                return match.group(0)
            changed += 1
            return f"[{label}]({resolved}{('#' + fragment) if fragment else ''})"

        updated = link_pattern.sub(replace, original)
        if updated != original:
            page.write_text(updated, encoding="utf-8")
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("chm", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--source-label", default="doc/1033/xbox360sdk.chm")
    parser.add_argument("--xdk-version", required=True)
    parser.add_argument("--keep-raw", action="store_true")
    args = parser.parse_args()

    if not args.chm.is_file():
        parser.error(f"CHM not found: {args.chm}")
    args.output.mkdir(parents=True, exist_ok=True)
    raw = extract_chm(args.chm, args.output)
    try:
        copy_assets(raw, args.output)
        pages = sorted(raw.glob("*.htm"))
        api_pages = {page.name for page in pages if is_api_page(page)}
        for number, page in enumerate(pages, 1):
            convert_page(
                page,
                args.output / f"{page.stem}.md",
                args.source_label,
                args.xdk_version,
                assets=True,
                api=page.name in api_pages,
            )
            if number % 500 == 0:
                print(f"converted={number}/{len(pages)}")
        toc_count = hhc_toc(raw, args.output, args.xdk_version)
        index_count = hhk_index(raw, args.output, api_pages, args.xdk_version)
        normalized_links = normalize_local_links(args.output)
        readme = (
            "# Xbox 360 XDK API (Markdown)\n\n"
            f"Generated from the local `{args.source_label}` for XDK version `{args.xdk_version}`.\n\n"
            f"- Pages converted: **{len(pages)}**\n"
            f"- API-like pages: **{len(api_pages)}**\n"
            f"- Table-of-contents entries: **{toc_count}**\n"
            f"- Keyword entries: **{index_count}**\n\n"
            f"- Case-normalized local links: **{normalized_links}**\n\n"
            "Use [api-index.md](api-index.md) for API pages, [index.md](index.md) for the keyword index, and [toc.md](toc.md) for the original navigation tree.\n\n"
            "> Provenance and source hashes: see [PROVENANCE.md](PROVENANCE.md). The `.chi` files are identical indexes; the English `.chm` supplies the page bodies.\n"
        )
        (args.output / "README.md").write_text(readme, encoding="utf-8")
        chi_hashes = []
        for chi in sorted(args.chm.parent.glob("*.chi")):
            chi_hashes.append(f"- `{chi.name}`: `{sha256(chi)}`")
        provenance = (
            "# Markdown conversion provenance\n\n"
            f"- Source: `{args.source_label}`\n"
            f"- Source SHA-256: `{sha256(args.chm)}`\n"
            f"- XDK version: `{args.xdk_version}`\n"
            f"- Pages converted: `{len(pages)}` (API-like: `{len(api_pages)}`)\n"
            "- Conversion: `tools/xdk_chm_to_markdown.py`, BeautifulSoup HTML parsing, local links rewritten from `.htm` to `.md`.\n"
            "- The `.chi` files found beside the CHM:\n"
            + ("\n".join(chi_hashes) if chi_hashes else "- none")
            + "\n\nThe corpus is a derived local working copy. Code blocks and cross-references are preserved where the CHM exposes them; Microsoft-specific layout widgets and decorative images are intentionally omitted.\n"
        )
        (args.output / "PROVENANCE.md").write_text(provenance, encoding="utf-8")
        print(f"pages={len(pages)} api_pages={len(api_pages)} toc_entries={toc_count} keyword_entries={index_count}")
    finally:
        if args.keep_raw:
            print(f"raw={raw}")
        else:
            shutil.rmtree(raw, ignore_errors=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
