#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>


int main(int argc, char* argv[])
{
	const char* appName = "Writer";
	
	if( argc < 3 )
	{
		syslog(LOG_ERR, "Invalid Number of arguments: %d", argc );
		return 1;
	}
	openlog(appName, 0, LOG_USER);
	umask(0000);
	char* write_file = argv[1];
	char* write_string = argv[2];

	int fd = open(write_file, O_WRONLY | O_TRUNC |  O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO );
	if(fd == -1)
	{
		syslog(LOG_ERR, "Could not create output file, errno : %d", errno );
		return 1;
	}

	ssize_t nr = write(fd, write_string, strlen(write_string)) ;
	if(nr == -1 )
	{
		syslog(LOG_ERR, "Could not write to the output file, errno : %d", errno);
	}
	else
	{
		syslog(LOG_DEBUG, "Writing %s to %s",write_file, write_string);
	}
	close(fd);
	return 0;
}