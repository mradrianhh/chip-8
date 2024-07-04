#include <stdlib.h>
#include <string.h>

#include "types/hashtable.h"
#include "memory/memory.h"

static uint32_t HashString(const char *string);
static void AdjustCapacity(HashTable *table, int capacity);
static Entry *FindEntry(Entry *entries, int capacity, const char *key);
static bool IsTombstone(Entry *entry);

/// @brief Initializes hash-table. Sets everything to zero/NULL.
/// @param table to initialize.
void InitHashTable(HashTable *table)
{
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

/// @brief Deletes data in 'table'.
/// @param table to delete.
void FreeHashTable(HashTable *table)
{
    FREE_ARRAY(Entry, table->entries, table->capacity);
    InitHashTable(table);
}

/// @brief Adds an entry with 'key' and 'value' to 'table'.
/// @param table to add into.
/// @param key to add.
/// @param value to add.
/// @return true if entry does not exist. False if it does.
bool AddEntryHashTable(HashTable *table, const char *key, void *value)
{
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD)
    {
        int capacity = GROW_CAPACITY(table->capacity);
        AdjustCapacity(table, capacity);
    }

    Entry *entry = FindEntry(table->entries, table->capacity, key);
    bool exists = entry->key != NULL;
    if (IsTombstone(entry))
        table->count++;

    entry->key = key;
    entry->value = value;
    entry->hash = HashString(key);
    return !exists;
}

/// @brief Copies entries in 'src' that does not exist in 'dest' to 'dest'.
/// @param src is copied from.
/// @param dest is copied to.
void CopyHashTable(HashTable *src, HashTable *dest)
{
    for (int i = 0; i < src->capacity; i++)
    {
        Entry *entry = &dest->entries[i];
        if (entry->key != NULL)
        {
            AddEntryHashTable(dest, entry->key, entry->value);
        }
    }
}

/// @brief Retrieves a value into 'value' if there exists an element with 'key'.
///        If no elements exists with 'key', value is NULL.
/// @param table to search.
/// @param key to find.
/// @param value to return.
/// @return true if found, false if not.
bool GetEntryHashTable(HashTable *table, const char *key, void *value)
{
    if (table->count == 0)
        return false;

    Entry *entry = FindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    value = entry->value;
    return true;
}

/// @brief Removes the entry with 'key' from 'table'.
/// @param table to remove from.
/// @param key to find entry to remove.
/// @return true if found and deleted. False if not.
bool RemoveEntryHashTable(HashTable *table, const char *key)
{
    if (table->count == 0)
        return false;

    // Find the entry.
    Entry *entry = FindEntry(table->entries, table->capacity, key);
    if (entry->key == NULL)
        return false;

    // Place a tombstone in the entry.
    entry->key = NULL;
    entry->value = NULL;
    entry->hash = 0;
    return true;
}

uint32_t HashString(const char *string)
{
    uint32_t hash = 2166136261u;
    for (int i = 0; i < strlen(string); i++)
    {
        hash ^= (uint8_t)string[i];
        hash *= 16777619;
    }
    return hash;
}

void AdjustCapacity(HashTable *table, int capacity)
{
    // Allocate new hash-table.
    Entry *entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++)
    {
        entries[i].key = NULL;
        entries[i].value = NULL;
    }

    // Insert old(used) buckets into new hash-table.
    table->count = 0;
    for (int i = 0; i < table->capacity; i++)
    {
        Entry *entry = &table->entries[i];
        if (entry->key == NULL)
            continue;

        Entry *dest = FindEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        table->count++;
    }

    // Free old hash-table.
    FREE_ARRAY(Entry, table->entries, table->capacity);

    // Update hash-table structure.
    table->entries = entries;
    table->capacity = capacity;
}

Entry *FindEntry(Entry *entries, int capacity, const char *key)
{
    uint32_t hash = HashString(key);
    uint32_t index = hash % capacity;
    Entry *tombstone = NULL;

    for (;;)
    {
        Entry *entry = &entries[index];
        if (entry->key == NULL)
        {
            if (IsTombstone(entry))
            {
                // We found a tombstone.
                if (tombstone == NULL)
                    tombstone = entry;
            }
            else
            {
                // Empty entry.
                return tombstone != NULL ? tombstone : entry;
            }
        }
        else if (entry->key == key)
        {
            // We found the key.
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool IsTombstone(Entry *entry)
{
    return entry->key == NULL && entry->value == NULL && entry->hash == 0;
}
