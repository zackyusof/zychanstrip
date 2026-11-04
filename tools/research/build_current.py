#!/usr/bin/env python3
"""Link a legacy shim harness with the current, already-built NAM engine objects."""
from pathlib import Path
import os,subprocess,sys
root=Path(__file__).resolve().parents[2]
objroot=root/'build/CMakeFiles/zychanstrip.dir'
objects=[str(p) for p in objroot.rglob('*.o') if '/NAM/' in str(p) or p.name=='ColourEngine.cpp.o']
if not objects:raise SystemExit('Build the current macOS beta with Ninja first (NAM engine objects required).')
name=sys.argv[1]
cmd=[os.environ.get('CXX','c++'),'-std=c++20','-O3','-DNDEBUG','-I',str(Path(__file__).with_name('shim')),str(Path(__file__).with_name(name+'.cpp')),*objects,'-framework','Accelerate','-o',str(root/'build'/name)]
subprocess.run(cmd,check=True)
print('built',root/'build'/name)
