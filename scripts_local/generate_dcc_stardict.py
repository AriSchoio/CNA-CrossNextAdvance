#!/usr/bin/env python3
"""Generate a Dungeon Crawler Carl series-reference StarDict dictionary."""

from pathlib import Path
from bs4 import BeautifulSoup
import html
import re
import struct
import zipfile


ROOT = Path(__file__).resolve().parents[1]
ATTACHMENTS = Path("/home/codespace/.vscode-remote/data/agentSessionData/17efbd24-0c06-4ecc-a40c-59459a3cfdc5/attachments")
OUTPUT = ROOT / "assets_local/sd-resources/dictionaries/Dungeon Crawler Carl - Series Dictionary"

STOP_WORDS = {
    "the", "a", "an", "and", "or", "but", "if", "then", "than", "while",
    "when", "where", "what", "who", "why", "how", "this", "that", "these",
    "those", "there", "here", "chapter", "part", "book", "table", "contents",
    "copyright", "about", "author", "matt", "dinniman", "new", "york",
    "one", "two", "three", "first", "second", "third", "he", "she", "they",
    "his", "her", "their", "we", "you", "it", "i", "chapter",
}

LOCATION_HINTS = (
    "floor", "level", "dungeon", "world", "city", "station", "planet", "moon",
    "room", "hall", "arena", "forest", "island", "river", "market", "zone",
    "rail", "valley", "kingdom", "camp", "cave", "stage",
)
SPECIES_HINTS = (
    "species", "creature", "monster", "race", "mob", "npc", "animal", "beast",
    "goblin", "orc", "elf", "human", "robot", "dog", "cat", "dragon",
)
ITEM_HINTS = (
    "weapon", "sword", "armor", "potion", "ring", "key", "spell", "item",
    "loot", "box", "chest", "card", "shield", "staff", "recipe", "enchanted",
    "skill", "achievement", "upgrade", "equipment",
)
CHARACTER_HINTS = (
    "said", "asked", "replied", "told", "whispered", "shouted", "laughed",
    "looked", "stood", "sat", "walked", "ran", "attacked",
)


def clean_text(raw):
    soup = BeautifulSoup(raw, "html.parser")
    text = soup.get_text(" ", strip=True)
    text = html.unescape(text)
    return re.sub(r"\s+", " ", text).strip()


def epub_text(epub_path):
    with zipfile.ZipFile(epub_path) as archive:
        paths = [
            name for name in archive.namelist()
            if name.lower().endswith((".xhtml", ".html", ".htm"))
            and not name.lower().endswith("nav.xhtml")
        ]
        texts = []
        for name in paths:
            raw = archive.read(name).decode("utf-8", errors="replace")
            cleaned = clean_text(raw)
            if cleaned:
                texts.append(cleaned)
    return "\n".join(texts)


def sentences(text):
    parts = re.split(r"(?<=[.!?])(?:\s+|$)", text)
    return [re.sub(r"\s+", " ", part).strip() for part in parts if len(part.strip()) > 20]


def classify(word, context, sentence_text):
    low = (" " + (context + " " + sentence_text).lower() + " ").replace("-", " ")
    if any(hint in low for hint in CHARACTER_HINTS):
        return "Character or named participant"
    if any(hint in low for hint in LOCATION_HINTS):
        return "Location, environment, or dungeon area"
    if any(hint in low for hint in SPECIES_HINTS):
        return "Species or creature"
    if any(hint in low for hint in ITEM_HINTS):
        return "Item, equipment, ability, or game mechanic"
    return "Named series entity, title, organization, term, or location"


def escape(value):
    return value.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


