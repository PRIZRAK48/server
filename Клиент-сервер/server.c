#include <stdio.h>
#include <string.h>	//strlen
#include <sys/socket.h>
#include <arpa/inet.h>	//inet_addr
#include <unistd.h>	//write
#include <pthread.h>
#include <stdlib.h>    //strlen
#include <pthread.h> //for threading , link with lpthread
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h> 

#define N 3

pthread_t tid[N];
int is_working[N];
int client_sockets[N];

char current_dir[N][5000];

void* connection_handler(void* number)
{
	int thread_num = *(int*)number;
	int read_size;
	char client_message[2000];

	getcwd(current_dir[N], 5000);

	printf("[%d] created.\n", thread_num);

	while(!is_working[thread_num]);

	while( (read_size = recv(client_sockets[thread_num] , client_message , 2000 , 0)) > 0 )
	{
		char* command = strtok(client_message, " ");
		printf("[%d] Command: [%s]\n", thread_num, command);
		if (strcmp("LIST", command) == 0)
		{
			char response[10000];

			DIR           *d;
			struct dirent *dir;
			d = opendir(current_dir[thread_num]);
			if (d)
			{
				strcpy(response, "");
				while ((dir = readdir(d)) != NULL)
				{
					//printf("%s\n", dir->d_name);
					strcat(response, dir->d_name);
					strcat(response, "\n");
				}

				closedir(d);
			}

			write(client_sockets[thread_num] , response , strlen(response));
		}
		else if (strcmp("CWD", command) == 0)
		{
			char* path = strtok(NULL, " ");
			printf("[%d] Path: [%s]\n", thread_num, path);			
			//printf("[%d] Changing to: [%s]\n", thread_num, current_dir[N]);

			char response[1000];

			struct stat s;
			int err = stat(path, &s);

			if (err != -1 && S_ISDIR(s.st_mode)) // is a dir
			{
				strcpy(current_dir[thread_num], path);
				sprintf(response, "Ok, current_path = %s\n", current_dir[thread_num]);
			}
			else
			{
				sprintf(response, "Wrong path, sorry.\n");
			}

			//sprintf(response, "Ok, current_path = %s\n", current_dir[N]);
			write(client_sockets[thread_num] , response , strlen(response));
		}

		memset(client_message,0,sizeof(client_message));

		
	}

	close(client_sockets[thread_num]);

	printf("[%d] Connection closed.\n", thread_num);

	pthread_create(&tid[thread_num] , NULL , connection_handler , (void*) &thread_num);
	is_working[thread_num] = 0;

	printf("[%d] destroyed.\n", thread_num);
}


int main(int argc , char *argv[])
{
	for (int i = 0; i < N; i++)
	{
		pthread_create( &tid[i] , NULL , connection_handler , (void*) &i);
		is_working[i] = 0;
	}

	int socket_desc , client_sock , c , read_size;
	struct sockaddr_in server , client;
	char client_message[2000];

	//Create socket
	socket_desc = socket(AF_INET , SOCK_STREAM , 0);
	if (socket_desc == -1)
	{
		printf("Could not create socket");
	}
	puts("Socket created");

	//Prepare the sockaddr_in structure
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(8888);

	//Bind
	if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
	{
		//print the error message
		perror("bind failed. Error");
		return 1;
	}
	puts("bind done");

	//Listen
	listen(socket_desc , 3);

	//Accept and incoming connection
	puts("Waiting for incoming connections...");
	c = sizeof(struct sockaddr_in);

	//accept connection from an incoming client
	while(1)
	{
		int client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
		int passed = 0;

		for(int i = 0; i < N; i++)
			if(!is_working[i])
			{				
				client_sockets[i] = client_sock;
				is_working[i] = 1;
				passed = 1;
				printf("Connection passed to thread %d.\n", i);
				break;
			}

		if (!passed)
		{
			printf("No free threads, closing.\n");
			close(client_sock); // no free threads, closing
		}
	}
	return 0;
}