#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef HANDLETEXT
#define HANDLETEXT

#include "multiThread.c"
#include "wordmap.c"

struct chunkSize {
  long start;
  long end;
};

// get the next word from the file
int getWord(FILE *file, char *output, int maxSize) {
  char c = fgetc_unlocked(file);
  int i = 0;
  while (c != '\t' & c != ' ' && c != '\n' && i < maxSize) {
    if (c == EOF) {
      return EOF;
    }
    output[i] = c;
    i++;
    // escape " character for csv
    if (c == '"') {
      output[i] = c;
      i++;
    }
    c = fgetc_unlocked(file);
  }
  output[i] = '\0';
  return i;
}

// return the next chunk size, `errno = 0` if theres still more chunk to
// read, `errno = EOF` otherwise
struct chunkSize getNextChunkPosition(FILE *file, long chunkSize) {
  struct chunkSize chunk;
  // get the start of the chunk
  chunk.start = ftell(file);
  fseek(file, chunkSize - 1, SEEK_CUR);

  // look for a space, newline or end of line, to seperate the chunk
  // this is to make sure the chunk don't cut off a word midway
  char c = fgetc(file);
  while (c != ' ' && c != '\n' && c != EOF) {
    fputc(c, file);
    fseek(file, -2, SEEK_CUR);
    c = fgetc(file);
  }

  errno = 0;
  if (c != EOF) {
    fputc(c, file);
    chunk.end = ftell(file);
  } else {
    errno = EOF;
    chunk.end = chunk.start + chunkSize;
  }

  return chunk;
}

// thread function for processing the text
void *processText(void *args) {
  // initialized datas needed
  struct ThreadArgs *arg = args;
  FILE *file = fopen(arg->filePath, "r");

  mqd_t threadQueue =
      createMqQueue(arg->qName, O_RDONLY | O_NONBLOCK, arg->qAttr);

  char *word = malloc(sizeof(char[256]));
  struct chunkSize chunk;
  while (1) {
    // check if the parent thread want this thread to be terminated
    if (arg->terminate) {
      struct mq_attr attr;
      mq_getattr(threadQueue, &attr);
      // if theres nothing in the queue then terminate
      if (attr.mq_curmsgs == 0) {
        break;
      }
    }

    // get the next task in the queue
    if (mq_receive(threadQueue, (char *)&chunk, sizeof(struct chunkSize),
                   NULL) != -1) {
      // seek to the chunk needed to scan
      if (fseek(file, chunk.start, SEEK_SET) != 0) {
        fprintf(stderr, "Error file are not seekable: %s\n", strerror(errno));
        exit(1);
      }
      // read each word until the end of the chunk
      while (ftell(file) < chunk.end) {
        int size = getWord(file, word, sizeof(char[256]) - 1);
        if (size > 0) {
          // if a word is found then increment it
          // if its in the words we need to find
          wordmapIncrement(arg->map, word);
        } else if (size == EOF) {
          break;
        }
      }
    } else if (errno != EAGAIN) {
      printf("Error thread mq_receive: %s\n", strerror(errno));
    }
  }

  // clean up
  free(word);
  pthread_exit(0);
}

#endif /* ifndef HANDLETEXT */
