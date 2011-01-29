/* 
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains client interaction routines.
 */

#include "../inc/shrike.h"

/* introduce a client */
user_t *introduce_nick(char *nick, char *user, char *host, char *real,
                       char *modes)
{
  user_t *u;

  sts("NICK %s 1 %ld +%s %s %s %s :%s",
      nick, CURRTIME, modes, user, host, me.name, real);

  u = user_add(nick, user, host, me.me);
  if (strchr(modes, 'o'))
    u->flags |= UF_IRCOP;

  return u;
}

/* join a channel */
void join(char *chan, char *nick)
{
  channel_t *c = channel_find(chan);
  chanuser_t *cu;

  if (!c)
  {
    sts(":%s SJOIN %ld %s +nt :@%s", me.name, CURRTIME, chan, nick);

    c = channel_add(chan, CURRTIME);
  }
  else
  {
    if (chanuser_find(c, user_find(svs.nick)))
    {
      slog(LG_DEBUG, "join(): i'm already in `%s'", c->name);
      return;
    }

    sts(":%s SJOIN %ld %s + :@%s", me.name, c->ts, chan, nick);
  }

  cu = chanuser_add(c, nick);
  cu->modes |= CMODE_OP;
}

/* bring on the services client */
void services_init(void)
{
  user_t *u;

  u = introduce_nick(svs.nick, svs.user, svs.host, svs.real, "io");
  svs.svs = u;
}

/* PRIVMSG wrapper */
void msg(char *target, char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  sts(":%s PRIVMSG %s :%s", svs.nick, target, buf);
}

/* NOTICE wrapper */
void notice(char *target, char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  sts(":%s NOTICE %s :%s", svs.nick, target, buf);
}

/* WALLOPS wrapper */
void wallops(char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];

  if (svs.silent)
    return;

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  sts(":%s WALLOPS :%s", svs.nick, buf);
}

/* KILL wrapper */
void skill(char *nick, char *reason)
{
  sts(":%s KILL %s :%s!%s!%s (%s)", svs.nick, nick, svs.host, svs.user,
      svs.nick, reason);
}

/* server-to-server KLINE wrapper */
void kline_sts(char *server, char *user, char *host, long duration,
               char *reason)
{
  if (!me.connected)
    return;

  sts(":%s KLINE %s %ld %s %s :%s", svs.nick, server, duration, user, host,
      reason);
}

/* server-to-server UNKLINE wrapper */
void unkline_sts(char *server, char *user, char *host)
{
  if (!me.connected)
    return;

  sts(":%s UNKLINE %s %s %s", svs.nick, server, user, host);
}

void verbose(mychan_t *mychan, char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  if (MC_VERBOSE & mychan->flags)
    sts(":%s NOTICE @%s :[VERBOSE] %s", svs.nick, mychan->name, buf);
}

void snoop(char *fmt, ...)
{
  va_list ap;
  char buf[BUFSIZE];

  if (!svs.chan)
    return;

  if (me.bursting)
    return;

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  msg(svs.chan, "%s", buf);
}

void part(char *chan, char *nick)
{
  user_t *u = user_find(nick);
  channel_t *c = channel_find(chan);

  if (!u || !c)
    return;

  if (!chanuser_find(c, u))
    return;

  /*if (!irccasecmp(svs.chan, c->name))
     return; */

  sts(":%s PART %s", u->nick, c->name);

  chanuser_delete(c, u);
}

void expire_check(void *arg)
{
  uint32_t i, j, w, tcnt;
  myuser_t *mu;
  mychan_t *mc, *tmc;
  node_t *n1, *n2, *tn, *n3;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n1, mulist[i].head)
    {
      mu = (myuser_t *)n1->data;

      if (MU_HOLD & mu->flags || mu->identified)
        continue;

      if (((CURRTIME - mu->lastlogin) >= me.expire) ||
          ((mu->flags & MU_WAITAUTH) && (CURRTIME - mu->registered >= 86400)))
      {
        /* kill all their channels */
        for (j = 0; j < HASHSIZE; j++)
        {
          LIST_FOREACH(tn, mclist[j].head)
          {
            mc = (mychan_t *)tn->data;

            if (mc->founder == mu)
            {
              if (mc->successor)
              {
                /* make sure they're within limits */
                for (w = 0, tcnt = 0; w < HASHSIZE; w++)
                {
                  LIST_FOREACH(n3, mclist[i].head)
                  {
                    tmc = (mychan_t *)n3->data;

                    if (is_founder(tmc, mc->successor))
                      tcnt++;
                  }
                }

                if ((tcnt >= me.maxchans) && (!is_sra(mc->successor)))
                  continue;

                snoop("SUCCESSION: \2%s\2 -> \2%s\2 from \2%s\2",
                      mc->successor->name, mc->name, mc->founder->name);

                chanacs_delete(mc, mc->successor, CA_SUCCESSOR);
                chanacs_add(mc, mc->successor, CA_FOUNDER);
                mc->founder = mc->successor;
                mc->successor = NULL;

                if (mc->founder->user)
                  notice(mc->founder->user->nick,
                         "You are now founder on \2%s\2.", mc->name);

                return;
              }

              snoop("EXPIRE: \2%s\2 from \2%s\2", mc->name, mu->name);

              part(mc->name, svs.nick);
              mychan_delete(mc->name);
            }
          }
        }

        snoop("EXPIRE: \2%s\2 from \2%s\2 ", mu->name, mu->email);
        myuser_delete(mu->name);
      }
    }
  }

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n2, mclist[i].head)
    {
      mc = (mychan_t *)n2->data;

      if (MU_HOLD & mc->founder->flags)
        continue;

      if (MC_HOLD & mc->flags)
        continue;

      if ((CURRTIME - mc->used) >= me.expire)
      {
        snoop("EXPIRE: \2%s\2 from \2%s\2", mc->name, mc->founder->name);

        part(mc->name, svs.nick);
        mychan_delete(mc->name);
      }
    }
  }
}

/* main services client routine */
void services(char *origin, uint8_t parc, char *parv[])
{
  char *cmd, *s;
  char orig[BUFSIZE];
  struct command_ *c;

  /* this should never happen */
  if (parv[0][0] == '&')
  {
    slog(LG_ERROR, "services(): got parv with local channel: %s", parv[0]);
    return;
  }

  /* make a copy of the original for debugging */
  strlcpy(orig, parv[1], BUFSIZE);

  /* lets go through this to get the command */
  cmd = strtok(parv[1], " ");

  if (!cmd)
    return;

  /* ctcp? case-sensitive as per rfc */
  if (!strcmp(cmd, "\001PING"))
  {
    if (!(s = strtok(NULL, " ")))
      s = " 0 ";

    strip(s);
    notice(origin, "\001PING %s\001", s);
    return;
  }
  else if (!strcmp(cmd, "\001VERSION\001"))
  {
    notice(origin,
           "\001VERSION shrike-%s. %s %s%s%s%s%s%s%s%s%s TS5ow\001",
           version, me.name,
           (match_mapping) ? "A" : "",
           (me.loglevel & LG_DEBUG) ? "d" : "",
           (me.auth) ? "e" : "",
           (svs.flood_msgs) ? "F" : "",
           (svs.leave_chans) ? "l" : "",
           (svs.join_chans) ? "j" : "",
           (!match_mapping) ? "R" : "",
           (svs.raw) ? "r" : "", (runflags & RF_LIVE) ? "n" : "");

    return;
  }
  else if (!strcmp(cmd, "\001CLIENTINFO\001"))
  {
    /* easter egg :X */
    notice(origin, "\001CLIENTINFO 114 97 107 97 117 114\001");
    return;
  }

  /* ctcps we don't care about are ignored */
  else if (*cmd == '\001')
    return;

  /* take the command through the hash table */
  if ((c = cmd_find(origin, cmd)))
    if (c->func)
      c->func(origin);
}

