/*
 * header file to be used by applications.
 */

int printu(const char *s, ...);
int exit(int code);
void printRegs();
int getRa(void);
int print_backtrace(int depth);