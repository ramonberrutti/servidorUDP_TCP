#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/shm.h>      //Needed for shared memory
#include <unistd.h>
#include <time.h>

#include <sys/types.h>    // Needed for system defined identifiers.
#include <netinet/in.h>   // Needed for internet address structure.
#include <sys/socket.h>   // Needed for socket(), bind(), etc...
#include <arpa/inet.h>    // Needed for inet_ntoa()
#include <fcntl.h>        // Needed for sockets stuff
#include <netdb.h>        // Needed for sockets stuff

#include <pthread.h>

//#define  PORT_NUM 2016  // Port number used
#define	IP_ADDR "127.0.0.1" // IP address of server1 (*** HARDWIRED ***)
#define	IP_PORT 2016
#define  BACKLOG 10

#define MAX_THREADS 10
pthread_t threads[MAX_THREADS];


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

char *parser(char *buf, const char *delimiter)
{
	char *path  = NULL; //dirección de retorno

	if(strtok(buf, delimiter)) //agarro una cadena y limita
	{
		path = strtok(NULL, delimiter);
		if(path)
		{
			path = strdup(path); // Copia el String
		}
	}

	return(path);
}

void *thr_request(int *clientId)
{
	int client = *clientId;
	char strBuffer[3000];

	int cont = recv(client, strBuffer, sizeof(strBuffer), 0);
	strBuffer[cont] = '\0';
	//printf("Pidiendo: %s (%d)\n",strBuffer, cont);

	char *strRequest = parser(strBuffer, " ");	

	strcpy(strBuffer, "HTTP/1.1 200 OK\nConnection: Closed\nContent-Type: text/html\n\n"); //Envía Protocolo TCP 
	send(client, strBuffer, (strlen(strBuffer) + 1), 0);


	//printf("%s\n", strRequest );


	//
	char line[256];
	//char sensores
	char strTable[2048];
	//


	if( strRequest && strcmp(strRequest, "/") == 0 ) //pagina de inicio
	{
		datos_t sensores[256];
		FILE *file = fopen("datos.txt", "r"); //abre el archivo de los datos
		while (fgets(line, sizeof(line), file)) 
		{
			//printf("%s", line);
			//Armo cuadro			
			char *timestr = strtok(line, ",");
			if( !timestr ) continue; //si no hay mas cosas en la linea

			char *idstr = strtok(NULL, "," );
			if( !idstr ) continue;
	
			char *tempstr = strtok(NULL, ",");
			if( !tempstr ) continue;

			char *rhstr = strtok(NULL, "," );
			if( !rhstr ) continue;

			int id = atoi(idstr);

			sensores[id].dato.ID = id;
			sensores[id].seconds = strtoul(timestr, NULL, 10);
			sensores[id].dato.temperatura = atoi(tempstr);
			sensores[id].dato.RH = atoi(rhstr);

			//printf("%s %s\n", timestr, idstr );
		}

		sprintf(strTable, "<table border=1 style=width:100%%><tr><th>ID</th><th>Temp</th><th>RH</th><th>Time</th></tr>");
		for( int i = 0; i < 256; i++ )
		{
			if( sensores[i].seconds != 0 )
			{
				char timBuff[26];
				struct tm* tm_info;
				tm_info = localtime(&sensores[i].seconds);
				strftime(timBuff, 26, "%Y-%m-%d %H:%M:%S", tm_info);

				sprintf(strTable, "%s<tr><td><a href=\"sensor/%i\">%i</a></td><td>%.1f</td><td>%i</td><td>%s</td></tr>", strTable, i, i, sensores[i].dato.temperatura / 10.0f, sensores[i].dato.RH, timBuff );
				//printf("Sensor: %i %lu\n", i, sensores[i].seconds );
			}
		}
		sprintf(strTable, "%s</table>", strTable);

		sprintf(strBuffer, "<html><head><title>Home</title></head><body>%s<div><a href=\"/\">Home</a> <a href=\"/prom\">Prom 1Hora</a> <a href=\"/prom/6\">Prom 6Horas</a> <a href=\"/prom/12\">Prom 12Horas</a> <a href=\"/prom/24\">Prom 24Hora</a></div></body></html>", strTable);
		send(client, strBuffer, (strlen(strBuffer) + 1), 0);
	}
	else if( strRequest )
	{
		//char *path = parser(strRequest, "/" );

		char *opc = strtok(strRequest, "/" );

		printf("%s\n", opc );

		if( strcmp(opc, "sensor") == 0 )
		{
			char *path = strtok(NULL, "/" );

			int sensorId = 0;
			if( path )
				sensorId = atoi(path);

			int tempMax = -550;
			int tempMin = 1399;
			int tempAct;
			int rhMax = 0;
			int rhMin = 100;
			int rhAct;

			struct
			{
				int cant;
				long long sumTemp;
				long long sumRH;
			} prom[4];

			for( int j = 0; j < 4 ; j++ )
			{
				prom[j].cant = 0;
				prom[j].sumTemp = 0;
				prom[j].sumRH = 0;
			}
		
			//printf("%s\n", path);

			size_t horaactual = time(NULL);

			FILE *file = fopen("datos.txt", "r");
			while (fgets(line, sizeof(line), file)) 
			{
				//printf("%s", line);
				char *timestr = strtok(line, ",");
				if( !timestr ) continue;

				char *idstr = strtok(NULL, "," );
				if( !idstr ) continue;
		
				char *tempstr = strtok(NULL, ",");
				if( !tempstr ) continue;
	
				char *rhstr = strtok(NULL, "," );
				if( !rhstr ) continue;

				int id = atoi(idstr);
				size_t hora = strtoul(timestr, NULL, 10);
				int temp = atoi(tempstr);
				int rh = atoi(rhstr);

				if( id == sensorId )
				{
					tempAct = temp;
					rhAct = rh;

					if( temp > tempMax )
					{
						tempMax = temp;
					}
					if( temp < tempMin )
					{
						tempMin = temp;
					}

					if( rh > rhMax )
					{
						rhMax = rh;
					}
					if( rh < rhMin )
					{
						rhMin = rh;
					}

					if( (horaactual - hora) <= 3600 )
					{
						prom[0].cant++;
						prom[0].sumTemp += temp;
						prom[0].sumRH += rh;
					}

					if( (horaactual - hora) <= 21600 )
					{
						prom[1].cant++;
						prom[1].sumTemp += temp;
						prom[1].sumRH += rh;
					}

					if( (horaactual - hora) <= 43200 )
					{
						prom[2].cant++;
						prom[2].sumTemp += temp;
						prom[2].sumRH += rh;
					}

					if( (horaactual - hora) <= 86400 )
					{
						prom[3].cant++;
						prom[3].sumTemp += temp;
						prom[3].sumRH += rh;
					}
				}
			}
			fclose(file);

			sprintf(strTable, "<table border=1 style=width:100%%><tr><th>Dato:</th><th>Temp</th><th>RH</th></tr>");
			sprintf(strTable, "%s<tr><td>Actual</td><td>%.1f</td><td>%i</td></tr>", strTable, tempAct/10.0f, rhAct );
			sprintf(strTable, "%s<tr><td>Minima</td><td>%.1f</td><td>%i</td></tr>", strTable, tempMin/10.0f, rhMin );
			sprintf(strTable, "%s<tr><td>Maxima</td><td>%.1f</td><td>%i</td></tr>", strTable, tempMax/10.0f, rhMax );

			sprintf(strTable, "%s<tr><td>Ultima Hora</td><td>%.1f</td><td>%lli</td></tr>", strTable, (prom[0].sumTemp/(float)prom[0].cant) /10.0f, prom[0].sumRH/prom[0].cant );
			sprintf(strTable, "%s<tr><td>Ultimas 6 Horas</td><td>%.1f</td><td>%lli</td></tr>", strTable, (prom[1].sumTemp/(float)prom[1].cant) /10.0f, prom[1].sumRH/prom[1].cant );
			sprintf(strTable, "%s<tr><td>Ultimas 12 Horas</td><td>%.1f</td><td>%lli</td></tr>", strTable, (prom[2].sumTemp/(float)prom[2].cant) /10.0f, prom[2].sumRH/prom[2].cant );
			sprintf(strTable, "%s<tr><td>Ultimas 24 Horas</td><td>%.1f</td><td>%lli</td></tr>", strTable, (prom[3].sumTemp/(float)prom[3].cant) /10.0f, prom[3].sumRH/prom[3].cant );


			sprintf(strTable, "%s</table>", strTable);
			sprintf(strBuffer, "<html><head><title>Datos Sensor: %i</title></head><body>%s<div><a href=\"/\">Home</a> <a href=\"/prom\">Prom 1Hora</a> <a href=\"/prom/6\">Prom 6Horas</a> <a href=\"/prom/12\">Prom 12Horas</a> <a href=\"/prom/24\">Prom 24Hora</a></div></body></html>", sensorId, strTable);
			send(client, strBuffer, (strlen(strBuffer) + 1), 0);
	
		}
		else if( strcmp(opc, "prom") == 0 )
		{
			char *path = strtok(NULL, "/" );
			int promHora = 1;
			
			if( path )
				promHora = atoi(path);
		
			promHora = promHora * 3600;
			size_t horaactual = time(NULL);

			struct
			{
				int cant;
				long long sumTemp;
				long long sumRH;
			} prom[256];
			for( int j = 0; j < 256 ; j++ )
			{
				prom[j].cant = 0;
				prom[j].sumTemp = 0;
				prom[j].sumRH = 0;
			}

			FILE *file = fopen("datos.txt", "r");
			while (fgets(line, sizeof(line), file)) 
			{
				//printf("%s", line);
				char *timestr = strtok(line, ",");
				if( !timestr ) continue;

				char *idstr = strtok(NULL, "," );
				if( !idstr ) continue;
		
				char *tempstr = strtok(NULL, ",");
				if( !tempstr ) continue;
	
				char *rhstr = strtok(NULL, "," );
				if( !rhstr ) continue;

				int id = atoi(idstr);
				size_t hora = strtoul(timestr, NULL, 10);
				int temp = atoi(tempstr);
				int rh = atoi(rhstr);

				if( (horaactual - hora) <= promHora )
				{
					prom[id].cant++;
					prom[id].sumTemp += temp;
					prom[id].sumRH += rh;
				}
			}
			fclose(file);

			sprintf(strTable, "<table border=1 style=width:100%%><tr><th>ID</th><th>Temp</th><th>RH</th></tr>");
			for( int i = 0; i < 256; i++ )
			{
				if( prom[i].cant != 0 )
				{

					sprintf(strTable, "%s<tr><td><a href=\"/sensor/%i\">%i</a></td><td>%.1f</td><td>%lli</td></tr>", strTable, i, i, prom[i].sumTemp/(float)prom[i].cant / 10.0f, prom[i].sumRH/prom[i].cant );
				}
			}
			sprintf(strTable, "%s</table>", strTable);

			sprintf(strBuffer, "<html><head><title>Promedios de %i Horas</title></head><body>%s<div><a href=\"/\">Home</a> <a href=\"/prom\">Prom 1Hora</a> <a href=\"/prom/6\">Prom 6Horas</a> <a href=\"/prom/12\">Prom 12Horas</a> <a href=\"/prom/24\">Prom 24Hora</a></div></body></html>", promHora/3600, strTable);
			send(client, strBuffer, (strlen(strBuffer) + 1), 0);

		}
		
	}

	if( strRequest )
	{
		strcpy(strBuffer, "\r\n\r\n"); 
		send(client, strBuffer, (strlen(strBuffer) + 1), 0);
	}

	free(strRequest); // Prevent Memory Leek!!
	close(client);
	return NULL;
}


