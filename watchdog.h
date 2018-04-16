#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>
#include <signal.h> 
#include <ctype.h> 
#include <pthread.h> 
#include <sys/socket.h> 
#include <netdb.h> 
#include <strings.h>
#include <sys/time.h> 


#define WD_INTERVAL 5000
