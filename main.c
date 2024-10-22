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

struct Msg {
  char *str;
};
struct WordsToFind {
  char **words;
  int count;
};

struct Directory {
  char **files;
  int amount;
};

void formatStrArray(char *output, char **list, int length) {
  int len = 0;
  len += sprintf(output + len, "[");
  for (int i = 0; i < length; i++) {
    len += sprintf(output + len, "%s, ", list[i]);
  }
  sprintf(output + len - 2, "]");
}

struct Directory readDirectory(char *directory) {
  struct Directory dir;
  dir.files = malloc(sizeof(char *));
  dir.amount = 0;

  DIR *dirStream = opendir(directory);
  if (dirStream == NULL) {
    fprintf(stderr, "Error open directory: %s\n", strerror(errno));
    exit(1);
  }

  struct dirent *file = readdir(dirStream);
  int i = 1;
  while (file != NULL) {
    // ignore hidden files
    if (file->d_name[0] != '.') {
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

mqd_t createMqQueue(int flags) {
  const struct mq_attr attr = {0, 10, sizeof(char *), 0};
  // get the queue id
  mqd_t queueId = mq_open(QUEUE_NAME, flags | O_CREAT, 0666, &attr);
  if (queueId == -1) {
    fprintf(stderr, "mq_open: %s\n", strerror(errno));
    exit(1);
  }

  return queueId;
}

void processFile(struct WordsToFind words, char *file) {
  mqd_t queueId = createMqQueue(O_WRONLY);
  struct Msg msg = {file};
  if (mq_send(queueId, (const char *)&msg, sizeof(char *), 0) != 0) {
    puts("error sending queue");
  }
  mq_close(queueId);
}

void readFiles(struct WordsToFind words, struct Directory dir, int offset) {
  if (dir.amount <= offset) {
    free(dir.files);
    return;
  }
  switch (fork()) {
  case -1:
    fprintf(stderr, "Error forking directory: %s\n", strerror(errno));
    exit(1);
  case CHILD_ID:
    processFile(words, dir.files[offset]);
    exit(0);
  default:
    readFiles(words, dir, offset + 1);
  }
}

int main(void) {
  mq_unlink(QUEUE_NAME);

  char *tempwords[] = {"stuff", "hi"};
  const int wordsToFindLength = 2;

  char **words = malloc(wordsToFindLength * sizeof(char *));
  for (int i = 0; i < wordsToFindLength; i++) {
    words[i] = tempwords[i];
  }

  struct WordsToFind wordsToFind = {words, wordsToFindLength};

  // todo read files from directory
  struct Directory dir = readDirectory("./files/");

  readFiles(wordsToFind, dir, 0);

  mqd_t queueId = createMqQueue(O_RDONLY | O_NONBLOCK);
  struct Msg msg;

  int deadProcesses = 0;
  while (1) {
    if (mq_receive(queueId, (char *)&msg, sizeof(char *) + 1, NULL) != -1) {
      printf("%s\n", msg.str);
    } else if (errno == EAGAIN && waitpid(-1, NULL, WNOHANG) != 0) {
      deadProcesses++;
    } else if (errno != EAGAIN) {
      printf("Error mq_receive: %s\n", strerror(errno));
    }

    if (deadProcesses == dir.amount) {
      break;
    }
  }

  mq_close(queueId);
  mq_unlink(QUEUE_NAME);
  return 0;
}
