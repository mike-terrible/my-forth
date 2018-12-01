/*
  low level access to MCU memory
*/


/*
static union { unsigned a; volatile unsigned* ptr; } MCUstorage;
*/


unsigned myGetValueAt(unsigned addr) {
/*  MCUstorage.a=addr;
    return MCUstorage.ptr[0]; */
  return 0;
}

void mySetValueAt(unsigned addr,unsigned v) {
  /* MCUstorage.a=addr; MCUstorage.ptr[0]=v; */
}


