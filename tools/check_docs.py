#!/usr/bin/env python3
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]

PROSE_SUFFIXES = {'.md', '.html', '.yml', '.yaml'}
SOURCE_SUFFIXES = {'.c', '.cpp', '.h', '.hpp', '.inc', '.py', '.cmake', '.txt', '.csv'}
EXCLUDED_PREFIXES = ('managed_components/', 'build/', 'release/')
EXCLUDED_FILES = {'LICENSE', 'docs/NotoSansCJK-OFL.txt', 'tools/check_docs.py'}

PROCESS_PHRASES = (
    '交接', '实施指南', '本轮', '这一轮', '脱敏', '迁移前', '私有历史', '私有目录',
    'handoff', 'codex', 'claude.ai', 'antigravity',
)
STATUS_PHRASES = (
    '未验证', '未经验证', '待验证', '待实机验证', '待验收', '未测', '仍需验证', '仍需实测', '仍需目检',
    '实机通过', '实机验收', '真机验收', '测试阶段',
    'unverified', 'not yet verified', 'still in testing',
)
PROSE_PATTERNS = (
    (re.compile(r'\bCOM\d+\b'), 'machine-specific serial port'),
    (re.compile(r'\b[A-Za-z]:\\(?:Users|Espressif|workspace)\\', re.I), 'machine-specific path'),
)
SOURCE_MARKERS = re.compile(r'\b(?:TODO|FIXME|XXX|HACK)\b')
MARKDOWN_LINK = re.compile(r'!?\[[^\]]*\]\(([^)\s]+)(?:\s+"[^"]*")?\)')
FENCED_CODE = re.compile(r'```.*?```', re.S)


def repository_files() -> list[str]:
    listed = subprocess.run(
        ['git', 'ls-files', '--cached', '--others', '--exclude-standard'],
        cwd=ROOT, check=True, capture_output=True, text=True, encoding='utf-8',
    ).stdout.splitlines()
    return sorted(
        rel for rel in set(listed)
        if (ROOT / rel).is_file() and not rel.startswith(EXCLUDED_PREFIXES) and rel not in EXCLUDED_FILES
    )


def line_of(text: str, index: int) -> int:
    return text.count('\n', 0, index) + 1


def find_phrases(text: str, phrases: tuple[str, ...]):
    lowered = text.lower()
    for phrase in phrases:
        start = lowered.find(phrase.lower())
        while start != -1:
            yield line_of(text, start), phrase
            start = lowered.find(phrase.lower(), start + 1)


def check_prose(rel: str, text: str, errors: list[str]) -> None:
    for line, phrase in find_phrases(text, PROCESS_PHRASES):
        errors.append(f'{rel}:{line}: process wording "{phrase}"')
    for line, phrase in find_phrases(text, STATUS_PHRASES):
        errors.append(f'{rel}:{line}: verification status wording "{phrase}"')
    for pattern, label in PROSE_PATTERNS:
        for match in pattern.finditer(text):
            errors.append(f'{rel}:{line_of(text, match.start())}: {label} "{match.group(0)}"')


def check_links(rel: str, text: str, errors: list[str]) -> None:
    base = (ROOT / rel).parent
    prose = FENCED_CODE.sub(lambda m: '\n' * m.group(0).count('\n'), text)
    for match in MARKDOWN_LINK.finditer(prose):
        target = match.group(1)
        if urlsplit(target).scheme or target.startswith('#'):
            continue
        path = unquote(target.split('#', 1)[0])
        if path and not (base / path).exists():
            errors.append(f'{rel}:{line_of(prose, match.start())}: broken local link "{target}"')


def check_source(rel: str, text: str, errors: list[str]) -> None:
    for match in SOURCE_MARKERS.finditer(text):
        errors.append(f'{rel}:{line_of(text, match.start())}: unfinished-work marker "{match.group(0)}"')


def main() -> int:
    for stream in (sys.stdout, sys.stderr):
        stream.reconfigure(encoding='utf-8')
    errors: list[str] = []
    files = repository_files()
    for rel in files:
        suffix = Path(rel).suffix.lower()
        if suffix not in PROSE_SUFFIXES | SOURCE_SUFFIXES:
            continue
        text = (ROOT / rel).read_text(encoding='utf-8')
        if suffix in PROSE_SUFFIXES:
            check_prose(rel, text, errors)
        else:
            check_source(rel, text, errors)
        if suffix == '.md':
            check_links(rel, text, errors)

    for required in ('LICENSE', 'docs/THIRD_PARTY.md', 'docs/NotoSansCJK-OFL.txt'):
        if not (ROOT / required).is_file():
            errors.append(f'{required}: missing required license file')

    if errors:
        print(f'FAIL: {len(errors)} problems in {len(files)} files.', file=sys.stderr)
        for message in errors:
            print(f'  {message}', file=sys.stderr)
        return 1
    print(f'PASS: {len(files)} files checked for process wording, verification status, '
          'unfinished-work markers, local links and license files.')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
