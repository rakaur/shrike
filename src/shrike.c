/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the main() routine.
 *
 * $$Id$
 */

#include "../inc/shrike.h"
#include "../inc/version.h"

extern char **environ;
char *config_file;

/* *INDENT-OFF* */
static void print_help(void)
{
printf(
"usage: %s [-c config] [-dhnv]\n\n"

"-c <file>    Specify the config file\n"
"-d           Start in debugging mode\n"
"-h           Print this message and exit\n"
"-n           Don't fork into the background (log screen + log file)\n"
"-v           Print version information and exit\n", "shrike");
}

static void print_version(void)
{
printf(
"Shrike IRC Services (shrike-%s.%s)\n\n"

"Copyright (c) 2003-2004 E. Will et al.\n"
"Rights to this code are documented in doc/LICENSE.\n", version, build
);
}
/* *INDENT-ON* */

/* handles signals */
static void sighandler(int signum)
{
  /* rehash */
  if (signum == SIGHUP)
  {
    slog(0, LG_NOTICE, "sighandler(): got SIGHUP, rehashing %s", config_file);

    wallops("Got SIGHUP; reloading \2%s\2.", config_file);

    snoop("UPDATE: \2%s\2", "system console");
    wallops("Updating database by request of \2%s\2.", "system console");
    expire_check(NULL);
    db_save();

    snoop("REHASH: \2%s\2", "system console");
    wallops("Rehashing \2%s\2 by request of \2%s\2.", config_file,
            "system console");

    if (!conf_rehash())
      wallops("REHASH of \2%s\2 failed. Please corrrect any errors in the "
              "file and try again.", config_file);

    return;
  }

  /* usually cause by ^C */
  else if (signum == SIGINT)
  {
    slog(0, LG_INFO, "sighandler(): caught interrupt; exiting...");
    /* XXX disconnections? */
    runflags |= RF_SHUTDOWN;
  }

  else if (signum == SIGTERM)
  {
    /* XXX disconnections? */
    slog(0, LG_INFO, "sighandler(): got SIGTERM; exiting...");
    runflags |= RF_SHUTDOWN;
  }

  else if (signum == SIGUSR1)
  {
    /* XXX disconnections? */
    slog(0, LG_INFO, "sighandler(): out of memory; exiting");
    runflags |= RF_SHUTDOWN;
  }

  else if (signum == SIGUSR2)
  {
    /* XXX disconnections? */
    slog(0, LG_INFO, "sighandler(): restarting...");
    runflags |= RF_RESTART;
  }
}

int main(int argc, char *argv[])
{
  boolean_t have_conf = FALSE;
  char r;
  int i;
  FILE *restart_file, *pid_file;
  struct rlimit rlim;

  /* change to our local directory */
  if (chdir(PREFIX) < 0)
  {
    perror(PREFIX);
    return 20;
  }

  /* it appears certian systems *ahem*linux*ahem*
   * don't dump cores by default, so we do this here.
   */
  if (!getrlimit(RLIMIT_CORE, &rlim))
  {
    rlim.rlim_cur = rlim.rlim_max;
    setrlimit(RLIMIT_CORE, &rlim);
  }

  /* do command-line options */
  while ((r = getopt(argc, argv, "c:dhnv")) != -1)
  {
    switch (r)
    {
      case 'c':
        config_file = sstrdup(optarg);
        have_conf = TRUE;
        break;
      case 'd':
        me.loglevel |= LG_DEBUG;
        break;
      case 'h':
        print_help();
        exit(EXIT_SUCCESS);
        break;
      case 'n':
        runflags |= RF_LIVE;
        break;
      case 'v':
        print_version();
        exit(EXIT_SUCCESS);
        break;
      default:
        printf("usage: shrike [-c conf] [-dhnv]\n");
        exit(EXIT_SUCCESS);
        break;
    }
  }

  if (have_conf == FALSE)
    config_file = sstrdup("etc/shrike.conf");

  /* we're starting up */
  if (!(restart_file = fopen("var/shrike.restart", "r")))
    runflags |= RF_STARTING;
  else
  {
    fclose(restart_file);
    remove("var/shrike.restart");
  }

  me.start = time(NULL);
  me.uplinkpong = time(NULL);

  /* set signal handlers */
  signal(SIGINT, sighandler);
  signal(SIGTERM, sighandler);
  signal(SIGFPE, sighandler);
  signal(SIGILL, sighandler);
  signal(SIGPIPE, SIG_IGN);
  signal(SIGQUIT, sighandler);
  signal(SIGHUP, sighandler);
  signal(SIGTRAP, sighandler);
  signal(SIGIOT, sighandler);
  signal(SIGALRM, SIG_IGN);
  signal(SIGUSR2, sighandler);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGWINCH, SIG_IGN);
  signal(SIGTTIN, SIG_IGN);
  signal(SIGTTOU, SIG_IGN);
  signal(SIGTSTP, SIG_IGN);
  signal(SIGUSR1, sighandler);

  /* open log */
  log_open();

  /* since me.loglevel isn't there until after the
   * config routines run, we set the default here
   */
  me.loglevel |= LG_NOTICE;

  printf("shrike: version shrike-%s\n", version);

  if (!(runflags & RF_STARTING))
    slog(0, LG_INFO, "main(): restarted; not sending anything to stdout");

  /* check for pid file */
  if ((pid_file = fopen("var/shrike.pid", "r")))
  {
    fprintf(stderr, "shrike: daemon is already running (pid file found)\n");
    exit(EXIT_FAILURE);
  }

