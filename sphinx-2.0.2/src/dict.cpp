/*-------------------------------------------------------------------------*/
/**
   @file    dict.c
   @author  N. Devillard
   @date    Apr 2011
   @brief   Dictionary object
*/
/*--------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
                                Includes
 ---------------------------------------------------------------------------*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dict.h"

/*---------------------------------------------------------------------------
                                Defines
 ---------------------------------------------------------------------------*/
/** Minimum dictionary size to start with */
#define DICT_MIN_SZ     8

/** Dummy pointer to reference deleted keys */
#define DUMMY_PTR       ((void*)-1)
/** Used to hash further when handling collisions */
#define PERTURB_SHIFT   5
/** Beyond this size, a dictionary will not be grown by the same factor */
#define DICT_BIGSZ      64000

/** Define this to:
    0 for no debugging
    1 for moderate debugging
    2 for heavy debugging
 */
#define DEBUG           2

/*
 * Specify which hash function to use
 * MurmurHash is fast but may not work on all architectures
 * Dobbs is a tad bit slower but not by much and works everywhere
 */
#define dict_hash   dict_hash_murmur
/* #define dict_hash   dict_hash_dobbs */

/* Forward definitions */
static int dict_resize(dict * d);


/** Replacement for strdup() which is not always provided by libc */
static char * xstrdup(char * s)
{
    char * t ;
    if (!s)
        return NULL ;
    t = (char *)malloc(strlen(s)+1) ;
    if (t) {
        strcpy(t,s);
    }
    return t ;
}


/**
  This hash function has been taken from an Article in Dr Dobbs Journal.
  There are probably better ones out there but this one does the job.
 */
static unsigned dict_hash_dobbs(char * key)
{
    int         len ;
    unsigned    hash ;
    int         i ;

    len = strlen(key);
    for (hash=0, i=0 ; i<len ; i++) {
        hash += (unsigned)key[i] ;
        hash += (hash<<10);
        hash ^= (hash>>6) ;
    }
    hash += (hash <<3);
    hash ^= (hash >>11);
    hash += (hash <<15);
    return hash ;
}


/* Murmurhash */
static unsigned dict_hash_murmur(char * key)
{
    int         len ;
    unsigned    h, k, seed ;
    unsigned    m = 0x5bd1e995 ;
    int         r = 24 ;
    unsigned char * data ;

    seed = 0x0badcafe ;
    len  = (int)strlen(key);

    h = seed ^ len ;
    data = (unsigned char *)key ;
    while(len >= 4) {
        k = *(unsigned int *)data;

        k *= m;
        k ^= k >> r;
        k *= m;

        h *= m;
        h ^= k;

        data += 4;
        len -= 4;
    }
    switch(len) {
        case 3: h ^= data[2] << 16;
        case 2: h ^= data[1] << 8;
        case 1: h ^= data[0];
                h *= m;
    };

    h ^= h >> 13;
    h *= m;
    h ^= h >> 15;

    return h;
}

/** Lookup an element in a dict
    This implementation copied almost verbatim from the Python dictionary
    object, without the Pythonisms.
    */
static keypair * dict_lookup(dict * d, char * key, unsigned hash)
{
    keypair * freeslot ;
    keypair * ep ;
    unsigned    i ;
    unsigned    perturb ;

    if (!d || !key)
        return NULL ;

    i = hash & (d->size-1) ;
    /* Look for empty slot */
    ep = d->table + i ;
    if (ep->key==NULL || ep->key==key) {
        return ep ;
    }
    if (ep->key == DUMMY_PTR) {
        freeslot = ep ;
    } else {
        if (ep->hash == hash &&
            !strcmp(key, ep->key)) {
                return ep ;
        }
        freeslot = NULL ;
    }
    for (perturb=hash ; ; perturb >>= PERTURB_SHIFT) {
        i = (i<<2) + i + perturb + 1;
        i &= (d->size-1);
        ep = d->table + i;
        if (ep->key==NULL) {
            return freeslot==NULL ? ep : freeslot ;
        }
        if ((ep->key == key) ||
            (ep->hash == hash &&
             ep->key  != DUMMY_PTR &&
             !strcmp(ep->key, key))) {
            return ep ;
        }
        if (ep->key==DUMMY_PTR && freeslot==NULL) {
            freeslot = ep ;
        }
    }
    return NULL ;
}

/** Add an item to a dictionary without copying key/val
    Used by dict_resize() only.
 */
static int dict_add_p(dict * d, char * key, int val)
{
    unsigned  hash ;
    keypair * slot ;

    if (!d || !key)
        return -1 ;

    hash = dict_hash(key);
    slot = dict_lookup(d, key, hash);
    if (slot) {
        slot->key  = key;
        slot->val  = val ;
        slot->hash = hash ;
        d->used++ ;
        d->fill++ ;
        if ((3*d->fill) >= (d->size*2)) {
            if (dict_resize(d)!=0) {
                return -1 ;
            }
        }
    }
    return 0;
}