/* LOGIN <username> <password> */
static void do_login(char *origin)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  chanuser_t *cu;
  chanacs_t *ca;
  node_t *n;
  char *username = strtok(NULL, " ");
  char *password = strtok(NULL, " ");
  char buf[BUFSIZE], strfbuf[32];
  struct tm tm;

  if (!username || !password)
  {
    notice(origin, "Insufficient parameters for \2LOGIN\2.");
    notice(origin, "Syntax: LOGIN <username> <password>");
    return;
  }

  if (u->myuser && u->myuser->identified)
  {
    notice(origin, "You are already logged in as \2%s\2.", u->myuser->name);
    notice(origin, "Please use the \2LOGOUT\2 command first.");
    return;
  }

  mu = myuser_find(username);
  if (!mu)
  {
    notice(origin, "No such username: \2%s\2.", username);
    return;
  }

  if (mu->user)
  {
    notice(origin, "\2%s\2 is already logged in as \2%s\2.", mu->user->nick,
           mu->name);
    return;
  }

  if (!strcmp(password, mu->pass))
  {
    snoop("LOGIN:AS: \2%s\2 to \2%s\2", u->nick, mu->name);

    if (is_sra(mu))
    {
      snoop("SRA: \2%s\2 as \2%s\2", u->nick, mu->name);
      wallops("\2%s\2 (as \2%s\2) is now an SRA.", u->nick, mu->name);
    }

    u->myuser = mu;
    mu->user = u;
    mu->identified = TRUE;

    notice(origin,
           "Authentication successful. You are now logged in as \2%s\2.",
           u->myuser->name);

    /* check for failed attempts and let them know */
    if (u->myuser->failnum != 0)
    {
      tm = *localtime(&mu->lastlogin);
      strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

      notice(origin, "\2%d\2 failed %s since %s.",
             u->myuser->failnum,
             (u->myuser->failnum == 1) ? "login" : "logins", strfbuf);

      tm = *localtime(&mu->lastfailon);
      strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

      notice(origin, "Last failed attempt from: \2%s\2 on %s.",
             u->myuser->lastfail, strfbuf);

      u->myuser->failnum = 0;
      free(u->myuser->lastfail);
      u->myuser->lastfail = NULL;
    }

    mu->lastlogin = CURRTIME;

    /* now we get to check for xOP */
    LIST_FOREACH(n, mu->chanacs.head)
    {
      ca = (chanacs_t *)n->data;

      cu = chanuser_find(ca->mychan->chan, u);
      if (cu)
      {
        if (should_voice(ca->mychan, ca->myuser))
        {
          cmode(svs.nick, ca->mychan->name, "+v", u->nick);
          cu->modes |= CMODE_VOICE;
        }
        if (should_op(ca->mychan, ca->myuser))
        {
          cmode(svs.nick, ca->mychan->name, "+o", u->nick);
          cu->modes |= CMODE_OP;
        }
      }
    }

    return;
  }

  snoop("LOGIN:AF: \2%s\2 to \2%s\2", u->nick, mu->name);

  notice(origin,
         "Authentication failed. Invalid password for \2%s\2.", mu->name);

  /* record the failed attempts */
  if (mu->lastfail)
  {
    free(mu->lastfail);
    mu->lastfail = NULL;
  }

  strlcpy(buf, u->nick, BUFSIZE);
  strlcat(buf, "!", BUFSIZE);
  strlcat(buf, u->user, BUFSIZE);
  strlcat(buf, "@", BUFSIZE);
  strlcat(buf, u->host, BUFSIZE);
  mu->lastfail = sstrdup(buf);
  mu->failnum++;
  mu->lastfailon = CURRTIME;

  if (mu->failnum == 10)
  {
    tm = *localtime(&mu->lastfailon);
    strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

    wallops("Warning: Numerous failed login attempts to \2%s\2. Last attempt "
            "received from \2%s\2 on %s.", mu->name, mu->lastfail, strfbuf);
  }
}

/* LOGOUT [username] [password] */
static void do_logout(char *origin)
{
  user_t *u = user_find(origin);
  char *user = strtok(NULL, " ");
  char *pass = strtok(NULL, " ");

  if ((!u->myuser) && (!user || !pass))
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if (user && pass)
  {
    myuser_t *mu = myuser_find(user);

    if (!mu)
    {
      notice(origin, "No such username: \2%s\2.", user);
      return;
    }

    if ((!strcmp(mu->pass, pass)) && (mu->user))
    {
      u = mu->user;
      notice(u->nick, "You were logged out by \2%s\2.", origin);
    }
    else
    {
      notice(origin, "Authentication failed. Invalid password for \2%s\2.",
             mu->name);
      return;
    }
  }

  if (is_sra(u->myuser))
    snoop("DESRA: \2%s\2 as \2%s\2", u->nick, u->myuser->name);

  snoop("LOGOUT: \2%s\2 from \2%s\2", u->nick, u->myuser->name);

  if (irccasecmp(origin, u->nick))
    notice(origin, "\2%s\2 has been logged out.", u->nick);
  else
    notice(origin, "You have been logged out.");

  u->myuser->user = NULL;
  u->myuser->identified = FALSE;
  u->myuser->lastlogin = CURRTIME;
  u->myuser = NULL;
}

