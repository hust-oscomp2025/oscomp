/*
 * Below is the given application for lab1_challenge1_backtrace.
 * This app prints all functions before calling print_backtrace().
 */

#include "user_lib.h"
#include "util/types.h"

void f8()
{
  //print_backtrace(4);
  print_backtrace(11);
  //printu("Printing program status in f8()\n");
  //printRegs();
}
void f7()
{
  //printu("Printing program status in f7()\n");
  //printRegs();
  f8();
}
void f6()
{
  //printu("Printing program status in f6()\n");
  //printRegs();

  f7();
}
void f5()
{
  //printu("Printing program status in f5()\n");
  //printRegs();
  f6();
}
void f4()
{
  //printu("Printing program status in f4()\n");
  //printRegs();
  f5();
}
void f3()
{
  //printu("Printing program status in f3()\n");
  //printRegs();
  f4();
}
void f2()
{
  //printu("Printing program status in f2()\n");
  //printRegs();
  f3();
}
void f1()
{
  //printu("Printing program status in f1()\n");
  //printRegs();
  f2();
}

int main(void)
{
  //printRegs();
  //printu("back trace the user app in the following:\n");
  //print_backtrace(7);
  f1();
  exit(0);
  return 0;
}
