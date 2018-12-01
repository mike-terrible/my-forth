#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef union {
  int i; int ref; unsigned u;
  double f;
} Number;

typedef struct {
  Number n;
  char xtag;
} Atom;

typedef struct {
   const int code; void (*cmd)(void);
} instruction;

static char cmdbuf[128];

static Atom pp[128];
static int ip=0;
static Atom a1,a2;

static int xforget=0;
static int xmode=0;
static char fname[32];

static int IsHex=0,IsDecimal=1;

enum Tags {
  EOJ = 0, INT = 1, WORD = 2, STD = 3, VARNAME = 4, STRING = 5
};

typedef struct {
  const int opcode; const char* xname;
} std_op;

typedef struct {
   const int opcode; const char* xname; void (*cmd)(void);
} imm_op;

typedef struct {
  int xtag,ok,fail,br;
  union { int opcode; int ref; int n; } w;
} Icode;

typedef struct {
  char xname[16];
  int xtag; int ncells;
  union { int ref; int i; double d;  } w;
} Xvar;

typedef struct { int ref; char xname[16]; } Xname;

static int StringLit=0;
static char space[8192];
static int sp=0;
static char bytes[128];
static int ib=0;
static Icode xec[1024];
static Xvar vars[64];
static Xname lib[16];
static int ret[16];
static int nret=0;

static int Ok[256];
static int nOk=0;

static int Loop[16];
static int nLoop=0;

static int NeedVar=0;
static int start=0,curvar=-1;
static int inlib=0,pc=0,ic=0,nvars=0;
static int AbortIP=0;

static int xcells=0;

static int NeedInc=0;
static char IncFn[128];


void myForth(void);

/**********  api functions */

void apiPushInt(int d) { pp[ip].n.i=d; pp[ip].xtag='i'; ip++;  }

void apiPushFloat(float d) { pp[ip].n.f=d; pp[ip].xtag='f'; ip++; }

void apiPushAtom(Atom *a) { 
  pp[ip]=a[0]; ip++; 
}

int apiPopAtom(Atom *a) { ip--; if(ip<0) { ip=0; return -1; };
  a[0]=pp[ip]; return 0;
}

int apiGetAtomAt(int p,Atom *a) {
  if(p>=ip) return -1; if(p<0) return -1; 
  a[0]=pp[p];
  return 0;
}

int apiSetAtomAt(int p,Atom *a) { 
  if(p>=ip) return -1; if(p<0) return -1;
  pp[p]=a[0];
  return 0;
}

char* apiString(Atom *a) { if(a[0].xtag!='s') return NULL;
  return space+a[0].n.ref;
}
int apiGetStackPtr(void) { return ip; }
int apiSetStackPtr(int z) { ip=z; if(ip<0) ip=0; return ip; }
void apiForth(char* cmd) { strcpy(cmdbuf,cmd); myForth(); }

/**************************************************************/

extern char* xgets(char* t);

void resetForthEnv(void) {
  StringLit=0; sp=0; ib=0; nret=0; nOk=0; nLoop=0; NeedVar=0;
  start=0; curvar=-1; inlib=0; pc=0; ic=0; nvars=0; AbortIP=0; xcells=0;
}

enum Opcodes {
  X_DOT = 1,X_PLUS = 2,X_MINUS = 3,X_MUL = 4,X_DIV = 5,X_EMIT = 6,X_CR = 7,
  X_SETVAR = 8,X_PUSHVAR = 9,X_TYPE = 10,X_BL = 11,X_EQ = 12,X_GT = 13,X_NE = 14,
  X_LT = 15, X_IF = 17,X_ELSE = 18,X_THEN = 19,X_BEGIN = 20,X_UNTIL = 21,X_AGAIN = 22,
  X_WHILE = 23,X_REPEAT = 24,X_LEAVE = 25,X_FOR = 26,X_NEXT = 27,X_SWAP = 28, X_DROP = 29,
  X_DOT_S = 30,X_DUP = 31,X_DO = 32,X_LOOP = 33,X_HEX = 34,X_DECIMAL = 35,X_ASK = 36,
  X_ALLOT = 37,X_CELLS = 38,X_FORGET = 39,X_PEEK = 40,X_POKE = 41,X_SVC = 42,X_INIT = 43,
  X_INCLUDE = 44
};

