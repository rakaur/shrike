/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains IRC interaction routines.
 */

#include "../inc/shrike.h"

static int8_t sjtoken(char *message, char **parv)
{
  char *next;
  uint16_t count;

  if (!message)
    return 0;

  /* now we take the beginning of the message and find all the spaces...
   * set them to \0 and use 'next' to go through the string
   */
  next = message;

  /* eat any additional delimiters */
  while (*next == ' ')
    next++;

  parv[0] = next;
  count = 1;

  while (*next)
  {
    /* this is fine here, since we don't have a :delimited
     * parameter like tokenize
     */

    if (count == 256)
    {
      /* we've reached our limit */
      slog(LG_DEBUG, "sjtokenize(): reached param limit");
      return count;
    }

    if (*next == ' ')
    {
      *next = '\0';
      next++;
      /* eat any additional delimiters */
      while (*next == ' ')
        next++;
      /* if it's the end of the string, it's simply
       ** an extra space at the end.  here we break.
       */
      if (*next == '\0')
        break;

      /* if it happens to be a stray \r, break too */
      if (*next == '\r')
        break;

      parv[count] = next;
      count++;
    }
    else
      next++;
  }

  return count;
}

/* this splits apart a message with origin and command picked off already */
static int8_t tokenize(char *message, char **parv)
{
  char *pos = NULL;
  char *next;
  uint8_t count = 0;

  if (!message)
    return -1;

  /* first we fid out of there's a : in the message, save that string
   * somewhere so we can set it to the last param in parv
   * also make sure there's a space before it... if not then we're screwed
   */
  pos = message;
  while (TRUE)
  {
    if ((pos = strchr(pos, ':')))
    {
      pos--;
      if (*pos != ' ')
      {
        pos += 2;
        continue;
      }
      *pos = '\0';
      pos++;
      *pos = '\0';
      pos++;
      break;
    }
    else
      break;
  }

  /* now we take the beginning of the message and find all the spaces...
   * set them to \0 and use 'next' to go through the string
   */

  next = message;
  parv[0] = message;
  count = 1;

  while (*next)
  {
    if (count == 19)
    {
      /* we've reached one less than our max limit
       * to handle the parameter we already ripped off
       */
      slog(LG_DEBUG, "tokenize(): reached para limit");
      return count;
    }
    if (*next == ' ')
    {
      *next = '\0';
      next++;
      /* eat any additional spaces */
      while (*next == ' ')
        next++;
      /* if it's the end of the string, it's simply
       * an extra space before the :parameter. break.
       */
      if (*next == '\0')
        break;
      parv[count] = next;
      count++;
    }
    else
      next++;
  }

  if (pos)
  {
    parv[count] = pos;
    count++;
  }

  return count;
}

/* parse the incoming server line, then toss it to its function */
void irc_parse(char *line)
{
  char *origin = NULL;
  char *pos;
  char *command = NULL;
  char *message = NULL;
  char *parv[20];
  static char coreLine[BUFSIZE];
  uint8_t parc = 0;
  uint8_t i;
  struct message_ *m;

  /* clear the parv */
  for (i = 0; i < 20; i++)
    parv[i] = NULL;

  if (line != NULL)
  {
    /* sometimes we'll get a blank line with just a \n on it...
     * catch those here... they'll core us later on if we don't
     */
    if (*line == '\n')
      return;
    if (*line == '\000')
      return;

    /* copy the original line so we know what we crashed on */
    memset((char *)&coreLine, '\0', BUFSIZE);
    strlcpy(coreLine, line, BUFSIZE);

    slog(LG_DEBUG, "-> %s", line);

    /* find the first spcae */
    if ((pos = strchr(line, ' ')))
    {
      *pos = '\0';
      pos++;
      /* if it starts with a : we have a prefix/origin
       * pull the origin off into `origin', and have pos for the
       * command, message will be the part afterwards
       */
      if (*line == ':')
      {
        origin = line;
        origin++;
        if ((message = strchr(pos, ' ')))
        {
          *message = '\0';
          message++;
          command = pos;
        }
        else
        {
          command = pos;
          message = NULL;
        }
      }
      else
      {
        message = pos;
        command = line;
      }
    }
    /* okay, the nasty part is over, now we need to make a
     * parv out of what's left
     */

    if (message)
    {
      if (*message == ':')
      {
        message++;
        parv[0] = message;
        parc = 1;
      }
      else
        parc = tokenize(message, parv);
    }
    else
      parc = 0;

    /* now we should have origin (or NULL), command, and a parv
     * with it's accompanying parc... let's make ABSOLUTELY sure
     */
    if (!command)
    {
      slog(LG_DEBUG, "irc_parse(): command not found: %s", coreLine);
      return;
    }

    /* take the command through the hash table */
    if ((m = msg_find(command)))
      if (m->func)
        m->func(origin, parc, parv);
  }
}

