#include <dirent.h>
#include <errno.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef HANDLEFILES
#define HANDELFILES

#include "handleText.c"
#include "multiThread.c"
#include "wordmap.c"

#ifndef SINGLETHREAD
#define MAX_THREADS 5
#else
#define MAX_THREADS 1
#endif
#define MAX_BYTES_PER_READ 1024
#define INITIAL_DIR_FILES_SIZE 5
#define CHILD_ID 0

struct Directory {
  char *root;
  char **files;
  int amount;
};

// function to read files inside a directory
struct Directory readDirectory(char *directory) {
  struct Directory dir;
  dir.root = directory;
  dir.files = malloc(sizeof(*dir.files) * INITIAL_DIR_FILES_SIZE);
  dir.amount = 0;

  // open the folder
  DIR *dirStream = opendir(directory);
  if (dirStream == NULL) {
    fprintf(stderr, "Error open directory: %s\n", strerror(errno));
    exit(1);
  }

  // loop through the files
  struct dirent *file = readdir(dirStream);
  int current_size = 1;
  while (file != NULL) {
    // ignore hidden files
    if (file->d_name[0] != '.') {
      // add the file name to the array of files
      dir.files[dir.amount] = malloc(sizeof(char[256]) + 1);
      if (dir.files[dir.amount] == NULL) {
        fprintf(stderr, "Error malloc dir files: %s\n", strerror(errno));
      }
      strcpy(dir.files[dir.amount], file->d_name);
      dir.amount++;
      if (dir.amount >= current_size) {
        current_size *= 2;
        char **temp = reallocarray(dir.files, sizeof(*dir.files), current_size);
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

// process each file by creating threads for it
void processFile(char *root, char *fileName) {
  struct wordMap *map = wordmapCreate();

  // get the pid to create a inter thread communication
  pid_t pid = getpid();
  char *qName = malloc(sizeof(char[256]));
  // make sure the queue name don't collide with other threads
  snprintf(qName, 255, "/thread%i", pid);
  mq_unlink(qName);

  // create the queue
  const struct mq_attr tAttr = {0, 10, sizeof(struct chunkSize), 0};
  mqd_t threadQueue = createMqQueue(qName, O_WRONLY, &tAttr);

  pthread_t threads[MAX_THREADS];

  // add in the arguments for the thread function
  char *filePath = malloc(sizeof(char[256 * 2]));
  snprintf(filePath, sizeof(char[256 * 2]), "%s%s", root, fileName);

  struct ThreadArgs arg = {map, filePath, &tAttr, qName, 0};

  // create the threads
  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_create(&threads[i], NULL, processText, (void *)&arg);
  }

  FILE *file = fopen(filePath, "r");
  struct chunkSize tMsg;
  do {
    // get the next chunk and put it in the queue
    tMsg = getNextChunkPosition(file, MAX_BYTES_PER_READ);
    if (mq_send(threadQueue, (char *)&tMsg, sizeof(tMsg), 0) != 0) {
      fprintf(stderr, "Error sending queue to thread: %s\n", strerror(errno));
      exit(1);
    }
  } while (errno != EOF);

  // turn on flag to terminate all the thread when its done
  arg.terminate = 1;

  // join to make sure its all done
  for (int i = 0; i < MAX_THREADS; i++) {
    pthread_join(threads[i], NULL);
  }

  // create queue to send all the data back to the main process
  const struct mq_attr pAttr = {0, 10, sizeof(struct wordElement), 0};
  mqd_t processQueue = createMqQueue("/process", O_WRONLY, &pAttr);
  // send all the data over to the main thread
  /* wordmapPrintAllElements(map); */
  wordmapSendAllElements(map, processQueue);

  // clean up
  free(filePath);
  mq_close(processQueue);
  mq_close(threadQueue);
  mq_unlink(qName);
  free(qName);
}

// read all the files and fork a new process for each file
void readFiles(struct Directory dir, int offset) {
  if (dir.amount <= offset) {
    // clean up since we don't need it anymore
    free(dir.files);
    return;
  }

  // fork a new process and process each files
  int pid = fork();
  if (pid == -1) {
    fprintf(stderr, "Error forking directory: %s\n", strerror(errno));
    exit(1);
  } else if (pid == CHILD_ID) {
    char *fileName = malloc(sizeof(char[256]));
    strncpy(fileName, dir.files[offset], sizeof(char[256]));
    processFile(dir.root, fileName);
    free(fileName);
    exit(0);
  } else {
    readFiles(dir, offset + 1);
  }
}

#endif /* ifndef HANDLEFILES */