/** Add an item to a dictionary by copying key/val into the dict. */
int dict_add(dict * d, char * key, int val)
{
    unsigned  hash ;
    keypair * slot ;

    if (!d || !key)
        return -1 ;

    hash = dict_hash(key);
    slot = dict_lookup(d, key, hash);
    if (slot) {
        slot->key  = xstrdup(key);
        if (!(slot->key)) {
            return -1 ;
        }
        slot->val  = val;
        slot->hash = hash ;
        d->used++ ;
        d->fill++ ;
        if ((3*d->fill) >= (d->size*2)) {
            if (dict_resize(d)!=0) {
                return -1 ;
            }
        }
    }
    return 0;
}

/** Resize a dictionary */
static int dict_resize(dict * d)
{
    unsigned      newsize ;
    keypair *   oldtable ;
    unsigned      i ;
    unsigned      oldsize ;
    unsigned      factor ;

    newsize = d->size ;
    /*
     * Re-sizing factor depends on the current dict size.
     * Small dicts will expand 4 times, bigger ones only 2 times
     */
    factor = (d->size>DICT_BIGSZ) ? 2 : 4 ;
    while (newsize<=(factor*d->used)) {
        newsize*=2 ;
    }
    /* Exit early if no re-sizing needed */
    if (newsize==d->size)
        return 0 ;
#if DEBUG>2
    printf("resizing %d to %d (used: %d)\n", d->size, newsize, d->used);
#endif
    /* Shuffle pointers, re-allocate new table, re-insert data */
    oldtable = d->table ;
    d->table = (keypair *)calloc(newsize, sizeof(keypair));
    if (!(d->table)) {
        /* Memory allocation failure */
        return -1 ;
    }
    oldsize  = d->size ;
    d->size  = newsize ;
    d->used  = 0 ;
    d->fill  = 0 ;
    for (i=0 ; i<oldsize ; i++) {
        if (oldtable[i].key && (oldtable[i].key!=DUMMY_PTR)) {
            dict_add_p(d, oldtable[i].key, oldtable[i].val);
        }
    }
    free(oldtable);
    return 0 ;
}


/** Public: allocate a new dict */
dict * dict_new(void)
{
    dict * d ;

    d = (dict *)calloc(1, sizeof(dict));
    if (!d)
        return NULL;
    d->size  = DICT_MIN_SZ ;
    d->used  = 0 ;
    d->fill  = 0 ;
    d->table = (keypair *)calloc(DICT_MIN_SZ, sizeof(keypair));
    return d ;
}

/** Public: deallocate a dict */
void dict_free(dict * d)
{
    unsigned i ;

    if (!d)
        return ;

    for (i=0 ; i<d->size ; i++) {
        if (d->table[i].key && d->table[i].key!=DUMMY_PTR) {
            free(d->table[i].key);
        }
    }
    free(d->table);
    free(d);
    return ;
}

/** Public: get an item from a dict */
int dict_get(dict * d, char * key)
{
    keypair * kp ;
    unsigned  hash ;
    int ret = -1;

    if (!d || !key)
        return ret ;

    hash = dict_hash(key);
    kp = dict_lookup(d, key, hash);
    if (kp) {
        return kp->val ;
    }
    return ret;
}

/** Public: delete an item in a dict */
int dict_del(dict * d, char * key)
{
    unsigned    hash ;
    keypair *   kp ;

    if (!d || !key)
        return -1 ;

    hash = dict_hash(key);
    kp = dict_lookup(d, key, hash);
    if (!kp)
        return -1 ;
    if (kp->key && kp->key!=DUMMY_PTR)
        free(kp->key);
    kp->key = (char *)DUMMY_PTR;
    d->used -- ;
    return 0 ;
}

/** Public: enumerate a dictionary */
int dict_enumerate(dict * d, int rank, char ** key, int * val)
{
    if (!d || !key || !val || (rank<0))
        return -1 ;

    while ((d->table[rank].key==NULL || d->table[rank].key==DUMMY_PTR) &&
           (rank<d->size))
        rank++;

    if (rank>=d->size) {
        *key = NULL;
        *val = -1;
        rank = -1 ;
    } else {
        *key = d->table[rank].key ;
        *val = d->table[rank].val ;
        rank++;
    }
    return rank ;
}

/** Public: dump a dict to a file pointer */
void dict_dump(dict * d, FILE * out)
{
    char * key ;
    int  val ;
    int    rank=0 ;

    if (!d || !out)
        return ;

    while (1) {
        rank = dict_enumerate(d, rank, &key, &val);
        if (rank<0)
            break ;
        fprintf(out, "%20s: %d\n", key, val);
    }
    return ;
}

/** string start with */
int start_with(const char * string, const char * prefix)
{
    while(*prefix){
        if(*prefix++ != *string++)
            return 0;
    }

    return 1;
}


/** load dict from file, ret 0(ok) or -1(error) */
int dict_load(dict * d,  char * path)
{
	FILE * fp;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;
	char *k, *v;
	int num;

	if(!d)
		return -1;

    fp = fopen(path, "r");
    if (fp == NULL)
         return -1;

    while ((read = getline(&line, &len, fp)) != -1) {
        if(start_with(line, "x:"))
        	continue;
        k=strtok(line,"	");
        v=strtok(NULL, "	");
        if(k && v){
        	num = atoi(v);
            dict_add(d, k, num);
        }
    }

    if (line)
        free(line);
    if (fp)
        fclose();
    return 0;
}
