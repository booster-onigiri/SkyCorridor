"""Check bilingual format signatures and exact display dictionaries."""
from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "Project/Source/EndlessWorld"
STRING = r'TEXT\("((?:[^"\\]|\\.)*)"\)'
PAIR = re.compile(STRING + r'\s*,\s*' + STRING)
FORMATS = re.compile(r'%(?:[-+ #0]*)(?:\d+|\*)?(?:\.(?:\d+|\*))?(?:hh|ll|I64|[hlztL])?[diuoxXfFeEgGaAcsp%]')


def formats(value):
    return FORMATS.findall(value)


def check():
    problems = []
    pairs = 0
    entries = {}
    for path in SOURCE.glob("EWLocalization*.inl"):
        for match in PAIR.finditer(path.read_text(encoding="utf-8-sig")):
            ja, en = match.groups()
            if ja in entries and entries[ja] != en:
                problems.append(f"Conflicting display translation: {ja}")
            entries[ja] = en
            if not en.strip():
                problems.append(f"Empty translation in {path.name}")
    for path in SOURCE.glob("*.cpp"):
        text = path.read_text(encoding="utf-8-sig")
        for match in re.finditer(r'EWL::(?:Pick|Format)\(\s*' + STRING + r'\s*,\s*' + STRING, text):
            ja, en = match.groups()
            pairs += 1
            if formats(ja) != formats(en):
                problems.append(f"Format signature mismatch in {path.name}: {ja}")
    return {"success": not problems, "bilingual_calls": pairs, "dictionary_entries": len(entries), "problems": problems}


if __name__ == "__main__":
    result = check()
    print(json.dumps(result, ensure_ascii=False, indent=2))
    raise SystemExit(0 if result["success"] else 1)
