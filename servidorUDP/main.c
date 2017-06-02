// Librerias

#include <stdio.h>          
#include <string.h>         
#include <sys/types.h>    
#include <netinet/in.h>   
#include <sys/socket.h>   
#include <arpa/inet.h>    
#include <fcntl.h>        
#include <netdb.h>        
#include <time.h>         
#include <sys/shm.h>      
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

union semun 
{ 
	int val;
	struct semid_ds *buf;
	unsigned short int *array;
	struct seminfo *__buf;
};


// Defines
#define  PORT_NUM 2019 	// Numero de puerto usado
#define  IP_ADDR "127.0.0.1"	// IP del servidor1

// Acotamos los valores randoms para sensor 0 y los valores recibidos
#define clamp( x , m , M) ((x)<(m) ? (m) : (x)>(M) ? (M) : (x)) 

//Semaforo
#define ENTRAR(OP) ((OP).sem_op = -1)
#define SALIR(OP) ((OP).sem_op = +1)


// Estructura de datos del sensor
typedef struct  
{
	unsigned char ID; /* Identificación de sensor 0 – 255 */
	int temperatura; /* Valor de temperatura multiplicado por 10 */
	int RH; /* Valor de Humedad Relativa Ambiente */
                
} dato_t;


// Estructura interna del sensor
typedef struct  
{
	dato_t dato;
	time_t seconds;
} datos_t;


void main()
{
	// Creación de semáforo para la memoria compartida entre sensor 0 y los restantes por el servidor UDP
	key_t ClaveSem; //LLave para la utilizacion del semáforo
	int Id_Semaforo; //Id del semáforo
	struct sembuf Operacion;
	union semun arg;


	ClaveSem = ftok ("/bin/ls", 33); //Generamos desde un archivo la clave
	if (ClaveSem == (key_t)-1) //Error de obtencion de clave
	{
		printf("No puedo conseguir clave de semáforo\n");
		exit(0);
	}

	Id_Semaforo = semget (ClaveSem, 1, 0666 | IPC_CREAT); //Generación de ID del semaforo
	if (Id_Semaforo == -1) //Error de generación de ID del semáforo
	{
		printf("No puedo crear semáforo\n");
		exit (0);
	}


	arg.val =1;
	semctl (Id_Semaforo, 0, SETVAL, arg);

	Operacion.sem_num = 0;
	Operacion.sem_flg = 0;


	//
	// Memoria compartida
	//
	key_t key; //Variables para la Llave de la memoria compartida
	int idMemoria; // Identificador del lugar de la memoria
	datos_t *memoria; //bloque de memoria compartida

	// Creamos la llave a partir de un archivo existente. 
	printf("Creando Llave\n");
	key = ftok("/bin/bash", '5');

	// Creamos el identificador de memoria y le asignamos la cantidad a alocar y
	// los permisos necesarios de lectura/escritura/ejecucion
	printf("Creando Identificador...\n");
	idMemoria = shmget(key, sizeof(datos_t) * 129, 0777 | IPC_CREAT);

	// Agarramos el arreglo de memoria
	printf("Alocando Memoria Compartida...\n");
	memoria = shmat( idMemoria, NULL, 0 );

	memoria[128].seconds = 0; //Contador para localizar en que lugar de la memoria nos encontramos
	
	datos_t dato;


	// Creamos un proceso hijo del programa para atender al sensor con ID: 0	
	printf("Creando Sensor 0\n");
	pid_t pid = fork();
	
	//Sensor 0
	if( pid == 0 )
	{
		// Iniciamos la semilla del rand con el tiempo actual.
		srand(time(NULL)); 

		// Inicializacion de datos del Sensor 0
		dato.dato.ID = 0;
		dato.dato.temperatura = (rand() % 1955) - 550;
		dato.dato.RH = rand() % 101;
		dato.seconds = time(NULL);
	

		while(1)
		{	
			//Miro al semaforo
			ENTRAR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);

			dato.dato.temperatura = clamp(dato.dato.temperatura + ((rand() % 3) - 1) * (( rand() % 101 ) - 50), -550, 1399);
			dato.dato.RH = clamp(dato.dato.RH + ((rand() % 3) - 1) * (( rand() % 7 ) - 3), 0, 100);
			dato.seconds = time(NULL); //tiempo en el que registra el dato
			
			memoria[memoria[128].seconds++] = dato;
			//Para el ciclo circular del buffer
			if( memoria[128].seconds >= 128 )
			{
				memoria[128].seconds = 0;
			}
			//Semáforo da la opcion para que escriban los demas sensores
			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);

			sleep(1); //Espera 1 segundo 
		}
	}
	else // Programa principal
	{
		datos_t dato;

		unsigned int server_s;
		struct sockaddr_in server_addr;
		struct sockaddr_in client_addr;

		// Crea el socket
		//   - AF_INET is Address Family Internet and SOCK_DGRAM is datagram
		server_s = socket(AF_INET, SOCK_DGRAM, 0);

		// Fill-in my socket's address information
		server_addr.sin_family      = AF_INET;            // Address family to use
		server_addr.sin_port        = htons(PORT_NUM);    // Port number to use
		server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address
		bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr));

		// Fill-in client1 socket's address information
		client_addr.sin_family      = AF_INET;            // Address family to use
		client_addr.sin_port        = htons(PORT_NUM);    // Port num to use
		client_addr.sin_addr.s_addr = inet_addr(IP_ADDR); // IP address to use

		while(1)
		{	
			//Escuha petición de cliente
			recvfrom(server_s, &dato.dato, sizeof(dato_t), 0, &client_addr, sizeof( client_addr) );
			
			//Mira al semaforo
			ENTRAR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);
			
			//Recibe los datos del cliente
			dato.dato.temperatura = clamp(dato.dato.temperatura, -550, 1399);
			dato.dato.RH = clamp(dato.dato.RH, 0, 100);
			dato.seconds = time(NULL);
		
			//Impresión de prueba
			printf("Recibiendo: --> Id: %d Temp: %.1f Hum: %d\n", dato.dato.ID, dato.dato.temperatura/10.0f, dato.dato.RH);
			
			//Buffer circular
			memoria[memoria[128].seconds++] = dato;
			if( memoria[128].seconds >= 128 )
			{
				memoria[128].seconds = 0;
			}

			SALIR(Operacion);
			semop (Id_Semaforo, &Operacion, 1);

		}
		
		close( server_s );
	}
}
