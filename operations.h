#ifndef OPERATIONS_H
#define OPERATIONS_H
#include <_regex.h>

int compileRegexOperations(regex_t *regex);
double getOperationResultFromRequest(regex_t *regex, char* request, int* regexecResult);

#endif //OPERATIONS_H
