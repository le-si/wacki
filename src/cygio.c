/*
 * cygio.c — Cygert's "Base_IO_CPP" file class shim.
 *
 * In the original WACKI.EXE these are methods of a small C++ file class
 * (string "Base_IO_CPP by Henryk Cygert" sits in this object's vtable).
 * We map them to the stdio counterparts.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct CygFile { FILE *fp; } CygFile;

CygFile *fopen_cyg(const char *name, const char *mode) {
    FILE *fp = fopen(name, mode);
    if (!fp) return NULL;
    CygFile *f = (CygFile *)malloc(sizeof *f);
    if (!f) { fclose(fp); return NULL; }
    f->fp = fp;
    return f;
}
void fclose_cyg(CygFile *f) {
    if (!f) return;
    fclose(f->fp);
    free(f);
}
uint32_t fread_cyg(void *dst, uint32_t sz, uint32_t n, CygFile *f) {
    return (uint32_t)fread(dst, sz, n, f->fp);
}
void fseek_cyg(CygFile *f, int32_t off, int whence) {
    fseek(f->fp, (long)off, whence);
}
int32_t ftell_cyg(CygFile *f) {
    return (int32_t)ftell(f->fp);
}
