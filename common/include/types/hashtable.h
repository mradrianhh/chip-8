#ifndef COMMON_HASHTABLE_H
#define COMMON_HASHTABLE_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "string.h"

#define TABLE_MAX_LOAD 0.75

typedef struct
{
    uint32_t hash;
    const char *key;
    void *value;
} Entry;

typedef struct
{
    size_t count;
    size_t capacity;
    Entry *entries;
} HashTable;

void InitHashTable(HashTable *table);
void FreeHashTable(HashTable *table);
bool AddEntryHashTable(HashTable *table, const char *key, void *value);
void CopyHashTable(HashTable *src, HashTable *dest);
bool GetEntryHashTable(HashTable *table, const char *key, void *value);
bool RemoveEntryHashTable(HashTable* table, const char *key);
const char *FindStringHashTable(HashTable *table, const char *chars, int length, uint32_t hash);

#endif
