#include "BootstrapServer.h"

int main() 
{
    // Crea el servidor en el puerto 50000
    BootstrapServer server(50000);


    server.Run();

    return 0;

}