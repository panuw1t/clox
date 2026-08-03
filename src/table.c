#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

void initTable(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void freeTable(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  initTable(table);
}

uint32_t hashNumber(double value) {
  union {
    double d;
    uint64_t i;
  } u;

  u.d = value;

  return (uint32_t)(u.i ^ (u.i >> 32));
}

uint32_t hashValue(Value value) {
  switch (value.type) {
  case VAL_BOOL:   return AS_BOOL(value) ? 1231 : 1237;
  case VAL_NIL:    return 1407;
  case VAL_NUMBER: return hashNumber(AS_NUMBER(value));
  case VAL_OBJ:    return AS_STRING(value)->hash;
  default:         return 0;
  }
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
  uint32_t hash = hashValue(key);
  uint32_t index = hash % capacity;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    if (IS_NIL(entry->key)) {
      if (IS_NIL(entry->value)) {
        // Empty entry.
        return tombstone != NULL ? tombstone : entry;
      } else {
        // We found a tombstone.
        if (tombstone == NULL) tombstone = entry;
      }
    } else if (valuesEqual(entry->key, key)) {
      // We found the key.
      return entry;
    }

    index = (index + 1) % capacity;
  }
}

bool tableGet(Table* table, Value key, Value* value) {
  if (table->count == 0) return false;

  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (IS_NIL(entry->key)) return false;

  *value = entry->value;
  return true;
}

static void adjustCapacity(Table* table, int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NIL_VAL;
    entries[i].value = NIL_VAL;
  }

  table->count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (IS_NIL(entry->key)) continue;

    Entry* dest = findEntry(entries, capacity, entry->key);
    dest->key = entry->key;
    dest->value = entry->value;
    table->count++;
  }

  FREE_ARRAY(Entry, table->entries, table->capacity);
  table->entries = entries;
  table->capacity = capacity;
}

bool tableSet(Table* table, Value key, Value value) {
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjustCapacity(table, capacity);
  }

  Entry* entry = findEntry(table->entries, table->capacity, key);
  bool isNewKey = IS_NIL(entry->key);
  if (isNewKey && IS_NIL(entry->value)) table->count++;

  entry->key = key;
  entry->value = value;
  return isNewKey;
}

bool tableDelete(Table* table, Value key) {
  if (table->count == 0) return false;

  // Find the entry.
  Entry* entry = findEntry(table->entries, table->capacity, key);
  if (IS_NIL(entry->key)) return false;

  // Place a tombstone in the entry.
  entry->key = NIL_VAL;
  entry->value = BOOL_VAL(true);
  return true;
}

void tableAddAll(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (IS_NIL(entry->key)) {
      tableSet(to, entry->key, entry->value);
    }
  }
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
  if (table->count == 0) return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry* entry = &table->entries[index];
    if (IS_NIL(entry->key)) {
      // Stop if we find an empty non-tombstone entry.
      if (IS_NIL(entry->value)) return NULL;
    } else if (IS_STRING(entry->key) &&
               AS_STRING(entry->key)->length == length &&
               hashValue(entry->key) == hash &&
               memcmp(AS_STRING(entry->key)->chars, chars, length) == 0) {
      // We found it.
      return AS_STRING(entry->key);
    }

    index = (index + 1) % table->capacity;
  }
}

char* getGlobalNameByIndex(Table* table, int index) {
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (IS_STRING(entry->key) && IS_NUMBER(entry->value)) {
      if (AS_INT(entry->value) == index) {
        return AS_CSTRING(entry->key);
      }
    }
  }
  return NULL;
}
