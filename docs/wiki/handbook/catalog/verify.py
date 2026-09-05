#!/usr/bin/env python3
"""Verify this historical catalog against its exact Git source snapshot.

Run from any directory: python3 docs/wiki/handbook/catalog/verify.py.
This checks documentation coverage, not runtime behavior or current product state.
"""
import json
import os
from pathlib import Path
import subprocess

BASE = '9728612046940b55d69f85c3811eb38a08a0963b'
HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[3]


def source(path):
    return subprocess.check_output(['git', 'show', f'{BASE}:{path}'], cwd=ROOT, text=True)


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def escaped(value):
    return str(value).replace('|', '&#124;').replace('\n', ' ')


def literal(value):
    return '`' + escaped(json.dumps(value, ensure_ascii=False, separators=(',', ':'))) + '`'


def main():
    tracked = subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', BASE], cwd=ROOT, text=True).splitlines()
    pages = {p.stem: p.read_text() for p in HERE.glob('*.md')}
    for name, text in pages.items():
        require(BASE in text, f'{name}: missing snapshot provenance')
    ledger = json.loads(source('ops/team/features.json'))['features']
    steps = [step for feature in ledger for step in feature.get('steps', [])]
    evidence_count = 0
    for feature in ledger:
        require(feature['id'] + ' — ' + feature['title'] in pages['features'], feature['id'])
        nodes = [feature.get('stoppingPoint', {})] + feature.get('steps', [])
        for node in [feature] + nodes:
            for field in ('summary', 'caveat'):
                require(node.get(field, '') in pages['features'], f"{feature['id']}: missing {field}")
            if 'state' in node:
                require(f"**State:** `{node['state']}`" in pages['features'], 'missing state')
        for step in feature.get('steps', []):
            require(step['id'] + ' — ' + step['title'] in pages['features'], step['id'])
            require(f"**Weight:** {step['weight']}." in pages['features'], 'missing weight')
        for node in nodes:
            for item in node.get('evidence', []):
                evidence_count += 1
                require(item.get('summary', '') in pages['features'], 'missing evidence summary')
                reference = item['reference']
                target = os.path.relpath(ROOT / reference, HERE) if reference.startswith('docs/wiki/') else reference
                require(target in pages['features'], f'missing evidence reference: {reference}')
    settings = json.loads(source('data/settings/schema-v2.json'))['settings']
    for setting in settings:
        row = next((line for line in pages['settings'].splitlines() if line.startswith('| `' + setting['key'] + '` |')), '')
        require(row and literal(setting['default']) in row and setting['type'] in row, setting['key'])
        if 'constraints' in setting:
            require(literal(setting['constraints']) in row, setting['key'] + ' constraints')
    wiki = [p for p in tracked if p.startswith('docs/wiki/') and p.endswith('.md')]
    for path in wiki:
        require(os.path.relpath(ROOT / path, HERE) in pages['reading'], 'missing wiki page: ' + path)
    counts = {}
    for category in ('profiles', 'themes', 'applets'):
        paths = [p for p in tracked if p.startswith('data/' + category + '/') and p.endswith('.json')]
        counts[category] = len(paths)
        for path in paths:
            require('`' + path + '`' in pages['assets'], 'missing asset: ' + path)
            asset = json.loads(source(path))
            require('### ' + asset['id'] + ' — ' + asset['name'] in pages['assets'], 'missing asset identity')
            if category != 'profiles':
                for key, value in asset.items():
                    if key not in ('id', 'name', 'description'):
                        require('| ' + key + ' | ' + literal(value) + ' |' in pages['assets'], path + ': ' + key)
            else:
                for panel in asset['panels']:
                    require('| ' + panel['id'] + ' |' in pages['assets'], path + ' panel')
                    for applet in panel.get('applets', []):
                        require('`' + applet['id'] + '` → `' + applet['plugin'] + '`' in pages['assets'], path + ' applet')
    tool_paths = [p for p in tracked if p.startswith('tools/')]
    for path in tool_paths:
        require('`' + path + '`' in pages['repository'], 'missing tool: ' + path)
    print(json.dumps(dict(features=len(ledger), steps=len(steps), evidence_items=evidence_count,
                         wiki_pages=len(wiki), settings_keys=len(settings), tools=len(tool_paths), **counts)))
    print('PASS: snapshot coverage; run tools/validate-docs and mkdocs build --strict for integrated navigation.')


if __name__ == '__main__':
    main()
