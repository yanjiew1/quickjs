/* Copyright (c) 2026 Yan-Jie Wang. MIT license; see LICENSE. */
/* MSYS2 deliberately omits IANA-shaped TZ values from native child
 * environments. Set the requested zone inside this native launcher before
 * starting a fresh interpreter, preserving runtime snapshot semantics. */
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s time-zone executable [arguments...]\n", argv[0]);
        return 1;
    }
#ifdef _WIN32
    if (_putenv_s("TZ", argv[1])) {
        perror("set TZ");
        return 1;
    }
    {
        intptr_t status = _spawnvp(_P_WAIT, argv[2],
                                  (const char *const *)(argv + 2));
        if (status == -1) {
            perror(argv[2]);
            return 1;
        }
        return (int)status;
    }
#else
    if (setenv("TZ", argv[1], 1)) {
        perror("set TZ");
        return 1;
    }
    execvp(argv[2], argv + 2);
    perror(argv[2]);
    return 1;
#endif
}
