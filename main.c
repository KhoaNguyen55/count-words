#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_FILES_COUNT 7
#define CHILD_ID 0
#define QUEUE_NAME "/queue"

struct Directory {
  char **files;
  int amount;
};

struct Directory readDirectory(char *directory) {
  struct Directory dir;
  dir.files = malloc(sizeof(char *));
  dir.amount = 1;

  DIR *dirStream = opendir(directory);
  if (dirStream == NULL) {
    fprintf(stderr, "Error open directory: %s\n", strerror(errno));
    exit(1);
  }

  struct dirent *file = readdir(dirStream);
  int i = 0;
  while (file != NULL) {
    // ignore hidden files
    if (file->d_name[0] != '.') {
      dir.files[i] = file->d_name;
      i++;
      if (dir.amount < i) {
        dir.amount *= 2;
        dir.files = reallocarray(dir.files, sizeof(char *), dir.amount);
      }
    }
    file = readdir(dirStream);
  }

  return dir;
}

void processFile(char *file) {
  mqd_t queueId = mq_open(QUEUE_NAME, O_WRONLY);
  if (queueId == -1) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }

  // todo read the file and count the words
  puts(file);
}

void readFiles(struct Directory dir, int offset) {
  if (dir.amount <= offset) {
    free(dir.files);
    return;
  }
  switch (fork()) {
  case -1:
    fprintf(stderr, "Error forking directory: %s\n", strerror(errno));
    exit(1);
  case CHILD_ID:
    processFile(dir.files[offset]);
    exit(0);
  default:
    readFiles(dir, offset + 1);
  }
}

int main(void) {
  // todo read files from directory
  struct Directory dir = readDirectory("./files/");

  readFiles(dir, 0);

  // get the queue id
  mqd_t queueId = mq_open(QUEUE_NAME, O_RDONLY);
  if (queueId == -1) {
    fprintf(stderr, "%s\n", strerror(errno));
    exit(1);
  }

  wait(NULL);
  return 0;
}
