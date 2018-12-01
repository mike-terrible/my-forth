# my-forth  
  
`gcc *.c -s -o ./my-forth`  
  
It's simplified implementation of Forth to embed it into stanalone applications. API to run my-forth from C routines is supported. Additional C functions may be registered as SVC-routines. SVC-routines may be directly called from my-forth using **svc** word. 
