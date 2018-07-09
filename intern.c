#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Debugging. */
#include <stdio.h>

#include "intern.h"

/*
 * Implementation of interntables.
 */

/* 
 * The number of hash buckets we use.
 *
 * This should be a prime number.
 */
#define BUCKET_COUNT 13

/* Convert a string to a hashcode. */
static unsigned long hashstring(const char *val) {
	/* This is the DJB2K33 algorithm. It generally works quite well. */
	unsigned long hash = 5381;
	int c;

	/* Get the current string character. */
	c    = *val;
	val += 1;

	/* While we're not at the end of the string. */
	while(c) {
		/* hash * 33 + c */
		hash = ((hash << 5) + hash) + c; 

		/* Advance to the next character. */
		c    = *val;
		val += 1;
	}

	return hash;
}

/* Convert an integer to a hashcode. */
static unsigned long hashkey(const int val) {
	return val;
}

/* A bucket in the intern table */
struct bucket {
	/* The string in this bucket. */
	char      *val;
	/* The intern key for this string. */
	internkey  key;

	/*
	 * Next/prev form a circular list of buckets.
	 */

	/* The next bucket in the chain. */
	struct bucket *next;
	/* The previous bucket in the chain. */
	struct bucket *prev;
};

/* Definition of an intern table. */
struct interntab {
	/* The next intern key to use. */
	internkey nextkey;
	
	/* The table of strings */
	struct bucket *strings[BUCKET_COUNT];
	/* The table of keys */
	struct bucket *keys[BUCKET_COUNT];
};

/* Allocate/initialize an intern table. */
struct interntab *makeinterntab() {
	/* The table. */
	struct interntab *tab;
	/* The counter for entry initialization */
	int i;

	/* Create a new intern table, and set its key value. */
	tab = malloc(sizeof(struct interntab));
	assert(tab != NULL);
	tab->nextkey = 1;

	/* Initialize the tables. */
	for(i = 0; i < BUCKET_COUNT; i++) {
		/* Initialize string table. */
		tab->strings[i]       = malloc(sizeof(struct bucket));
		tab->strings[i]->val  = NULL;
		tab->strings[i]->key  = SIINVALID;
		tab->strings[i]->next = NULL;
		tab->strings[i]->prev = NULL;

		/* Initialize key table. */
		tab->keys[i]       = malloc(sizeof(struct bucket));
		tab->keys[i]->val  = NULL;
		tab->keys[i]->key  = SIINVALID;
		tab->keys[i]->next = NULL;
		tab->keys[i]->prev = NULL;
	}

	return tab;
}

/* Destroy a bucket chain. */
static void killbucket(struct bucket *buck, int freestrings) {
	/* Terminate the chain. */
	/*
	 * NOTE:
	 * 	buck->prev is only null in the case where nothing has
	 * 	been inserted into this bucket before. 
	 */
	if(buck->prev != NULL) {
		buck->prev->next = NULL;
	}

	while(buck != NULL && buck->next != NULL) {
		/* Next bucket. */
		struct bucket *tmp;

		/* Advance the queue. */
		tmp  = buck;
		buck = buck->next;

		/* Free the string we own. */
		if(tmp->val != NULL && freestrings == 1) free(tmp->val);
		/* Free the bucket. */
		free(tmp);
	}
}

void killinterntab(struct interntab *table) {
	/* Loop index. */
	int i;

	/* Free strings table. */
	for(i = 0; i < BUCKET_COUNT; i++) {
		killbucket(table->strings[i], 0);
	}
	/* Free keys table. */
	for(i = 0; i < BUCKET_COUNT; i++) {
		killbucket(table->keys[i], 1);
	}

	free(table);
}

/* Add a bucket to a bucket chain. */
static void addbucket(struct bucket *bucket) {
	/* Allocate a new bucket. */
	struct bucket *nbucket;
	nbucket = malloc(sizeof(struct bucket));
	assert(nbucket != NULL);

	if(bucket->next == NULL) {
		/* Solo bucket. */
		bucket->next  = nbucket;
		bucket->prev  = nbucket;

		nbucket->next = bucket;
		nbucket->prev = bucket;
	} else {
		/* Chain bucket. */

		/* The next bucket in the list. */
		struct bucket *nextbucket;
		nextbucket = bucket->next;

		/* Insert our bucket into the chain. */
		bucket->next     = nbucket;
		nextbucket->prev = nbucket;

		nbucket->next = nextbucket;
		nbucket->prev = bucket;
	}
}