static const char Cdot[]=".",Cplus[]="+",Cminus[]="-",Cmul[]="*",
  Cdiv[]="/",Cemit[]="emit",Ccr[]="cr",Csetvar[]="!",Cask[]="?",
  Cpushvar[]="@",Ctype[]="type",Cbl[]="bl",Ceq[]="=",Cgt[]=">",Cne[]="<>",
  Clt[]="<",Cif[]="if",Celse[]="else",Cthen[]="then",Cbegin[]="begin",Cuntil[]="until",
  Cagain[]="again",Cwhile[]="while",Crepeat[]="repeat",Cleave[]="leave",
  Cfor[]="for",Cnext[]="next",Cswap[]="swap",Cdrop[]="drop",Cdot_s[]=".S",Cdup[]="dup",
  Cdo[]="do",Cloop[]="loop",Chex[]="hex",Cdecimal[]="decimal",Callot[]="allot",
  Ccells[]="cells",Cforget[]="forget",Cpeek[]="peek",Cpoke[]="poke",Csvc[]="svc",Cinit[]="init",
  Cinclude[]="include",
  CEOJ[]="EOJ",CINT[]="INT",CWORD[]="WORD",CSTD[]="STD",CVARNAME[]="VARNAME",CSTRING[]="STRING";

static const std_op StdOp[]={
  { X_DOT,Cdot },{ X_PLUS,Cplus },{ X_MINUS,Cminus},{ X_MUL,Cmul},{X_DIV,Cdiv},
  { X_EMIT,Cemit },{ X_CR,Ccr},
  { X_SETVAR,Csetvar},{ X_PUSHVAR,Cpushvar },{ X_TYPE,Ctype},{X_BL,Cbl},{X_EQ,Ceq},{X_GT,Cgt},{X_NE,Cne},
  { X_LT,Clt},{ X_IF,Cif},{X_ELSE,Celse},{ X_THEN,Cthen},{ X_BEGIN,Cbegin},{ X_UNTIL,Cuntil},
  { X_AGAIN,Cagain },{ X_WHILE,Cwhile },{ X_REPEAT,Crepeat},{ X_LEAVE,Cleave },
  { X_FOR,Cfor },{ X_NEXT,Cnext },{ X_SWAP,Cswap }, { X_DROP,Cdrop },{ X_DOT_S,Cdot_s },{ X_DUP,Cdup },
  { X_DO,Cdo },{ X_LOOP,Cloop },{ X_HEX,Chex },{ X_DECIMAL,Cdecimal },{ X_ASK,Cask },
  { X_ALLOT,Callot },{ X_CELLS,Ccells },{ X_FORGET,Cforget },{ X_PEEK,Cpeek},{ X_POKE,Cpoke },
  { X_SVC,Csvc },{ X_INIT,Cinit },{ X_INCLUDE,Cinclude },
  { -1, NULL }
};

static const std_op TagNames[]={
  { EOJ,CEOJ }, { INT,CINT }, { WORD,CWORD }, { STD,CSTD },
  { VARNAME,CVARNAME },{ STRING,CSTRING },
  { -1, NULL }
};

void putOp(int c) {
  int i; i=0;
  while(StdOp[i].opcode!=-1) {
    if(StdOp[i].opcode==c) { printf("  %s ",StdOp[i].xname); return; };
    i++;
  };
  printf(" opcode=%d ",c);
}

void putTag(int c) {
  int i; i=0;
  while(TagNames[i].opcode!=-1) { if(TagNames[i].opcode==c) { printf(" tag=%s ",TagNames[i].xname); return; };
    i++;
  };
  printf(" tag=%d ",c);
}

