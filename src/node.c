/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains linked list functions.
 * Idea from ircd-hybrid.
 *
 * $$Id$
 */

#include "../inc/shrike.h"

list_t eventlist;
list_t sralist;
list_t servlist[HASHSIZE];
list_t userlist[HASHSIZE];
list_t chanlist[HASHSIZE];
list_t mulist[HASHSIZE];
list_t mclist[HASHSIZE];

boolean_t check_events = FALSE;

/*************
 * L I S T S *
 *************/

/* creates a new node */
node_t *node_create(void)
{
  node_t *n;

  /* allocate it */
  n = (node_t *)scalloc(sizeof(node_t), 1);

  /* initialize */
  n->next = n->prev = n->data = NULL;

  /* up the count */
  cnt.node++;

  /* return a pointer to the new node */
  return n;
}

/* frees a node */
void node_free(node_t *n)
{
  /* free it */
  free(n);

  /* down the count */
  cnt.node--;
}

/* adds a node to the end of a list */
void node_add(void *data, node_t *n, list_t *l)
{
  node_t *tn;

  n->data = data;

  /* first node? */
  if (!l->head)
  {
    l->head = n;
    l->tail = NULL;
    l->count++;
    return;
  }

  /* find the last node */
  LIST_FOREACH_NEXT(tn, l->head);

  /* set the our `prev' to the last node */
  n->prev = tn;

  /* set the last node's `next' to us */
  n->prev->next = n;

  /* set the list's `tail' to us */
  l->tail = n;

  /* up the count */
  l->count++;
}

/* removes a node from a list */
void node_del(node_t *n, list_t *l)
{
  /* are we the head? */
  if (!n->prev)
    l->head = n->next;
  else
    n->prev->next = n->next;

  /* are we the tail? */
  if (!n->next)
    l->tail = n->prev;
  else
    n->next->prev = n->prev;

  /* down the count */
  l->count--;
}

/* finds a node by `data' */
node_t *node_find(void *data, list_t *l)
{
  node_t *n;

  LIST_FOREACH(n, l->head) if (n->data == data)
    return n;

  return NULL;
}

/***************
 * E V E N T S *
 ***************/

/* checks all pending events */
void event_check(void)
{
  event_t *e, *te;
  node_t *n;
  uint32_t now = time_msec();
  int32_t diff = 0;

  if (check_events == TRUE)
    return;

  check_events = TRUE;

  LIST_FOREACH(n, eventlist.head)
  {
    e = (event_t *)n->data;

    diff = e->timeout - now;

    if (!e->timeout)
    {
      te = e->next;

      node_del(n, &eventlist);
      node_free(n);

      free(e);

      cnt.event--;

      e = te;
    }

    if (diff > 0)
      continue;

    e->func(e);

    if (e->repeat == TRUE)
    {
      e->timeout = now + e->delay;
      continue;
    }

    te = e->next;

    node_del(n, &eventlist);
    node_free(n);

    free(e);

    cnt.event--;

    e = te;
  }

  if (me.connected)
  {
    /* check to see if our uplink is dead
       diff = now - me.uplinkpong;

       if (diff > 300)
       {
       slog(0, LG_INFO,
       "check_events(): no response from server in %d seconds; "
       "disconnecting", diff);

       close(servsock);
       servsock = -1;
       me.connected = FALSE;

       LIST_FOREACH(n, eventlist.head)
       {
       e = (event_t *)n->data;

       if (!strcasecmp("Uplink ping", e->name))
       {
       event_del(e);
       return;
       }
       }
       } */
  }

  check_events = FALSE;
}

/* add an event to the list to be triggered in `delay' seconds.  if `repeat' is
 * TRUE, do not delete the event after it's triggered.
 */
event_t *event_add(char *name,
                   int delay, void (*func) (event_t *), boolean_t repeat)
{
  if (delay > 4294967)
    delay = 4294967;

  return event_add_ms(name, delay * 1000, func, repeat);
}

event_t
    *event_add_ms(char *name, int delay, void (*func) (event_t *),
                  boolean_t repeat)
{
  node_t *n;
  event_t *e;

  n = node_create();
  e = (event_t *)scalloc(sizeof(event_t), 1);

  e->settime = CURRTIME;
  e->timeout = time_msec() + delay;
  e->delay = delay;
  e->func = func;
  e->repeat = repeat;
  strlcpy(e->name, name, 127);

  node_add(e, n, &eventlist);

  slog(0, LG_DEBUG, "event_add_ms(): ``%s''", e->name);

  cnt.event++;

  return e;
}