def main():
    if not ATTACHMENTS.is_dir():
        raise FileNotFoundError(ATTACHMENTS)

    epubs = sorted(ATTACHMENTS.rglob("*.epub"))
    if len(epubs) < 2:
        raise RuntimeError(f"Expected EPUB attachments, found {len(epubs)}")

    occurrences = {}
    book_texts = {}
    for epub in epubs:
        title = epub.name.removesuffix(".epub")
        text = epub_text(epub)
        book_texts[title] = text
        for sentence_text in sentences(text):
            for match in re.finditer(
                r"\b(?:[A-Z][A-Za-z0-9'’.-]*|[A-Z]{2,}[A-Za-z0-9]*)(?:\s+(?:of\s+|the\s+|[A-Z][A-Za-z0-9'’.-]*)){0,3}\b",
                sentence_text,
            ):
                term = re.sub(r"\s+", " ", match.group(0)).strip(" \t,.;:!?\"()[]{}")
                if len(term) < 2 or term.lower() in STOP_WORDS:
                    continue
                if term[0].isdigit():
                    continue
                key = term.casefold()
                entry = occurrences.setdefault(key, {"term": term, "contexts": [], "books": set()})
                entry["books"].add(title)
                if sentence_text not in entry["contexts"] and len(entry["contexts"]) < 4:
                    entry["contexts"].append(sentence_text[:700])

    # Add selected frequent all-caps and system-style terms that may begin lowercase.
    system_pattern = re.compile(
        r"\b(?:achievement|class|skill|spell|quest|loot|mob|boss|NPC|stat|stats|"
        r"debuff|buff|mana|xp|experience|enchantment|dungeon|crawl|crawler|"
        r"prize|package|sponsor|viewer|inventory|pet|mount|guild|follower)\b",
        re.IGNORECASE,
    )
    for title, text in book_texts.items():
        for sentence_text in sentences(text):
            for match in system_pattern.finditer(sentence_text):
                term = match.group(0).lower()
                key = term.casefold()
                entry = occurrences.setdefault(key, {"term": term, "contexts": [], "books": set()})
                entry["books"].add(title)
                if sentence_text not in entry["contexts"] and len(entry["contexts"]) < 4:
                    entry["contexts"].append(sentence_text[:700])

    rows = []
    for key in sorted(occurrences, key=lambda value: value.encode("utf-8")):
        entry = occurrences[key]
        term = entry["term"]
        context = " ".join(entry["contexts"])
        category = classify(term, context, context)
        books = sorted(entry["books"])
        definition = (
            f"{escape(category)} in the Dungeon Crawler Carl series"
            + (f". Appears in: {escape('; '.join(books))}" if len(books) <= 8 else ". Appears across multiple books")
            + ".\n\nContext excerpts:\n"
            + "\n".join("• " + escape(text) for text in entry["contexts"])
        )
        rows.append((term.casefold(), term, definition))

    OUTPUT.mkdir(parents=True, exist_ok=True)
    index = bytearray()
    definitions = bytearray()
    seen_terms = set()
    for _, term, definition in sorted(rows, key=lambda row: row[0].encode("utf-8")):
        key = term.casefold()
        if key in seen_terms:
            continue
        seen_terms.add(key)
        word = term.encode("utf-8")
        value = definition.encode("utf-8")
        offset = len(definitions)
        index += word + b"\0" + struct.pack(">II", offset, len(value))
        definitions += value

    stem = OUTPUT / "dcc-series"
    stem.with_suffix(".dict").write_bytes(definitions)
    stem.with_suffix(".idx").write_bytes(index)
    stem.with_suffix(".ifo").write_text(
        "StarDict's dict ifo file\n"
        "version=2.4.2\n"
        f"bookname=Dungeon Crawler Carl - Series Dictionary\n"
        f"wordcount={len(seen_terms)}\n"
        f"idxfilesize={len(index)}\n"
        "sametypesequence=m\n",
        encoding="utf-8",
    )
    (OUTPUT / "README.md").write_text(
        "# Dungeon Crawler Carl - Series Dictionary\n\n"
        "Generated locally from the EPUB files attached to this workspace.\n"
        "Entries contain named-series references, system vocabulary, and contextual excerpts "
        "from the books. Common English words should be looked up with the bundled English Simple dictionary.\n\n"
        "Source books: " + ", ".join(sorted(epub.name for epub in epubs)) + "\n",
        encoding="utf-8",
    )
    print(f"Generated {len(seen_terms)} entries in {OUTPUT}")


if __name__ == "__main__":
    main()