void checkStart(int z) {
  int i;
  i=0;
  while(i<inlib) {
    if(lib[i].ref==z) { printf(" %s ::",lib[i].xname); return; };
    i++;
  };
}

void list(void) {
  int i,t,c,st;
  i=0;
  while(i<pc) {
    printf("%08d ",i); t=xec[i].xtag; c=xec[i].w.opcode;
    checkStart(i);
    putTag(t);
    if(t==INT) {
      printf(" value= %d",xec[i].w.n);
    } else if(t==VARNAME) {
      printf(" nameof(%s)",vars[xec[i].w.ref].xname);
    } else if(t==WORD) {
      st=xec[i].br;
      printf(" word at %d ",st); checkStart(st);
    } else if(t==STD) {
      printf(" "); putOp(c);
      if(xec[i].ok!=-1) printf(" ok=%d",xec[i].ok);
      if(xec[i].fail!=-1) printf(" fail=%d",xec[i].fail);
      if(xec[i].br!=-1) printf(" br=%d",xec[i].br);
    };
    printf("\r\n");
    i++;
  };
  printf("\r\n list completed\r\n");
}


void initString(void) { ib=0; }
void addString(char* t) { int k=0; while(t[k]!=0) { bytes[ib]=t[k]; ib++; k++; bytes[ib]=0; }; }
void allocString(void) { 
  strcpy(space+sp,bytes); space[sp+ib]=0; 
  if(xmode==2) {
    xec[pc].xtag=STRING;
    xec[pc].w.ref=sp;
    pc++;
    sp=sp+ib+1;
  } else {
   pp[ip].xtag='s'; pp[ip].n.ref=sp; ip++;
  };
}

void prtAtom(Atom *p) {
  if(p->xtag=='i') { printf("%d ",p->n.i); }
  else if(p->xtag=='s') { printf("%s ",&space[p->n.ref]); };
}

void pop1(void) { a1.n.i=0; if(ip<1) return;
  ip--; a1=pp[ip];
}

void pop2(void) { a1.n.i=0; a2.n.i=0; if(ip<2) return;
  ip--; a2=pp[ip]; ip--; a1=pp[ip];
}

unsigned myGetValueAt(unsigned addr);
void mySetValueAt(unsigned addr,unsigned v);


int mySvcCount();
int mySvc(int);

void gINCLUDE(void) { NeedInc=1; }

void gINIT(void) { resetForthEnv(); }

void gSVC(void) {
  pop1(); if(a1.n.i==0) { pp[ip].xtag='i'; pp[ip].n.i=mySvcCount(); ip++; return; };
  mySvc(a1.n.i);
}

void gPEEK(void) {
  pop1(); pp[ip].n.u=myGetValueAt(a1.n.u); ip++;
}

void gPOKE(void) {
  pop2(); mySetValueAt(a1.n.u,a2.n.u);
}

void gFORGET(void) { xforget=1; }

void gHEX(void) { IsHex=1; IsDecimal=0; }
void gDECIMAL(void) { IsHex=0; IsDecimal=1; }


void gALLOT(void) {
  int k,i;
  if(nvars==0) { AbortIP=1; return; };
  k=nvars; k--; k++; i=1;
  while(i<xcells) { vars[k].xname[0]=0; vars[k].ncells=i; k++; i++; };
  nvars=k; xcells=0; 
}

void gCELLS(void) { if(ip==0) { AbortIP=1; return; };
  ip--; a1=pp[ip]; xcells=a1.n.i;
}

void gDOT_S(void) {
  int i;
  i=0;
  while(i<ip) { prtAtom(pp+i); i++; };
}

void gDUP(void) {
  if(ip>0) { pp[ip]=pp[ip-1]; ip++; };
}

void gDROP(void) { pop1(); }

void gSWAP(void) { pop2();
  pp[ip]=a2; ip++; pp[ip]=a1; ip++;
}

