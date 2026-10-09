#include "logger.h"

int main()
{
    ILOG("this is an info log");
    ELOG("this is an error log, code=%d", 404);
    DLOG("this is a debug log"); // 默认 LDEFAULT=LINF，DLOG 不会输出
    return 0;
}