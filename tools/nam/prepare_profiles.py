#!/usr/bin/env python3
"""Fetch pinned profiles for a LOCAL evaluation build; never commit/publish T3K weights.
Author permission is required before redistributing a binary containing T3K profiles.
"""
import hashlib,json,subprocess,sys
from pathlib import Path
root=Path(__file__).resolve().parents[2]
out=Path(sys.argv[1] if len(sys.argv)>1 else root/'build'/'nam-assets')
out.mkdir(parents=True,exist_ok=True)
models=[]
for item in json.loads((Path(__file__).with_name('profiles.json')).read_text()):
 p=out/(item['id']+'.nam')
 if not p.exists():subprocess.run(['curl','-fsSL','--retry','2',item['url'],'-o',str(p)],check=True)
 if hashlib.sha256(p.read_bytes()).hexdigest()!=item['sha256']:raise RuntimeError('Profile hash mismatch: '+item['id'])
 data=json.loads(p.read_text())
 # Pin the supplied A2-Lite submodel. It is an author-provided variant, not retrained.
 model=data['config']['submodels'][0]['model']
 model['sample_rate']=data['sample_rate']
 models.append(json.dumps(model,separators=(',',':')))
(out/'EmbeddedNam.h').write_text('#pragma once\nstatic const char* const embeddedNam[] = {\n'+',\n'.join('R"NAMJSON('+m+')NAMJSON"' for m in models)+'\n};\n')
print('Prepared four hash-verified A2-Lite models for LOCAL evaluation only.')