static void m_ping(char *origin, uint8_t parc, char *parv[])
{
  /* reply to PING's */
  sts(":%s PONG %s %s", me.name, me.name, parv[0]);
}

static void m_pong(char *origin, uint8_t parc, char *parv[])
{
  /* someone replied to our PING */
  if ((!parv[0]) || (strcasecmp(me.actual, parv[0])))
    return;

  me.uplinkpong = CURRTIME;

  /* -> :test.projectxero.net PONG test.projectxero.net :shrike.malkier.net */
  if (me.bursting)
  {
#ifdef HAVE_GETTIMEOFDAY
    e_time(burstime, &burstime);

    slog(LG_INFO, "m_pong(): finished synching with uplink (%d %s)",
         (tv2ms(&burstime) >
          1000) ? (tv2ms(&burstime) / 1000) : tv2ms(&burstime),
         (tv2ms(&burstime) > 1000) ? "s" : "ms");

    wallops("Finished synching to network in %d %s.",
            (tv2ms(&burstime) >
             1000) ? (tv2ms(&burstime) / 1000) : tv2ms(&burstime),
            (tv2ms(&burstime) > 1000) ? "s" : "ms");
#else
    slog(LG_INFO, "m_pong(): finished synching with uplink");
    wallops("Finished synching to network.");
#endif

    me.bursting = FALSE;
  }
}

static void m_privmsg(char *origin, uint8_t parc, char *parv[])
{
  user_t *u;
  char buf[BUFSIZE];

  /* we should have no more and no less */
  if (parc != 2)
    return;

  /* don't care about channels. */
  if (parv[0][0] == '#')
    return;

  if (!(u = user_find(origin)))
  {
    slog(LG_DEBUG, "m_privmsg(): got message from nonexistant user `%s'",
         origin);
    return;
  }

  /* run it through flood checks */
  if ((svs.flood_msgs) && (!is_sra(u->myuser)) && (!is_ircop(u)))
  {
    /* check if they're being ignored */
    if (u->offenses > 10)
    {
      if ((CURRTIME - u->lastmsg) > 30)
      {
        u->offenses -= 10;
        u->lastmsg = CURRTIME;
        u->msgs = 0;
      }
      else
        return;
    }

    if ((CURRTIME - u->lastmsg) > svs.flood_time)
    {
      u->lastmsg = CURRTIME;
      u->msgs = 0;
    }

    u->msgs++;

    if (u->msgs > svs.flood_msgs)
    {
      /* they're flooding. */
      if (!u->offenses)
      {
        /* ignore them the first time */
        u->lastmsg = CURRTIME;
        u->msgs = 0;
        u->offenses = 11;

        notice(origin, "You have triggered services flood protection.");
        notice(origin, "This is your first offense. You will be ignored for "
               "30 seconds.");

        snoop("FLOOD: \2%s\2", u->nick);

        return;
      }

      if (u->offenses == 1)
      {
        /* ignore them the second time */
        u->lastmsg = CURRTIME;
        u->msgs = 0;
        u->offenses = 12;

        notice(origin, "You have triggered services flood protection.");
        notice(origin, "This is your last warning. You will be ignored for "
               "30 seconds.");

        snoop("FLOOD: \2%s\2", u->nick);

        return;
      }

      if (u->offenses == 2)
      {
        kline_t *k;

        /* kline them the third time */
        k = kline_add("*", u->host, "ten minute ban: flooding services", 600);
        k->setby = sstrdup(svs.nick);

        snoop("FLOOD:KLINE: \2%s\2", u->nick);

        return;
      }
    }
  }

  /* is it for our services client? */
  if (!irccasecmp(parv[0], svs.nick))
  {
    /* for nick@server messages.. */
    snprintf(buf, sizeof(buf), "%s@%s", svs.nick, me.name);

    /* okay, give it to our client */
    services(origin, parc, parv);
    return;
  }
}

