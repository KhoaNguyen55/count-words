#include <dirent.h>
#include <errno.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Directory {
  char *root;
  char **files;
  int amount;
};

struct chunkSize {
  long start;
  long end;
};

// create a message queue
mqd_t createMqQueue(char *queueName, int flags, const struct mq_attr *attr) {
  mqd_t queueId = mq_open(queueName, flags | O_CREAT, 0600, attr);
  if (queueId == -1) {
    fprintf(stderr, "Error mq_open: %s\n", strerror(errno));
    exit(1);
  }

  return queueId;
}

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
//
// function to read files inside a directory
struct Directory readDirectory(char *directory) {
  struct Directory dir;
  dir.root = directory;
  dir.files = malloc(sizeof(char *));
  dir.amount = 0;

  // open the folder
  DIR *dirStream = opendir(directory);
  if (dirStream == NULL) {
    fprintf(stderr, "Error open directory: %s\n", strerror(errno));
    exit(1);
  }

  // loop through the files
  struct dirent *file = readdir(dirStream);
  int i = 1;
  while (file != NULL) {
    // ignore hidden files
    if (file->d_name[0] != '.') {
      // add the file name to the array of files
      dir.files[dir.amount] = file->d_name;
      dir.amount++;
      if (dir.amount > i) {
        i *= 2;
        char **temp = reallocarray(dir.files, sizeof(char *), i);
        if (temp == NULL) {
          fprintf(stderr, "Error reallocating memory: %s\n", strerror(errno));
          exit(1);
        }
        dir.files = temp;
      }
    }
    file = readdir(dirStream);
  }

  return dir;
}
