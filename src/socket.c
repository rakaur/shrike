/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains socket routines.
 * Based off of W. Campbell's code.
 *
 * $$Id$
 */

#include "../inc/shrike.h"

fd_set readfds, writefds, nullfds;
char IRC_RAW[BUFSIZE + 1];
uint16_t IRC_RAW_LEN = 0;
int servsock = -1;
time_t CURRTIME;

static int irc_read(int fd, char *buf)
{
  int n = read(fd, buf, BUFSIZE);
  buf[n] = '\0';
  return n;
}

static void irc_packet(char *buf)
{
  char *ptr, buf2[BUFSIZE * 2];
  static char tmp[BUFSIZE * 2 + 1];

  while ((ptr = strchr(buf, '\n')))
  {
    *ptr = '\0';

    if (*(ptr - 1) == '\r')
      *(ptr - 1) = '\0';

    snprintf(buf2, (BUFSIZE * 2), "%s%s", tmp, buf);
    *tmp = '\0';

    irc_parse(buf2);

    buf = ptr + 1;
  }

  if (*buf)
  {
    strncpy(tmp, buf, (BUFSIZE * 2) - strlen(tmp));
    tmp[BUFSIZE * 2] = '\0';
  }
}

/* send a line to the server, append the \r\n */
int8_t sts(char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];
  int16_t len, n;

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  if (servsock == -1)
  {
    slog(0, LG_WARN, "sts(): couldn't write: `%s'", buf);
    return 1;
  }

  slog(0, LG_DEBUG, "<- %s", buf);

  strlcat(buf, "\r\n", BUFSIZE);
  len = strlen(buf);
  /* counting out stuff? */

  /* write it */
  if ((n = write(servsock, buf, len)) == -1)
  {
    if (errno != EAGAIN)
    {
      slog(0, LG_WARN, "sts(): write error to server");
      close(servsock);
      servsock = -1;
      me.connected = FALSE;
      return 1;
    }
  }

  if (n != len)
    slog(0, LG_WARN, "sts(): incomplete write: total: %d; written: %d", len,
         n);

  va_end(ap);

  return 0;
}

/* login to our uplink */
static uint8_t server_login(void)
{
  int8_t ret;

  ret = sts("PASS %s :TS", me.pass);
  if (ret == 1)
    return 1;

  me.bursting = TRUE;

  sts("CAPAB :QS EOB");
  sts("SERVER %s 1 :%s", me.name, me.desc);
  sts("SVINFO 5 3 0 :%ld", CURRTIME);

  return 0;
}

static void ping_uplink(event_t *e)
{
  sts("PING %s", me.uplink);
}

/* called to finalize the uplink connection */
static int8_t irc_estab(void)
{
  uint8_t ret;

  ret = server_login();
  if (ret == 1)
  {
    slog(0, LG_ERR, "irc_estab(): unable to connect to `%s' on port %d",
         me.uplink, me.port);
    servsock = -1;
    me.connected = FALSE;
    return 0;
  }

  me.connected = TRUE;

  slog(0, LG_INFO, "irc_estab(): connection to uplink established");
  slog(0, LG_INFO, "irc_estab(): synching with uplink");

#ifdef HAVE_GETTIMEOFDAY
  /* start our burst timer */
  s_time(&burstime);
#endif

  /* add our server */
  me.me = server_add(me.name, 0, me.desc);

  /* bring on our client */
  services_init();

  /* done bursting by this time... */
  sts(":%s EOB", me.name);

  /* ping our uplink every 5 minutes */
  event_add("Uplink ping", 300, (void *)ping_uplink, TRUE);

  return 1;
}