void gDOT(void) { 
  char* fmt;
  fmt="%d"; if(IsHex) fmt="%X";
  pop1(); printf(fmt,a1.n.i);
}

void gAGAIN(void) { ic=xec[ic].br-1; }
void gREPEAT(void) { gAGAIN(); }
void gLOOP(void) { gAGAIN(); }

void gLEAVE(void) { int r; r=xec[ic].br; ic=xec[r].br-1; }

void gBEGIN(void) { }

void gIF(void) {
  if(ip==0) { AbortIP=1; return; };
  pop1();
  if(a1.xtag!='i') { AbortIP=1; return; };
  if(a1.n.i==1) {
    ic=xec[ic].ok-1; 
  } else {
    ic=xec[ic].fail-1; 
  };
}

void gDO(void) {
  int r,rf;
  if(ip<2) { AbortIP=1; return; };
  r=ip-1;
  a1=pp[r];
  if(a1.xtag!='i') { AbortIP=1; return; }
  rf=ip-2;
  a2=pp[rf];
  if(a2.xtag!='i') { AbortIP=1; return; };
  if(a1.n.i<a2.n.i) { a1.n.i++; pp[r]=a1; return; };
  ic=xec[ic].br-1;
}

void gFOR(void) {
  int r;
  if(ip==0) { AbortIP=1; return; };
  r=ip-1;
  a1=pp[r];
  if(a1.xtag!='i') { AbortIP=1; return; }
  if(a1.n.i>0) { a1.n.i--; pp[r]=a1;  return; };
  ic=xec[ic].br-1;
}

void gNEXT(void) { ic=xec[ic].br-1; }

void gWHILE(void) {
  int r;
  if(ip==0) { AbortIP=1; return; };
  pop1();
  if(a1.xtag!='i') { AbortIP=1; return; };
  if(a1.n.i==1) {
    ic=xec[ic].ok-1;
  } else {
    r=xec[ic].fail;  ic=xec[r].br-1;
  };
}

void gUNTIL(void) { gIF(); }

void gTHEN(void) { }

void gELSE(void) {  ic=xec[ic].br;  ic--; }

void exitCmp(void) { a1.xtag='i'; pp[ip]=a1; ip++; }

void gLT(void) { pop2();
  if(a1.xtag==a2.xtag) {
    if(a1.xtag=='i') if(a1.n.i<a2.n.i) { a1.n.i=1; goto RET; }
  };
  a1.n.i=0; RET: exitCmp();
}

void gGT(void) { pop2();
  if(a1.xtag==a2.xtag) {
    if(a1.xtag=='i') if(a1.n.i>a2.n.i) { a1.n.i=1; goto RET; }
  };
  a1.n.i=0; RET: exitCmp();
}

void gNE(void) { pop2();
  if(a1.xtag==a2.xtag) {
    if(a1.xtag=='i') if(a1.n.i!=a2.n.i) { a1.n.i=1; goto RET; }
  };
  a1.n.i=0; RET: exitCmp();
}

void gEQ(void) { pop2();
  if(a1.xtag==a2.xtag) {
    if(a1.xtag=='i') if(a1.n.i==a2.n.i) { a1.n.i=1; goto RET; }
  };
  a1.n.i=0; RET: exitCmp();
}
void gPLUS(void) {
  if(xcells>0) return;
  pop2(); pp[ip].xtag=a1.xtag; pp[ip].n.i=a1.n.i+a2.n.i; ip++;
}
void gMINUS(void) { pop2(); pp[ip].n.i=a1.n.i-a2.n.i; ip++; }
void gMUL(void) { pop2();  pp[ip].xtag=a1.xtag; pp[ip].n.i=a1.n.i*a2.n.i; ip++; }
void gDIV(void) { pop2(); pp[ip].xtag=a1.xtag; pp[ip].n.i=a1.n.i/a2.n.i; ip++; }
void gEMIT(void) { pop1(); putchar(a1.n.i); }
void gCR(void) { printf("\r\n"); } void gBL(void) { printf(" "); }
void gASK(void) {
  char* fmt;
  fmt="%d";
  if(xcells>0) { curvar=curvar+xcells; xcells=0; };
  if(vars[curvar].xtag=='i') {
    if(IsHex) fmt="%X";
    printf(fmt,vars[curvar].w.i);
    return;
  };
  if(vars[curvar].xtag=='s') {
    char *s;
    s=space+vars[curvar].w.ref;
    printf("%s",s);
  };

}

