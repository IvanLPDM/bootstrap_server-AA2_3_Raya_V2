#include "BootstrapServer.h"

int main() 
{
    BootstrapServer server(50000);

    server.Run();

    return 0;

}