set pagination off
set confirm off
set debuginfod enabled off

break AC6_PPC_LDARX
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC op=ldarx address=0x%08x lr=0x%08x r3=0x%016lx r10=0x%016lx tick=%llu thread=%u\\n", address, (unsigned)context->lr, context->r3.u64, context->r10.u64, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 7
  continue
end

break AC6_PPC_STDCX
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC op=stdcx address=0x%08x lr=0x%08x r3=0x%016lx value=0x%016lx tick=%llu thread=%u\\n", address, (unsigned)context->lr, context->r3.u64, value, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 7
  continue
end

break __imp__sub_821B9648
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC_ENTRY function=0x821B9648 lr=0x%08x r3=0x%08x tick=%llu thread=%u\\n", (unsigned)*(unsigned long long*)((char*)$rdi + 256), *(unsigned int*)$rdi, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 4
  continue
end

break __imp__sub_821B96B8
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC_ENTRY function=0x821B96B8 lr=0x%08x r3=0x%08x tick=%llu thread=%u\\n", (unsigned)*(unsigned long long*)((char*)$rdi + 256), *(unsigned int*)$rdi, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 4
  continue
end

break __imp__sub_822E4240
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC_ENTRY function=0x822E4240 lr=0x%08x r3=0x%08x tick=%llu thread=%u\\n", (unsigned)*(unsigned long long*)((char*)$rdi + 256), *(unsigned int*)$rdi, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 4
  continue
end

break __imp__sub_822E4268
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC_ENTRY function=0x822E4268 lr=0x%08x r3=0x%08x tick=%llu thread=%u\\n", (unsigned)*(unsigned long long*)((char*)$rdi + 256), *(unsigned int*)$rdi, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 4
  continue
end

break __imp__sub_822E4290
commands
  silent
  set $bridge = '(anonymous namespace)::active_bridge'
  printf "AC6_RR_ATOMIC_ENTRY function=0x822E4290 lr=0x%08x r3=0x%08x tick=%llu thread=%u\\n", (unsigned)*(unsigned long long*)((char*)$rdi + 256), *(unsigned int*)$rdi, *(unsigned long long*)((char*)$bridge + 328), '(anonymous namespace)::current_guest_thread_id'
  bt 4
  continue
end
