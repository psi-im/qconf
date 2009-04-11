/*
Copyright (C) 2009  Justin Karneges

This file is free software; unlimited permission is given to copy and/or
distribute it, with or without modifications, as long as this notice is
preserved.
*/

#ifndef EMBED_H
#define EMBED_H

unsigned int read32(const unsigned char *in);

int embed_get_data(const char *argv0, unsigned char **ret_data, unsigned int *ret_size);

#endif
