#!/usr/bin/env python3
"""Verify the PAL AC6 D3D scratch-callback / PM4 interrupt bridge.

The input is the reconstructed 0x009F0000-byte memory image. The script emits
only addresses, instruction words, hashes and decoded metadata; it never emits
game bytes.
"""
from __future__ import annotations
import argparse, bisect, csv, hashlib, json, struct
from dataclasses import asdict, dataclass
from pathlib import Path

BASE=0x82000000
TEXT_BEGIN=0x82090000
TEXT_END=0x82375984
PDATA_RVA=0x00077200
PDATA_SIZE=0x00010600
EXPECTED_IMAGE_SHA256="b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01"

@dataclass(frozen=True)
class CallbackProducer:
    callsite:int
    encoder:int
    callback:int
    route:str
    parameter:str
    flags:str

EXPECTED_ANCHORS={
  # StartWorkerQueue callback materialisation and wrapper call.
  0x821B9110:0x3D60821C,
  0x821B9118:0x38AB4A60,
  0x821B9120:0x48001959,
  # Direct StartWorkerQueue packet builder.
  0x821C4D20:0x3D60821C,
  0x821C4D28:0x38CB4A60,
  0x821C4D30:0x4BFF54C9,
  # Direct D3D callback 0x821C5190 packet builder.
  0x821C5438:0x3D40821C,
  0x821C5440:0x38CA5190,
  0x821C5458:0x4BFF4DA1,
  # Wrapper r5/r6 -> encoder r6/r7.
  0x821BAAAC:0x7F87E378,
  0x821BAAB0:0x7FA6EB78,
  0x821BAAB4:0x7FC5F378,
  0x821BAABC:0x4BFFF73D,
  # Scratch4/5 type-0 packet.
  0x821BA248:0x3D000001,
  0x821BA250:0x6108057C,
  0x821BA254:0x95090004,
  0x821BA258:0x94C90004,
  0x821BA25C:0x94E90004,
  # Scratch0 and PM4 interrupt sequence.
  0x821BA268:0x39000578,
  0x821BA27C:0x95090004,
  0x821BA280:0x95490004,
  0x821BA2C4:0x810B31A4,
  0x821BA2C8:0x6508C000,
  0x821BA2CC:0x61085400,
  0x821BA2D0:0x95090004,
  0x821BA2D4:0x95490004,
  # CPU callback dispatcher, source 1.
  0x821B9720:0x2B030001,
  0x821B9728:0x815E2A94,
  0x821B9734:0x83EA0010,
  0x821B9758:0x817E2A94,
  0x821B975C:0x806B0014,
  0x821B9764:0x4E800421,
  0x821B9768:0x894D010C,
}

EXPECTED_PRODUCERS=(
  CallbackProducer(0x821B9120,0x821BAA78,0x821C4A60,"wrapper","computed queue parameter","dynamic CPU-mask flags"),
  CallbackProducer(0x821C4D30,0x821BA1F8,0x821C4A60,"direct","computed queue parameter","r30 | 1"),
  CallbackProducer(0x821C5458,0x821BA1F8,0x821C5190,"direct","device callback state","0"),
  CallbackProducer(0x822E4444,0x821BAA78,0x822E4240,"tail wrapper","incoming r3","0"),
  CallbackProducer(0x822E4470,0x821BAA78,0x822E4268,"wrapper","incoming r3","0"),
)

EXPECTED_FUNCTIONS={
  0x821B8ED8:(0x821B9410,"frame/bootstrap command producer"),
  0x821B9710:(0x821B9810,"D3D graphics interrupt dispatcher"),
  0x821B9BC8:(0x821B9DB0,"D3D::CDevice::AddCallsToPrimaryBuffer"),
  0x821BA1F8:(0x821BA388,"scratch callback and PM4 interrupt encoder"),
  0x821BA780:(0x821BA880,"D3D::CDevice::KickOff"),
  0x821BAA78:(0x821BAAD0,"callback-packet wrapper"),
  0x821C4A60:(0x821C4B38,"D3D::StartWorkerQueue callback"),
  0x821C5190:(0x821C5328,"D3D command-processor callback"),
}

def u32(data:bytes,va:int)->int:
    off=va-BASE
    if off<0 or off+4>len(data): raise ValueError(f"outside image: 0x{va:08X}")
    return struct.unpack_from(">I",data,off)[0]

def decode_branch(pc:int,word:int):
    if word>>26!=18:return None
    disp=word&0x03FFFFFC
    if disp&0x02000000:disp-=0x04000000
    target=(disp if ((word>>1)&1) else pc+disp)&0xFFFFFFFF
    return target,word&1

def callsites(data:bytes,target:int):
    found=[]
    for pc in range(TEXT_BEGIN,TEXT_END,4):
        if decode_branch(pc,u32(data,pc))==(target,1):found.append(pc)
    return found

