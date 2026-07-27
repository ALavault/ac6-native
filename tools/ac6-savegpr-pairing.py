import sys,re,pathlib
gen=pathlib.Path(sys.argv[1])
FN=re.compile(r'^PPC_FUNC_IMPL\(__imp__((?:rex_)?sub_[0-9A-F]+)\)')
tot=sv=rs=0
for p in sorted(gen.glob('*.cpp')):
    cur=None; a=b=0
    for line in p.read_text(errors='replace').splitlines():
        m=FN.match(line)
        if m:
            if cur:
                tot+=1
                if a>b: sv+=1
                elif b>a: rs+=1
            cur=m.group(1); a=b=0
        elif cur:
            if 'savegprlr' in line: a+=1
            if 'restgprlr' in line: b+=1
    if cur:
        tot+=1
        if a>b: sv+=1
        elif b>a: rs+=1
print(f"{sys.argv[2]:<22} funcs={tot:<7} save_wo_restore={sv:<5} restore_wo_save={rs:<6} ({100*rs/max(tot,1):.1f}%)")