/* Intern a string. */
internkey internstring(struct interntab *table, const char *string) {
	/* The intern key we use. */
	internkey key;
	/* The key/string hashes. */
	int khash, shash;
	/* The string/key buckets. */
	struct bucket *sbucket, *kbucket;

	/* Don't duplicate an already interned string. */
	key = lookupstring(table, string);
	if(key != SIINVALID) return key;

	/* Get the next valid intern key. */
	key = table->nextkey;
	table->nextkey += 1;
	/* Fail if we ran out of space for strings. */
	if(key < 0) return SITABFULL;
	/* Make sure we never make the invalid key valid. */
	assert(key != SIINVALID);

	/* Create a copy of the string we own. */
	char *strang = (char *)strdup(string);

	/* Get both hashes. */
	shash = hashstring(strang) % BUCKET_COUNT;
	khash = hashkey(key)       % BUCKET_COUNT;

	/* Get the buckets for each hash. */
	sbucket = table->strings[shash];
	kbucket = table->keys[khash];

	if(sbucket->key != SIINVALID) {
		/*
		 * Non-empty bucket. 
		 *
		 * Insert into a new one.
		 */
		sbucket = sbucket->prev;
	}
	if(kbucket->key != SIINVALID) {
		/*
		 * Non-empty bucket. 
		 *
		 * Insert into a new one.
		 */
		kbucket = kbucket->prev;
	}

	/* Insert into the right bucket in both tables. */
	sbucket->val = strang;
	kbucket->val = strang;

	sbucket->key = key;
	kbucket->key = key;

	/* Add a new empty bucket to both tables. */
	addbucket(sbucket);
	addbucket(kbucket);

	return key;
}

/* Lookup a interned key for a string. */
internkey lookupstring(struct interntab *table, const char *string) {
	/* The string hash. */
	int shash;
	/* The string bucket. */
	struct bucket *sbucket;
	/* The first key in a bucket. */
	int fkey;

	/* Get the bucket for this string. */
	shash   = hashstring(string) % BUCKET_COUNT;
	sbucket = table->strings[shash];
	fkey    = sbucket->key;

	/* Bail out early if the bucket is empty. */
	if(sbucket->key == SIINVALID) return SIINVALID;

	do {
		/* Only check buckets with valid values. */
		if(sbucket->val != NULL) {
			/* Check if this is the right bucket. */
			if(strcmp(string, sbucket->val) == 0) return sbucket->key;
		}

		/* Move to next bucket. */
		sbucket = sbucket->next;

		/* Bail out if we've looped or hit an empty bucket. */
	} while(sbucket->key != fkey && sbucket->key != SIINVALID);

	/* We didn't find anything. */
	return SIINVALID;
}

/* Lookup the string for an interned key. */
const char *lookupkey(struct interntab *table, const internkey key) {
	/* The key hash. */
	int khash;
	/* The key bucket. */
	struct bucket *kbucket;
	/* The first key in a bucket. */
	int fkey;

	/* This key can't possibly be in the table. */
	if(table->nextkey < key) return NULL;

	/* Get the bucket for this key. */
	khash   = hashkey(key) % BUCKET_COUNT;
	kbucket = table->keys[khash];
	fkey    = kbucket->key;

	/* Bail out early if the bucket is empty. */
	if(fkey == SIINVALID) return NULL;

	do {
		/* Check if this is the right bucket. */
		if(key == kbucket->key) return kbucket->val;

		/* Move to next bucket. */
		kbucket = kbucket->next;

		/* Bail out if we've looped or hit an empty bucket. */
	} while(kbucket->key != fkey && kbucket->key != SIINVALID);

	/*
	 * We didn't find anything. */
	return NULL;
}

/* Execute an action for every key in a table. */
void foreachintern(struct interntab *table, tableitr itr, void *pvData) {
	int i;
	/* Iterate over every bucket. */
	for(i = 0; i < BUCKET_COUNT; i++) {
		struct bucket *buck;
		internkey      kFirst;
		
		/* Get the first bucket, and its key. */
		buck = table->keys[i];
		kFirst = buck->key;
		
		/* Ignore this bucket if its empty. */
		if(kFirst == SIINVALID) continue;

		do {
			itr(buck->val, buck->key, pvData);
			/* Bail out if we've looped or hit an empty bucket. */
		} while(buck->key != kFirst && buck->key != SIINVALID);
	}
}

/*
 * Implementation of internlists
 */
struct internlist {
	/* The dynamic array for storing data. */
	void **paData;

