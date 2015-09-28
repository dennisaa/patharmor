#ifndef __INTERCEPT_H__
#define __INTERCEPT_H__

int intercept_syscalls_init(void);
void intercept_syscalls_exit(void);

int intercept_symbol(const char *symbol, void *jmp_dst, void *jmp_back_src);
int restore_symbol(const char *symbol);

#endif // __INTERCEPT_H__

