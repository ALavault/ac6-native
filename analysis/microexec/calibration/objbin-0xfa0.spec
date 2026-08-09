# Calibration of MicroExecuteFunction.java against MicroExecuteScenarioParser.java.
#
# This reproduces, through the parameterised harness, the exact case the
# specialised one produced as analysis/microexec/objbin-read.ppc.json. If the
# two disagree on anything the comparator compares - exit, registers, calls,
# memory_writes - the general harness is wrong before it has been used for
# anything, which is the point of measuring the instrument first.
#
# Relative paths resolve against the working directory; run from the repo root.

function 0x82330158
case ObjBin@node+0xfa0

region payload 0xB0000000 file:reports/logs/cycle-739-pac-mission-gate/fhm/idx_0009/000_00_00_00_10.bin
region record  0xB4000000 poison:0x100
region buffer  0xB5000000 poison:0x8000
region stack   0xC0000000 zero:0x1000

sp  stack+0xE00
gpr r3 record
gpr r4 payload+0xfa0
gpr r5 buffer

stub 0x823828B8 error printer
