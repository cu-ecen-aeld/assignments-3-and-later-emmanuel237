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
#include <stdlib.h>
#include <malloc.h>
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define BUF_CHUNCK_SIZE 1024
#define SERVER_PORT "9000"
#define CLIENT_NAME_SIZE 100

pthread_mutex_t file_write_mutex;
FILE *output_file_ptr;

struct thread_param_t
{
    int socket_fd;
    FILE *file_ptr;
    const char* output_file_name;
    char client_name[CLIENT_NAME_SIZE];
    pthread_mutex_t file_write_mutex;
};

struct node
{
    pthread_t thread;
    TAILQ_ENTRY(node) nodes;
};

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

void *tcpConnectionHandler(void *thread_args)
{
    struct thread_param_t *thread_data = (struct thread_param_t *)thread_args;

    int rcvd_bytes_counter = 0;
    bool is_new_line_char_found = false;
     char *buffer = NULL;
    int nber_memory_chunks = 1;
    buffer = (char *)malloc(BUF_CHUNCK_SIZE * sizeof(char));
    memset(buffer, '\0', BUF_CHUNCK_SIZE);
    
    // receive data
    do
    {
        int bytes_rcvd = recv(thread_data->socket_fd, buffer + rcvd_bytes_counter, (BUF_CHUNCK_SIZE * nber_memory_chunks) - rcvd_bytes_counter, 0);

        if (bytes_rcvd <= 0)
        {
            // remote client disconnected
            syslog(LOG_INFO, "Closed connection from %s", thread_data->client_name);
            close(thread_data->socket_fd);
            free(buffer);
            buffer = NULL;
            free(thread_data);
            pthread_exit(NULL);
        }

        rcvd_bytes_counter = rcvd_bytes_counter + bytes_rcvd;
        if (buffer[rcvd_bytes_counter - 1] == '\n' || buffer[rcvd_bytes_counter - 1] == '\r')
        {
            is_new_line_char_found = true;
        }
        if ( ( rcvd_bytes_counter == (BUF_CHUNCK_SIZE * nber_memory_chunks)) && ( is_new_line_char_found == false ) ) // extend the allocated memory
        {
            char *temp_ptr = (char *)realloc(buffer, ++nber_memory_chunks * BUF_CHUNCK_SIZE);
            if (temp_ptr == NULL) // ran out of memory on the heap, the incomming message should be discarded
            {
                free(buffer);
                buffer = NULL;
                break;
            }
            else
            {
                buffer = temp_ptr;
                memset(buffer + (nber_memory_chunks - 1) * BUF_CHUNCK_SIZE, '\0', BUF_CHUNCK_SIZE);
            }
        }

    } while (is_new_line_char_found == false);

    // write data to file
    if (buffer != NULL)
    {
        pthread_mutex_lock(&thread_data->file_write_mutex);
        fputs(buffer, thread_data->file_ptr);
        fflush(thread_data->file_ptr);
        pthread_mutex_unlock(&thread_data->file_write_mutex);

        free(buffer);
        buffer = NULL;
    }
    // send data back all the file content to sender
    FILE *in_file_ptr = fopen(thread_data->output_file_name, "r");
    if (in_file_ptr == NULL)
    {
        fprintf(stderr, "fopen() could not open input file : %s\n", thread_data->output_file_name);
        close(thread_data->socket_fd);
        //fclose(thread_data->file_ptr); 
        free(thread_data);
        pthread_exit(NULL);
    }
    char *text_buffer = (char *)malloc(BUF_CHUNCK_SIZE * sizeof(char));
    memset(text_buffer, '\0', BUF_CHUNCK_SIZE);
    int ch='\0';
    int char_counter = 0;
    do
    {
        ch = fgetc(in_file_ptr);
        if( ch != EOF && char_counter < BUF_CHUNCK_SIZE )
        {
            text_buffer[char_counter++] = ch;
        }
        
        if ( ( ch == EOF && strlen(text_buffer) > 0 ) || (strlen(text_buffer) == BUF_CHUNCK_SIZE) )
        {
            int bytes_sent = send( thread_data->socket_fd, text_buffer, strlen(text_buffer), 0);
            if (bytes_sent < 0 || bytes_sent != (int)strlen(text_buffer))
            {
                fprintf(stderr, "send() could not send data to client, errno (%d)\n", errno);
                close(thread_data->socket_fd);
                //fclose( thread_data->file_ptr);
                free(text_buffer);
                free(thread_data);
                pthread_exit(NULL);
            }
        }
        
         if( char_counter == BUF_CHUNCK_SIZE)
        {
            memset(text_buffer, '\0', BUF_CHUNCK_SIZE);
            char_counter = 0;
            text_buffer[char_counter++] = ch;
        }
    }while(ch != EOF);

    free(text_buffer);
    fclose(in_file_ptr);
    close(thread_data->socket_fd);
    syslog(LOG_INFO, "Closed connection from %s", thread_data->client_name);
    free(thread_data);
    pthread_exit(NULL);
}

