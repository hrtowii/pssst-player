#ifndef DECODE_THREAD_H
#define DECODE_THREAD_H

#include <pspkernel.h>
#include <stdbool.h>

bool decoder_open(const char *path);
void decoder_close(void);
int  decode_thread(SceSize args, void *argp);

#endif
