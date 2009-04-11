#ifndef EMBED_H
#define EMBED_H

unsigned int read32(const unsigned char *in);

int embed_get_data(const char *argv0, unsigned char **ret_data, unsigned int *ret_size);

#endif
