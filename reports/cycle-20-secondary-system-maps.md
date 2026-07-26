# Cycle 20 — AC6 game-owned caller after ABI subentry

For qualified AC6 PAL XEX
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
`Function_821D0CF8` is now read after the resolved ABI call: when its argument
byte `+0x0c` is clear, it calls the `r29` callee-save subentry at `0x821d0c58`
and sets that byte. It then conditionally calls `FUN_8211d0f0`, appends five
fixed pointers into a global indexed array, and calls `Function_82138430` and
`Function_82332318`.

The helper is game-owned control flow after ABI setup, but the pointer array's
consumer, capacity, ownership and system semantics remain unknown. This is not
a title, graphics, task, or stable-hook claim. Next: observe a bounded call in
Xenia/XenonTests before assigning subsystem meaning.
