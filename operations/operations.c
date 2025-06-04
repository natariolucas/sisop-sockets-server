#include "operations.h"

#include <regex.h>
#include <stdlib.h>
#include <string.h>

//Sin decimales #define OPERATIONS_REGEX_RULE "^([+-]?[0-9]+)[ ]*([+*/-])[ ]*([+-]?[0-9]+)[\r\n]*$"
#define OPERATIONS_REGEX_RULE "^([+-]?[0-9]+\\.?[0-9]*)[ ]*([+*/-])[ ]*([+-]?[0-9]+\\.?[0-9]*)[\r\n]*$"


int compileRegexOperations(regex_t *regex) {
    return regcomp(regex, OPERATIONS_REGEX_RULE, REG_EXTENDED);
}

char* getStringFromMatchIndex(regmatch_t* matches, int index, char* source) {
    long long startOffset = matches[index].rm_so;
    int charCount = (int)(matches[index].rm_eo - startOffset);
    char* result = malloc(sizeof(char) * (charCount + 1)); // +1 for end of string
    strncpy(result, source + startOffset, charCount);
    result[charCount] = '\0';

    return result;
}

double* processOperationFromString(const char* operand1, const char operator, const char* operand2) {
    char* endPointerOfParseOperand1;
    const double op1 = strtod(operand1, &endPointerOfParseOperand1);

    char* endPointerOfParseOperand2;
    const double op2 = strtod(operand2, &endPointerOfParseOperand2);

    double* result = malloc(sizeof(double));

    switch (operator) {
        case '+':
            *result = op1 + op2;
            break;
        case '-':
            *result = op1 - op2;
            break;
        case '*':
            *result = op1 * op2;
            break;
        case '/':
            *result = op1 / op2;
            break;
        default:
            return NULL;
    }

    return result;
}

double getOperationResultFromRequest(regex_t *regex, char* request, int* regexecResult, char* operator) {
    regmatch_t matches[4];

    *regexecResult = regexec(regex, request, 4, matches, 0);
    if (*regexecResult != 0) {
        return 0;
    }

    char *operand1 = getStringFromMatchIndex(matches, 1, request);
    char *regexOperator = getStringFromMatchIndex(matches, 2, request);
    char *operand2 = getStringFromMatchIndex(matches, 3, request);

    double* resultOperation = processOperationFromString(operand1, *regexOperator, operand2);
    *operator = *regexOperator;

    free(operand1);
    free(regexOperator);
    free(operand2);

    if (resultOperation == NULL) {
        *regexecResult = -1;
        free(resultOperation);

        return 0;
    }

    double result = *resultOperation;
    free(resultOperation);

    return result;
}