void *timerHandler(void *thread_args)
{
    struct thread_param_t *thread_data = (struct thread_param_t *)thread_args;

    while (1 == 1)
    {
        time_t rawtime;
        struct tm *timeinfo;
        char buffer[80];

        time(&rawtime);
        timeinfo = localtime(&rawtime);

        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z", timeinfo);

        pthread_mutex_lock(&thread_data->file_write_mutex);
        fputs("timestamp:", thread_data->file_ptr);
        fputs(buffer, thread_data->file_ptr);
        fputs("\n", thread_data->file_ptr);
        fflush(thread_data->file_ptr);
        pthread_mutex_unlock(&thread_data->file_write_mutex);

        sleep(10); // Sleep for 10 seconds

        if(signal_termination_rcvd == true )
        {
            pthread_exit(NULL);
        }
    }
}


static timer_t timer_id; // Global variable to store the timer ID

void print_time(int signo) {
  if (signo == SIGALRM) {
    time_t rawtime;
    struct tm *timeinfo;
    char buffer[80];

    time(&rawtime);
    timeinfo = localtime(&rawtime);

    strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S %z", timeinfo);

    pthread_mutex_lock(&file_write_mutex);
    fputs("timestamp:", output_file_ptr);
    fputs(buffer, output_file_ptr);
    fputs("\n", output_file_ptr);
    fflush(output_file_ptr);
    pthread_mutex_unlock(&file_write_mutex);
  }
}


