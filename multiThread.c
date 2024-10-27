#include <errno.h>
#include <mqueue.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef MULTITHREAD
#define MULTITHREAD

struct ThreadArgs {
  struct wordMap *map;
  char *filePath;
  const struct mq_attr *qAttr;
  char *qName;
  int terminate;
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

#endif /* ifndef MULTITHREAD */
