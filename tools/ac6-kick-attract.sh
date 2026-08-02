#!/usr/bin/env bash
# The attract loop stalls the frame loop until Start is pressed (cycle 335), so
# an unattended session freezes before a human can connect. Give it Start.
export DISPLAY=:78
sleep 45
for i in 1 2 3; do
  win=$(xdotool search --class ac6recomp 2>/dev/null | head -1)
  [ -n "$win" ] && { xdotool windowactivate "$win" 2>/dev/null; xdotool windowfocus "$win" 2>/dev/null; }
  xdotool keydown Escape; sleep 0.1; xdotool keyup Escape
  sleep 8
done