int main(int argc, char *argv[])
{
    const char *app_name;
    bool is_deamon = false;
    app_name = argv[0] + 2;
    if (argc >= 2)
    {
        if (strcmp(argv[1], "-d") == 0)
        {
            is_deamon = true;
        }
    }
    openlog(app_name, LOG_PID | LOG_CONS, LOG_USER);

    struct sigaction termination_action;
    bool is_handler_registered = true;
    memset(&termination_action, 0, sizeof(struct sigaction));
    termination_action.sa_handler = signal_handler;

    if (sigaction(SIGTERM, &termination_action, NULL))
    {
        fprintf(stderr, "Error %d (%s) registering for SIGTERM", errno, strerror(errno));
        is_handler_registered = false;
    }

    if (sigaction(SIGINT, &termination_action, NULL))
    {
        fprintf(stderr, "Error %d (%s)", errno, strerror(errno));
        is_handler_registered = false;
    }
    if (is_handler_registered == false)
    {
        fprintf(stderr, "could not register signal handler\n");
        return -1;
    }
    const char *const output_file_name = "/var/tmp/aesdsocketdata.txt";

    struct addrinfo addr_hints;
    memset(&addr_hints, 0, sizeof(addr_hints));

    addr_hints.ai_family = AF_INET;
    addr_hints.ai_socktype = SOCK_STREAM;
    addr_hints.ai_flags = AI_PASSIVE;

    struct addrinfo *bind_address;
    getaddrinfo(NULL, SERVER_PORT, &addr_hints, &bind_address);

    int socket_listen;
    socket_listen = socket(bind_address->ai_family, bind_address->ai_socktype, bind_address->ai_protocol);

    if (!isValidSocket(socket_listen))
    {
        fprintf(stderr, "socket() failled  errno :  (%d)\n", errno);
        freeaddrinfo(bind_address);
        return -1;
    }
    int reuse = -1;
    if (setsockopt(socket_listen, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1)
    {
        fprintf(stderr, "setsockopt() failled errno : (%d)\n", errno);
        freeaddrinfo(bind_address);
        return -1;
    }

    if (bind(socket_listen, bind_address->ai_addr, bind_address->ai_addrlen))
    {
        fprintf(stderr, "bind() failled. errno (%d)\n", errno);
        freeaddrinfo(bind_address);
        return -1;
    }
    freeaddrinfo(bind_address);
    // Starting the deamon
    if (is_deamon)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            fprintf(stderr, "Error starting the deamon\n");
            return -1;
        }
        if (pid > 0)
        {
            // deamon created exit the main process.
            return 0;
        }
    }
    const int MAX_CLIENT = 100;
    if (listen(socket_listen, MAX_CLIENT) < 0)
    {
        fprintf(stderr, "listen() failled . errno : (%d)\n", errno);
        return -1;
    }
    FILE *file_ptr = fopen(output_file_name, "a");
    if (file_ptr == NULL)
    {
        fprintf(stderr, "fopen(), could not open the output file : %s\n", output_file_name);
        return -1;
    }
    struct sockaddr_storage client_address;
    socklen_t client_len = sizeof(client_address);
    TAILQ_HEAD(head_s, node)
    head;
    TAILQ_INIT(&head);
    int connection_count = 0;
    output_file_ptr = file_ptr;

    struct sigevent sev;
    struct itimerspec timer_spec;

    // Set up signal handler for SIGALRM
    signal(SIGALRM, print_time);

    // Set timer specifications
    sev.sigev_notify = SIGEV_SIGNAL;  
    sev.sigev_signo = SIGALRM;        
    sev.sigev_value.sival_ptr = NULL; 

    timer_spec.it_value.tv_sec = 10;    
    timer_spec.it_value.tv_nsec = 0;    
    timer_spec.it_interval.tv_sec = 10; 
    timer_spec.it_interval.tv_nsec = 0; 

    // Create the timer
    if (timer_create(CLOCK_REALTIME, &sev, &timer_id) == -1)
    {
        fprintf(stderr, "timer_create");
        return 1;
    }

    // Start the timer
    if (timer_settime(timer_id, 0, &timer_spec, NULL) == -1)
    {
        fprintf(stderr,"timer_settime");
        return 1;
    }

    /*
        pthread_t timer_thread;
        struct thread_param_t timer_thread_data;
        timer_thread_data.file_ptr = file_ptr;
        timer_thread_data.file_write_mutex = file_write_mutex;
        timer_thread_data.output_file_name = "";
        timer_thread_data.socket_fd = -1;
        pthread_create(&timer_thread, NULL, timerHandler, (void*)&timer_thread_data);
        */

    while (1 == 1)
    {
        int socket_client = accept(socket_listen, (struct sockaddr *)&client_address, &client_len);

        if (isValidSocket(socket_client))
        {
            // getting the client's IP address
            char address_buffer[CLIENT_NAME_SIZE];
            getnameinfo((struct sockaddr *)&client_address, client_len, address_buffer, sizeof(address_buffer), 0, 0, NI_NUMERICHOST);
            syslog(LOG_INFO, "Accepted connection from %s", address_buffer); // replace this with syslog

            // create a new thread for each incomming connection
            if (connection_count <= MAX_CLIENT)
            {
                struct node *e = (struct node *)malloc(sizeof(struct node));
                struct thread_param_t *thread_args = (struct thread_param_t *)malloc(sizeof(struct thread_param_t));
                strncpy(thread_args->client_name, address_buffer, CLIENT_NAME_SIZE);
                thread_args->file_ptr = file_ptr;
                thread_args->file_write_mutex = file_write_mutex;
                thread_args->output_file_name = output_file_name;
                thread_args->socket_fd = socket_client;

                pthread_create(&e->thread, NULL, tcpConnectionHandler, (void *)thread_args);
                TAILQ_INSERT_TAIL(&head, e, nodes);
                ++connection_count;
            }
            else // wait for created threads to complete
            {
                // waiting for threads to terminate
                struct node *e;
                TAILQ_FOREACH(e, &head, nodes)
                {
                    pthread_join(e->thread, NULL);
                }

                while (!TAILQ_EMPTY(&head))
                {
                    e = TAILQ_FIRST(&head);
                    TAILQ_REMOVE(&head, e, nodes);
                    free(e);
                }
                connection_count = 0;
            }
        }

        if (signal_termination_rcvd == true)
        {
            syslog(LOG_INFO, "Caught signal, exiting");
            fclose(file_ptr);
            int result = remove(output_file_name);
            if (result != 0)
            {
                fprintf(stderr, "Error removing the file : %s", output_file_name);
            }

            struct node *e;
            TAILQ_FOREACH(e, &head, nodes)
            {
                pthread_join(e->thread, NULL);
            }

            while (!TAILQ_EMPTY(&head))
            {
                e = TAILQ_FIRST(&head);
                TAILQ_REMOVE(&head, e, nodes);
                free(e);
            }
            // pthread_join(timer_thread, NULL);
            return 0;
        }
    }

    fclose(file_ptr);
    closelog();
    return 0;
}