static void m_sjoin(char *origin, uint8_t parc, char *parv[])
{
  /* -> :proteus.malkier.net SJOIN 1073516550 #shrike +tn :@sycobuny @+rakaur */

  channel_t *c;
  uint8_t modec = 0;
  char *modev[16];
  uint8_t userc;
  char *userv[256];
  uint8_t i;
  time_t ts;

  if (origin)
  {
    /* :origin SJOIN ts chan modestr [key or limits] :users */
    modev[0] = parv[2];

    if (parc > 4)
      modev[++modec] = parv[3];
    if (parc > 5)
      modev[++modec] = parv[4];

    c = channel_find(parv[1]);
    ts = atol(parv[0]);

    if (!c)
    {
      slog(LG_DEBUG, "m_sjoin(): new channel: %s", parv[1]);
      c = channel_add(parv[1], ts);
    }

    if (ts < c->ts)
    {
      chanuser_t *cu;
      node_t *n;

      /* the TS changed.  a TS change requires the following things
       * to be done to the channel:  reset all modes to nothing, remove
       * all status modes on known users on the channel (including ours),
       * and set the new TS.
       */

      c->modes = 0;
      c->limit = 0;
      if (c->key)
        free(c->key);
      c->key = NULL;

      LIST_FOREACH(n, c->members.head)
      {
        cu = (chanuser_t *)n->data;
        cu->modes = 0;
      }

      slog(LG_INFO, "m_sjoin(): TS changed for %s (%ld -> %ld)", c->name,
           c->ts, ts);

      c->ts = ts;
    }

    channel_mode(c, modec, modev);

    userc = sjtoken(parv[parc - 1], userv);

    for (i = 0; i < userc; i++)
      chanuser_add(c, userv[i]);
  }
}

static void m_part(char *origin, uint8_t parc, char *parv[])
{
  slog(LG_DEBUG, "m_part(): user left channel: %s -> %s", origin, parv[0]);

  chanuser_delete(channel_find(parv[0]), user_find(origin));
}

