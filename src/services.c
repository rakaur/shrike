/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains client interaction routines.
 *
 * $$Id$
 */

#include "../inc/shrike.h"

/* introduce a client */
void introduce_nick(char *nick, char *user, char *host, char *real,
                    char *modes)
{
  time_t now = time(NULL);

  sts("NICK %s %ld 1 +%s %s %s %s :%s",
      nick, now, modes, user, host, me.name, real);

  user_add(nick, user, host, me.me);
}

/* join a channel */
void join(char *chan, char *nick)
{
  channel_t *c = channel_find(chan);
  chanuser_t *cu;
  time_t now = time(NULL);

  if (!c)
  {
    sts(":%s SJOIN %ld %s +nt :@%s", me.name, now, chan, nick);

    c = channel_add(chan, now);
  }
  else
  {
    if ((cu = chanuser_find(c, user_find(svs.nick))))
    {
      slog(0, LG_DEBUG, "join(): i'm already in `%s'", c->name);
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
  introduce_nick(svs.nick, svs.user, svs.host, svs.real, "io");
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

  va_start(ap, fmt);
  vsnprintf(buf, BUFSIZE, fmt, ap);

  sts(":%s WALLOPS :%s", svs.nick, buf);
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
  chanuser_t *cu;

  if (!u || !c)
    return;

  if (!(cu = chanuser_find(c, u)))
    return;

  if (!irccasecmp(svs.chan, c->name))
    return;

  sts(":%s PART %s", u->nick, c->name);

  chanuser_delete(c, u);
}

void expire_check(event_t *e)
{
  uint32_t i, j;
  myuser_t *mu;
  mychan_t *mc;
  node_t *n1, *n2, *tn;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n1, mulist[i].head)
    {
      mu = (myuser_t *)n1->data;

      if (MU_HOLD & mu->flags)
        continue;

      if ((time(NULL) - mu->lastlogin) >= me.expire)
      {
        /* kill all their channels */
        for (j = 0; j < HASHSIZE; j++)
        {
          LIST_FOREACH(tn, mclist[j].head)
          {
            mc = (mychan_t *)tn->data;

            if (mc->founder == mu)
            {
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

      if ((time(NULL) - mc->used) >= me.expire)
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

  /* we don't care about channels */
  if (*parv[0] == '#')
    return;

  /* this should never happen */
  if (*parv[0] == '&')
  {
    slog(0, LG_NOTICE, "services(): got parv with local channel: %s", parv[0]);
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
           "\001VERSION shrike-%s.%s. %s %s%s %s\001",
           version, build, me.name,
           (me.loglevel & LG_DEBUG) ? "d" : "",
           (runflags & RF_LIVE) ? "n" : "", version);

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

/* this is merely for debugging */
static void do_list(char *origin)
{
  sts(":%s PRIVMSG #shrike :servers: %d", svs.nick, cnt.server);
  sts(":%s PRIVMSG #shrike :users: %d", svs.nick, cnt.user);
  sts(":%s PRIVMSG #shrike :channels: %d", svs.nick, cnt.chan);
  sts(":%s PRIVMSG #shrike :chanusers: %d", svs.nick, cnt.chanuser);
  sts(":%s PRIVMSG #shrike :myusers: %d", svs.nick, cnt.myuser);
  sts(":%s PRIVMSG #shrike :chanacs: %d", svs.nick, cnt.chanacs);
  sts(":%s PRIVMSG #shrike :events: %d", svs.nick, cnt.event);
  sts(":%s PRIVMSG #shrike :sras: %d", svs.nick, cnt.sra);

  sts(":%s PRIVMSG #shrike :%s %s %s %s %s", svs.nick,
      me.pass, me.netname, me.adminname, me.adminemail, svs.chan);
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
    }

    mu->lastlogin = time(NULL);

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
    free(mu->lastfail);

  strlcpy(buf, u->nick, BUFSIZE);
  strlcat(buf, "!", BUFSIZE);
  strlcat(buf, u->user, BUFSIZE);
  strlcat(buf, "@", BUFSIZE);
  strlcat(buf, u->host, BUFSIZE);
  mu->lastfail = sstrdup(buf);
  mu->failnum++;
  mu->lastfailon = time(NULL);

  if (mu->failnum == 10)
  {
    tm = *localtime(&mu->lastfailon);
    strftime(strfbuf, sizeof(strfbuf) - 1, "%b %d %H:%M:%S %Y", &tm);

    wallops("Warning: Numerous failed login attempts to \2%s\2. Last attempt "
            "received from \2%s\2 on %s", mu->name, mu->lastfail, strfbuf);
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
  u->myuser = NULL;
}

/* SOP|AOP|VOP <#channel> ADD|DEL|LIST <username> */
static void do_xop(char *origin, uint8_t level)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;
  chanacs_t *ca;
  char *chan = strtok(NULL, " ");
  char *cmd = strtok(NULL, " ");
  char *uname = strtok(NULL, " ");

  if (!cmd || !chan)
  {
    notice(origin, "Insufficient parameters specificed for \2xOP\2.");
    notice(origin, "Syntax: SOP|AOP|VOP <#channel> ADD|DEL|LIST <username>");
    return;
  }

  if ((strcasecmp("LIST", cmd)) && (!uname))
  {
    notice(origin, "Insufficient parameters specificed for \2xOP\2.");
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
      if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if (MU_NOOP & mu->flags)
      {
        notice(origin, "\2%s\2 does not wish to be added to access lists.",
               mu->name);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_VOP)))
      {
        notice(origin, "\2%s\2 is already on the VOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_AOP)))
      {
        chanacs_delete(mc, mu, CA_AOP);
        chanacs_add(mc, mu, CA_VOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2AOP\2 to \2VOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2AOP\2 to \2VOP\2.",
                u->nick, mu->name);

        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_SOP)))
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
      if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_AOP)))
      {
        notice(origin, "\2%s\2 is already on the AOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_VOP)))
      {
        chanacs_delete(mc, mu, CA_VOP);
        chanacs_add(mc, mu, CA_AOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2VOP\2 to \2AOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2VOP\2 to \2AOP\2.",
                u->nick, mu->name);

        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_SOP)))
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
      if (!is_founder(mc, u->myuser))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_SOP)))
      {
        notice(origin, "\2%s\2 is already on the SOP list for \2%s\2.",
               mu->name, mc->name);
        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_VOP)))
      {
        chanacs_delete(mc, mu, CA_VOP);
        chanacs_add(mc, mu, CA_SOP);

        notice(origin, "\2%s\2's access on \2%s\2 has been changed from "
               "\2VOP\2 to \2SOP\2.", mu->name, mc->name);

        verbose(mc, "\2%s\2 changed \2%s\2's access from \2VOP\2 to \2SOP\2.",
                u->nick, mu->name);

        return;
      }

      if ((ca = chanacs_find(mc, mu, CA_AOP)))
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
      if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if (!(ca = chanacs_find(mc, mu, CA_VOP)))
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
      if ((!is_founder(mc, u->myuser)) && (!is_xop(mc, u->myuser, CA_SOP)))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if (!(ca = chanacs_find(mc, mu, CA_AOP)))
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
      if (!is_founder(mc, u->myuser))
      {
        notice(origin, "You are not authorized to perform this operation.");
        return;
      }

      mu = myuser_find(uname);
      if (!mu)
      {
        notice(origin, "The username \2%s\2 is not registered.", uname);
        return;
      }

      if (!(ca = chanacs_find(mc, mu, CA_SOP)))
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
    node_t *n;

    if ((!is_founder(mc, u->myuser)) &&
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
          if (ca->myuser->user)
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
          if (ca->myuser->user)
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

  if (!chan)
  {
    notice(origin, "Insufficient parameters specificed for \2OP\2.");
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

  if ((!is_founder(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_SOP | CA_VOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* figure out who we're going to op */
  if (nick)
  {
    if (!(u = user_find(nick)))
    {
      notice(origin, "No such nickname: \2%s\2.", nick);
      return;
    }
  }

  if ((MC_SECURE & mc->flags) && (!u->myuser))
  {
    notice(origin, "The \2SECURE\2 flag is set for \2%s\2.", mc->name);
    return;
  }
  else if ((MC_SECURE & mc->flags) && (!is_founder(mc, u->myuser)) &&
           (!is_xop(mc, u->myuser, (CA_SOP | CA_AOP))))
  {
    notice(origin, "\2%s\2 could not be opped on \2%s\2.", u->nick, mc->name);
    return;
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
    notice(origin, "Insufficient parameters specificed for \2DEOP\2.");
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

  if ((!is_founder(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* figure out who we're going to devoice */
  if (nick)
  {
    if (!(u = user_find(nick)))
    {
      notice(origin, "No such nickname: \2%s\2.", nick);
      return;
    }
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
    notice(origin, "Insufficient parameters specificed for \2VOICE\2.");
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

  if ((!is_founder(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* figure out who we're going to voice */
  if (nick)
  {
    if (!(u = user_find(nick)))
    {
      notice(origin, "No such nickname: \2%s\2.", nick);
      return;
    }
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
    notice(origin, "Insufficient parameters specificed for \2DEVOICE\2.");
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

  if ((!is_founder(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* figure out who we're going to devoice */
  if (nick)
  {
    if (!(u = user_find(nick)))
    {
      notice(origin, "No such nickname: \2%s\2.", nick);
      return;
    }
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
    notice(origin, "Insufficient parameters specificed for \2INVITE\2.");
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

  if ((!is_founder(mc, u->myuser)) &&
      (!is_xop(mc, u->myuser, (CA_SOP | CA_AOP | CA_VOP))))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  /* figure out who we're going to invite */
  if (nick)
  {
    if (!(u = user_find(nick)))
    {
      notice(origin, "No such nickname: \2%s\2.", nick);
      return;
    }
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
  myuser_t *mu;
  mychan_t *mc;
  char *name = strtok(NULL, " ");
  char buf[BUFSIZE], strfbuf[32];
  struct tm tm;

  if (!name)
  {
    notice(origin, "Insufficient parameters specificed for \2INFO\2.");
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
      notice(origin, "Founder    : %s (not logged in)");

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
        notice(origin, "Mode lock  : %s %s", buf, (params) ? params : "");
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

    notice(origin, "Email      : %s", mu->email);

    *buf = '\0';

    if (MU_HOLD & mu->flags)
      strcat(buf, "HOLD");

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

/* REGISTER <username|#channel> <password> [email] */
static void do_register(char *origin)
{
  user_t *u;
  channel_t *c;
  chanuser_t *cu;
  myuser_t *mu;
  mychan_t *mc;
  char *name = strtok(NULL, " ");
  char *pass = strtok(NULL, " ");

  if (!name)
  {
    notice(origin, "Insufficient parameters specificed for \2REGISTER\2.");
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

    u = user_find(origin);
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

    /* make sure they're logged in */
    if (!u->myuser || !u->myuser->identified)
    {
      notice(origin, "You are not logged in.");
      return;
    }

    /* make sure it isn't registered already */
    mc = mychan_find(name);
    if (mc != NULL)
    {
      notice(origin, "\2%s\2 is already registered to \2%s\2.",
             name, mc->founder->name);
      return;
    }

    snoop("REGISTER: \2%s\2 to \2%s\2 as \2%s\2", name,
          u->nick, u->myuser->name);

    mc = mychan_add(name, pass);
    mc->founder = u->myuser;
    mc->registered = time(NULL);
    mc->used = time(NULL);
    mc->mlock_on |= (CMODE_NOEXT | CMODE_TOPIC);
    mc->mlock_off |= (CMODE_INVITE | CMODE_LIMIT | CMODE_KEY);
    mc->flags |= MC_SECURE;

    chanacs_add(mc, u->myuser, CA_FOUNDER);

    notice(origin, "\2%s\2 is now registered to \2%s\2.",
           mc->name, mc->founder->name);

    notice(origin,
           "The password is \2%s\2. Please write this down for "
           "future reference.", mc->pass);

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

    if ((strlen(name) > 32) || (strlen(pass) > 32) || (strlen(email) > 256))
    {
      notice(origin, "Invalid parameters specified for \2REGISTER\2.");
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

    snoop("REGISTER: \2%s\2 to \2%s\2", name, email);

    mu = myuser_add(name, pass, email);
    u = user_find(origin);

    u->myuser = mu;
    mu->user = u;
    mu->registered = time(NULL);
    mu->identified = TRUE;
    mu->lastlogin = time(NULL);

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

    snoop("DROP: \2%s\2 by \2%s\2 as \2%s\2", mu->name,
          u->nick, u->myuser->name);
    if (u->myuser == mu)
      u->myuser = NULL;
    myuser_delete(mu->name);
    notice(origin, "The username \2%s\2 has been dropped.", name);
    return;
  }
}

static void do_update(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save();
}

/* REHASH */
static void do_rehash(char *origin)
{
  snoop("UPDATE: \2%s\2", origin);
  wallops("Updating database by request of \2%s\2.", origin);
  expire_check(NULL);
  db_save();

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

/* RAW <parameters> */
static void do_raw(char *origin)
{
  char *s = strtok(NULL, "");

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

/* *INDENT-OFF* */

/* commands we understand */
struct command_ commands[] = {
  { "LIST",     AC_NONE, do_list     },
  { "LOGIN",    AC_NONE, do_login    },
  { "LOGOUT",   AC_NONE, do_logout   },
  { "HELP",     AC_NONE, do_help     },
  { "SET",      AC_NONE, do_set      },
  { "SOP",      AC_NONE, do_sop      },
  { "AOP",      AC_NONE, do_aop      },
  { "VOP",      AC_NONE, do_vop      },
  { "OP",       AC_NONE, do_op       },
  { "DEOP",     AC_NONE, do_deop     },
  { "VOICE",    AC_NONE, do_voice    },
  { "DEVOICE",  AC_NONE, do_devoice  },
  { "INVITE",   AC_NONE, do_invite   },
  { "INFO",     AC_NONE, do_info     },
  { "REGISTER", AC_NONE, do_register },
  { "DROP",     AC_NONE, do_drop     },
  { "UPDATE",   AC_SRA,  do_update   },
  { "REHASH",   AC_SRA,  do_rehash   },
  { "STATUS",   AC_NONE, do_status   },
  { "RAW",      AC_SRA,  do_raw      },
  { "INJECT",   AC_SRA,  do_inject   },
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
      if ((c->access == AC_IRCOP) && (is_ircop(u)))
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