/* SOP|AOP|VOP <#channel> ADD|DEL|LIST <username|hostmask> */
static void do_xop(char *origin, uint8_t level)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;
  chanacs_t *ca;
  chanuser_t *cu;
  node_t *n;
  char *chan = strtok(NULL, " ");
  char *cmd = strtok(NULL, " ");
  char *uname = strtok(NULL, " ");
  char *hostbuf;

  if (!cmd || !chan)
  {
    notice(origin, "Insufficient parameters specified for \2xOP\2.");
    notice(origin, "Syntax: SOP|AOP|VOP <#channel> ADD|DEL|LIST <username>");
    return;
  }

  if ((strcasecmp("LIST", cmd)) && (!uname))
  {
    notice(origin, "Insufficient parameters specified for \2xOP\2.");
    notice(origin, "Syntax: SOP|AOP|VOP <#channel> ADD|DEL|LIST <username>");
    return;
  }

  /* make sure they're registered, logged in
   * and the founder of the channel before
   * we go any further.
   */
  if (!u->myuser || !u->myuser->identified)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "The channel \2%s\2 is not registered.", chan);
    return;
  }

  /* ADD */
  if (!strcasecmp("ADD", cmd))
  {
    /* VOP */
    if (CA_VOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
          (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        /* we might be adding a hostmask */
        if (!validhostmask(uname))
        {
          notice(origin, "\2%s\2 is neither a username nor hostmask.", uname);
          return;
        }

        if (chanacs_find_host(mc, uname, CA_VOP))
        {
          notice(origin, "\2%s\2 is already on the VOP list for \2%s\2",
                 uname, mc->name);
          return;
        }

        uname = collapse(uname);

        chanacs_add_host(mc, uname, CA_VOP);

        verbose(mc, "\2%s\2 added \2%s\2 to the VOP list.", u->nick, uname);

        notice(origin, "\2%s\2 has been added to the VOP list for \2%s\2.",
               uname, mc->name);

        /* run through the channel's user list and do it */
        LIST_FOREACH(n, mc->chan->members.head)
        {
          cu = (chanuser_t *)n->data;

          if (cu->modes & CMODE_VOICE)
            return;

          hostbuf = make_hostmask(cu->user->nick, cu->user->user,
                                  cu->user->host);

          if (should_voice_host(mc, hostbuf))
          {
            cmode(svs.nick, mc->name, "+v", cu->user->nick);
            cu->modes |= CMODE_VOICE;
          }
        }

        return;
      }

      if (MU_NOOP & mu->flags)
      {
        notice(origin, "\2%s\2 does not wish to be added to access lists.",
               mu->name);
        return;
      }

      if (chanacs_find(mc, mu, CA_VOP))
      {
        notice(origin, "\2%s\2 is already on the VOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if (chanacs_find(mc, mu, CA_AOP))
      {
        chanacs_delete(mc, mu, CA_AOP);
        chanacs_add(mc, mu, CA_VOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2AOP\2 to \2VOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2AOP\2 to \2VOP\2.",
                u->nick, mu->name);

        return;
      }

      if (chanacs_find(mc, mu, CA_SOP))
      {
        chanacs_delete(mc, mu, CA_SOP);
        chanacs_add(mc, mu, CA_VOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2SOP\2 to \2VOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2SOP\2 to \2VOP\2.",
                u->nick, mu->name);

        return;
      }

      chanacs_add(mc, mu, CA_VOP);

      notice(origin, "\2%s\2 has been added to the VOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 added \2%s\2 to the VOP list.", u->nick, mu->name);

      return;
    }

    /* AOP */
    if (CA_AOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
          (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        /* we might be adding a hostmask */
        if (!validhostmask(uname))
        {
          notice(origin, "\2%s\2 is neither a username nor hostmask.", uname);
          return;
        }

        if (chanacs_find_host(mc, uname, CA_AOP))
        {
          notice(origin, "\2%s\2 is already on the AOP list for \2%s\2",
                 uname, mc->name);
          return;
        }

        uname = collapse(uname);

        chanacs_add_host(mc, uname, CA_AOP);

        verbose(mc, "\2%s\2 added \2%s\2 to the AOP list.", u->nick, uname);

        notice(origin, "\2%s\2 has been added to the AOP list for \2%s\2.",
               uname, mc->name);

        /* run through the channel's user list and do it */
        LIST_FOREACH(n, mc->chan->members.head)
        {
          cu = (chanuser_t *)n->data;

          if (cu->modes & CMODE_OP)
            return;

          hostbuf = make_hostmask(cu->user->nick, cu->user->user,
                                  cu->user->host);

          if (should_op_host(mc, hostbuf))
          {
            cmode(svs.nick, mc->name, "+o", cu->user->nick);
            cu->modes |= CMODE_OP;
          }
        }

        return;
      }

      if (chanacs_find(mc, mu, CA_AOP))
      {
        notice(origin, "\2%s\2 is already on the AOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if (chanacs_find(mc, mu, CA_VOP))
      {
        chanacs_delete(mc, mu, CA_VOP);
        chanacs_add(mc, mu, CA_AOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2VOP\2 to \2AOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2VOP\2 to \2AOP\2.",
                u->nick, mu->name);

        return;
      }

      if (chanacs_find(mc, mu, CA_SOP))
      {
        chanacs_delete(mc, mu, CA_SOP);
        chanacs_add(mc, mu, CA_AOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2SOP\2 to \2AOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2SOP\2 to \2AOP\2.",
                u->nick, mu->name);

        return;
      }

      chanacs_add(mc, mu, CA_AOP);

      notice(origin, "\2%s\2 has been added to the AOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 added \2%s\2 to the AOP list.", u->nick, mu->name);

      return;
    }

    /* SOP */
    if (CA_SOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        if (validhostmask(uname))
          notice(origin, "Hostmasks cannot be added to the SOP list.");
        else
          notice(origin, "No such username: \2%s\2.", uname);

        return;
      }

      if (chanacs_find(mc, mu, CA_SOP))
      {
        notice(origin, "\2%s\2 is already on the SOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if (chanacs_find(mc, mu, CA_VOP))
      {
        chanacs_delete(mc, mu, CA_VOP);
        chanacs_add(mc, mu, CA_SOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2VOP\2 to \2SOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2VOP\2 to \2SOP\2.",
                u->nick, mu->name);

        return;
      }

      if (chanacs_find(mc, mu, CA_AOP))
      {
        chanacs_delete(mc, mu, CA_AOP);
        chanacs_add(mc, mu, CA_SOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2AOP\2 to \2SOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2AOP\2 to \2SOP\2.",
                u->nick, mu->name);

        return;
      }

      chanacs_add(mc, mu, CA_SOP);

      notice(origin, "\2%s\2 has been added to the SOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 added \2%s\2 to the SOP list.", u->nick, mu->name);

      return;
    }
  }

  /* DEL */
  else if (!strcasecmp("DEL", cmd))
  {
    /* VOP */
    if (CA_VOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
          (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        /* we might be deleting a hostmask */
        if (!validhostmask(uname))
        {
          notice(origin, "\2%s\2 is neither a username nor hostmask.", uname);
          return;
        }

        if (!chanacs_find_host(mc, uname, CA_VOP))
        {
          notice(origin, "\2%s\2 is not on the VOP list for \2%s\2.",
                 uname, mc->name);
          return;
        }

        chanacs_delete_host(mc, uname, CA_VOP);

        verbose(mc, "\2%s\2 removed \2%s\2 from the VOP list.", u->nick,
                uname);

        notice(origin, "\2%s\2 has been removed to the VOP list for \2%s\2.",
               uname, mc->name);

        return;
      }

      if (!chanacs_find(mc, mu, CA_VOP))
      {
        notice(origin, "\2%s\2 is not on the VOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      chanacs_delete(mc, mu, CA_VOP);

      notice(origin, "\2%s\2 has been removed from the VOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 removed \2%s\2 from the VOP list.", u->nick,
              mu->name);

      return;
    }

    /* AOP */
    if (CA_AOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
          (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        /* we might be deleting a hostmask */
        if (!validhostmask(uname))
        {
          notice(origin, "\2%s\2 is neither a username nor hostmask.", uname);
          return;
        }

        if (!chanacs_find_host(mc, uname, CA_AOP))
        {
          notice(origin, "\2%s\2 is not on the AOP list for \2%s\2.",
                 uname, mc->name);
          return;
        }

        chanacs_delete_host(mc, uname, CA_AOP);

        verbose(mc, "\2%s\2 removed \2%s\2 from the AOP list.", u->nick,
                uname);
        notice(origin, "\2%s\2 has been removed to the AOP list for \2%s\2.",
               uname, mc->name);

        return;
      }

      if (!chanacs_find(mc, mu, CA_AOP))
      {
        notice(origin, "\2%s\2 is not on the AOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      chanacs_delete(mc, mu, CA_AOP);

      notice(origin, "\2%s\2 has been removed from the AOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 removed \2%s\2 from the AOP list.", u->nick,
              mu->name);

      return;
    }

    /* SOP */
    if (CA_SOP & level)
    {
      if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "No such username: \2%s\2.", uname);
        return;
      }

      if (!chanacs_find(mc, mu, CA_SOP))
      {
        notice(origin, "\2%s\2 is not on the SOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      chanacs_delete(mc, mu, CA_SOP);

      notice(origin, "\2%s\2 has been removed from the SOP list for \2%s\2.",
             mu->name, mc->name);

      verbose(mc, "\2%s\2 removed \2%s\2 from the SOP list.", u->nick,
              mu->name);

      return;
    }
  }

  else if (!strcasecmp("LIST", cmd))
  {
    if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
        (!is_xop(mc, u->myuser, (CA_VOP | CA_AOP | CA_SOP))))
    {
      notice(origin, "You are not authorized to perform this operation.");
      return;
    }

    /* VOP */
    if (CA_VOP & level)
    {
      uint8_t i = 0;

      notice(origin, "VOP list for \2%s\2:", mc->name);

      LIST_FOREACH(n, mc->chanacs.head)
      {
        ca = (chanacs_t *)n->data;

        if (CA_VOP & ca->level)
        {
          if (ca->host)
            notice(origin, "%d: \2%s\2", ++i, ca->host);

          else if (ca->myuser->user)
            notice(origin, "%d: \2%s\2 (logged in from \2%s\2)", ++i,
                   ca->myuser->name, ca->myuser->user->nick);
          else
            notice(origin, "%d: \2%s\2 (not logged in)", ++i,
                   ca->myuser->name);
        }
      }

      notice(origin, "Total of \2%d\2 %s in \2%s\2's VOP list.",
             i, (i == 1) ? "entry" : "entries", mc->name);
    }

    /* AOP */
    if (CA_AOP & level)
    {
      uint8_t i = 0;

      notice(origin, "AOP list for \2%s\2:", mc->name);

      LIST_FOREACH(n, mc->chanacs.head)
      {
        ca = (chanacs_t *)n->data;

        if (CA_AOP & ca->level)
        {
          if (ca->host)
            notice(origin, "%d: \2%s\2", ++i, ca->host);

          else if (ca->myuser->user)
            notice(origin, "%d: \2%s\2 (logged in from \2%s\2)", ++i,
                   ca->myuser->name, ca->myuser->user->nick);
          else
            notice(origin, "%d: \2%s\2 (not logged in)", ++i,
                   ca->myuser->name);
        }
      }

      notice(origin, "Total of \2%d\2 %s in \2%s\2's AOP list.",
             i, (i == 1) ? "entry" : "entries", mc->name);
    }

    /* SOP */
    if (CA_SOP & level)
    {
      uint8_t i = 0;

      notice(origin, "SOP list for \2%s\2:", mc->name);

      LIST_FOREACH(n, mc->chanacs.head)
      {
        ca = (chanacs_t *)n->data;

        if (CA_SOP & ca->level)
        {
          if (ca->myuser->user)
            notice(origin, "%d: \2%s\2 (logged in from \2%s\2)", ++i,
                   ca->myuser->name, ca->myuser->user->nick);
          else
            notice(origin, "%d: \2%s\2 (not logged in)", ++i,
                   ca->myuser->name);
        }
      }

      notice(origin, "Total of \2%d\2 %s in \2%s\2's SOP list.",
             i, (i == 1) ? "entry" : "entries", mc->name);
    }
  }
}

static void do_sop(char *origin)
{
  do_xop(origin, CA_SOP);
}

static void do_aop(char *origin)
{
  do_xop(origin, CA_AOP);
}

static void do_vop(char *origin)
{
  do_xop(origin, CA_VOP);
}

static void do_op(char *origin)
{
  char *chan = strtok(NULL, " ");
  char *nick = strtok(NULL, " ");
  mychan_t *mc;
  user_t *u;
  chanuser_t *cu;
  char *hostbuf;

  if (!chan)
  {
    notice(origin, "Insufficient parameters specified for \2OP\2.");
    notice(origin, "Syntax: OP <#channel> [nickname]");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "No such channel: \2%s\2.", chan);
    return;
  }

  u = user_find(origin);
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_SOP | CA_AOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* assume they want to be op'd */
  if (!nick)
    nick = origin;

  /* figure out who we're going to op */
  if (!(u = user_find(nick)))
  {
    notice(origin, "No such nickname: \2%s\2.", nick);
    return;
  }

  hostbuf = make_hostmask(u->nick, u->user, u->host);

  if (!chanacs_find_host(mc, hostbuf, CA_AOP))
  {
    if ((MC_SECURE & mc->flags) && (!u->myuser))
    {
      notice(origin, "The \2SECURE\2 flag is set for \2%s\2.", mc->name);
      return;
    }
    else if ((MC_SECURE & mc->flags) && (!is_founder(mc, u->myuser)) &&
             (!is_successor(mc, u->myuser)) &&
             (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
    {
      notice(origin, "\2%s\2 could not be opped on \2%s\2.", u->nick,
             mc->name);
      return;
    }
  }

  cu = chanuser_find(mc->chan, u);
  if (!cu)
  {
    notice(origin, "\2%s\2 is not on \2%s\2.", u->nick, mc->name);
    return;
  }

  if (CMODE_OP & cu->modes)
  {
    notice(origin, "\2%s\2 is already opped on \2%s\2.", u->nick, mc->name);
    return;
  }

  cmode(svs.nick, chan, "+o", u->nick);
  cu->modes |= CMODE_OP;
  notice(origin, "\2%s\2 has been opped on \2%s\2.", u->nick, mc->name);
}

static void do_deop(char *origin)
{
  char *chan = strtok(NULL, " ");
  char *nick = strtok(NULL, " ");
  mychan_t *mc;
  user_t *u;
  chanuser_t *cu;

  if (!chan)
  {
    notice(origin, "Insufficient parameters specified for \2DEOP\2.");
    notice(origin, "Syntax: DEOP <#channel> [nickname]");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "No such channel: \2%s\2.", chan);
    return;
  }

  u = user_find(origin);
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* assume they want to be opped */
  if (!nick)
    nick = origin;

  /* figure out who we're going to deop */
  if (!(u = user_find(nick)))
  {
    notice(origin, "No such nickname: \2%s\2.", nick);
    return;
  }

  cu = chanuser_find(mc->chan, u);
  if (!cu)
  {
    notice(origin, "\2%s\2 is not on \2%s\2.", u->nick, mc->name);
    return;
  }

  if (!(CMODE_OP & cu->modes))
  {
    notice(origin, "\2%s\2 is not opped on \2%s\2.", u->nick, mc->name);
    return;
  }

  cmode(svs.nick, chan, "-o", u->nick);
  cu->modes &= ~CMODE_OP;
  notice(origin, "\2%s\2 has been deopped on \2%s\2.", u->nick, mc->name);
}

static void do_voice(char *origin)
{
  char *chan = strtok(NULL, " ");
  char *nick = strtok(NULL, " ");
  mychan_t *mc;
  user_t *u;
  chanuser_t *cu;

  if (!chan)
  {
    notice(origin, "Insufficient parameters specified for \2VOICE\2.");
    notice(origin, "Syntax: VOICE <#channel> [nickname]");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "No such channel: \2%s\2.", chan);
    return;
  }

  u = user_find(origin);
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* assume they want to be voiced */
  if (!nick)
    nick = origin;

  /* figure out who we're going to voice */
  if (!(u = user_find(nick)))
  {
    notice(origin, "No such nickname: \2%s\2.", nick);
    return;
  }

  cu = chanuser_find(mc->chan, u);
  if (!cu)
  {
    notice(origin, "\2%s\2 is not on \2%s\2.", u->nick, mc->name);
    return;
  }

  if (CMODE_VOICE & cu->modes)
  {
    notice(origin, "\2%s\2 is already voiced on \2%s\2.", u->nick, mc->name);
    return;
  }

  cmode(svs.nick, chan, "+v", u->nick);
  cu->modes |= CMODE_VOICE;
  notice(origin, "\2%s\2 has been voiced on \2%s\2.", u->nick, mc->name);
}

static void do_devoice(char *origin)
{
  char *chan = strtok(NULL, " ");
  char *nick = strtok(NULL, " ");
  mychan_t *mc;
  user_t *u;
  chanuser_t *cu;

  if (!chan)
  {
    notice(origin, "Insufficient parameters specified for \2DEVOICE\2.");
    notice(origin, "Syntax: DEVOICE <#channel> [nickname]");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "No such channel: \2%s\2.", chan);
    return;
  }

  u = user_find(origin);
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* assume they want to be devoiced */
  if (!nick)
    nick = origin;

  /* figure out who we're going to devoice */
  if (!(u = user_find(nick)))
  {
    notice(origin, "No such nickname: \2%s\2.", nick);
    return;
  }

  cu = chanuser_find(mc->chan, u);
  if (!cu)
  {
    notice(origin, "\2%s\2 is not on \2%s\2.", u->nick, mc->name);
    return;
  }

  if (!(CMODE_VOICE & cu->modes))
  {
    notice(origin, "\2%s\2 is not voiced on \2%s\2.", u->nick, mc->name);
    return;
  }

  cmode(svs.nick, chan, "-v", u->nick);
  cu->modes &= ~CMODE_VOICE;
  notice(origin, "\2%s\2 has been devoiced on \2%s\2.", u->nick, mc->name);
}

/* INVITE <channel> [nickname] */
static void do_invite(char *origin)
{
  char *chan = strtok(NULL, " ");
  char *nick = strtok(NULL, " ");
  mychan_t *mc;
  user_t *u;
  chanuser_t *cu;

  if (!chan)
  {
    notice(origin, "Insufficient parameters specified for \2INVITE\2.");
    notice(origin, "Syntax: INVITE <#channel> [nickname]");
    return;
  }

  mc = mychan_find(chan);
  if (!mc)
  {
    notice(origin, "\2%s\2 is not registered.", chan);
    return;
  }

  u = user_find(origin);
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_SOP | CA_AOP | CA_VOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* assume they want to be invited */
  if (!nick)
    nick = origin;

  /* figure out who we're going to invite */
  if (!(u = user_find(nick)))
  {
    notice(origin, "No such nickname: \2%s\2.", nick);
    return;
  }

  cu = chanuser_find(mc->chan, u);
  if (cu)
  {
    notice(origin, "\2%s\2 is already on \2%s\2.", u->nick, mc->name);
    return;
  }

  sts(":%s INVITE %s :%s", svs.nick, u->nick, chan);
  notice(origin, "\2%s\2 has been invited to \2%s\2.", u->nick, mc->name);
}

/* INFO <username|#channel> */
static void do_info(char *origin)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;
  char *name = strtok(NULL, " ");
  char buf[BUFSIZE], strfbuf[32];
  struct tm tm;

  if (!name)
  {
    notice(origin, "Insufficient parameters specified for \2INFO\2.");
    notice(origin, "Syntax: INFO <username|#channel>");
    return;
  }

  if (*name == '#')
  {
    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    snoop("INFO: \2%s\2 by \2%s\2", name, origin);

    tm = *localtime(&mc->registered);
    strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

    notice(origin, "Information on \2%s\2", mc->name);

    if (mc->founder->user)
      notice(origin, "Founder    : %s (logged in from \2%s\2)",
             mc->founder->name, mc->founder->user->nick);
    else
      notice(origin, "Founder    : %s (not logged in)", mc->founder->name);

    if (mc->successor)
    {
      if (mc->successor->user)
        notice(origin, "Successor  : %s (logged in from \2%s\2)",
               mc->successor->name, mc->successor->user->nick);
      else
        notice(origin, "Successor  : %s (not logged in)", mc->successor->name);
    }

    notice(origin, "Registered : %s (%s ago)", strfbuf,
           time_ago(mc->registered));

    if (mc->mlock_on || mc->mlock_off)
    {
      char params[BUFSIZE];

      *buf = 0;
      *params = 0;

      if (mc->mlock_on)
      {
        strcat(buf, "+");
        strcat(buf, flags_to_string(mc->mlock_on));

        /* add these in manually */
        if (mc->mlock_limit)
        {
          strcat(buf, "l");
          strcat(params, " ");
          strcat(params, itoa(mc->mlock_limit));
        }

        if (mc->mlock_key)
          strcat(buf, "k");
      }

      if (mc->mlock_off)
      {
        strcat(buf, "-");
        strcat(buf, flags_to_string(mc->mlock_off));
      }

      if (*buf)
        notice(origin, "Mode lock  : %s %s", buf, params != NULL ? params : "");
    }

    *buf = '\0';

    if (MC_HOLD & mc->flags)
      strcat(buf, "HOLD");

    if (MC_NEVEROP & mc->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "NEVEROP");
    }
    if (MC_SECURE & mc->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "SECURE");
    }
    if (MC_VERBOSE & mc->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "VERBOSE");
    }

    if (*buf)
      notice(origin, "Flags      : %s", buf);
  }
  else
  {
    if (!(mu = myuser_find(name)))
    {
      notice(origin, "No such username: \2%s\2.", name);
      return;
    }

    snoop("INFO: \2%s\2 by \2%s\2", name, origin);

    tm = *localtime(&mu->registered);
    strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

    notice(origin, "Information on \2%s\2", mu->name);

    notice(origin, "Registered : %s (%s ago)", strfbuf,
           time_ago(mu->registered));

    if ((!(mu->flags & MU_HIDEMAIL)) ||
        (is_sra(u->myuser) || is_ircop(u) || u->myuser == mu))
      notice(origin, "Email      : %s", mu->email);

    *buf = '\0';

    if (MU_HIDEMAIL & mu->flags)
      strcat(buf, "HIDEMAIL");

    if (MU_HOLD & mu->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "HOLD");
    }
    if (MU_NEVEROP & mu->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "NEVEROP");
    }
    if (MU_NOOP & mu->flags)
    {
      if (*buf)
        strcat(buf, " ");

      strcat(buf, "NOOP");
    }

    if (*buf)
      notice(origin, "Flags      : %s", buf);
  }
}