	/* Amount available/used in the array. */
	int dataspace;
	int datausage;

	/* Mapping from string keys to array indexes. */
	struct interntab *ptKeys;

	/* Destructor function for data. */
	void (*pfDestroy)(void *);
};

/*
 * Create an intern list.
 *
 * Takes the initial capacity for the intern list, and the function to use to
 * destroy items in the intern list.
 */
struct internlist *makeinternlist(int initcap, void (*pfDestroy)(void *)) {
	/* Allocate the list, and fail if allocation fails. */
	struct internlist *plList = malloc(sizeof(struct internlist));
	assert(plList != NULL);

	/* Create the key mapping. */
	plList->ptKeys = makeinterntab();
	/* Remember the destructor. */
	plList->pfDestroy = pfDestroy;

	/* Allocate space for the data. */
	plList->paData = calloc(initcap, sizeof(void *));
	assert(plList->paData != NULL);
	/* Initialize capacity tracking. */
	plList->dataspace = initcap;
	plList->datausage = 0;

	return plList;
}

/* Destroy an intern list. */
void killinternlist(struct internlist * plList) {
	/* Destruct the contained data. */
	int i;
	for(i = 0; i < plList->datausage; i++) {
		plList->pfDestroy(plList->paData[i]);
	}
	/* Free the data storage. */
	free(plList->paData);
	/* Free key storage. */
	killinterntab(plList->ptKeys);
	/* Free the list. */
	free(plList);
}

/* Insert an item into the list. */
void putinternlist(struct internlist *plList, char *pszKey, void *pvData) {
	/* Intern the name, and fail if interning fails */
	internkey  kData = internstring(plList->ptKeys, pszKey);
	assert(kData > 0);

	if(plList->dataspace == plList->datausage) {
		/* Allocate more space for the list. */
		plList->dataspace *= 2;
		plList->dataspace += 1;
		plList->paData     = realloc(plList->paData, sizeof(void *) * plList->dataspace);
		/* Fail if allocation fails. */
		assert(plList->paData != NULL);
	}

	/* Delete any item that was in that slot. */
	if(plList->paData[kData -1] != NULL) {
		plList->pfDestroy(plList->paData[kData - 1]);
	}

	/* Insert the item. */
	plList->paData[kData - 1]  = pvData;
	plList->datausage         += 1;
}

/* 
 * Get an item from an intern list. 
 *
 * Returns NULL if there is no item for that key. 
 */
void *getinternlist(struct internlist * plList, char *pszKey) {
	/* Lookup the index. */
	internkey kData = lookupstring(plList->ptKeys, pszKey);
	/* Error if we don't have that key. */
	if(kData == SIINVALID) return NULL;

	/* Give back the stored data. */
	return plList->paData[kData - 1];
}

/* Delete an item from an intern list. */
void deleteinternlist(struct internlist *plList, char *pszKey) {
	/* Lookup the index. */
	internkey kData = lookupstring(plList->ptKeys, pszKey);
	/* Error if we don't have that key. */
	if(kData == SIINVALID) return;

	if(plList->paData[kData - 1] != NULL)
		plList->pfDestroy(plList->paData[kData - 1]);

	plList->paData[kData - 1] = NULL;
}

/* 
 * Check if an item is in an intern list. 
 *
 * Returns 0 if the item is not cotinained, 1 oterhwise.
 */
int containsinternlist(struct internlist *plList, char *pszKey) {
	/* Lookup the index. */
	internkey kData = lookupstring(plList->ptKeys, pszKey);
	/* Error if we don't have that key. */
	if(kData == SIINVALID) {
		return 0;
	} else {
		return plList->paData[kData - 1] != NULL;
	}
}

struct listitrdata {
	struct internlist *plList;

	internlistitr pfFunc;

	void *pvData;
};

static void dointernlistitr(const char *pszName, internkey kKey, void *pvData) {
	void *pvItem;

	struct listitrdata *pData = (struct listitrdata *)pvData;

	pvItem = getinternlist(pData->plList, (char *)pszName);

	if(pvItem != NULL) {
		pData->pfFunc((char *)pszName, pvItem, pData->pvData);
	}
}

void foreachinternlist(struct internlist *plList, internlistitr pfFunc, void *pvData) {
	struct listitrdata dat;

	dat.pfFunc = pfFunc;
	dat.plList = plList;
	dat.pvData = pvData;

	foreachintern(plList->ptKeys, &dointernlistitr, &dat);
}
