"""Audit quest-reader native helper IDs against an explicitly supplied 1.11.240 database."""
import argparse, hashlib, json, re, struct
from pathlib import Path
p = argparse.ArgumentParser()
p.add_argument('--commonlib', required=True)
p.add_argument('--database', required=True)
p.add_argument('--output', required=True)
a = p.parse_args()
lib = Path(a.commonlib)
text = (lib / 'include/RE/IDs.h').read_text()
stack, pending, ids = [], None, {}
for m in re.finditer(r'namespace\s+([\w:]+)|inline constexpr REL::ID\s+(\w+)\{\s*(\d+)\s*\}|([{}])', text):
    ns, name, number, brace = m.groups()
    if ns: pending = ns
    elif name: ids['::'.join([s for s in stack if s] + [name]).removeprefix('RE::ID::')] = int(number)
    elif brace == '{': stack.append(pending); pending = None
    else: stack.pop()
names = [
    'PlayerCharacter::Singleton', 'TESDataHandler::Singleton', 'TESQuest::GetAliasedRef',
    'TESObjectREFR::GetDisplayFullName', 'BSSpinLock::lock', 'BSSpinLock::BSReadWriteLock::lock_read',
    'BSPointerHandle::BSPointerHandleManagerInterface::GetSmartPointer',
    'TESForm::AllForms', 'TESForm::AllFormsMapLock', 'TESForm::AllFormsByEditorID',
    'TESForm::AllFormsEditorIDMapLock', 'TESFullName::SparseFullNameMap',
    'BSStringPool::GetEntry_char', 'BSStringPool::Entry::Release',
]
raw = Path(a.database).read_bytes()
count, = struct.unpack_from('<Q', raw)
assert len(raw) == 8 + count * 16
db = dict(struct.iter_unpack('<QQ', raw[8:]))
records = []
for name in names:
    number = ids[name]
    assert number and number in db, (name, number)
    record = dict(name=name, id=number, offset=hex(db[number]))
    records.append(record)
    print(name, number, record['offset'])
result = dict(runtime='1.11.240.0', database_sha256=hashlib.sha256(raw).hexdigest(), entries=count,
              note='Presence confirms a mapping, not live correctness. Native inline data access has no relocation. Existing RTTI/runtime helper coverage is retained in the full 0.9.20 audit.',
              helpers=records, missing=[])
Path(a.output).write_text(json.dumps(result, indent=2) + '\n')
print(f'{len(records)} quest-reader helper IDs mapped; no missing IDs')
