#include <stdio.h>          // Needed for printf()
#include <stdlib.h>  	  // rand(), srand()
#include <time.h>   	 // time()
#include <string.h>         // Needed for memcpy() and strcpy()
#include <sys/types.h>    // Needed for system defined identifiers.
#include <netinet/in.h>   // Needed for internet address structure.
#include <sys/socket.h>   // Needed for socket(), bind(), etc...
#include <arpa/inet.h>    // Needed for inet_ntoa()
#include <fcntl.h>        // Needed for sockets stuff
#include <netdb.h>        // Needed for sockets stuff
#include <unistd.h>       // Needed for sleep()


typedef struct  
{
	unsigned char ID; /* Identificación de sensor 0 – 255 */
	int temperatura; /* Valor de temperatura multiplicado por 10 */
	int RH; /* Valor de Humedad Relativa Ambiente */
                
} dato_t;


// Defines
#define  PORT_NUM 2019 	// Numero de puerto usado
#define  IP_ADDR "127.0.0.1"	// IP del servidor1

// Acotamos los valores randoms
#define clamp( x , m , M) ((x)<(m) ? (m) : (x)>(M) ? (M) : (x)) 

void main( int argc, char *argv[] )
{
	// chequeamos los parametros
	if( argc != 2 )
	{
		printf("Use: %s <id>\n", argv[0] );
		return;
	}

	// Semilla de valores aleatorios
	srand(time(NULL) + atoi(argv[1]));

	unsigned int client_s;
	struct sockaddr_in server_addr;

  	dato_t dato;

	client_s = socket( AF_INET, SOCK_DGRAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(PORT_NUM);
	server_addr.sin_addr.s_addr = inet_addr(IP_ADDR);

	dato.ID = (char)atoi(argv[1]); //ID del sensor
	dato.temperatura = (rand() % 1955) - 550; // Valor aleatoreo
	dato.RH = rand() % 101;
	

	// Genera valores aleatorios simulando los sensores
	while(1)
	{	
		//Usamos el clamp para limitar los valores y la rand para mover esos valores
		dato.temperatura = clamp(dato.temperatura + ((rand() % 3) - 1) * (( rand() % 101 ) - 50), -550, 1399);
		dato.RH = clamp(dato.RH + ((rand() % 3) - 1) * (( rand() % 7 ) - 3), 0, 100);
		
		//Imprime cuando manda informacion del sensor
		printf("ID: %i Enviando--> Temp: %.1f Hum: %d\n", dato.ID, dato.temperatura/10.0f, dato.RH);

		//Envía los datos al servidor
		sendto(client_s, &dato, sizeof(dato_t) , 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
		sleep(1);	
	}

	close(client_s);

}


