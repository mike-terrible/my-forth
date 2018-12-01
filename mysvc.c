/*
  mysvc.c - svc calls
*/

#include <stdio.h>
#include <stdlib.h>
#include "myapi.h"


/***** svc api functions - may be added to project *********/
void svc1(void) {
  printf("\r\n [svc1]stackptr is %d \r\n",apiGetStackPtr());
}

void svc2(void) {
  printf("\r\n [svc2] \r\n");
}

/*********************************/

typedef struct { void (*xec)(void); } do_exec;

static const do_exec X[]={
  { NULL }, { svc1 }, { svc2 }
};

int mySvcCount(void) { return 2; }

void mySvc(int c) {
  int n; 
  n=mySvcCount();
  if(c>0) if(c<=n) X[c].xec();
}
