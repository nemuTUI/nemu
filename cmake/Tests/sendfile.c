#include <sys/sendfile.h>

int main()
{
    sendfile(0, 0, 0, 0);
    return 0;
}