/* RECOVER <#channel> */
static void do_recover(char *origin)
{
  user_t *u = user_find(origin);
  chanuser_t *cu, *tcu;
  mychan_t *mc;
  node_t *n;
  char *name = strtok(NULL, " ");
  char *hostbuf;
  boolean_t in_chan;

  if (!name)
  {
    notice(origin, "Insufficient parameters specified for \2RECOVER\2.");
    notice(origin, "Syntax: RECOVER <#channel>");
    return;
  }

  /* make sure they're logged in */
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  /* make sure the channel's registered */
  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  /* make sure they can USE this (XXX - successor?) */
  if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* are they in it? */
  if ((cu = chanuser_find(mc->chan, u)))
  {
    /* make sure they're not opped */
    if ((CMODE_OP & cu->modes))
    {
      notice(origin, "You're already a channel operator in \2%s\2.", name);
      return;
    }

    in_chan = TRUE;
  }
  else
    in_chan = FALSE;

  verbose(mc, "\2%s\2 used RECOVER.", origin);

  /* deop everyone */
  LIST_FOREACH(n, mc->chan->members.head)
  {
    tcu = (chanuser_t *)n->data;

    if ((CMODE_OP & tcu->modes) && (irccasecmp(svs.nick, tcu->user->nick)) &&
        (irccasecmp(origin, tcu->user->nick)))
    {
      cmode(svs.nick, mc->chan->name, "-o", tcu->user->nick);
      tcu->modes &= ~CMODE_OP;
    }
  }

  /* remove modes that keep people out */
  if (CMODE_LIMIT & mc->chan->modes)
  {
    cmode(svs.nick, mc->chan->name, "-l", NULL);
    mc->chan->modes &= ~CMODE_LIMIT;
  }

  if (CMODE_INVITE & mc->chan->modes)
  {
    cmode(svs.nick, mc->chan->name, "-i", NULL);
    mc->chan->modes &= ~CMODE_INVITE;
  }

  if (CMODE_KEY & mc->chan->modes)
  {
    cmode(svs.nick, mc->chan->name, "-k", mc->chan->key);
    mc->chan->modes &= ~CMODE_KEY;
    free(mc->chan->key);
    mc->chan->key = NULL;
  }

  if (!in_chan)
  {
    /* set an exempt on the user calling this */
    hostbuf = make_hostmask(u->nick, u->user, u->host);

    sts(":%s MODE %s +e %s", svs.nick, mc->chan->name, hostbuf);

    /* invite them back. */
    sts(":%s INVITE %s %s", svs.nick, u->nick, mc->chan->name);
  }
  else
  {
    cmode(svs.nick, mc->chan->name, "+o", cu->user->nick);
    cu->modes |= CMODE_OP;
  }

  notice(origin, "Recover complete for \2%s\2.", mc->chan->name);
}