static void m_nick(char *origin, uint8_t parc, char *parv[])
{
  server_t *s;
  user_t *u;
  kline_t *k;

  /* got the right number of args for an introduction? */
  if (parc == 8)
  {
    s = server_find(parv[6]);
    if (!s)
    {
      slog(LG_DEBUG, "m_nick(): new user on nonexistant server: %s", parv[6]);
      return;
    }

    slog(LG_DEBUG, "m_nick(): new user on `%s': %s", s->name, parv[0]);

    if ((k = kline_find(parv[4], parv[5])))
    {
      /* the new user matches a kline.
       * the server introducing the user probably wasn't around when
       * we added the kline or isn't accepting klines from us.
       * either way, we'll KILL the user and send the server
       * a new KLINE.
       */

      skill(parv[0], k->reason);
      kline_sts(parv[6], k->user, k->host, (k->expires - CURRTIME), k->reason);

      return;
    }

    user_add(parv[0], parv[4], parv[5], s);

    user_mode(user_find(parv[0]), parv[3]);
  }

  /* if it's only 2 then it's a nickname change */
  else if (parc == 2)
  {
    node_t *n;

    u = user_find(origin);
    if (!u)
    {
      slog(LG_DEBUG, "m_nick(): nickname change from nonexistant user: %s",
           origin);
      return;
    }

    slog(LG_DEBUG, "m_nick(): nickname change from `%s': %s", u->nick,
         parv[0]);

    /* remove the current one from the list */
    n = node_find(u, &userlist[u->hash]);
    node_del(n, &userlist[u->hash]);
    node_free(n);

    /* change the nick */
    free(u->nick);
    u->nick = sstrdup(parv[0]);

    /* readd with new nick (so the hash works) */
    n = node_create();
    u->hash = UHASH((unsigned char *)u->nick);
    node_add(u, n, &userlist[u->hash]);
  }
  else
  {
    int i;
    slog(LG_DEBUG, "m_nick(): got NICK with wrong number of params");

    for (i = 0; i < parc; i++)
      slog(LG_DEBUG, "m_nick():   parv[%d] = %s", i, parv[i]);
  }
}

static void m_quit(char *origin, uint8_t parc, char *parv[])
{
  slog(LG_DEBUG, "m_quit(): user leaving: %s", origin);

  /* user_delete() takes care of removing channels and so forth */
  user_delete(origin);
}

static void m_mode(char *origin, uint8_t parc, char *parv[])
{
  if (!origin)
  {
    slog(LG_DEBUG, "m_mode(): received MODE without origin");
    return;
  }

  if (parc < 2)
  {
    slog(LG_DEBUG, "m_mode(): missing parameters in MODE");
    return;
  }

  if (*parv[0] == '#')
    channel_mode(channel_find(parv[0]), parc - 1, &parv[1]);
  else
    user_mode(user_find(parv[0]), parv[1]);
}

static void m_kick(char *origin, uint8_t parc, char *parv[])
{
  user_t *u = user_find(parv[1]);
  channel_t *c = channel_find(parv[0]);

  /* -> :rakaur KICK #shrike rintaun :test */
  slog(LG_DEBUG, "m_kick(): user was kicked: %s -> %s", parv[1], parv[0]);

  if (!u)
  {
    slog(LG_DEBUG, "m_kick(): got kick for nonexistant user %s", parv[1]);
    return;
  }

  if (!c)
  {
    slog(LG_DEBUG, "m_kick(): got kick in nonexistant channel: %s", parv[0]);
    return;
  }

  if (!chanuser_find(c, u))
  {
    slog(LG_DEBUG, "m_kick(): got kick for %s not in %s", u->nick, c->name);
    return;
  }

  chanuser_delete(c, u);

  /* if they kicked us, let's rejoin */
  if (!irccasecmp(svs.nick, parv[1]))
  {
    slog(LG_DEBUG, "m_kick(): i got kicked from `%s'; rejoining", parv[0]);
    join(parv[0], parv[1]);
  }
}

static void m_kill(char *origin, uint8_t parc, char *parv[])
{
  mychan_t *mc;
  node_t *n;
  int i;

  slog(LG_DEBUG, "m_kill(): killed user: %s", parv[0]);
  user_delete(parv[0]);

  if (!irccasecmp(svs.nick, parv[0]))
  {
    services_init();

    if (svs.chan)
      join(svs.chan, svs.nick);

    for (i = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mclist[i].head)
      {
        mc = (mychan_t *)n->data;

        if ((svs.join_chans) && (mc->chan) && (mc->chan->nummembers >= 1))
          join(mc->name, svs.nick);

        if ((svs.join_chans) && (!svs.leave_chans) && (mc->chan) &&
            (mc->chan->nummembers == 0))
          join(mc->name, svs.nick);
      }
    }
  }
}

