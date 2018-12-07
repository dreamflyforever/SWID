#include "common.h"

static int DefaultCodeFrequency[] = { 15000, 15150, 15300, 15450, 15600, 15750, 15900, 16050, 16200, 16350, 16500, 16650, 16800, 16950, 17100, 17250, 17400, 17550, 17700 };

const char DEFAULT_CODE_BOOK[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
int *DEFAULT_CODE_FREQUENCY = (int *)DefaultCodeFrequency;

int DEFAULT_CODE_BOOK_SIZE = sizeof(DEFAULT_CODE_BOOK)/sizeof(char);
