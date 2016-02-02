#ifndef _DICT_H_
#define _DICT_H_


#include <stdlib.h>
#include <stdio.h>


/** Keypair: holds a key/value pair. Key must be a hashable C string */
typedef struct _keypair_ {
    char    * key ;
    int     val ;
    unsigned  hash ;
} keypair ;

/** Dict is the only type needed for clients of the dict object */
typedef struct _dict_ {
    unsigned  fill ;
    unsigned  used ;
    unsigned  size ;
    keypair * table ;
} dict ;

#ifdef __cplusplus
extern "C"
{
#endif

// create dict.
dict * dict_new(void);

//free dict
void   dict_free(dict * d);

// Add an item to a dictionary
int    dict_add(dict * d, char * key, int val);

// Get an item from a dictionary
int dict_get(dict * d, char * key);

// Delete an item in a dictionary
int dict_del(dict * d, char * key);

// Enumerate a dictionary
int dict_enumerate(dict * d, int rank, char ** key, int * val);

// Dump dict contents to an opened file pointer
void dict_dump(dict * d, FILE * out);

// Load dict from file
int dict_load(dict * d,  char * path);

#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif
