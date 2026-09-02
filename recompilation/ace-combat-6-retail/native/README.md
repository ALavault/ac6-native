# AC6 native lane

This directory is product-side Gate 1/early Gate 4 code. It
contains hardware-numbered Xenos PM4 command handling, a conservative Vulkan
boundary, PPC guest ABI primitives and offline service contracts. It does not contain an XEX,
ISO, PAC, generated C++ or an oracle SDK.

The host-side XEX decoder uses OpenSSL `libcrypto` for AES-128-CBC; no
XenonRecomp/XenosRecomp or ReXGlue library is linked into the native product.

`NativeGuestInputService` (`include/ac6/native_guest_input.h`) wraps SDL2's
`SDL_GameController` API to serve `XamInputGetState`/`SetState`/
`GetCapabilities` real controller state (r180). SDL2 is a real, linked
runtime dependency of this product for that reason alone.

The frontend contract accepts one existing ISO or `assets/` directory and maps
mutable state to XDG data storage; it does not copy or reinterpret retail data.

`ac6recomp <ISO|assets/>` is the installed native entry point. It currently
validates media, creates the XDG mutable-data root, and exercises a clean
runtime boot/shutdown lifecycle; it does not claim that the retail guest has
booted or reached Mission 01. `--self-test` is side-effect free and used by
static validation.

For an assets directory or ISO, `NativeRuntime::boot()` validates the XEX2
header, Xenon image base/entry point, TLS bounds, stack metadata, decrypts the
qualified AES-CBC payload, expands basic compression, and maps the resulting
image at its guest load address. Normal/LZX compression remains fail-closed;
the image stays in process memory and is never copied to the product tree.

`GuestAddressSpace` reserves the 4 GiB virtual Xenon address range with
`MAP_NORESERVE` and exposes checked views. It is tested at guest address
`0x82000000` but is not yet wired to generated guest execution.

`ac6_native_xenos_tests`, `ac6_native_services_tests` and
`ac6_native_ppc_abi_tests` are deterministic contract tests. Guest codegen is
linked only from an ignored, qualified build output; generated C++ and retail
media never enter installation. Guest SDK bindings, natural boot/gameplay and
campaign/save evidence remain Gate 2 work.