static void m_squit(char *origin, uint8_t parc, char *parv[])
{
  slog(LG_DEBUG, "m_squit(): server leaving: %s from %s", parv[0], parv[1]);
  server_delete(parv[0]);
}

static void m_server(char *origin, uint8_t parc, char *parv[])
{
  slog(LG_DEBUG, "m_server(): new server: %s", parv[0]);
  server_add(parv[0], atoi(parv[1]), parv[2]);

  if (cnt.server == 2)
    me.actual = sstrdup(parv[0]);
}

static void m_stats(char *origin, uint8_t parc, char *parv[])
{
  user_t *u = user_find(origin);
  kline_t *k;
  node_t *n;
  int i;

  if (!parv[0][0])
    return;

  if (irccasecmp(me.name, parv[1]))
    return;

  snoop("STATS:%c: \2%s\2", parv[0][0], origin);

  switch (parv[0][0])
  {
    case 'C':
    case 'c':
      sts(":%s 213 %s C *@127.0.0.1 A %s %d uplink", me.name, u->nick,
          (is_ircop(u)) ? me.uplink : "127.0.0.1", me.port);
      break;

    case 'E':
    case 'e':
      if (!is_ircop(u))
        break;

      sts(":%s 249 %s E :Last event to run: %s", me.name, u->nick,
          last_event_ran);

      for (i = 0; i < MAX_EVENTS; i++)
      {
        if (event_table[i].active)
          sts(":%s 249 %s E :%s (%d)", me.name, u->nick,
              event_table[i].name, event_table[i].frequency);
      }

      break;

    case 'H':
    case 'h':
      sts(":%s 244 %s H * * %s", me.name, u->nick,
          (is_ircop(u)) ? me.uplink : "127.0.0.1");
      break;

    case 'I':
    case 'i':
      sts(":%s 215 %s I * * *@%s 0 nonopered", me.name, u->nick, me.name);
      break;

    case 'K':
      if (!is_ircop(u))
        break;

      LIST_FOREACH(n, klnlist.head)
      {
        k = (kline_t *)n->data;

        if (!k->duration)
          sts(":%s 216 %s K %s * %s :%s", me.name, u->nick, k->host,
              k->user, k->reason);
      }

      break;

    case 'k':
      if (!is_ircop(u))
        break;

      LIST_FOREACH(n, klnlist.head)
      {
        k = (kline_t *)n->data;

        if (k->duration)
          sts(":%s 216 %s k %s * %s :%s", me.name, u->nick, k->host,
              k->user, k->reason);
      }

      break;

    case 'T':
    case 't':
      if (!is_ircop(u))
        break;

      sts(":%s 249 %s :event      %7d", me.name, u->nick, cnt.event);
      sts(":%s 249 %s :sra        %7d", me.name, u->nick, cnt.sra);
      sts(":%s 249 %s :tld        %7d", me.name, u->nick, cnt.tld);
      sts(":%s 249 %s :kline      %7d", me.name, u->nick, cnt.kline);
      sts(":%s 249 %s :server     %7d", me.name, u->nick, cnt.server);
      sts(":%s 249 %s :user       %7d", me.name, u->nick, cnt.user);
      sts(":%s 249 %s :chan       %7d", me.name, u->nick, cnt.chan);
      sts(":%s 249 %s :myuser     %7d", me.name, u->nick, cnt.myuser);
      sts(":%s 249 %s :mychan     %7d", me.name, u->nick, cnt.mychan);
      sts(":%s 249 %s :chanuser   %7d", me.name, u->nick, cnt.chanuser);
      sts(":%s 249 %s :chanacs    %7d", me.name, u->nick, cnt.chanacs);
      sts(":%s 249 %s :node       %7d", me.name, u->nick, cnt.node);

      sts(":%s 249 %s :bytes sent %7.2f%s", me.name, u->nick,
          bytes(cnt.bout), sbytes(cnt.bout));
      sts(":%s 249 %s :bytes recv %7.2f%s", me.name, u->nick,
          bytes(cnt.bin), sbytes(cnt.bin));
      break;

    case 'u':
      sts(":%s 242 %s :Services Up %s", me.name, u->nick,
          timediff(CURRTIME - me.start));
      break;

    default:
      break;
  }

  sts(":%s 219 %s %c :End of STATS report", me.name, u->nick, parv[0][0]);
}