#if HAVE_UMASK
  /* file creation mask */
  umask(077);
#endif

  /* parse our config file */
  conf_parse();

  /* check our config file */
  if (!conf_check())
    exit(EXIT_FAILURE);

  /* load our db */
  db_load();

  /* fork into the background */
  if (!(runflags & RF_LIVE))
  {
    if ((i = fork()) < 0)
    {
      fprintf(stderr, "shrike: can't fork into the background\n");
      exit(EXIT_FAILURE);
    }

    /* parent */
    else if (i != 0)
    {
      printf("shrike: pid %d\n", i);
      printf("shrike: running in background mode from %s\n", PREFIX);
      exit(EXIT_SUCCESS);
    }

    /* parent is gone, just us now */
    if (setpgid(0, 0) < 0)
    {
      fprintf(stderr, "shrike: unable to set process group\n");
      exit(EXIT_FAILURE);
    }
  }
  else
  {
    printf("shrike: pid %d\n", getpid());
    printf("shrike: running in foreground mode from %s\n", PREFIX);
  }

  /* write pid */
  if ((pid_file = fopen("var/shrike.pid", "w")))
  {
    fprintf(pid_file, "%d\n", getpid());
    fclose(pid_file);
  }
  else
  {
    fprintf(stderr, "shrike: unable to write pid file\n");
    exit(EXIT_FAILURE);
  }

  /* no longer starting */
  runflags &= ~RF_STARTING;

  /* we probably have a few open already... */
  me.maxfd = 5;

  /* save dbs every 5 minutes */
  event_add("Save database", 300, (void *)db_save, TRUE);

  /* check expires every hour */
  event_add("Check expires", 3600, (void *)expire_check, TRUE);

  /* connect to our uplink */
  slog(0, LG_INFO, "main(): connecting to `%s' on %d as `%s'",
       me.uplink, me.port, me.name);

  servsock = conn(me.uplink, me.port);

  /* main loop */
  io_loop();

  /* we're shutting down */
  db_save();

  /* should we restart? */
  if (runflags & RF_RESTART)
  {
    slog(0, LG_INFO, "main(): restarting in %d seconds", me.restarttime);
    restart_file = fopen("var/shrike.db", "w");
    fclose(restart_file);
    remove("var/shrike.pid");

    sleep(me.restarttime);

    fclose(log_file);

    execve(argv[0], argv, environ);
  }

  slog(0, LG_INFO, "main(): shutting down: io_loop() exited");

  fclose(log_file);
  remove("var/shrike.pid");

  return 0;
}
