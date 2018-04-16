//watchdogd.c, by Jordan Simons. Continuously monitors a collection of TCP/IP hosts for availability.
#include "watchdog.h"

/*  Notes:
        Monitor program: tail -f /var/log/system.log
        Kill daemon: kill -USR1 `cat watchdog.pid` <-- Make sure to run in /tmp directory.
        Check if daemon is running: ps axu
				Assumes there is a list of hosts in /tmp/watchdog.pid
				TODO: syslog() is not actually logging messages, however the behavior works as expected when printing to stderr.
*/

												//Global Variables
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
pid_t pid, sid;
													//Functions
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Signal handler for SIG_USR1
void exitHandle () {

	syslog(LOG_NOTICE, "TCP/IP Watchdog stopped");
    closelog();
    exit(EXIT_SUCCESS);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Modifies string in place, to strip of all spaces.
void stripAllSpaces(char* theString)
{
	//Remove leading spaces.
	char* firstChar = theString;

	while( *firstChar != '\0' && isspace(*firstChar)) { firstChar++; }

	int length = strlen(firstChar) + 1;
	memmove(theString, firstChar, length);

	//Remove trailing spaces and new line character.
	strtok(theString, " ");
  strtok(theString, "\n");
  strcat(theString, "\0");
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Iterates through the passed string, determining if it's all whitespace.
int isWhiteSpace(char* theString)
{
	while (*theString != '\0')
	{
		//Check if character isn't white space.
		if( !isspace(*theString) ) {
			return 0;
		}
		theString++;
	}
	//Otherwise, it must be all whitespace.
	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Prepares the program to be run as a daemon.
int daemonize() {

    //Fork, then terminate parent process.
    pid = fork();

    if (pid < 0) {
        syslog(LOG_ERR, "fork");
        return EXIT_FAILURE;
    }

    //Parent
    if (pid > 0) {  exit(EXIT_SUCCESS); }

    //Set umask
    umask(0);

    //Set new session ID
    sid = setsid();
    if (sid < 0) {
        syslog(LOG_ERR, "setsid");
        return EXIT_FAILURE;
    }

    //Change working Directory
    if (( chdir("/") < 0 )) {
        syslog(LOG_ERR, "chdir");
        return EXIT_FAILURE;
    }

    //Close Standard Input/Output Files
		// NOTE: Comment these out to test with printing to STDOUT/STDERR
    close(STDIN_FILENO);
    close(STDOUT_FILENO);
    close(STDERR_FILENO);

    return EXIT_SUCCESS;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Each thread monitors a host.
void *threadRoutine(void *hostName) {

  while (1) {

	char * passedClient = (char *) hostName;

    //Create a TCP/IP socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == sockfd) {
        syslog(LOG_ERR, "socket");
        pthread_exit(NULL);
    }

    // Get the remote host name
   struct hostent *hp = gethostbyname(passedClient);
    if (!hp) {
        syslog(LOG_ERR, "%s is not a valid host name", passedClient);
        pthread_exit(NULL);
    }

    // Prepare the socket for connection
    struct sockaddr_in server;
    memset(&server, '0', sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(80);
    bcopy(hp->h_addr_list[0], &(server.sin_addr.s_addr), hp->h_length);

     //Attemt to connect
    if (-1 == connect(sockfd, (struct sockaddr *)&server, sizeof(server))) {
        syslog(LOG_WARNING, "Host %s is not responding", passedClient);
        //pthread_exit(NULL);
    }

    // fprintf(stderr, "http://%s:80 is alive!\n", passedClient);
		syslog(LOG_NOTICE, "http://%s:80 is alive!\n", passedClient);

    /*  We are certain connection was successful.
        Disconnect and repeat the attempt after WD_INTERVAL milliseconds */

    close(sockfd);
    usleep(WD_INTERVAL * 1000); //WD_INTERVAL is a value in milliseconds.
    }

    pthread_exit(NULL);
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//Opens and processes watchdog.pid/watchdog.dat, creating a new thread for each valid hostname in watchdog.dat.
void process(){

	FILE *pidFile, *watchdogDat;
    struct sigaction exitSigHandle;
    int fbSize = 150;
    char fileBuffer[fbSize];
    pthread_t newThread = NULL;
    int fileLines = 25; //Assume file has 25 lines, will double if surpassed.
    int currentLine = 0;
    int maxStringLength = 50; //Assume each (parsed) host will not exceed 50 characters.
    char **stringArray = NULL;
    int currentIndex = 0;

  //Open watchdog.pid
	pidFile = fopen("/tmp/watchdog.pid", "w");
	if (pidFile == NULL) {
		syslog(LOG_ERR, "open watchdog.pid");
		}

	//Write pid into watchdog.pid
	if ( fprintf(pidFile, "%d", getpid() ) < 0){
			syslog(LOG_ERR, "write watchdog.pid");
		}

	fclose(pidFile);

	//Create an exit signal-handler.
    exitSigHandle.sa_handler = exitHandle;
    sigemptyset(&exitSigHandle.sa_mask);
    exitSigHandle.sa_flags = SA_RESTART;

    if ( sigaction(SIGUSR1, &exitSigHandle, NULL) == -1 ) {
        syslog(LOG_ERR, "sigaction SIGUSR1");
        exit(EXIT_FAILURE);}


    //Open existing watchdog.dat to read list of hosts to watch.
    watchdogDat = fopen("/tmp/watchdog.dat", "r");
    if (pidFile == NULL) {
        syslog(LOG_ERR, "open watchdog.dat");
        }

     //Malloc array to store parsed strings in
     stringArray = malloc(fileLines * sizeof(char*));
     if (stringArray == NULL) {
     	syslog(LOG_ERR, "malloc");
     	exit(EXIT_FAILURE);
     }

     //Initialize each string pointed to.
     for (int i = 0 ; i < fileLines ; i++) {

        if ( (stringArray[i] = malloc(maxStringLength)) == NULL ) {
            syslog(LOG_ERR, "string malloc");
            exit(EXIT_FAILURE);
        }
     }

    //Read until EOF
    while ( fgets(fileBuffer, fbSize, watchdogDat) != NULL)
    {
        //Strip strings of white space.
        stripAllSpaces(fileBuffer);

        //Ignore strings beginning with # or consisting only of whitespace.
        if (strncmp(fileBuffer, "#", 1) == 0 || isWhiteSpace(fileBuffer) == 1) { continue; }

        currentLine++;
    	if (currentLine == fileLines) { //Need to realloc memory for stringArray if surpassed.
            //Double size of array of pointers.
    		fileLines = fileLines * 2;
    		stringArray = realloc(stringArray, fileLines * sizeof(char*));
    		if (stringArray == NULL) {
    			syslog(LOG_ERR, "realloc");
     			exit(EXIT_FAILURE);
    		}

            //Malloc size for new strings.
            for (int j = fileLines / 2 ; j < fileLines ; j++) {

                if ( (stringArray[j] = malloc(maxStringLength)) == NULL ) {
                    syslog(LOG_ERR, "string malloc");
                    exit(EXIT_FAILURE);
                }
            }
    	}

    	//Add host of current iteration to malloc'd stringArray.
        strcpy(stringArray[currentIndex], fileBuffer);

    	//Create new thread, passing current string.
    	if ( 0 != pthread_create(&newThread, NULL, threadRoutine, (void *) stringArray[currentIndex]) ) {
		    syslog(LOG_ERR, "pthread_create");
		    exit(EXIT_FAILURE);
		}

        currentIndex++;
    }

    //Pause main thread while other threads run.
    pause();

    //Free memory.
    for (int k = 0 ; k < fileLines ; k++) { free(stringArray[k]);}
    free(stringArray);

    //raise(SIGUSR1); //NOTE: Used for testing purposes.

    return;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main () {

    setlogmask (LOG_UPTO (LOG_NOTICE));
    openlog ("watchdogd", LOG_CONS | LOG_PID | LOG_NDELAY, LOG_LOCAL1);
    syslog(LOG_NOTICE, "TCP/IP Watchdog started");

    daemonize();
    process();
}
