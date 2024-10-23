#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHILD_ID 0
#define MAX_THREADS 1
#define MAX_BYTES_PER_READ 1024

struct chunkSize {
  long start;
  long end;
};

struct WordsToFind {
  char **words;
  int length;
};

struct ThreadArgs {
  char **words;
  int *count;
  pthread_mutex_t *mutexes;
  char *filePath;
  const struct mq_attr *qAttr;
  int terminate;
};

struct Directory {
  char *root;
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
  dir.root = directory;
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

mqd_t createMqQueue(char *queueName, int flags, const struct mq_attr *attr) {
  mqd_t queueId = mq_open(queueName, flags | O_CREAT, 0600, attr);
  if (queueId == -1) {
    fprintf(stderr, "Error mq_open: %s\n", strerror(errno));
    exit(1);
  }

  return queueId;
}

int getWord(FILE *file, char *output, int maxSize) {
  char c = fgetc_unlocked(file);
  int i = 0;
  while (c != ' ' && c != '\n' && i < maxSize) {
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

void *processText(void *args) {
  struct ThreadArgs *arg = args;
  FILE *file = fopen(arg->filePath, "r");

  mqd_t threadQueue =
      createMqQueue("/thread", O_RDONLY | O_NONBLOCK, arg->qAttr);

  char *word = malloc(100 * sizeof(char));
  struct chunkSize chunk;

  while (1) {
    if (arg->terminate) {
      struct mq_attr attr;
      mq_getattr(threadQueue, &attr);
      if (attr.mq_curmsgs == 0) {
        break;
      }
    }
    if (mq_receive(threadQueue, (char *)&chunk, sizeof(struct chunkSize),
                   NULL) != -1) {
      if (fseek(file, chunk.start, SEEK_SET) != 0) {
        fprintf(stderr, "Error file are not seekable: %s\n", strerror(errno));
        exit(1);
      }
      while (ftell(file) < chunk.end) {
        int size = getWord(file, word, 100);
        if (size > 0) {
          printf("chunk: %lu - %lu, '%s'\n", chunk.start, chunk.end, word);
        } else if (size == EOF) {
          break;
        }
      }
    } else if (errno != EAGAIN) {
      printf("Error thread mq_receive: %s\n", strerror(errno));
    }
  }

  free(word);
  pthread_exit(0);
}

// return the next chunk size, `errno = 0` if theres still more chunk to
// read, `errno = EOF` otherwise
struct chunkSize getNextChunkPosition(FILE *file, long chunkSize) {
  struct chunkSize chunk;
  chunk.start = ftell(file);
  fseek(file, chunkSize - 1, SEEK_CUR);

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

void processFile(struct WordsToFind words, char *root, char *fileName) {
  const struct mq_attr pAttr = {0, 10, sizeof(char *), 0};
  mqd_t processQueue = createMqQueue("/process", O_WRONLY, &pAttr);

  const struct mq_attr tAttr = {0, words.length, sizeof(struct chunkSize), 0};
  mqd_t threadQueue = createMqQueue("/thread", O_WRONLY, &tAttr);

  pthread_mutex_t mutexes[words.length];
  int count[words.length];
  for (int i = 0; i < words.length; i++) {
    count[i] = 0;
    if (pthread_mutex_init(&mutexes[i], NULL) != 0) {
      fprintf(stderr, "Error making mutex: %s\n", strerror(errno));
      exit(1);
    }
  }

  pthread_t threads[MAX_THREADS];

  char *filePath = malloc(2 * sizeof(char[256]));
  strcpy(filePath, root);
  strcat(filePath, fileName);

  struct ThreadArgs arg = {words.words, count, mutexes, filePath, &tAttr, 0};

  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_create(&threads[i], NULL, processText, (void *)&arg);
  }

  FILE *file = fopen(filePath, "r");
  struct chunkSize msg;
  do {
    msg = getNextChunkPosition(file, MAX_BYTES_PER_READ);
    if (mq_send(threadQueue, (char *)&msg, sizeof(struct chunkSize), 0) != 0) {
      fprintf(stderr, "Error sending queue to thread: %s\n", strerror(errno));
      exit(1);
    }
  } while (errno != EOF);

  arg.terminate = 1;

  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  free(filePath);
  mq_close(processQueue);
  mq_close(threadQueue);
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
    processFile(words, dir.root, dir.files[offset]);
    exit(0);
  default:
    readFiles(words, dir, offset + 1);
  }
}

int main(void) {
  mq_unlink("/process");
  mq_unlink("/thread");

  char *tempwords[] = {"test", "hi"};
  const int wordsToFindLength = 2;

  char **words = malloc(wordsToFindLength * sizeof(char *));
  for (int i = 0; i < wordsToFindLength; i++) {
    words[i] = tempwords[i];
  }

  struct WordsToFind wordsToFind = {words, wordsToFindLength};

  struct Directory dir = readDirectory("./test/");

  readFiles(wordsToFind, dir, 0);

  const struct mq_attr pAttr = {0, 10, sizeof(char *), 0};
  mqd_t queueId = createMqQueue("/process", O_RDONLY | O_NONBLOCK, &pAttr);
  struct chunkSize msg;

  int deadProcesses = 0;
  while (1) {
    if (mq_receive(queueId, (char *)&msg, sizeof(msg) + 1, NULL) != -1) {
      /* printf("%lu\n", msg); */
    } else if (errno == EAGAIN && waitpid(-1, NULL, WNOHANG) != 0) {
      deadProcesses++;
    } else if (errno != EAGAIN) {
      printf("Error process mq_receive: %s\n", strerror(errno));
    }

    if (deadProcesses == dir.amount) {
      break;
    }
  }

  free(words);
  mq_close(queueId);
  mq_unlink("/thread");
  mq_unlink("/process");
  return 0;
}
