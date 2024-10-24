#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "helper.c"
#include "wordmap.c"

#define CHILD_ID 0
#define MAX_THREADS 5
#define MAX_BYTES_PER_READ 1024

struct ThreadArgs {
  struct wordMap *map;
  char *filePath;
  const struct mq_attr *qAttr;
  char *qName;
  int terminate;
};

// thread function for processing the text
void *processText(void *args) {
  // initialized datas needed
  struct ThreadArgs *arg = args;
  FILE *file = fopen(arg->filePath, "r");

  mqd_t threadQueue =
      createMqQueue(arg->qName, O_RDONLY | O_NONBLOCK, arg->qAttr);

  char *word = malloc(sizeof(char[100]));

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
        int size = getWord(file, word, 100);
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
  char *filePath = malloc(2 * sizeof(char[256]));
  strcpy(filePath, root);
  strcat(filePath, fileName);

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
  switch (fork()) {
  case -1:
    fprintf(stderr, "Error forking directory: %s\n", strerror(errno));
    exit(1);
  case CHILD_ID:
    processFile(dir.root, dir.files[offset]);
    exit(0);
  default:
    // recursive loop
    readFiles(dir, offset + 1);
  }
}

int main(void) {
  // initalizing all the variables needed
  char directory[] = "./files/";

  mq_unlink("/process");

  // read the directory
  struct Directory dir = readDirectory(directory);

  // process the files
  readFiles(dir, 0);

  // initialize data for receiving
  const int msgSize = sizeof(struct wordElement);
  const struct mq_attr pAttr = {0, 10, msgSize, 0};
  mqd_t queueId = createMqQueue("/process", O_RDONLY | O_NONBLOCK, &pAttr);

  // recreate the wordmap from this side
  struct wordMap *map = wordmapCreate();
  struct wordElement wordCount;
  int deadProcesses = 0;
  while (1) {
    // get the words count
    if (mq_receive(queueId, (char *)&wordCount, msgSize, NULL) != -1) {
      // add it to the total
      wordmapAdd(map, &wordCount);
    } else if (errno == EAGAIN && waitpid(-1, NULL, WNOHANG) != 0) {
      deadProcesses++;
    } else if (errno != EAGAIN) {
      printf("Error process mq_receive: %s\n", strerror(errno));
    }

    // check to make sure all process are killed
    if (deadProcesses == dir.amount) {
      break;
    }
  }

  wordmapPrintAllElements(map);

  // clean up
  hashmap_free(map->map);
  free(map);
  mq_close(queueId);
  mq_unlink("/process");
  return 0;
}