/* REGISTER <username|#channel> <password> [email] */
static void do_register(char *origin)
{
  user_t *u = user_find(origin);
  channel_t *c;
  chanuser_t *cu;
  myuser_t *mu, *tmu;
  mychan_t *mc, *tmc;
  node_t *n;
  char *name = strtok(NULL, " ");
  char *pass = strtok(NULL, " ");
  uint32_t i, tcnt;

  if (!name)
  {
    notice(origin, "Insufficient parameters specified for \2REGISTER\2.");
    notice(origin, "Syntax: REGISTER <username|#channel> <password> [email]");
    return;
  }

  if (*name == '#')
  {
    if (!name || !pass)
    {
      notice(origin, "Insufficient parameters specified for \2REGISTER\2.");
      notice(origin, "Syntax: REGISTER <#channel> <password>");
      return;
    }

    if (strlen(pass) > 32)
    {
      notice(origin, "Invalid parameters specified for \2REGISTER\2.");
      return;
    }

    /* make sure they're logged in */
    if (!u->myuser || !u->myuser->identified)
    {
      notice(origin, "You are not logged in.");
      return;
    }

    /* make sure it isn't already registered */
    if ((mc = mychan_find(name)))
    {
      notice(origin, "\2%s\2 is already registered to \2%s\2.", mc->name,
             mc->founder->name);
      return;
    }

    /* make sure the channel exists */
    if (!(c = channel_find(name)))
    {
      notice(origin,
             "The channel \2%s\2 must exist in order to register it.", name);
      return;
    }

    /* make sure they're in it */
    if (!(cu = chanuser_find(c, u)))
    {
      notice(origin, "You must be in \2%s\2 in order to register it.", name);
      return;
    }

    /* make sure they're opped */
    if (!(CMODE_OP & cu->modes))
    {
      notice(origin,
             "You must be a channel operator in \2%s\2 in order to "
             "register it.", name);
      return;
    }

    /* make sure they're within limits */
    for (i = 0, tcnt = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mclist[i].head)
      {
        tmc = (mychan_t *)n->data;

        if (is_founder(tmc, u->myuser))
          tcnt++;
      }
    }
    if ((tcnt >= me.maxchans) && (!is_sra(u->myuser)))
    {
      notice(origin, "You have too many channels registered.");
      return;
    }

    snoop("REGISTER: \2%s\2 to \2%s\2 as \2%s\2", name,
          u->nick, u->myuser->name);

    mc = mychan_add(name, pass);
    mc->founder = u->myuser;
    mc->registered = CURRTIME;
    mc->used = CURRTIME;
    mc->mlock_on |= (CMODE_NOEXT | CMODE_TOPIC);
    mc->mlock_off |= (CMODE_INVITE | CMODE_LIMIT | CMODE_KEY);
    mc->flags |= svs.defcflags;

    chanacs_add(mc, u->myuser, CA_FOUNDER);

    notice(origin, "\2%s\2 is now registered to \2%s\2.",
           mc->name, mc->founder->name);

    notice(origin,
           "The password is \2%s\2. Please write this down for "
           "future reference.", mc->pass);

    if (svs.join_chans)
      join(mc->name, svs.nick);
  }

  else
  {
    char *email = strtok(NULL, " ");

    if (!name || !pass || !email)
    {
      notice(origin, "Insufficient parameters specified for \2REGISTER\2.");
      notice(origin, "Syntax: REGISTER <username> <password> <email>");
      return;
    }

    if (!strcasecmp("KEY", pass))
    {
      if (!(mu = myuser_find(name)))
      {
        notice(origin, "No such username: \2%s\2.", name);
        return;
      }

      if (!(mu->flags & MU_WAITAUTH))
      {
        notice(origin, "\2%s\2 is not awaiting authorization.", name);
        return;
      }

      if (mu->key == atoi(email))
      {
        mu->key = 0;
        mu->flags &= ~MU_WAITAUTH;

        snoop("REGISTER:VS: \2%s\2 by \2%s\2", mu->email, origin);

        notice(origin, "\2%s\2 has now been verified.", mu->email);

        return;
      }

      snoop("REGISTER:VF: \2%s\2 by \2%s\2", mu->email, origin);

      notice(origin, "Verification failed. Invalid key for \2%s\2.",
             mu->email);

      return;
    }

    if ((strlen(name) > 32) || (strlen(pass) > 32) || (strlen(email) > 256))
    {
      notice(origin, "Invalid parameters specified for \2REGISTER\2.");
      return;
    }

    if (!validemail(email))
    {
      notice(origin, "\2%s\2 is not a valid email address.", email);
      return;
    }

    /* make sure it isn't registered already */
    mu = myuser_find(name);
    if (mu != NULL)
    {
      notice(origin, "\2%s\2 is already registered to \2%s\2.",
             mu->name, mu->email);
      return;
    }

    /* make sure they're within limits */
    for (i = 0, tcnt = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mulist[i].head)
      {
        tmu = (myuser_t *)n->data;

        if (!strcasecmp(email, tmu->email))
          tcnt++;
      }
    }
    if (tcnt >= me.maxusers)
    {
      notice(origin, "\2%s\2 has too many usernames registered.", email);
      return;
    }

    snoop("REGISTER: \2%s\2 to \2%s\2", name, email);

    mu = myuser_add(name, pass, email);
    u = user_find(origin);

    if (u->myuser)
    {
      u->myuser->identified = FALSE;
      u->myuser->user = NULL;
    }

    u->myuser = mu;
    mu->user = u;
    mu->registered = CURRTIME;
    mu->identified = TRUE;
    mu->lastlogin = CURRTIME;
    mu->flags |= svs.defuflags;

    if (me.auth == AUTH_EMAIL)
    {
      mu->key = makekey();
      mu->flags |= MU_WAITAUTH;

      notice(origin, "An email containing username activiation instructions "
             "has been sent to \2%s\2.", mu->email);
      notice(origin, "If you do not complete registration within one day your "
             "username will expire.");

      sendemail(mu->name, itoa(mu->key), 1);
    }

    notice(origin, "\2%s\2 is now registered to \2%s\2.", mu->name, mu->email);
    notice(origin,
           "The password is \2%s\2. Please write this down for "
           "future reference.", mu->pass);
  }
}

