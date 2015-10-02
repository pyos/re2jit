#include "re2jit.h"
#include <string>

typedef bool (*TestFunction)();
typedef struct { std::string name; TestFunction fn; } TestDescription;
#define Tests static TestDescription __Tests[]