void gSETVAR(void) {
  ip--;
  if(xcells>0) { curvar=curvar+xcells; xcells=0; };
  vars[curvar].w.i=pp[ip].n.i;
  vars[curvar].xtag=pp[ip].xtag;
}

void gPUSHVAR(void) {
  if(xcells>0) { curvar=curvar+xcells; xcells=0; };
  pp[ip].xtag=vars[curvar].xtag; pp[ip].n.i=vars[curvar].w.i;
  ip++;
}

void gTYPE(void) { pop1(); printf("%s",space+a1.n.ref); }

void defVar(char* t) {
  int i,rc,f;
  i=0; f=-1;
  while(i<nvars) {
    rc=strcmp(t,vars[i].xname); if(rc==0) { f=i; break; };
    i++;
  };
  if(f==-1) {
    strcpy(vars[nvars].xname,t); f=nvars; 
    vars[nvars].ncells=0;
    nvars++;
    /* printf("\r\n variable %s defined at %04u\r\n",t,f); */
  };
}

static const instruction InstructionSet[] = {
  { X_LT, gLT }, { X_GT, gGT }, { X_NE, gNE }, { X_EQ, gEQ }, { X_TYPE, gTYPE }, { X_PUSHVAR, gPUSHVAR },
  { X_SETVAR, gSETVAR }, { X_DOT, gDOT }, { X_PLUS, gPLUS }, { X_MINUS, gMINUS }, { X_MUL, gMUL },
  { X_DIV, gDIV }, { X_EMIT, gEMIT }, { X_CR, gCR }, { X_BL, gBL }, { X_ASK, gASK },
  { X_IF, gIF },{ X_ELSE, gELSE },{ X_THEN, gTHEN },{ X_BEGIN, gBEGIN },{ X_UNTIL, gUNTIL },
  { X_AGAIN, gAGAIN },{ X_LEAVE, gLEAVE },{ X_WHILE, gWHILE },{ X_REPEAT, gREPEAT },
  { X_FOR, gFOR }, { X_NEXT, gNEXT },{ X_SWAP, gSWAP }, { X_DROP, gDROP }, { X_DOT_S, gDOT_S },
  { X_DUP, gDUP }, { X_DO, gDO },{ X_LOOP, gLOOP },{ X_HEX , gHEX },{ X_DECIMAL, gDECIMAL },
  { X_ALLOT,gALLOT }, { X_CELLS,gCELLS },{ X_PEEK, gPEEK },{ X_POKE, gPOKE },{ X_SVC, gSVC },
  { X_INIT,gINIT },{ X_INCLUDE, gINCLUDE },{ -1, NULL  }
};

int goInstruction(int command) {
  int i;
  i=0;
  while(InstructionSet[i].code!=-1) { 
    if(InstructionSet[i].code==command) { InstructionSet[i].cmd(); return 0; };
    i++;
  };
  return -1;
}