/* removes a timer from the list */
void event_del(event_t *e)
{
  node_t *n;
  event_t *te;

  n = node_find(e, &eventlist);
  te = (event_t *)n->data;

  if (!te)
    return;

  if (check_events == TRUE)
  {
    e->timeout = 0;
    return;
  }

  node_del(n, &eventlist);
  node_free(n);

  cnt.event--;

  slog(0, LG_DEBUG, "event_del(): ``%s''", e->name);

  free(te);
}

/***********
 * S R A S *
 ***********/

sra_t *sra_add(char *name)
{
  sra_t *sra;
  myuser_t *mu = myuser_find(name);
  node_t *n = node_create();

  slog(0, LG_DEBUG, "sra_add(): %s", (mu) ? mu->name : name);

  sra = scalloc(sizeof(sra_t), 1);

  node_add(sra, n, &sralist);

  if (mu)
  {
    sra->myuser = mu;
    mu->sra = sra;
  }
  else
    sra->name = sstrdup(name);

  cnt.sra++;

  return sra;
}

void sra_delete(myuser_t *myuser)
{
  sra_t *sra = sra_find(myuser);
  node_t *n;

  if (!sra)
  {
    slog(0, LG_CRIT, "sra_delete(): called for nonexistant sra: %s",
         myuser->name);

    return;
  }

  slog(0, LG_DEBUG, "sra_delete(): %s",
       (sra->myuser) ? sra->myuser->name : sra->name);

  n = node_find(sra, &sralist);
  node_del(n, &sralist);
  node_free(n);

  if (sra->myuser)
    sra->myuser->sra = NULL;

  if (sra->name)
    free(sra->name);

  free(sra);

  cnt.sra--;
}

sra_t *sra_find(myuser_t *myuser)
{
  sra_t *sra;
  node_t *n;

  LIST_FOREACH(n, sralist.head)
  {
    sra = (sra_t *)n->data;

    if (sra->myuser == myuser)
      return sra;
  }

  return NULL;
}

/*****************
 * S E R V E R S *
 *****************/

server_t *server_add(char *name, uint8_t hops, char *desc)
{
  server_t *s;
  node_t *n = node_create();

  slog(0, LG_DEBUG, "server_add(): %s", name);

  s = scalloc(sizeof(server_t), 1);

  s->hash = SHASH((unsigned char *)name);

  node_add(s, n, &servlist[s->hash]);

  s->name = sstrdup(name);
  s->desc = sstrdup(desc);
  s->hops = hops;
  s->connected_since = CURRTIME;

  /* check to see if it's hidden */
  if ((desc[0] == '(') && (desc[1] == 'H') && (desc[2] == ')'))
    s->flags |= SF_HIDE;

  cnt.server++;

  return s;
}

void server_delete(char *name)
{
  server_t *s = server_find(name);
  user_t *u;
  node_t *n, *tn;
  uint32_t i;

  if (!s)
  {
    slog(0, LG_CRIT, "server_delete(): called for nonexistant server: %s",
         name);

    return;
  }

  slog(0, LG_DEBUG, "server_delete(): %s", s->name);

  /* first go through it's users and kill all of them */
  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH_SAFE(n, tn, userlist[i].head)
    {
      u = (user_t *)n->data;

      if (!strcasecmp(s->name, u->server->name))
        user_delete(u->nick);
    }
  }

  /* now remove the server */
  n = node_find(s, &servlist[s->hash]);
  node_del(n, &servlist[s->hash]);
  node_free(n);

  free(s->name);
  free(s->desc);
  free(s);

  cnt.server--;
}

server_t *server_find(char *name)
{
  server_t *s;
  node_t *n;

  LIST_FOREACH(n, servlist[SHASH((unsigned char *)name)].head)
  {
    s = (server_t *)n->data;

    if (!strcasecmp(name, s->name))
      return s;
  }

  return NULL;
}

/*************
 * U S E R S *
 *************/

