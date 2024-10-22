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
#define QUEUE_NAME "/queue"
#define MAX_THREADS 1
#define BYTES_PER_READ 1024

struct Msg {
  char *str;
};
struct WordsToFind {
  char **words;
  int length;
};

struct TaskQueue {
  long *queue;
  int count;
  int current;
  pthread_mutex_t mutex;
};

struct ThreadArgs {
  char **words;
  int *count;
  pthread_mutex_t *mutexes;
  struct TaskQueue *taskQueue;
  char *filePath;
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

mqd_t createMqQueue(int flags) {
  const struct mq_attr attr = {0, 10, sizeof(char *), 0};
  // get the queue id
  mqd_t queueId = mq_open(QUEUE_NAME, flags | O_CREAT, 0666, &attr);
  if (queueId == -1) {
    fprintf(stderr, "Error mq_open: %s\n", strerror(errno));
    exit(1);
  }

  return queueId;
}

int getWord(FILE *file, char *output, int maxSize) {
  char c = fgetc(file);
  int i = 0;
  while (c != ' ' && c != '\n' && i < maxSize) {
    if (c == EOF) {
      return EOF;
    }
    output[i] = c;
    i++;
    c = fgetc(file);
  }
  output[i] = '\0';
  return 0;
}

void *processText(void *args) {
  struct ThreadArgs *arg = args;
  FILE *file = fopen(arg->filePath, "r");

  long offset = 0;

  if (fseek(file, offset, SEEK_SET) != 0) {
    fprintf(stderr, "Error file are not seekable: %s\n", strerror(errno));
    exit(1);
  }

  char *word = malloc(100 * sizeof(char));
  while (getWord(file, word, 100) != EOF) {
    printf("'%s'\n", word);
  }

  free(word);
  pthread_exit(0);
}

void processFile(struct WordsToFind words, char *root, char *file) {
  mqd_t queueId = createMqQueue(O_WRONLY);

  struct TaskQueue taskQueue;
  long queue[words.length];
  pthread_mutex_t qMutex;
  if (pthread_mutex_init(&qMutex, NULL) != 0) {
    fprintf(stderr, "Error making mutex: %s\n", strerror(errno));
    exit(1);
  }
  taskQueue.queue = queue;
  taskQueue.count = words.length;
  taskQueue.current = 0;
  taskQueue.mutex = qMutex;

  pthread_mutex_t mutexes[words.length];
  int count[words.length];
  for (int i = 0; i < words.length; i++) {
    count[i] = 0;
    queue[i] = 0;
    if (pthread_mutex_init(&mutexes[i], NULL) != 0) {
      fprintf(stderr, "Error making mutex: %s\n", strerror(errno));
      exit(1);
    }
  }

  pthread_t threads[MAX_THREADS];

  char *filePath = malloc(2 * sizeof(char[256]));
  strcpy(filePath, root);
  strcat(filePath, file);

  struct ThreadArgs arg = {words.words, count, mutexes, &taskQueue, filePath};

  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_create(&threads[i], NULL, processText, (void *)&arg);
  }

  /* struct Msg msg = {file}; */
  /* if (mq_send(queueId, (const char *)&msg, sizeof(char *), 0) != 0) { */
  /*   puts("error sending queue"); */
  /* } */
  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  free(filePath);
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
    processFile(words, dir.root, dir.files[offset]);
    exit(0);
  default:
    readFiles(words, dir, offset + 1);
  }
}

int main(void) {
  mq_unlink(QUEUE_NAME);

  char *tempwords[] = {"test", "hi"};
  const int wordsToFindLength = 2;

  char **words = malloc(wordsToFindLength * sizeof(char *));
  for (int i = 0; i < wordsToFindLength; i++) {
    words[i] = tempwords[i];
  }

  struct WordsToFind wordsToFind = {words, wordsToFindLength};

  struct Directory dir = readDirectory("./test/");

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

  free(words);
  mq_close(queueId);
  mq_unlink(QUEUE_NAME);
  return 0;
}