/* DROP <username|#channel> <password> */
static void do_drop(char *origin)
{
  uint32_t i;
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc, *tmc;
  node_t *n;
  char *name = strtok(NULL, " ");
  char *pass = strtok(NULL, " ");

  if (!name || !pass)
  {
    notice(origin, "Insufficient parameters specified for \2DROP\2.");
    notice(origin, "Syntax: DROP <username|#channel> <password>");
    return;
  }

  if (*name == '#')
  {
    /* we know it's a channel to DROP now */

    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    if (!is_founder(mc, u->myuser))
    {
      notice(origin, "You are not authorized to perform this operation.");
      return;
    }

    if (strcmp(pass, mc->pass))
    {
      notice(origin,
             "Authentication failed. Invalid password for \2%s\2.", mc->name);
      return;
    }

    snoop("DROP: \2%s\2 by \2%s\2 as \2%s\2", mc->name,
          u->nick, u->myuser->name);

    part(mc->name, svs.nick);
    mychan_delete(mc->name);

    notice(origin, "The channel \2%s\2 has been dropped.", name);
    return;
  }

  else
  {
    /* we know it's a username to DROP now */

    if (!(mu = myuser_find(name)))
    {
      notice(origin, "No such username: \2%s\2.", name);
      return;
    }

    if (strcmp(pass, mu->pass))
    {
      notice(origin,
             "Authentication failed. Invalid password for \2%s\2.", mu->name);
      return;
    }

    /* find all channels that are theirs and drop them */
    for (i = 1; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mclist[i].head)
      {
        tmc = (mychan_t *)n->data;
        if (tmc->founder == mu)
        {
          snoop("DROP: \2%s\2 by \2%s\2 as \2%s\2", tmc->name,
                u->nick, u->myuser->name);

          notice(origin, "The channel \2%s\2 has been dropped.", tmc->name);

          part(tmc->name, svs.nick);
          mychan_delete(tmc->name);
        }
      }
    }

    snoop("DROP: \2%s\2 by \2%s\2", mu->name, u->nick);
    if (u->myuser == mu)
      u->myuser = NULL;
    myuser_delete(mu->name);
    notice(origin, "The username \2%s\2 has been dropped.", name);
    return;
  }
}

/* UNBANME <#channel> */
static void do_unbanme(char *origin)
{
  user_t *u = user_find(origin);
  mychan_t *mc;
  char *name = strtok(NULL, " ");
  char *hostbuf;

  if (!name)
  {
    notice(origin, "Insufficient parameters specified for \2UNBANME\2.");
    notice(origin, "Syntax: UNBANME <#channel>");
    return;
  }

  /* make sure they're logged in */
  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  /* make sure the channel's registered */
  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  /* make sure they can USE this */
  if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)) &&
      (!is_xop(mc, u->myuser, CA_AOP)) && (!is_xop(mc, u->myuser, CA_VOP)))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* set an exempt on the user calling this */
  hostbuf = make_hostmask(u->nick, u->user, u->host);

  sts(":%s MODE %s +e %s", svs.nick, mc->chan->name, hostbuf);

  /* invite them back. */
  sts(":%s INVITE %s %s", svs.nick, u->nick, mc->chan->name);

  notice(origin, "You have been unbanned from \2%s\2.", mc->chan->name);
  notice(origin, "If you still cannot rejoin, consider using \2RECOVER\2");
}