void *thr_filewrite( void *nada)
{
	//
	// Memoria compartida
	//
	key_t key; // Variables para la Llave
	int idMemoria; // Identificador de la memoria
	datos_t *memoria; // bloque de memoria compartida

	// Creamos la llave a partir de un archivo existente. 
	printf("Creando Llave\n");
	key = ftok("/bin/bash", '5');

	// Creamos el identificador de memoria y le asignamos la cantidad a alocar y
	// los permisos necesarios de lectura/escritura/ejecucion -- Borramos el IPC_CREAT
	printf("Creando Identificador...\n");
	idMemoria = shmget(key, sizeof(datos_t) * 129, 0777 );

	// Alocamos la memoria
	printf("Alocando Memoria Compartida...\n");
	memoria = shmat( idMemoria, NULL, 0 );

	//Valores para saber si se completo el buffer
	time_t seconds1 = memoria[63].seconds;
	time_t seconds2 = memoria[127].seconds;

	while(1)
	{

		//Si es distinta al valor que tomamos significa que llenó la primer parte del buffer
		if( memoria[63].seconds != seconds1 ) 
		{
			FILE *file = fopen("datos.txt", "a"); //abro el archivo y arranco desde el final del archivo

			printf("Cambio de Buffer 2\n");
			//Escribo valores del buffer en el archivo
			for( int i = 0; i < 64; i++ )
			{
				fprintf(file, "%lu,%i,%d,%d\n", memoria[i].seconds, memoria[i].dato.ID, memoria[i].dato.temperatura, memoria[i].dato.RH); 

			}
			
			fclose(file); //Cierro para guardar en disco

			seconds1 = memoria[63].seconds; //Vuelvo a actualizar el valor 64 del buffer
		}
		//Hacemos lo mismo que para la primer parte del buffer
		else if( memoria[127].seconds != seconds2 )
		{
			FILE *file = fopen("datos.txt", "a");
			printf("Cambio de Buffer 1\n");

			for( int i = 64; i < 128; i++ )
			{
				fprintf(file, "%lu,%i,%d,%d\n", memoria[i].seconds, memoria[i].dato.ID, memoria[i].dato.temperatura, memoria[i].dato.RH);
			}

			fclose(file);
			
			seconds2 = memoria[127].seconds;
		}

		sleep( 1 );
	}
}