/* connect to our uplink */
int conn(char *host, uint32_t port)
{
  struct hostent *hp;
  struct sockaddr_in sa;
  struct in_addr *in;
  uint32_t s, optval, flags;

  /* get the socket */
  if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
  {
    slog(0, LG_CRIT, "conn(): unable to create socket");
    return -1;
  }

  if (s > me.maxfd)
    me.maxfd = s;

  if (me.vhost)
  {
    memset((void *)&sa, '\0', sizeof(sa));
    sa.sin_family = AF_INET;

    /* resolve the vhost */
    if ((hp = gethostbyname(me.vhost)) == NULL)
    {
      close(s);
      slog(0, LG_CRIT, "conn(): unable to resolve `%s' for vhost", me.vhost);
      return -1;
    }

    in = (struct in_addr *)(hp->h_addr_list[0]);
    sa.sin_addr.s_addr = in->s_addr;
    sa.sin_port = 0;

    optval = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&optval, sizeof(optval));

    /* bind to vhost */
    if (bind(s, (struct sockaddr *)&sa, sizeof(sa)) < 0)
    {
      close(s);
      slog(0, LG_CRIT, "conn(): unable to bind to vhost `%s'", me.vhost);
      return -1;
    }
  }

  /* resolve it */
  if ((hp = gethostbyname(host)) == NULL)
  {
    close(s);
    slog(0, LG_CRIT, "conn(): unable to resolve `%s'", host);
    return -1;
  }

  memset((void *)&sa, '\0', sizeof(sa));
  sa.sin_family = AF_INET;
  sa.sin_port = htons(port);
  memcpy((void *)&sa.sin_addr, (void *)hp->h_addr, hp->h_length);

  /* set non-blocking the POSIX way */
  flags = fcntl(s, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl(s, F_SETFL, flags);

  /* connect it */
  connect(s, (struct sockaddr *)&sa, sizeof(sa));

  return s;
}

void reconn(event_t *e)
{
  uint32_t i;
  server_t *s;
  node_t *n;

  if (me.connected)
    return;

  if (servsock == -1)
  {
    shutdown(servsock, SHUT_RDWR);
    close(servsock);

    slog(0, LG_DEBUG,
         "reconn(): ----------------------- clearing -----------------------");

    /* we have to kill everything.
     * we only clear servers here because when you delete a server,
     * it deletes its users, which removes them from the channels, which
     * deletes the channels.
     */
    for (i = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, servlist[i].head)
      {
        s = (server_t *)n->data;
        server_delete(s->name);
      }
    }

    slog(0, LG_DEBUG,
         "reconn(): ------------------------- done -------------------------");

    slog(0, LG_INFO, "reconn(): connecting to `%s' on %d as `%s'",
         me.uplink, me.port, me.name);

    servsock = conn(me.uplink, me.port);
  }
}

/* here is where it all happens.
 * get a line from the connection and send it to a parsing function.
 * events are also called from here.
 */
void io_loop(void)
{
  int8_t sr;
  struct timeval to;
  static char buf[BUFSIZE + 1];
  boolean_t eadded = FALSE;

  while (!(runflags & (RF_SHUTDOWN | RF_RESTART)))
  {
    /* update the current time */
    CURRTIME = time(NULL);

    /* check for events */
    event_check();

    memset(buf, '\0', BUFSIZE + 1);
    FD_ZERO(&readfds);
    FD_ZERO(&writefds);
    to.tv_sec = 1;
    to.tv_usec = 0L;

    if ((!me.connected) && (servsock != -1))
      FD_SET(servsock, &writefds);
    else if (servsock != -1)
      FD_SET(servsock, &readfds);

    if ((servsock == -1) && (!eadded))
    {
      event_add("Uplink connect", me.recontime, (void *)reconn, FALSE);
      eadded = TRUE;
    }

    /* select() time */
    if ((sr = select(me.maxfd + 1, &readfds, &writefds, &nullfds, &to)) > 0)
    {
      if (FD_ISSET(servsock, &writefds))
      {
        if (irc_estab() == 0)
          continue;

        eadded = FALSE;
      }

      if (!irc_read(servsock, buf))
      {
        slog(0, LG_INFO, "io_loop(): lost connection to uplink.");
        close(servsock);
        servsock = -1;
        me.connected = FALSE;
      }

      irc_packet(buf);
    }
    else
    {
      if (sr == 0)
      {
        /* select() timed out */
      }
      else if ((sr == -1) && (errno = EINTR))
      {
        /* some signal interrupted us, restart select() */
      }
      else if (sr == -1)
      {
        /* something bad happened */
        slog(0, LG_CRIT, "io_loop(): lost connection: %d: %s", errno,
             strerror(errno));

        close(servsock);
        servsock = -1;
        me.connected = FALSE;
        event_add("Uplink connect", me.recontime, (void *)reconn, FALSE);
        return;
      }
    }
  }
}