void goWord(char* t) {
  int i,rc,v;
  if(t[0]==8) return;
  if(xmode==2) {
    i=0; v=-1;
    while(i<nvars) {
      rc=strcmp(t,vars[i].xname);
      if(rc==0) { v=i; break; };
      i++;
    };
    if(v!=-1) { xec[pc].xtag=VARNAME; xec[pc].w.ref=v; pc++; return; };
    i=0; v=-1;
    while(i<inlib) {
      rc=strcmp(t,lib[i].xname);
      if(rc==0) { v=lib[i].ref; break; };
      i++;
    };
    if(v!=-1) { xec[pc].xtag=WORD; xec[pc].br=v; pc++; return; };
    printf("\r\n[!]illegal word: %s >\r\n",t);
    return;
  };
  i=0; ic=-1; AbortIP=0;
  while(i<inlib) {
    rc=strcmp(t,lib[i].xname);
    if(rc==0) { ic=lib[i].ref; break; };
    i++;
  };
  if(ic==-1) {
    i=0; v=-1;
    while(i<nvars) {
      rc=strcmp(t,vars[i].xname);
      if(rc==0) { v=i; break; };
      i++;
    };
    if(v!=-1) {
      if(xmode==0) { curvar=v; return; };
      if(xmode==2) { xec[pc].xtag=VARNAME; xec[pc].w.ref=v; pc++; return; };
    };
    printf("\r\nword %s not found!\r\n",t); return; 
  };
  ret[nret]=-1; nret++; 
  for(;;) {
    if(xec[ic].xtag==STRING) {
      pp[ip].xtag='s'; pp[ip].n.ref=xec[ic].w.ref;
      ip++; ic++; continue;
    };
    if(xec[ic].xtag==EOJ) {
      nret--;
      if(nret>=0) { ic=ret[nret]; 
        if(ic==-1) return;
        continue; 
      };
      /* printf("\r\n execution completed at %04u\r\n",ic); */
      return;
    };
    if(xec[ic].xtag==VARNAME) {
      curvar=xec[ic].w.ref;
      ic++; continue;
    };
    if(xec[ic].xtag==WORD) {
      ret[nret]=ic+1; nret++;
      ic=xec[ic].br; continue;
    };
    if(xec[ic].xtag==INT) {
      pp[ip].xtag='i';
      pp[ip].n.i=xec[ic].w.n; ip++;
      ic++; continue;
    };
    if(xec[ic].xtag==STD) {
      rc=goInstruction(xec[ic].w.opcode); 
      if(!rc) {
        if(AbortIP) { printf("\r\n Execution terminated due stack is empty at %04u\r\n",ic);  
          return;
        };
        ic++; continue;
      };
      printf("\r\n illegal opcode at %04u %d\r\n",ic,xec[ic].w.opcode);
      break;
    };
    printf("\r\n illegal opcode! Execution terminated!\r\n"); break;
  };
}

void addNumber(int a) { xec[pc].xtag=INT; xec[pc].w.n=a; pc++; }

void addOp(int opcode) { 
  int r,x,q;
  xec[pc].xtag=STD; xec[pc].w.opcode=opcode; 
  xec[pc].ok=-1; xec[pc].fail=-1; xec[pc].br=-1;
  r=pc; pc++;
  switch(opcode) {
  case X_IF:   Ok[nOk]=r; xec[r].ok=pc; xec[r].fail=-1; xec[pc].br=-1; Ok[nOk]=r; nOk++; return;
  case X_ELSE: q=Ok[nOk-1]; xec[q].fail=pc; return;
  case X_THEN: nOk--; q=Ok[nOk];
    if(xec[q].fail==-1) xec[q].fail=pc;
    if(xec[q].br==-1) xec[q].br=pc;
    x=xec[q].fail; xec[x-1].br=pc;
    return;
  case X_FOR: Loop[nLoop]=r; nLoop++; return;
  case X_DO: Loop[nLoop]=r; nLoop++; return;
  case X_LOOP: nLoop--; q=Loop[nLoop]; xec[q].br=pc; xec[r].br=q; return;
  case X_NEXT: nLoop--; q=Loop[nLoop]; xec[q].br=pc; xec[r].br=q; return;
  case X_BEGIN:  Loop[nLoop]=r; nLoop++; return; 
  case X_AGAIN:  nLoop--; q=Loop[nLoop]; xec[r].br=q; xec[q].br=pc; return; 
  case X_UNTIL:  nLoop--; q=Loop[nLoop]; xec[r].fail=q; xec[r].ok=pc; xec[q].br=pc; return; 
  case X_REPEAT: nLoop--; q=Loop[nLoop]; xec[q].br=pc; xec[r].br=q; return; 
  case X_WHILE:  q=Loop[nLoop-1]; xec[r].fail=q; xec[r].ok=pc; return; 
  default: break;
  };
}

