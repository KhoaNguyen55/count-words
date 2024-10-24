#include <sys/wait.h>
#include <unistd.h>

#include "format.c"
#include "helper.c"
#include "wordmap.c"

#define CHILD_ID 0
#define MAX_THREADS 5
#define MAX_BYTES_PER_READ 1024

// each words and its associated count are implicit with the position
struct WordsToFind {
  char **words;
  int *count;
  pthread_mutex_t *mutexes;
  int length;
};

struct ThreadArgs {
  struct WordsToFind *words;
  char *filePath;
  const struct mq_attr *qAttr;
  char *qName;
  int terminate;
};

// thread function for processing the text
void *processText(void *args) {
  struct ThreadArgs *arg = args;
  FILE *file = fopen(arg->filePath, "r");

  mqd_t threadQueue =
      createMqQueue(arg->qName, O_RDONLY | O_NONBLOCK, arg->qAttr);

  char *word = malloc(100 * sizeof(char));

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
          findWordAndIncrement(arg->words, word);
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
void processFile(struct WordsToFind words, char *root, char *fileName) {
  // get the pid to create a inter thread communication
  pid_t pid = getpid();
  char *qName = malloc(sizeof(char[256]));
  // make sure the queue name don't collide with other threads
  snprintf(qName, 255, "/thread%i", pid);
  mq_unlink(qName);

  // create the queue
  const struct mq_attr tAttr = {0, words.length, sizeof(struct chunkSize), 0};
  mqd_t threadQueue = createMqQueue(qName, O_WRONLY, &tAttr);

  pthread_t threads[MAX_THREADS];

  // add in the arguments for the thread function
  char *filePath = malloc(2 * sizeof(char[256]));
  strcpy(filePath, root);
  strcat(filePath, fileName);

  struct ThreadArgs arg = {&words, filePath, &tAttr, qName, 0};

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
  const int pMsgSize = words.length * sizeof(int);
  const struct mq_attr pAttr = {0, 10, sizeof(pMsgSize), 0};
  mqd_t processQueue = createMqQueue("/process", O_WRONLY, &pAttr);

  // send the data
  if (mq_send(processQueue, (char *)words.count, pMsgSize, 0) != 0) {
    fprintf(stderr, "Error sending word count to process: %s\n",
            strerror(errno));
    exit(1);
  }

  // clean up
  free(filePath);
  mq_close(processQueue);
  mq_close(threadQueue);
  mq_unlink(qName);
  free(qName);
}

// read all the files and fork a new process for each file
void readFiles(struct WordsToFind words, struct Directory dir, int offset) {
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
    processFile(words, dir.root, dir.files[offset]);
    exit(0);
  default:
    // recursive loop
    readFiles(words, dir, offset + 1);
  }
}

int main(void) {
  // initalizing all the variables needed
  char *tempwords[] = {"void", "the", "a", "machine", "thing"};
  const int wordsToFindLength = 5;
  char directory[] = "./files/";

  mq_unlink("/process");
  char **words = malloc(wordsToFindLength * sizeof(char *));
  for (int i = 0; i < wordsToFindLength; i++) {
    words[i] = tempwords[i];
  }

  pthread_mutex_t mutexes[wordsToFindLength];
  int count[wordsToFindLength];
  for (int i = 0; i < wordsToFindLength; i++) {
    count[i] = 0;
    if (pthread_mutex_init(&mutexes[i], NULL) != 0) {
      fprintf(stderr, "Error making mutex: %s\n", strerror(errno));
      exit(1);
    }
  }

  struct WordsToFind wordsToFind = {words, count, mutexes, wordsToFindLength};

  // read the directory
  struct Directory dir = readDirectory(directory);

  // process the files
  readFiles(wordsToFind, dir, 0);

  // initialize data for receiving
  const int msgSize = wordsToFindLength * sizeof(int);
  const struct mq_attr pAttr = {0, 10, msgSize, 0};
  mqd_t queueId = createMqQueue("/process", O_RDONLY | O_NONBLOCK, &pAttr);

  int *wordsFound = malloc(msgSize);
  int deadProcesses = 0;
  while (1) {
    // get the words count
    if (mq_receive(queueId, (char *)wordsFound, msgSize, NULL) != -1) {
      for (int i = 0; i < wordsToFindLength; i++) {
        // add it to the total;
        wordsToFind.count[i] += wordsFound[i];
      }
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

  // print the words and it's counts
  char *outputStr = malloc(1000 * sizeof(char));
  formatStrArray(outputStr, wordsToFind.words, wordsToFind.length);
  char *outputInt = malloc(1000 * sizeof(char));
  formatIntArray(outputInt, wordsToFind.count, wordsToFind.length);
  printf("Words: %s \nFound: %s \n", outputStr, outputInt);

  // clean up
  free(outputInt);
  free(outputStr);
  free(wordsFound);
  free(words);
  mq_close(queueId);
  mq_unlink("/process");
  return 0;
}
