#ifndef INODEMAP_H
#define INODEMAP_H

#include <sys/types.h>
#include <sys/stat.h>

#define MAPSIZE 1024

const char * get_inode( ino_t );
void set_inode( ino_t, const char * );
void free_inode();

#endif /* INODEMAP_H */