void compileFn() { xmode=0; 
  xec[pc].xtag=EOJ; pc++;
  int i,rc;
  i=0;
  while(i<inlib) {
    rc=strcmp(fname,lib[i].xname); 
    if(rc==0) { printf("\r\n word %s is just already exists!\r\n",fname); return; };
    i++;
  };
  strcpy(lib[inlib].xname,fname); lib[inlib].ref=start; inlib++;
  printf("\r\n %s saved %d words\r\n",fname,pc-start);
}


void listWords() {
  int i;
  if(inlib==0) { printf("\r\n no user words in dictionary!\r\n"); return; }
  i=0;
  printf("\r\n");
  while(i<inlib) printf(" %s at %04u\r\n",lib[i].xname,lib[i].ref),i++;
  printf("\r\n");
}

/* void myNumber(char *t,Number *n) { } */

static char mydigits[]={ '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F',0 };

int digitPos(char q) {
  int i; char c;
  i=0; c=mydigits[i];
  while(c!=0) { if(c==q) return i;
    i++; c=mydigits[i];
  };
  return -1;
}

int myNum(char *t) {
  int k; int d; int r,p,xmul; char q;
  xmul=10; if(IsHex) xmul=16;
  d=1; r=0; k=0;
  while(t[k]!=0) k++;
  while(k>0) {
    k--; q=t[k]; if(q>='a') if(q<='f') q=(q-'a')+'A';
    p=digitPos(q); if(p==-1) continue;
    r+=p*d; d*=xmul;
  };
  return r;
}

void pushNum(int a) {
  if(xmode==2) { addNumber(a); return; };
  pp[ip].n.i=a; pp[ip].xtag='i'; ip++;
}

void newDef(void) { xmode=1; start=pc; }
void addFn(char* t) { xmode=2; strcpy(fname,t); start=pc;
  printf("\r\n creating word %s\r\n",fname);
}

static const std_op CompiledSt[] = {
  { X_LEAVE,Cleave},{ X_REPEAT,Crepeat },{ X_AGAIN,Cagain},{ X_WHILE,Cwhile },
  { X_UNTIL,Cuntil},{ X_BEGIN,Cbegin },{ X_IF,Cif },{ X_ELSE,Celse },{ X_THEN,Cthen },
  { X_FOR,Cfor },{ X_NEXT,Cnext },{ X_DO,Cdo },{X_LOOP,Cloop },
  { -1, NULL }
};

static const imm_op ImmSt[] = {
  { X_LT,Clt, gLT }, { X_GT, Cgt , gGT }, { X_NE, Cne , gNE }, { X_EQ, Ceq , gEQ },
  { X_PLUS,Cplus, gPLUS }, { X_MINUS, Cminus , gMINUS }, { X_MUL, Cmul , gMUL },{ X_DIV, Cdiv , gDIV },
  { X_EMIT,Cemit, gEMIT }, { X_CR,Ccr, gCR }, { X_BL,Cbl, gBL },
  { X_SETVAR,Csetvar, gSETVAR },{ X_PUSHVAR,Cpushvar, gPUSHVAR },{ X_TYPE,Ctype, gTYPE }, { X_DOT,Cdot,gDOT },
  { X_SWAP,Cswap, gSWAP },{ X_DROP,Cdrop, gDROP },{ X_DOT_S,Cdot_s, gDOT_S },{ X_DUP,Cdup,gDUP },
  { X_HEX,Chex, gHEX }, { X_DECIMAL, Cdecimal, gDECIMAL },{ X_ASK, Cask, gASK },
  { X_ALLOT,Callot,gALLOT }, { X_CELLS,Ccells,gCELLS },{ X_FORGET,Cforget,gFORGET },
  { X_PEEK,Cpeek,gPEEK },{ X_POKE,Cpoke,gPOKE }, { X_SVC,Csvc,gSVC },{ X_INIT,Cinit,gINIT },
  { X_INCLUDE,Cinclude,gINCLUDE },
  { -1, NULL, NULL }
};

