# Cycle 17 — AC6 task-loop slot append helper

Within the following task-loop phase, `0x821d7c80` calls `0x821d0cf8`. The
helper conditionally initializes its argument then appends five fixed pointer
slots into a global indexed array before two follow-up calls. The slot consumer,
capacity and dynamic ownership remain unknown, so this is not called a graphics
or title queue.