/* KLINE ADD|DEL|LIST */
static void do_kline(char *origin)
{
  user_t *u;
  kline_t *k;
  char *cmd = strtok(NULL, " ");
  char *s;

  if (!cmd)
  {
    notice(origin, "Insufficient parameters for \2KLINE\2.");
    notice(origin, "Syntax: KLINE ADD|DEL|LIST");
    return;
  }

  if (!strcasecmp(cmd, "ADD"))
  {
    char *target = strtok(NULL, " ");
    char *token = strtok(NULL, " ");
    char *treason, reason[BUFSIZE];
    long duration;

    if (!target || !token)
    {
      notice(origin, "Insufficient parameters for \2KLINE ADD\2.");
      notice(origin, "Syntax: KLINE ADD <nick|hostmask> [!P|!T <minutes>] "
             "<reason>");
      return;
    }

    if (!strcasecmp(token, "!P"))
    {
      duration = 0;
      treason = strtok(NULL, "");

      if (!treason)
      {
        notice(origin, "Insufficient parameters for \2KLINE ADD\2.");
        notice(origin, "Syntax: KLINE ADD <nick|hostmask> [!P|!T <minutes>] "
               "<reason>");
        return;
      }

      strlcpy(reason, treason, BUFSIZE);
    }
    else if (!strcasecmp(token, "!T"))
    {
      s = strtok(NULL, " ");
      duration = (atol(s) * 60);
      treason = strtok(NULL, "");

      if (!treason)
      {
        notice(origin, "Insufficient parameters for \2KLINE ADD\2.");
        notice(origin, "Syntax: KLINE ADD <nick|hostmask> [!P|!T <minutes>] "
               "<reason>");
        return;
      }

      strlcpy(reason, treason, BUFSIZE);
    }
    else
    {
      duration = svs.kline_time;
      strlcpy(reason, token, BUFSIZE);
      treason = strtok(NULL, "");

      if (treason)
      {
        strlcat(reason, " ", BUFSIZE);
        strlcat(reason, treason, BUFSIZE);
      }
    }

    if (!(strchr(target, '@')))
    {
      if (!(u = user_find(target)))
      {
        notice(origin, "No such user: \2%s\2.", target);
        return;
      }

      if (kline_find(u->user, u->host))
      {
        notice(origin, "KLINE \2%s@%s\2 is already matched in the database.",
               u->user, u->host);
        return;
      }

      k = kline_add(u->user, u->host, reason, duration);
      k->setby = sstrdup(origin);
    }
    else
    {
      char *userbuf = strtok(target, "@");
      char *hostbuf = strtok(NULL, "@");
      char *tmphost;
      int i = 0;

      /* make sure hostbuf tokenized into something!
       * bug fix via irc.malkier.net on 2009-12-09
       * first time i've edited this code in five years...
       *     -- rakaur
       */
      if (!hostbuf)
      {
          notice(origin, "Malformed hostname for \2KLINE\2.");
          return;
      }

      /* make sure there's at least 5 non-wildcards */
      for (tmphost = hostbuf; *tmphost; tmphost++)
      {
        if (*tmphost != '*' && *tmphost != '?' && *tmphost != '.')
          i++;
      }

      if (i < 5)
      {
        notice(origin, "Invalid host: \2%s\2.", hostbuf);
        return;
      }

      if (kline_find(userbuf, hostbuf))
      {
        notice(origin, "KLINE \2%s@%s\2 is already matched in the database.",
               userbuf, hostbuf);
        return;
      }

      k = kline_add(userbuf, hostbuf, reason, duration);
      k->setby = sstrdup(origin);
    }

    if (duration)
      notice(origin, "TKLINE on \2%s@%s\2 was successfully added and will "
             "expire in %s.", k->user, k->host, timediff(duration));
    else
      notice(origin, "KLINE on \2%s@%s\2 was successfully added.",
             k->user, k->host);

    snoop("KLINE:ADD: \2%s@%s\2 by \2%s\2 for \2%s\2", k->user, k->host,
          origin, k->reason);

    return;
  }

  if (!strcasecmp(cmd, "DEL"))
  {
    char *target = strtok(NULL, " ");
    char *userbuf, *hostbuf;
    uint32_t number;

    if (!target)
    {
      notice(origin, "Insuccicient parameters for \2KLINE DEL\2.");
      notice(origin, "Syntax: KLINE DEL <hostmask>");
      return;
    }

    if (strchr(target, ','))
    {
      int start = 0, end = 0, i;
      char t[16];

      s = strtok(target, ",");

      do
      {
        if (strchr(s, ':'))
        {
          for (i = 0; *s != ':'; s++, i++)
            t[i] = *s;

          t[++i] = '\0';
          start = atoi(t);

          s++;                  /* skip past the : */

          for (i = 0; *s != '\0'; s++, i++)
            t[i] = *s;

          t[++i] = '\0';
          end = atoi(t);

          for (i = start; i <= end; i++)
          {
            if (!(k = kline_find_num(i)))
            {
              notice(origin, "No such KLINE with number \2%d\2.", i);
              continue;
            }

            snoop("KLINE:DEL: \2%s@%s\2 by \2%s\2", k->user, k->host, origin);

            notice(origin, "KLINE on \2%s@%s\2 has been successfully removed.",
                   k->user, k->host);

            kline_delete(k->user, k->host);
          }

          continue;
        }

        number = atoi(s);

        if (!(k = kline_find_num(number)))
        {
          notice(origin, "No such KLINE with number \2%d\2.", number);
          return;
        }

        snoop("KLINE:DEL: \2%s@%s\2 by \2%s\2", k->user, k->host, origin);

        notice(origin, "KLINE on \2%s@%s\2 has been successfully removed.",
               k->user, k->host);

        kline_delete(k->user, k->host);
      } while ((s = strtok(NULL, ",")));

      return;
    }

    if (!strchr(target, '@'))
    {
      int start = 0, end = 0, i;
      char t[16];

      if (strchr(target, ':'))
      {
        for (i = 0; *target != ':'; target++, i++)
          t[i] = *target;

        t[++i] = '\0';
        start = atoi(t);

        target++;               /* skip past the : */

        for (i = 0; *target != '\0'; target++, i++)
          t[i] = *target;

        t[++i] = '\0';
        end = atoi(t);

        for (i = start; i <= end; i++)
        {
          if (!(k = kline_find_num(i)))
          {
            notice(origin, "No such KLINE with number \2%d\2.", i);
            continue;
          }

          snoop("KLINE:DEL: \2%s@%s\2 by \2%s\2", k->user, k->host, origin);

          notice(origin, "KLINE on \2%s@%s\2 has been successfully removed.",
                 k->user, k->host);

          kline_delete(k->user, k->host);
        }

        return;
      }

      number = atoi(target);

      if (!(k = kline_find_num(number)))
      {
        notice(origin, "No such KLINE with number \2%d\2.", number);
        return;
      }

      snoop("KLINE:DEL: \2%s@%s\2 by \2%s\2", k->user, k->host, origin);

      notice(origin, "KLINE on \2%s@%s\2 has been successfully removed.",
             k->user, k->host);

      kline_delete(k->user, k->host);

      return;
    }

    userbuf = strtok(target, "@");
    hostbuf = strtok(NULL, "@");

    if (!kline_find(userbuf, hostbuf))
    {
      notice(origin, "No such KLINE: \2%s@%s\2.", userbuf, hostbuf);
      return;
    }

    kline_delete(userbuf, hostbuf);

    snoop("KLINE:DEL: \2%s@%s\2 by \2%s\2", userbuf, hostbuf, origin);

    notice(origin, "KLINE on \2%s@%s\2 has been successfully removed.",
           userbuf, hostbuf);

    return;
  }

  if (!strcasecmp(cmd, "LIST"))
  {
    boolean_t full = FALSE;
    node_t *n;
    int i = 0;

    s = strtok(NULL, " ");

    if (s && !strcasecmp(s, "FULL"))
      full = TRUE;

    if (full)
      notice(origin, "KLINE list (with reasons):");
    else
      notice(origin, "KLINE list:");

    LIST_FOREACH(n, klnlist.head)
    {
      k = (kline_t *)n->data;

      i++;

      if (k->duration && full)
        notice(origin, "%d: %s@%s - by \2%s\2 - expires in \2%s\2 - (%s)",
               k->number, k->user, k->host, k->setby,
               timediff(k->expires - CURRTIME), k->reason);
      else if (k->duration && !full)
        notice(origin, "%d: %s@%s - by \2%s\2 - expires in \2%s\2", k->number,
               k->user, k->host, k->setby, timediff(k->expires - CURRTIME));
      else if (!k->duration && full)
        notice(origin, "%d: %s@%s - by \2%s\2 - \2permanent\2 - (%s)",
               k->number, k->user, k->host, k->setby, k->reason);
      else
        notice(origin, "%d: %s@%s - by \2%s\2 - \2permanent\2", k->number,
               k->user, k->host, k->setby);
    }

    notice(origin, "Total of \2%d\2 %s in KLINE list.", i,
           (i == 1) ? "entry" : "entries");
  }
}

static void do_update(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save(NULL);
}

/* REHASH */
static void do_rehash(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save(NULL);

  snoop("REHASH: \2%s\2", origin);
  wallops("Rehashing \2%s\2 by request of \2%s\2.", config_file, origin);

  if (!conf_rehash())
  {
    notice(origin,
           "REHASH of \2%s\2 failed. Please corrrect any errors in the "
           "file and try again.", config_file);
  }
}

/* STATUS [#channel] */
static void do_status(char *origin)
{
  user_t *u = user_find(origin);
  char *chan = strtok(NULL, " ");

  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if (chan)
  {
    mychan_t *mc = mychan_find(chan);

    if (*chan != '#')
    {
      notice(origin, "Invalid parameters specified for \2STATUS\2.");
      return;
    }

    if (!mc)
    {
      notice(origin, "No such channel: \2%s\2.", chan);
      return;
    }

    if (is_founder(mc, u->myuser))
    {
      notice(origin, "You are founder on \2%s\2.", mc->name);
      return;
    }

    if (is_xop(mc, u->myuser, CA_VOP))
    {
      notice(origin, "You are VOP on \2%s\2.", mc->name);
      return;
    }

    if (is_xop(mc, u->myuser, CA_AOP))
    {
      notice(origin, "You are AOP on \2%s\2.", mc->name);
      return;
    }

    if (is_xop(mc, u->myuser, CA_SOP))
    {
      notice(origin, "You are SOP on \2%s\2.", mc->name);
      return;
    }

    notice(origin, "You are a normal user on \2%s\2.", mc->name);

    return;
  }

  notice(origin, "You are logged in as \2%s\2.", u->myuser->name);

  if (is_sra(u->myuser))
    notice(origin, "You are a services root administrator.");

  if (is_admin(u))
    notice(origin, "You are a server administrator.");

  if (is_ircop(u))
    notice(origin, "You are an IRC operator.");
}

