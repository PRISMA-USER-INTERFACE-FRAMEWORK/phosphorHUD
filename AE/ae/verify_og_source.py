"""Check OG-active source against the reviewed version baseline; --legacy audits pre-AE 0.9.17."""
import argparse, difflib, hashlib, json, re
from pathlib import Path
root=Path(__file__).resolve().parents[1]
baseline=root/'.build/ae-feasibility/before'
def og(text):
 out=[];stack=[]
 for line in text.splitlines(True):
  if line.strip()=='#if defined(PHOSPHOR_AE)':stack.append(False);continue
  if line.strip()=='#if !defined(PHOSPHOR_AE)':stack.append(True);continue
  if stack and line.strip()=='#else':stack[-1]=not stack[-1];continue
  if stack and line.strip()=='#endif':stack.pop();continue
  if all(stack):out.append(line)
 assert not stack
 return ''.join(out)
def tokens(text):
 # Preserve strings (including whitespace); discard comments and insignificant whitespace.
 return [t for t in re.findall(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|\w+|[^\s]', text)
         if not t.startswith('//') and not t.startswith('/*')]

def fingerprint(text):
 return hashlib.sha256(json.dumps(tokens(og(text)), ensure_ascii=False, separators=(',', ':')).encode()).hexdigest()

def main():
 p=argparse.ArgumentParser(description=__doc__)
 p.add_argument('--legacy',action='store_true',help='Compare to the historical pre-AE 0.9.17 snapshot (expected to differ after authorized OG features).')
 a=p.parse_args()
 fail=[]
 if a.legacy:
  for old in sorted((baseline/'src').glob('*')):
   if not old.is_file():continue
   current=root/'src'/old.name
   before=og(old.read_text(encoding='utf-8'));after=og(current.read_text(encoding='utf-8'))
   if tokens(before)!=tokens(after):
    fail.append(old.name);print(''.join(difflib.unified_diff(before.splitlines(True),after.splitlines(True),fromfile='0.9.17/'+old.name,tofile='OG/'+old.name)))
 else:
  manifest=json.loads((root/'ae/og-source-0.9.22.json').read_text(encoding='utf-8'))
  actual={p.name:p for p in (root/'src').iterdir() if p.is_file()}
  expected=manifest['files']
  if set(actual)!=set(expected):fail.append('source file inventory changed')
  for name,digest in expected.items():
   if name not in actual or fingerprint(actual[name].read_text(encoding='utf-8'))!=digest:fail.append(name)
   else:print(name+': reviewed OG 0.9.22 tokens match')
 if fail:raise SystemExit('OG source verification FAILED: '+', '.join(fail))
 print('OG source equivalence: PASS')

if __name__=='__main__':main()
