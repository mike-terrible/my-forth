/*
   myapi.h
*/

typedef union {
  int i; int ref; unsigned u;
  double f;
} Number;

typedef struct {
  Number n;
  char xtag;
} Atom;

void apiPushInt(int d);
void apiPushFloat(double d);
void apiPushAtom(Atom *a);
int apiPopAtom(Atom *a);
int apiGetAtomAt(int p,Atom *a); /* get Atom to a from p-th element in stack without affecting stackptr */
int apiSetAtomAt(int p,Atom *a); /* set p-th element in stack to a without affecting stackptr */
char* apiString(Atom *a);  /* get String from Atom */
int apiGetStackPtr(void);  /* get stack pointer */
int apiSetStackPtr(int z); /* set stack pointer to */
void apiForth(char* cmd); /* run forth words */
