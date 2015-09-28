
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>

int main(int argc, char *argv[])
{
	void *b;
	void (*f)(void);
        char *exec_args[] = { "arg1" }, *exec_envp[] = {""};

	printf("Doing legitimate mmap() now.\n");
	b = mmap(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	printf("Done\n");

	printf("Doing a legitimate mprotect() now.\n");
	mprotect(NULL, 4096, PROT_READ | PROT_WRITE | PROT_EXEC);
	printf("Done\n");

	printf("Doing a legitimate sigaction() now.\n");
	sigaction(-1, NULL, NULL);
	printf("Done\n");

	if(argc != 2) {
		fprintf(stderr, "Usage: %s <hex call address>\n", argv[0]);
		exit(1);
	}

	if(sscanf(argv[1], "%p", &f) != 1) {
		printf("parse of first arg failed.\n");
	}
	printf("parsed arg %s as %p - calling it now (non-legit)\n", argv[1], f);
	f();
	printf("call returned\n");
}

