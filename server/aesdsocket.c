#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <error.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <syslog.h>
#include <signal.h>

#define BUFFER_SIZE 1024  
#define SERVER_PORT "9000"

inline static bool isValidSocket(const int fd)
{
    return (fd >= 0);
}

static bool signal_termination_rcvd = false; 

static void signal_handler( int signal_number )
{
    int errno_backup = errno;
    if( signal_number == SIGINT ||  signal_number == SIGTERM )
    {
        signal_termination_rcvd = true;
    }
    errno = errno_backup;
}


int main(int argc, char* argv[] )
{
    const char *app_name;
    if(argc > 0 )
    {
        app_name = argv[0] + 2;
    }
    openlog(app_name, LOG_PID | LOG_CONS, LOG_USER );    

    struct sigaction termination_action;
    bool is_handler_registered = true;
    memset(&termination_action,0,sizeof(struct sigaction));
    termination_action.sa_handler = signal_handler;

    if( sigaction(SIGTERM, &termination_action, NULL))
    {
        fprintf( stderr, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
        is_handler_registered = false;
    }

    if(sigaction(SIGINT, &termination_action, NULL))
    {
        fprintf(stderr,"Error %d (%s)", errno, strerror(errno));
        is_handler_registered = false;
    }
    if(is_handler_registered == false)
    {
        fprintf(stderr, "could not register signal handler\n");
        return -1;
    }
    const char* const output_file_name = "/var/tmp/aesdsocketdata.txt";

    struct addrinfo addr_hints;
    memset(&addr_hints, 0, sizeof(addr_hints));

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(NULL,  SERVER_PORT, &addr_hints, &bind_address);

    int socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);

    if(!isValidSocket(socket_listen))
    {
        fprintf(stderr, "socket() failled  errno :  (%d)\n", errno);
        return -1;
    }
    int reuse = -1;
    if(setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1 )
    {
        fprintf(stderr, "setsockopt() failled errno : (%d)\n", errno);
        return -1;
    }

    if(bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
    {
        fprintf(stderr, "bind() failled. errno (%d)\n",errno);
        return -1;
    }

    freeaddrinfo(bind_address);
    if( listen(socket_listen, 1) < 0  )
    {
        fprintf(stderr, "listen() failled . errno : (%d)\n",errno);
        return -1;
    }
    FILE *file_ptr = fopen(output_file_name, "a");
    if( file_ptr == NULL )
    {
        fprintf(stderr, "fopen(), could not open the output file : %s\n", output_file_name);
        return -1;
    }
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    char buffer[BUFFER_SIZE];
    
    while (1 == 1)
    {
        int socket_client = accept(socket_listen, (struct sockaddr *)&client_address, &client_len);

        if (signal_termination_rcvd == true)
        {
            syslog(LOG_INFO, "Caught signal, exiting");
            if(isValidSocket(socket_client)) close(socket_client);       
            fclose(file_ptr);
            int result = remove(output_file_name);
            if (result != 0)
            {
                fprintf(stderr, "Error removing the file : %s", output_file_name);
            }
            return 0;
        }

        if (!isValidSocket(socket_client))
        {
            fprintf(stderr, "accept() failled, errno (%d)\n", errno);
            fclose(file_ptr);
            return -1;
        }
        // getting the client's IP address
        char address_buffer[256];
        getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
        syslog(LOG_INFO, "Accepted connection from %s", address_buffer); // replace this with syslog
        memset(buffer,'\0',BUFFER_SIZE);
        int rcvd_bytes_counter = 0;
        bool is_new_line_char_found = false;
        //receive data
        do
        {
            int bytes_rcvd = recv(socket_client, buffer+rcvd_bytes_counter, BUFFER_SIZE-rcvd_bytes_counter, 0);

            if(bytes_rcvd <= 0 )
            {
                if(signal_termination_rcvd == true)
                {
                    syslog(LOG_INFO, "Caught signal, exiting");
                     close(socket_client);
                     fclose(file_ptr);
                     int result = remove(output_file_name);
                     if(result != 0 )
                     {
                        fprintf(stderr, "Error removing the file : %s", output_file_name);
                     }
                     return 0;
                }
                else
                {
                    //remote client disconnected, close the client socket
                     close(socket_client);
                     break;
                }         
            }
            is_new_line_char_found = strchr(buffer+rcvd_bytes_counter,'\n')  != NULL || strchr(buffer+rcvd_bytes_counter,'\r')  != NULL;
             rcvd_bytes_counter += bytes_rcvd;
        } while (  is_new_line_char_found == false && rcvd_bytes_counter < BUFFER_SIZE );
        //discarding over-length packages
        if(rcvd_bytes_counter >= BUFFER_SIZE)
        {
            memset(buffer, '\0', BUFFER_SIZE);
        }
        //write data to file
        fputs(buffer, file_ptr);
        fflush(file_ptr);
        //printf("data recieved from %s : \n %s ", address_buffer, buffer);
        //send data back all the file content to sender
        FILE *in_file_ptr = fopen(output_file_name, "r");
        if(in_file_ptr == NULL)
        {
            fprintf(stderr,"fopen() could not open input file : %s\n",output_file_name);
            close(socket_client);
            fclose(file_ptr);
            return -1;
        }
        char line_buffer[BUFFER_SIZE];
        while (fgets(line_buffer, sizeof(line_buffer), in_file_ptr) != NULL)
        {
            int bytes_sent = send(socket_client, line_buffer, strlen(line_buffer), 0);
            if (bytes_sent < 0 || bytes_sent != (int) strlen(line_buffer))
            {
                fprintf(stderr, "send() could not send data to client, errno (%d)\n", errno);
                close(socket_client);
                fclose(file_ptr);
                return -1;
            }
        }
        fclose(in_file_ptr);
        close(socket_client);
        syslog(LOG_INFO, "Closed connection from %s", address_buffer);
        
    }

    fclose(file_ptr);
    closelog();
    return 0;
}