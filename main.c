#include <stdlib.h>
#include <stdio.h>
#include <string.h>

char* xgets(char* b) {
  return fgets(b,128,stdin);
}

int zmain();

int main(int argc,char** argv) {
  return zmain();
}