int scanCmdImmediate(char *t) {
  int rc; const imm_op *a;
  a=&ImmSt[0];
  while(a->opcode!=-1) { rc=strcmp(t,a->xname);
    if(rc==0) {
      if(xmode!=2) { a->cmd(); }
      else addOp(a->opcode);
      return 0;
    };
    a++;
  };
  return 1;
}

int scanCmdCompiled(char *t) {
  int rc; const std_op *a;
  a=&CompiledSt[0];
  while(a->opcode!=-1) { rc=strcmp(t,a->xname);
    if(rc==0) { if(xmode!=2) return 2;
      addOp(a->opcode); return 0;
    };
    a++;
  };
  return 1;
}

void removeWord(char* t) {
  int i,rc;
  i=inlib;
  while(i>0) { i--;
    rc=strcmp(lib[i].xname,t);
    if(rc==0) { pc=lib[i].ref; inlib=i; return; };
  };
}

void goInclude(char* fn,char* cmd);

void getWord(char* t) {
  int rc,k;
  if(NeedInc) {
    NeedInc=0; strcpy(IncFn,t); goInclude(IncFn,cmdbuf);
    return;
  };
  if(StringLit) {
    k=strlen(t); k--;
    if(k<=0) return;
    if(t[k]=='\"') { t[k]=0; addString(t); allocString(); StringLit=0; return; };
    addString(t); return;
  };
  if(xmode==1) { addFn(t); return; };
  rc=strcmp(t,"s\"");
  if(rc==0) { StringLit=1; initString(); return; };
  /************************************************************************/
  rc=scanCmdCompiled(t); if(rc==0) return; 
  if(rc==2) {
    printf("\r\nword [%s] not allowed in immediate mode!\r\n",t);
    return;
  };
  /************************************************************************/
  rc=scanCmdImmediate(t); if(rc==0) return;
  /***********************************************************************/
  rc=strcmp(t,"words"); if(rc==0) { listWords(); return; };
  rc=strcmp(t,"list"); if(rc==0) { list(); return; };
  if(xmode==2) {
    rc=strcmp(t,";"); if(rc==0) { compileFn(); return; };
  };
  rc=strcmp(t,":"); if(rc==0) { newDef(); return; };
  if(t[0]<=' ') return;
  if(xforget==1) { removeWord(t); xforget=0; return; };
  goWord(t); 
}

void myForth(void) { char* t; int z,rc;
  t=strtok(cmdbuf," ");
  while(t!=NULL) { if(t[0]==0) { t=strtok(NULL," "); continue; };
    if(NeedVar) { NeedVar=0;
      defVar(t); t=strtok(NULL," "); continue;
    };
    if(t[0]>='0') if(t[0]<='9') {
      z=myNum(t);
      pushNum(z);
      t=strtok(NULL," "); continue;
    };
    rc=strcmp(t,"variable");
    if(rc==0) { NeedVar=1; t=strtok(NULL," "); continue; };
    getWord(t);
    t=strtok(NULL," ");
  };
}

int zmain(void)
{  printf("MyForth r 1.21 type \'bye\' to exit\n");
   for(;;) {
     xgets(cmdbuf); strtok(cmdbuf,"\n"); strtok(cmdbuf,"\r");
     if(strcmp(cmdbuf,"bye")==0) break;
     if(cmdbuf[0]==0) continue;
     myForth();
   };
   printf("\n MyForth exited\n");
   return 0;
}


