#ifndef OPERATIONS_H
#define OPERATIONS_H
// #include <_regex.h>
#include <regex.h>


int compileRegexOperations(regex_t *regex);
double getOperationResultFromRequest(regex_t *regex, char* request, int* regexecResult, char* operator);

#endif //OPERATIONS_H
