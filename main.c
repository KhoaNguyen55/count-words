#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "handleFiles.c"
#include "handleText.c"

#define CHILD_ID 0

int main(void) {
  // initalizing all the variables needed
  char directory[] = "../files/";

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

  wordmapOutputCsv(map);

  // clean up
  hashmap_free(map->map);
  free(map);
  mq_close(queueId);
  mq_unlink("/process");
  return 0;
}