void main()
{
	int i = 0;
	unsigned int         server_s;        // Descriptor de socket
	struct sockaddr_in   server_addr;     // Direccion del servidor
	struct sockaddr_in   client_addr;     // Direccion del cliente

	server_s = socket(AF_INET, SOCK_STREAM, 0); //crea el socket

	server_addr.sin_family      = AF_INET;            // Address family to use
	server_addr.sin_port        = htons(IP_PORT);    // Port number to use
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);  // Listen on any IP address
	memset(&(server_addr.sin_zero),'\0',8);

	bind(server_s, (struct sockaddr *)&server_addr, sizeof(server_addr)); //Abrir puerto disponible

	listen(server_s,BACKLOG); //Escucho el puerto
	int sin_size = sizeof(struct sockaddr_in);

	pthread_t asd;
	pthread_create(&asd, NULL, (void *)&thr_filewrite,(void *)0); //Crea el hilo para guardar datos a un archivo

	while(1)
	{
		printf("Servidor esperando conexion\n\n");
		int clientId = accept(server_s,(struct sockaddr *)&client_addr, &sin_size ); //Nueva conexion

		printf("%d New Request...\n", i);
		pthread_create(&threads[i], NULL, (void *)&thr_request,(void *)&clientId); //crea hilo que maneja la conexion
		i++;

		if( i >= MAX_THREADS ) //seguridad
		{
			i = 0;
		}

	}

	close(server_s);
}
