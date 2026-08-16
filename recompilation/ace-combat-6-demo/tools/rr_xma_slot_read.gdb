set pagination off
set confirm off
set debuginfod enabled off

break __imp__XMACreateContext
commands
  silent
  printf "AC6_RR_XMA_IMPORT pc=%p rdi=%p rsi=%p\n", $pc, $rdi, $rsi
  info registers rdi rsi
  x/24wx $rsi+0x17360050
  continue
end

continue
quit