user_t *user_add(char *nick, char *user, char *host, server_t *server)
{
  user_t *u;
  node_t *n = node_create();

  slog(0, LG_DEBUG, "user_add(): %s (%s@%s) -> %s", nick, user, host,
       server->name);

  u = scalloc(sizeof(user_t), 1);

  u->hash = UHASH((unsigned char *)nick);

  node_add(u, n, &userlist[u->hash]);

  u->nick = sstrdup(nick);
  u->user = sstrdup(user);
  u->host = sstrdup(host);
  u->server = server;

  u->server->users++;

  cnt.user++;

  return u;
}

void user_delete(char *nick)
{
  user_t *u = user_find(nick);
  node_t *n, *tn;
  chanuser_t *cu;

  if (!u)
  {
    slog(0, LG_CRIT, "user_delete(): called for nonexistant user: %s", nick);
    return;
  }

  slog(0, LG_DEBUG, "user_delete(): removing user: %s -> %s", u->nick,
       u->server->name);

  u->server->users--;

  /* remove the user from each channel */
  LIST_FOREACH_SAFE(n, tn, u->channels.head)
  {
    cu = (chanuser_t *)n->data;

    chanuser_delete(cu->chan, u);
  }

  /* set the user's myuser `identified' to FALSE if it exists */
  if (u->myuser)
    if (u->myuser->identified)
      u->myuser->identified = FALSE;

  n = node_find(u, &userlist[u->hash]);
  node_del(n, &userlist[u->hash]);
  node_free(n);

  if (u->myuser)
  {
    u->myuser->user = NULL;
    u->myuser = NULL;
  }

  free(u->nick);
  free(u->user);
  free(u->host);
  free(u);

  cnt.user--;
}

user_t *user_find(char *nick)
{
  user_t *u;
  node_t *n;
  uint32_t i;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n, userlist[i].head)
    {
      u = (user_t *)n->data;

      if (!irccasecmp(nick, u->nick))
        return u;
    }
  }

  slog(0, LG_DEBUG, "user_find(): called for nonexistant user `%s'", nick);

  return NULL;
}

/*******************
 * C H A N N E L S *
 *******************/
channel_t *channel_add(char *name, uint32_t ts)
{
  channel_t *c;
  mychan_t *mc;
  node_t *n;

  if (*name != '#')
  {
    slog(0, LG_ERR, "channel_add(): got non #channel: %s", name);
    return NULL;
  }

  c = channel_find(name);

  if (c)
  {
    slog(0, LG_CRIT, "channel_add(): channel already exists: %s", name);
    return c;
  }

  slog(0, LG_DEBUG, "channel_add(): %s", name);

  n = node_create();
  c = scalloc(sizeof(channel_t), 1);

  c->name = sstrdup(name);
  c->ts = ts;
  c->hash = CHASH((unsigned char *)name);

  if ((mc = mychan_find(c->name)))
    mc->chan = c;

  node_add(c, n, &chanlist[c->hash]);

  cnt.chan++;

  if (!irccasecmp(svs.chan, c->name))
    join(svs.chan, svs.nick);

  return c;
}

void channel_delete(char *name)
{
  channel_t *c = channel_find(name);
  mychan_t *mc;
  node_t *n;

  if (!c)
  {
    slog(0, LG_CRIT, "channel_delete(): called for nonexistant channel: %s",
         name);
    return;
  }

  slog(0, LG_DEBUG, "channel_delete(): %s", c->name);

  /* we assume all lists should be null */

  n = node_find(c, &chanlist[c->hash]);
  node_del(n, &chanlist[c->hash]);
  node_free(n);

  if ((mc = mychan_find(c->name)))
    mc->chan = NULL;

  free(c->name);
  free(c);

  cnt.chan--;
}

channel_t *channel_find(char *name)
{
  channel_t *c;
  node_t *n;
  uint32_t i;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n, chanlist[i].head)
    {
      c = (channel_t *)n->data;

      if (!irccasecmp(name, c->name))
        return c;
    }
  }

  return NULL;
}

/**********************
 * C H A N  U S E R S *
 **********************/