/* SENDPASS <username|#channel> */
static void do_sendpass(char *origin)
{
  myuser_t *mu;
  mychan_t *mc;
  char *name = strtok(NULL, " ");

  if (!name)
  {
    notice(origin, "Insufficient parameters for \2SENDPASS\2.");
    notice(origin, "Syntax: SENDPASS <username|#channel>");
    return;
  }

  if (*name == '#')
  {
    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    if (mc->founder)
    {
      notice(origin, "The password for \2%s\2 has been sent to \2%s\2.",
             mc->name, mc->founder->email);

      sendemail(mc->name, mc->pass, 4);

      return;
    }
  }
  else
  {
    if (!(mu = myuser_find(name)))
    {
      notice(origin, "No such username: \2%s\2.", name);
      return;
    }

    notice(origin, "The password for \2%s\2 has been sent to \2%s\2.",
           mu->name, mu->email);

    sendemail(mu->name, mu->pass, 2);

    return;
  }
}

/* GLOBAL <parameters>|SEND|CLEAR */
static void do_global(char *origin)
{
  static BlockHeap *glob_heap = NULL;
  struct global_ *global;
  static list_t globlist;
  node_t *n, *n2, *tn;
  tld_t *tld;
  char *params = strtok(NULL, "");
  static char *sender = NULL;

  if (!params)
  {
    notice(origin, "Insufficient parameters for \2GLOBAL\2.");
    notice(origin, "Syntax: GLOBAL <parameters>|SEND|CLEAR");
    return;
  }

  if (!strcasecmp("CLEAR", params))
  {
    if (!globlist.count)
    {
      notice(origin, "No message to clear.");
      return;
    }

    /* destroy the list we made */
    LIST_FOREACH_SAFE(n, tn, globlist.head)
    {
      global = (struct global_ *)n->data;
      node_del(n, &globlist);
      node_free(n);
      free(global->text);
      BlockHeapFree(glob_heap, global);
    }

    BlockHeapDestroy(glob_heap);
    glob_heap = NULL;
    free(sender);
    sender = NULL;

    notice(origin, "The pending message has been deleted.");

    return;
  }

  if (!strcasecmp("SEND", params))
  {
    if (!globlist.count)
    {
      notice(origin, "No message to send.");
      return;
    }

    introduce_nick(svs.global, svs.user, svs.host, svs.real, "io");

    LIST_FOREACH(n, globlist.head)
    {
      global = (struct global_ *)n->data;

      /* send to every tld */
      LIST_FOREACH(n2, tldlist.head)
      {
        tld = (tld_t *)n2->data;

        sts(":%s NOTICE $$*%s :[Network Notice] %s", svs.global, tld->name,
            global->text);
      }
    }

    /* destroy the list we made */
    LIST_FOREACH_SAFE(n, tn, globlist.head)
    {
      global = (struct global_ *)n->data;
      node_del(n, &globlist);
      node_free(n);
      free(global->text);
      BlockHeapFree(glob_heap, global);
    }

    BlockHeapDestroy(glob_heap);
    glob_heap = NULL;
    free(sender);
    sender = NULL;

    sts(":%s QUIT :finished", svs.global);
    user_delete(svs.global);

    snoop("GLOBAL: \2%s\2", origin);

    notice(origin, "The global notice has been sent.");

    return;
  }

  if (!glob_heap)
    glob_heap = BlockHeapCreate(sizeof(struct global_), 5);

  if (!sender)
    sender = sstrdup(origin);

  if (irccasecmp(sender, origin))
  {
    notice(origin, "There is already a GLOBAL in progress by \2%s\2.", sender);
    return;
  }

  global = BlockHeapAlloc(glob_heap);

  global->text = sstrdup(params);

  n = node_create();
  node_add(global, n, &globlist);

  notice(origin, "Stored text to be sent as line %d. Use \2GLOBAL SEND\2 "
         "to send message, \2GLOBAL CLEAR\2 to delete the pending message, "
         "or \2GLOBAL\2 to store additional lines.", globlist.count);
}

/* RAW <parameters> */
static void do_raw(char *origin)
{
  char *s = strtok(NULL, "");

  if (!svs.raw)
    return;

  if (!s)
  {
    notice(origin, "Insufficient parameters for \2RAW\2.");
    notice(origin, "Syntax: RAW <parameters>");
    return;
  }

  snoop("RAW: \"%s\" by \2%s\2", s, origin);
  sts("%s", s);
}

/* INJECT <parameters> */
static void do_inject(char *origin)
{
  char *inject;
  static boolean_t injecting = FALSE;
  inject = strtok(NULL, "");

  if (!svs.raw)
    return;

  if (!inject)
  {
    notice(origin, "Insufficient parameters for \2INJECT\2.");
    notice(origin, "Syntax: INJECT <parameters>");
    return;
  }

  /* looks like someone INJECT'd an INJECT command.
   * this is probably a bad thing.
   */
  if (injecting == TRUE)
  {
    notice(origin, "You cannot inject an INJECT command.");
    return;
  }

  injecting = TRUE;
  irc_parse(inject);
  injecting = FALSE;
}

static void do_restart(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save(NULL);

  snoop("RESTART: \2%s\2", origin);
  wallops("Restarting in \2%d\2 seconds by request of \2%s\2.", me.restarttime,
          origin);

  runflags |= RF_RESTART;
}

static void do_shutdown(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save(NULL);

  snoop("SHUTDOWN: \2%s\2", origin);
  wallops("Shutting down by request of \2%s\2.", origin);

  runflags |= RF_SHUTDOWN;
}

/* *INDENT-OFF* */

/* commands we understand */
struct command_ commands[] = {
  { "LOGIN",    AC_NONE,  do_login    },
  { "LOGOUT",   AC_NONE,  do_logout   },
  { "HELP",     AC_NONE,  do_help     },
  { "SET",      AC_NONE,  do_set      },
  { "SOP",      AC_NONE,  do_sop      },
  { "AOP",      AC_NONE,  do_aop      },
  { "VOP",      AC_NONE,  do_vop      },
  { "OP",       AC_NONE,  do_op       },
  { "DEOP",     AC_NONE,  do_deop     },
  { "VOICE",    AC_NONE,  do_voice    },
  { "DEVOICE",  AC_NONE,  do_devoice  },
  { "INVITE",   AC_NONE,  do_invite   },
  { "INFO",     AC_NONE,  do_info     },
  { "RECOVER",  AC_NONE,  do_recover  },
  { "REGISTER", AC_NONE,  do_register },
  { "DROP",     AC_NONE,  do_drop     },
  { "UNBANME",  AC_NONE,  do_unbanme  },
  { "KLINE",    AC_IRCOP, do_kline    },
  { "UPDATE",   AC_SRA,   do_update   },
  { "REHASH",   AC_SRA,   do_rehash   },
  { "STATUS",   AC_NONE,  do_status   },
  { "SENDPASS", AC_IRCOP, do_sendpass },
  { "GLOBAL",   AC_IRCOP, do_global   },
  { "RAW",      AC_SRA,   do_raw      },
  { "INJECT",   AC_SRA,   do_inject   },
  { "RESTART",  AC_SRA,   do_restart  },
  { "SHUTDOWN", AC_SRA,   do_shutdown },
  { NULL }
};

/* *INDENT-ON* */

struct command_ *cmd_find(char *origin, char *command)
{
  user_t *u = user_find(origin);
  struct command_ *c;

  for (c = commands; c->name; c++)
  {
    if (!strcasecmp(command, c->name))
    {
      /* no special access required, so go ahead... */
      if (c->access == AC_NONE)
        return c;

      /* sra? */
      if ((c->access == AC_SRA) && (is_sra(u->myuser)))
        return c;

      /* ircop? */
      if ((c->access == AC_IRCOP) && (is_sra(u->myuser) || (is_ircop(u))))
        return c;

      /* otherwise... */
      else
      {
        notice(origin, "You are not authorized to perform this operation.");
        return NULL;
      }
    }
  }

  /* it's a command we don't understand */
  notice(origin, "Invalid command. Please use \2HELP\2 for help.");
  return NULL;
}