static void m_admin(char *origin, uint8_t parc, char *parv[])
{
  sts(":%s 256 %s :Administrative info about %s", me.name, origin, me.name);
  sts(":%s 257 %s :%s", me.name, origin, me.adminname);
  sts(":%s 258 %s :Shrike IRC Services (shrike-%s)", me.name, origin, version);
  sts(":%s 259 %s :<%s>", me.name, origin, me.adminemail);
}

static void m_version(char *origin, uint8_t parc, char *parv[])
{
  sts(":%s 351 %s :shrike-%s. %s %s%s%s%s%s%s%s%s%s TS5ow",
      me.name, origin, version, me.name,
      (match_mapping) ? "A" : "",
      (me.loglevel & LG_DEBUG) ? "d" : "",
      (me.auth) ? "e" : "",
      (svs.flood_msgs) ? "F" : "",
      (svs.leave_chans) ? "l" : "",
      (svs.join_chans) ? "j" : "",
      (!match_mapping) ? "R" : "",
      (svs.raw) ? "r" : "", (runflags & RF_LIVE) ? "n" : "");
}

static void m_info(char *origin, uint8_t parc, char *parv[])
{
  uint8_t i;

  for (i = 0; infotext[i]; i++)
    sts(":%s 371 %s :%s", me.name, origin, infotext[i]);

  sts(":%s 374 %s :End of /INFO list", me.name, origin);
}

static void m_join(char *origin, uint8_t parc, char *parv[])
{
  user_t *u = user_find(origin);
  chanuser_t *cu;
  node_t *n, *tn;

  if (!u)
    return;

  /* JOIN 0 is really a part from all channels */
  if (parv[0][0] == '0')
  {
    LIST_FOREACH_SAFE(n, tn, u->channels.head)
    {
      cu = (chanuser_t *)n->data;
      chanuser_delete(cu->chan, u);
    }
  }
}

static void m_pass(char *origin, uint8_t parc, char *parv[])
{
  if (strcmp(me.pass, parv[0]))
  {
    slog(LG_INFO, "m_pass(): password mismatch from uplink; aborting");
    runflags |= RF_SHUTDOWN;
  }
}

static void m_error(char *origin, uint8_t parc, char *parv[])
{
  slog(LG_INFO, "m_error(): error from server: %s", parv[0]);
}

/* *INDENT-OFF* */

/* irc we understand */
struct message_ messages[] = {
  { "PING",    m_ping    },
  { "PONG",    m_pong    },
  { "PRIVMSG", m_privmsg },
  { "SJOIN",   m_sjoin   },
  { "PART",    m_part    },
  { "NICK",    m_nick    },
  { "QUIT",    m_quit    },
  { "MODE",    m_mode    },
  { "KICK",    m_kick    },
  { "KILL",    m_kill    },
  { "SQUIT",   m_squit   },
  { "SERVER",  m_server  },
  { "STATS",   m_stats   },
  { "ADMIN",   m_admin   },
  { "VERSION", m_version },
  { "INFO",    m_info    },
  { "JOIN",    m_join    },
  { "PASS",    m_pass    },
  { "ERROR",   m_error   },
  { NULL }
};

/* *INDENT-ON* */

/* find a matching function */
struct message_ *msg_find(const char *name)
{
  struct message_ *m;

  for (m = messages; m->name; m++)
    if (!strcasecmp(name, m->name))
      return m;

  return NULL;
}