chanuser_t *chanuser_add(channel_t *chan, char *nick)
{
  user_t *u;
  node_t *n1;
  node_t *n2;
  chanuser_t *cu, *tcu;
  mychan_t *mc;
  uint32_t flags = 0;

  if (*chan->name != '#')
  {
    slog(0, LG_ERR, "chanuser_add(): got non #channel: %s", chan->name);
    return NULL;
  }

  while (*nick != '\0')
  {
    if (*nick == '@')
    {
      flags |= CMODE_OP;
      nick++;

      if (*nick == '+')
        u = user_find(nick + 1);
      else
        u = user_find(nick);

      /* see if we need to deop them */
      if ((mc = mychan_find(chan->name)))
      {
        if ((MC_SECURE & mc->flags) && (!is_founder(mc, u->myuser)) &&
            (!is_successor(mc, u->myuser)) &&
            (!is_xop(mc, u->myuser, (CA_AOP | CA_SOP))))
        {
          cmode(svs.nick, mc->name, "-o", u->nick);
          flags &= ~CMODE_OP;
        }
      }

      continue;
    }
    if (*nick == '+')
    {
      flags |= CMODE_VOICE;
      nick++;
      continue;
    }
    break;
  }

  u = user_find(nick);
  if (u == NULL)
  {
    slog(0, LG_CRIT, "chanuser_add(): nonexist user: %s", nick);
    return NULL;
  }

  tcu = chanuser_find(chan, u);
  if (tcu != NULL)
  {
    slog(0, LG_WARN, "chanuser_add(): user is already present: %s -> %s",
         chan->name, u->nick);

    /* could be an OPME or other desyncher... */
    tcu->modes |= flags;

    return NULL;
  }

  slog(0, LG_DEBUG, "chanuser_add(): %s -> %s", chan->name, u->nick);

  n1 = node_create();
  n2 = node_create();

  cu = scalloc(sizeof(chanuser_t), 1);

  cu->chan = chan;
  cu->user = u;
  cu->modes |= flags;

  chan->nummembers++;

  if ((chan->nummembers == 1) && (irccasecmp(svs.chan, chan->name)))
  {
    if ((mc = mychan_find(chan->name)) && (svs.join_chans))
    {
      join(chan->name, svs.nick);
      mc->used = CURRTIME;
    }
  }

  node_add(cu, n1, &chan->members);
  node_add(cu, n2, &u->channels);

  cnt.chanuser++;

  /* auto stuff */
  if (((mc = mychan_find(chan->name))) && (u->myuser))
  {
    if (should_voice(mc, u->myuser))
    {
      cmode(svs.nick, chan->name, "+v", u->nick);
      cu->modes |= CMODE_VOICE;
    }
    if (should_op(mc, u->myuser))
    {
      cmode(svs.nick, chan->name, "+o", u->nick);
      cu->modes |= CMODE_OP;
    }
  }
  else if ((mc = mychan_find(chan->name)) && (!u->myuser))
  {
    char hostbuf[BUFSIZE];

    hostbuf[0] = '\0';

    strlcat(hostbuf, u->nick, BUFSIZE);
    strlcat(hostbuf, "!", BUFSIZE);
    strlcat(hostbuf, u->user, BUFSIZE);
    strlcat(hostbuf, "@", BUFSIZE);
    strlcat(hostbuf, u->host, BUFSIZE);

    if (should_voice_host(mc, hostbuf))
    {
      cmode(svs.nick, chan->name, "+v", u->nick);
      cu->modes |= CMODE_VOICE;
    }
    if (should_op_host(mc, hostbuf))
    {
      cmode(svs.nick, chan->name, "+o", u->nick);
      cu->modes |= CMODE_OP;
    }
  }

  return cu;
}

void chanuser_delete(channel_t *chan, user_t *user)
{
  chanuser_t *cu;
  node_t *n, *n2;

  LIST_FOREACH(n, chan->members.head)
  {
    cu = (chanuser_t *)n->data;

    if (cu->user == user)
    {
      slog(0, LG_DEBUG, "chanuser_delete(): %s -> %s (%d)", cu->chan->name,
           cu->user->nick, cu->chan->nummembers - 1);
      node_del(n, &chan->members);
      node_free(n);

      n2 = node_find(cu, &user->channels);
      node_del(n2, &user->channels);
      node_free(n2);

      chan->nummembers--;
      cnt.chanuser--;

      if (chan->nummembers == 1)
      {
        if (svs.leave_chans)
          part(chan->name, svs.nick);
      }

      else if (chan->nummembers == 0)
      {
        /* empty channels die */
        slog(0, LG_DEBUG, "chanuser_delete(): `%s' is empty, removing",
             chan->name);

        channel_delete(chan->name);
      }

      return;
    }
  }
}

