
#include <stdio.h>
#include <stdlib.h>

#include <syslog.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#define RUNNING_DIR	"/tmp"
#define LOCK_FILE	"mtdaemon.lock"

static int run;

void signal_handler(int sig)
{
	switch(sig) {
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
	int i,lfp;
	char str[10];

	if (getppid() == 1) return; /* already a daemon */

		i = fork();
	if (i < 0) exit(1); /* fork error */
	if (i > 0) exit(0); /* parent exits */

	/* child (daemon) continues */
	setsid(); /* obtain a new process group */
	for (i = getdtablesize();i >= 0;--i) close(i); /* close all descriptors */

	i = open("/dev/null", O_RDWR);
	dup(i);
	dup(i); /* handle standart I/O */

	umask(027); /* set newly created file permissions */
	chdir(RUNNING_DIR); /* change running directory */
	lfp = open(LOCK_FILE, O_RDWR|O_CREAT, 0640);
	if (lfp < 0) exit(1); /* can not open */

	if (lockf(lfp, F_TLOCK, 0) < 0) exit(0); /* can not lock */

	/* first instance continues */
	sprintf(str,"%d\n",getpid());
	write(lfp, str, strlen(str)); /* record pid to lockfile */
	signal(SIGCHLD, SIG_IGN); /* ignore child */
	signal(SIGTSTP, SIG_IGN); /* ignore tty signals */
	signal(SIGTTOU, SIG_IGN);
	signal(SIGTTIN, SIG_IGN);
	signal(SIGHUP,  signal_handler); /* catch hangup signal */
	signal(SIGTERM, signal_handler); /* catch kill signal */
}

int main(void)
{
	run = 1;
	printf("MTDaemon Stefan Koch s-koch@gmx.net\n");
	openlog("mtdaemon", LOG_CONS | LOG_PID, LOG_USER);
	syslog(LOG_DEBUG, "started");
	daemonize();
	syslog(LOG_DEBUG, "daemonized");

	while (1 == run) {
		sleep(1);
	}

	syslog(LOG_DEBUG, "shutdown application");
	closelog();
	
	return EXIT_SUCCESS;
}
