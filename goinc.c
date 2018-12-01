#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void myForth(void);

void goInclude(char* fn,char* cmd) {
  FILE *f=NULL;  
  f=fopen(fn,"r");
  if(f==NULL) return;
  while(fgets(cmd,120,f)!=NULL) { strtok(cmd,"\n"); strtok(cmd,"\r"); if(strcmp(cmd,"bye")==0) break;
    if(cmd[0]==0) continue;
    myForth();
  };
  fclose(f);
}