chanuser_t *chanuser_find(channel_t *chan, user_t *user)
{
  node_t *n;
  chanuser_t *cu;

  if ((!chan) || (!user))
    return NULL;

  LIST_FOREACH(n, chan->members.head)
  {
    cu = (chanuser_t *)n->data;

    if (cu->user == user)
      return cu;
  }

  return NULL;
}

/*****************
 * M Y U S E R S *
 *****************/
myuser_t *myuser_add(char *name, char *pass, char *email)
{
  myuser_t *mu;
  node_t *n;

  mu = myuser_find(name);

  if (mu)
  {
    slog(0, LG_CRIT, "myuser_add(): myuser already exists: %s", name);
    return mu;
  }

  slog(0, LG_DEBUG, "myuser_add(): %s -> %s", name, email);

  n = node_create();
  mu = scalloc(sizeof(myuser_t), 1);

  mu->name = sstrdup(name);
  mu->pass = sstrdup(pass);
  mu->email = sstrdup(email);
  mu->registered = CURRTIME;
  mu->hash = MUHASH((unsigned char *)name);

  node_add(mu, n, &mulist[mu->hash]);

  cnt.myuser++;

  return mu;
}

void myuser_delete(char *name)
{
  sra_t *sra;
  myuser_t *mu = myuser_find(name);
  mychan_t *mc;
  chanacs_t *ca;
  node_t *n;
  uint32_t i;

  if (!mu)
  {
    slog(0, LG_CRIT, "myuser_delete(): called for nonexistant myuser: %s",
         name);
    return;
  }

  slog(0, LG_DEBUG, "myuser_delete(): %s", mu->name);

  /* remove their chanacs shiz */
  LIST_FOREACH(n, mu->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    chanacs_delete(ca->mychan, ca->myuser, ca->level);
  }

  /* remove them as successors */
  for (i = 0; i < HASHSIZE; i++);
  {
    LIST_FOREACH(n, mclist[i].head)
    {
      mc = (mychan_t *)n->data;

      if ((mc->successor) && (mc->successor == mu))
        mc->successor = NULL;
    }
  }

  /* remove them from the sra list */
  if ((sra = sra_find(mu)))
    sra_delete(mu);

  n = node_find(mu, &mulist[mu->hash]);
  node_del(n, &mulist[mu->hash]);
  node_free(n);

  free(mu->name);
  free(mu->pass);
  free(mu->email);
  free(mu);

  cnt.myuser--;
}

myuser_t *myuser_find(char *name)
{
  myuser_t *mu;
  node_t *n;
  uint32_t i;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n, mulist[i].head)
    {
      mu = (myuser_t *)n->data;

      if (!irccasecmp(name, mu->name))
        return mu;
    }
  }

  return NULL;
}

/*****************
 * M Y C H A N S *
 *****************/

mychan_t *mychan_add(char *name, char *pass)
{
  mychan_t *mc;
  node_t *n;

  mc = mychan_find(name);

  if (mc)
  {
    slog(0, LG_CRIT, "mychan_add(): mychan already exists: %s", name);
    return mc;
  }

  slog(0, LG_DEBUG, "mychan_add(): %s", name);

  n = node_create();
  mc = scalloc(sizeof(mychan_t), 1);

  mc->name = sstrdup(name);
  mc->pass = sstrdup(pass);
  mc->founder = NULL;
  mc->successor = NULL;
  mc->registered = CURRTIME;
  mc->chan = channel_find(name);
  mc->hash = MCHASH((unsigned char *)name);

  node_add(mc, n, &mclist[mc->hash]);

  cnt.mychan++;

  return mc;
}

