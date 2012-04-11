
#include <stdio.h>
#include <stdlib.h>

#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define MYPORT 8000
#define BACKLOG 10

#define RUNNING_DIR	"/tmp"
#define LOCK_FILE	"mtdaemon.lock"

#define MIN(a,b) (((a)>(b))?(b):(a))

static int run;

void signal_handler(int sig)
{
    switch (sig) {
    case SIGHUP:
	syslog(LOG_DEBUG, "recevied hangup signal");
	run = 0;
	break;
    case SIGTERM:
	syslog(LOG_DEBUG, "recevied terminate signal");
	run = 0;
	exit(0);
	break;
    }
}

/* sequence by Levent Karakas */
void daemonize()
{
    int i, lfp;
    char str[10];

    if (getppid() == 1)
	return;			/* already a daemon */

    i = fork();
    if (i < 0)
	exit(1);		/* fork error */
    if (i > 0)
	exit(0);		/* parent exits */

    /* child (daemon) continues */
    setsid();			/* obtain a new process group */
    for (i = getdtablesize(); i >= 0; --i)
	close(i);		/* close all descriptors */

    i = open("/dev/null", O_RDWR);
    dup(i);
    dup(i);			/* handle standart I/O */

    umask(027);			/* set newly created file permissions */
    chdir(RUNNING_DIR);		/* change running directory */
    lfp = open(LOCK_FILE, O_RDWR | O_CREAT, 0640);
    if (lfp < 0)
	exit(1);		/* can not open */

    if (lockf(lfp, F_TLOCK, 0) < 0)
	exit(0);		/* can not lock */

    /* first instance continues */
    syslog(LOG_DEBUG, str, "%d\n", getpid());
    write(lfp, str, strlen(str));	/* record pid to lockfile */
    signal(SIGCHLD, SIG_IGN);	/* ignore child */
    signal(SIGTSTP, SIG_IGN);	/* ignore tty signals */
    signal(SIGTTOU, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);
    signal(SIGHUP, signal_handler);	/* catch hangup signal */
    signal(SIGTERM, signal_handler);	/* catch kill signal */
}

int get_ver_tiger(int fd)
{
	char buf[20];
	static ver = 1;

	snprintf(buf, sizeof(buf), "10.%d\n", ver);
	ver++;
	write(fd, buf, strlen(buf));
}

int try_parse(int fd, char *buf, const char token[], int toklen, int (*func)(int))
{
	if (0 == strncmp(buf, token, toklen)) {
		func(fd);
		return 1;
	}
	return 0;
}

void run_server()
{
    struct sockaddr_in serv_addr, cli_addr;
    int sockfd, newsockfd, portno, n, i, do_accept;
    int BUFLEN = 2000;
    int one = 1;
    char buffer[BUFLEN];
    socklen_t clilen;
    char msg_welcome[] = "PN_NODE_UPDATE_SERVER_V10001\n";

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
	syslog(LOG_DEBUG, "ERROR create socket");
	exit(1);
    }

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);	//allow reuse of port

    //bind to a local address
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = 8000;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) <
	0) {
	syslog(LOG_DEBUG, "ERROR on bind");
	exit(1);
    }

    listen(sockfd, 5);
    clilen = sizeof(cli_addr);

    do_accept = 1;
    while (1 == do_accept) {
	    newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

	    if (newsockfd < 0) {
		syslog(LOG_DEBUG, "ERROR on accept");
		exit(1);
	    }
	    write(newsockfd, msg_welcome, sizeof(msg_welcome));

	    while (1) {
		bzero(buffer, BUFLEN);

		n = read(newsockfd, buffer, BUFLEN);
		if (n < 0) {
		    syslog(LOG_DEBUG, "ERROR read from socket");
		}

		if (0 == strncmp(buffer, "exit", 4)) {
			break;
		}

		if (0 == strncmp(buffer, "kill", 4)) {
			write(newsockfd, "shutdown\n", 10);
			do_accept = 0;
			break;
		}

		if (1 == try_parse(newsockfd, buffer, "get version.tiger", 17, get_ver_tiger)) continue;

		write(newsockfd, "error 1\n", 8);
	    }
	    close(newsockfd);
    }
    close(sockfd);
}

int main(void)
{
    run = 1;
    syslog(LOG_DEBUG, "MTDaemon Stefan Koch s-koch@gmx.net\n");
    openlog("mtdaemon", LOG_CONS | LOG_PID, LOG_USER);
    syslog(LOG_DEBUG, "started");
    daemonize();
    syslog(LOG_DEBUG, "daemonized");

    run_server();

    syslog(LOG_DEBUG, "shutdown application");
    closelog();

    return EXIT_SUCCESS;
}
