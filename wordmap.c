#include <errno.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "thirdparty/hashmap.c"
#include "thirdparty/hashmap.h"

#define MAX_WORD_SIZE 256

struct wordElement {
  char word[MAX_WORD_SIZE];
  int count;
};

struct wordMap {
  struct hashmap *map;
  pthread_mutex_t mutex;
};

// function use for hashmap
int compare(const void *a, const void *b, void *data) {
  const struct wordElement *wa = a;
  const struct wordElement *wb = b;
  return strcmp(wa->word, wb->word);
}

// function use for hashmap
uint64_t hash(const void *item, uint64_t seed0, uint64_t seed1) {
  const struct wordElement *ele = item;
  return hashmap_sip(ele->word, strlen(ele->word), seed0, seed1);
}

// create a new wordMap
struct wordMap *wordmapCreate(void) {
  struct wordMap *wMap = malloc(sizeof(struct wordMap));
  struct hashmap *map = hashmap_new(sizeof(struct wordElement), 100, 0, 0, hash,
                                    compare, NULL, NULL);
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, NULL);
  wMap->map = map;
  wMap->mutex = mutex;
  return wMap;
}

// add a `wordElement` into the wordmap, combine the total if `word` already
// exists
void wordmapAdd(struct wordMap *wm, struct wordElement *word) {
  pthread_mutex_lock(&wm->mutex);
  const struct wordElement *item = hashmap_get(wm->map, word);
  if (item == NULL) {
    hashmap_set(wm->map, word);
  } else {
    word->count += item->count;
    hashmap_set(wm->map, word);
  }
  pthread_mutex_unlock(&wm->mutex);
}

// increment a word count if it exist, add it to the hashmap otherwise
void wordmapIncrement(struct wordMap *wm, char *word) {
  pthread_mutex_lock(&wm->mutex);
  struct wordElement w = {.count = 1};
  strncpy(w.word, word, MAX_WORD_SIZE);
  const struct wordElement *item = hashmap_get(wm->map, &w);
  if (item == NULL) {
    hashmap_set(wm->map, &w);
  } else {
    w.count = item->count + 1;
    hashmap_set(wm->map, &w);
  }
  pthread_mutex_unlock(&wm->mutex);
}

// print all elements
void wordmapPrintAllElements(struct wordMap *wm) {
  size_t iter = 0;
  void *item;
  while (hashmap_iter(wm->map, &iter, &item)) {
    const struct wordElement *element = item;
    printf("word: '%s' (count=%i)\n", element->word, element->count);
  }
}

// send all elements of a wordMap to a mqueue
void wordmapSendAllElements(struct wordMap *wm, mqd_t queueId) {
  size_t iter = 0;
  void *item;
  while (hashmap_iter(wm->map, &iter, &item)) {
    const struct wordElement *element = item;
    // send it to the queue
    if (mq_send(queueId, (const char *)element, sizeof(struct wordElement),
                0) != 0) {
      fprintf(stderr, "Error sending word count to process: %s\n",
              strerror(errno));
      exit(1);
    }
  }
}