void mychan_delete(char *name)
{
  mychan_t *mc = mychan_find(name);
  chanacs_t *ca;
  node_t *n;

  if (!mc)
  {
    slog(0, LG_CRIT, "mychan_delete(): called for nonexistant mychan: %s",
         name);
    return;
  }

  slog(0, LG_DEBUG, "mychan_delete(): %s", mc->name);

  /* remove the chanacs shiz */
  LIST_FOREACH(n, mc->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    chanacs_delete(ca->mychan, ca->myuser, ca->level);
  }

  n = node_find(mc, &mclist[mc->hash]);
  node_del(n, &mclist[mc->hash]);
  node_free(n);

  free(mc->name);
  free(mc->pass);
  free(mc);

  cnt.mychan--;
}

mychan_t *mychan_find(char *name)
{
  mychan_t *mc;
  node_t *n;
  uint32_t i;

  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n, mclist[i].head)
    {
      mc = (mychan_t *)n->data;

      if (!irccasecmp(name, mc->name))
        return mc;
    }
  }

  return NULL;
}

/*****************
 * C H A N A C S *
 *****************/

chanacs_t *chanacs_add(mychan_t *mychan, myuser_t *myuser, uint8_t level)
{
  chanacs_t *ca;
  node_t *n1;
  node_t *n2;

  if (*mychan->name != '#')
  {
    slog(0, LG_ERR, "chanacs_add(): got non #channel: %s", mychan->name);
    return NULL;
  }

  slog(0, LG_DEBUG, "chanacs_add(): %s -> %s", mychan->name, myuser->name);

  n1 = node_create();
  n2 = node_create();

  ca = scalloc(sizeof(chanacs_t), 1);

  ca->mychan = mychan;
  ca->myuser = myuser;
  ca->level |= level;

  node_add(ca, n1, &mychan->chanacs);
  node_add(ca, n2, &myuser->chanacs);

  cnt.chanacs++;

  return ca;
}

chanacs_t *chanacs_add_host(mychan_t *mychan, char *host, uint8_t level)
{
  chanacs_t *ca;
  node_t *n;

  if (*mychan->name != '#')
  {
    slog(0, LG_ERR, "chanacs_add_host(): got non #channel: %s", mychan->name);
    return NULL;
  }

  slog(0, LG_DEBUG, "chanacs_add_host(): %s -> %s", mychan->name, host);

  n = node_create();

  ca = scalloc(sizeof(chanacs_t), 1);

  ca->mychan = mychan;
  ca->myuser = NULL;
  ca->host = sstrdup(host);
  ca->level |= level;

  node_add(ca, n, &mychan->chanacs);

  cnt.chanacs++;

  return ca;
}

void chanacs_delete(mychan_t *mychan, myuser_t *myuser, uint8_t level)
{
  chanacs_t *ca;
  node_t *n, *n2;

  LIST_FOREACH(n, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->myuser == myuser) && (ca->level == level))
    {
      slog(0, LG_DEBUG, "chanacs_delete(): %s -> %s", ca->mychan->name,
           ca->myuser->name);
      node_del(n, &mychan->chanacs);
      node_free(n);

      n2 = node_find(ca, &myuser->chanacs);
      node_del(n2, &myuser->chanacs);
      node_free(n2);

      cnt.chanacs--;

      return;
    }
  }
}

void chanacs_delete_host(mychan_t *mychan, char *host, uint8_t level)
{
  chanacs_t *ca;
  node_t *n;

  LIST_FOREACH(n, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->host) && (!irccasecmp(host, ca->host)) && (ca->level == level))
    {
      slog(0, LG_DEBUG, "chanacs_delete_host(): %s -> %s", ca->mychan->name,
           ca->host);

      free(ca->host);
      node_del(n, &mychan->chanacs);
      node_free(n);

      cnt.chanacs--;

      return;
    }
  }
}

chanacs_t *chanacs_find(mychan_t *mychan, myuser_t *myuser, uint8_t level)
{
  node_t *n;
  chanacs_t *ca;

  if ((!mychan) || (!myuser))
    return NULL;

  LIST_FOREACH(n, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->myuser == myuser) && (ca->level & level))
      return ca;
  }

  return NULL;
}

chanacs_t *chanacs_find_host(mychan_t *mychan, char *host, uint8_t level)
{
  node_t *n;
  chanacs_t *ca;

  if ((!mychan) || (!host))
    return NULL;

  LIST_FOREACH(n, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->host) && (!match(ca->host, host)) && (ca->level & level))
      return ca;
  }

  return NULL;
}
