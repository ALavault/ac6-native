#pragma once

// The generated symbol is weak, as are the two already-qualified loader
// overrides.  This strong wrapper preserves the generated body exactly and
// adds the one function-exit snapshot needed by the bounded read-only probe.
// It is a product-side observation seam; generated C++ remains untouched.
extern "C" void __imp__sub_82323BB8(PPCContext &, std::uint8_t *);
extern "C" void __imp__sub_820D18C8(PPCContext &, std::uint8_t *);
extern "C" void __imp__sub_82323808(PPCContext &, std::uint8_t *);

void sub_82323808(PPCContext &context, std::uint8_t *base) {
  const auto snapshot = trace_title_child_constructor_enter(context);
  __imp__sub_82323808(context, base);
  trace_title_child_constructor_exit(snapshot);
}

void sub_820D18C8(PPCContext &context, std::uint8_t *base) {
  const auto token = trace_title_movie_factory_enter(context);
  try {
    __imp__sub_820D18C8(context, base);
  } catch (...) {
    trace_title_movie_factory_exit(context, token, true);
    throw;
  }
  trace_title_movie_factory_exit(context, token, false);
}

void sub_82323BB8(PPCContext &context, std::uint8_t *base) {
  const auto owner = context.r3.u32;
  const bool trace_exit = trace_title_terminal_wrap_drain(owner);
  try {
    __imp__sub_82323BB8(context, base);
  } catch (...) {
    if (trace_exit) {
      trace_title_terminal_drain_exit(owner, true);
    }
    throw;
  }
  if (trace_exit) {
    trace_title_terminal_drain_exit(owner, false);
  }
}