def pdata_starts(data:bytes):
    result=[]
    for off in range(PDATA_RVA,PDATA_RVA+PDATA_SIZE,8):
        begin=struct.unpack_from(">I",data,off)[0]
        if TEXT_BEGIN<=begin<TEXT_END:result.append(begin)
    if result!=sorted(result) or len(result)!=len(set(result)):
        raise ValueError(".pdata starts are not strict and unique")
    return result

def function_end(starts:list[int],begin:int)->int:
    i=bisect.bisect_left(starts,begin)
    if i>=len(starts) or starts[i]!=begin or i+1>=len(starts):
        raise ValueError(f"missing .pdata function: 0x{begin:08X}")
    return starts[i+1]

def hash_range(data:bytes,begin:int,end:int)->str:
    return hashlib.sha256(data[begin-BASE:end-BASE]).hexdigest()

def producer_rows():
    return [asdict(row) for row in EXPECTED_PRODUCERS]

def verify(data:bytes):
    failures=[]
    digest=hashlib.sha256(data).hexdigest()
    if digest!=EXPECTED_IMAGE_SHA256:failures.append(f"image SHA-256 {digest}")
    for va,expected in EXPECTED_ANCHORS.items():
        actual=u32(data,va)
        if actual!=expected:failures.append(f"0x{va:08X}: 0x{actual:08X} != 0x{expected:08X}")
    starts=pdata_starts(data)
    functions=[]
    for begin,(end,role) in EXPECTED_FUNCTIONS.items():
        actual_end=function_end(starts,begin)
        if actual_end!=end:failures.append(f"0x{begin:08X} end 0x{actual_end:08X} != 0x{end:08X}")
        functions.append({"begin":begin,"end_exclusive":end,"bytes":end-begin,"sha256":hash_range(data,begin,end),"role":role})
    expected_calls={
      0x821BA1F8:[0x821BAABC,0x821C4D30,0x821C5458],
      0x821BAA78:[0x821B9120,0x822E4470],
      0x821C4A60:[],0x821C5190:[],
    }
    actual_calls={}
    for target,expected in expected_calls.items():
        actual=callsites(data,target);actual_calls[f"0x{target:08X}"]=[f"0x{x:08X}" for x in actual]
        if actual!=expected:failures.append(f"calls 0x{target:08X}: {[hex(x) for x in actual]} != {[hex(x) for x in expected]}")
    summary={
      "schema":"ac6-demo-pal-d3d-callback-bridge/v1",
      "image_sha256":digest,
      "status":"FAIL" if failures else "PASS",
      "failures":failures,
      "transport":{
        "scratch_callback_register":4,
        "scratch_parameter_register":5,
        "scratch_type0_header":"0x0001057C",
        "interrupt_opcode":"0x54",
        "interrupt_header":"0xC0005400",
        "interrupt_payload":"CPU mask, bits 0..5",
        "guest_dispatcher":"0x821B9710",
        "source_one_semantics":"load [device+0x2A94]+0x10 callback and +0x14 parameter, call callback(parameter), clear current CPU bit"
      },
      "producers":[{**r,"callsite":f"0x{r['callsite']:08X}","encoder":f"0x{r['encoder']:08X}","callback":f"0x{r['callback']:08X}"} for r in producer_rows()],
      "callsites":actual_calls,
      "functions":[{**r,"begin":f"0x{r['begin']:08X}","end_exclusive":f"0x{r['end_exclusive']:08X}"} for r in functions],
      "native_model_gap":{
        "current_behavior":"the reached PM4_INTERRUPT mask 0x4 queues CPU index 2 and dispatches source=1 in the same lifecycle tick after the Xenos batch",
        "hardware_contract":"dispatch source=1 once for every set CPU bit while processing the PM4 packet",
        "remaining_boundary":"masks other than the reached 0x4 remain fail-closed until executed"
      }
    }
    return summary,functions

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("flat_image",type=Path)
    ap.add_argument("--json",type=Path)
    ap.add_argument("--csv",type=Path)
    args=ap.parse_args();data=args.flat_image.read_bytes();summary,functions=verify(data)
    if args.json:
        args.json.parent.mkdir(parents=True,exist_ok=True);args.json.write_text(json.dumps(summary,indent=2)+"\n")
    if args.csv:
        args.csv.parent.mkdir(parents=True,exist_ok=True)
        with args.csv.open("w",newline="") as f:
            w=csv.DictWriter(f,fieldnames=["begin","end_exclusive","bytes","sha256","role"]);w.writeheader()
            for r in functions:w.writerow({**r,"begin":f"0x{r['begin']:08X}","end_exclusive":f"0x{r['end_exclusive']:08X}"})
    print(json.dumps({"status":summary["status"],"anchors":len(EXPECTED_ANCHORS),"functions":len(functions),"producers":len(EXPECTED_PRODUCERS),"image_sha256":summary["image_sha256"]}))
    return 0 if summary["status"]=="PASS" else 1
if __name__=="__main__":raise SystemExit(main())
