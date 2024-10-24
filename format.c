#include <stdio.h>

// function to format list of int to string
void formatIntArray(char *output, int *list, int length) {
  int len = 0;
  len += sprintf(output + len, "[");
  for (int i = 0; i < length; i++) {
    len += sprintf(output + len, "%10i, ", list[i]);
  }
  sprintf(output + len - 2, "]");
}

// function to format list of string to string
void formatStrArray(char *output, char **list, int length) {
  int len = 0;
  len += sprintf(output + len, "[");
  for (int i = 0; i < length; i++) {
    len += sprintf(output + len, "%10s, ", list[i]);
  }
  sprintf(output + len - 2, "]");
}
