/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains linked list functions.
 * Idea from ircd-hybrid.
 */

#include "../inc/shrike.h"

list_t sralist;
list_t tldlist;
list_t klnlist;
list_t servlist[HASHSIZE];
list_t userlist[HASHSIZE];
list_t chanlist[HASHSIZE];
list_t mulist[HASHSIZE];
list_t mclist[HASHSIZE];

list_t sendq;

static BlockHeap *node_heap;
static BlockHeap *sra_heap;
static BlockHeap *tld_heap;
static BlockHeap *kline_heap;
static BlockHeap *serv_heap;
static BlockHeap *user_heap;
static BlockHeap *chan_heap;
static BlockHeap *myuser_heap;
static BlockHeap *mychan_heap;

static BlockHeap *chanuser_heap;
static BlockHeap *chanacs_heap;

/*************
 * L I S T S *
 *************/

void init_nodes(void)
{
  node_heap = BlockHeapCreate(sizeof(node_t), HEAP_NODE);
  sra_heap = BlockHeapCreate(sizeof(sra_t), 2);
  tld_heap = BlockHeapCreate(sizeof(tld_t), 4);
  kline_heap = BlockHeapCreate(sizeof(kline_t), 16);
  serv_heap = BlockHeapCreate(sizeof(server_t), HEAP_SERVER);
  user_heap = BlockHeapCreate(sizeof(user_t), HEAP_USER);
  chan_heap = BlockHeapCreate(sizeof(channel_t), HEAP_CHANNEL);
  myuser_heap = BlockHeapCreate(sizeof(myuser_t), HEAP_USER);
  mychan_heap = BlockHeapCreate(sizeof(mychan_t), HEAP_CHANNEL);
  chanuser_heap = BlockHeapCreate(sizeof(chanuser_t), HEAP_CHANUSER);
  chanacs_heap = BlockHeapCreate(sizeof(chanacs_t), HEAP_CHANACS);

  if (!node_heap || !tld_heap || !kline_heap || !serv_heap || !user_heap ||
      !chan_heap || !myuser_heap || !mychan_heap || !sra_heap ||
      !chanuser_heap || !chanacs_heap)
  {
    slog(LG_INFO, "init_nodes(): block allocator failed.");
    exit(EXIT_FAILURE);
  }
}

/* creates a new node */
node_t *node_create(void)
{
  node_t *n;

  /* allocate it */
  n = BlockHeapAlloc(node_heap);

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
  BlockHeapFree(node_heap, n);

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
  /* do we even have a node? */
  if (!n)
  {
    slog(LG_DEBUG, "node_del(): called with NULL node");
    return;
  }

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

void node_move(node_t *m, list_t *oldlist, list_t *newlist)
{
  /* Assumption: If m->next == NULL, then list->tail == m
   *      and:   If m->prev == NULL, then list->head == m
   */
  if (m->next)
    m->next->prev = m->prev;
  else
    oldlist->tail = m->prev;

  if (m->prev)
    m->prev->next = m->next;
  else
    oldlist->head = m->next;

  m->prev = NULL;
  m->next = newlist->head;
  if (newlist->head != NULL)
    newlist->head->prev = m;
  else if (newlist->tail == NULL)
    newlist->tail = m;
  newlist->head = m;

  oldlist->count--;
  newlist->count++;
}

/***********
 * S R A S *
 ***********/

sra_t *sra_add(char *name)
{
  sra_t *sra;
  myuser_t *mu = myuser_find(name);
  node_t *n = node_create();

  slog(LG_DEBUG, "sra_add(): %s", (mu) ? mu->name : name);

  sra = BlockHeapAlloc(sra_heap);

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
    slog(LG_DEBUG, "sra_delete(): called for nonexistant sra: %s",
         myuser->name);

    return;
  }

  slog(LG_DEBUG, "sra_delete(): %s",
       (sra->myuser) ? sra->myuser->name : sra->name);

  n = node_find(sra, &sralist);
  node_del(n, &sralist);
  node_free(n);

  if (sra->myuser)
    sra->myuser->sra = NULL;

  if (sra->name)
    free(sra->name);

  BlockHeapFree(sra_heap, sra);

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

/***********
 * T L D S *
 ***********/

tld_t *tld_add(char *name)
{
  tld_t *tld;
  node_t *n = node_create();

  slog(LG_DEBUG, "tld_add(): %s", name);

  tld = BlockHeapAlloc(tld_heap);

  node_add(tld, n, &tldlist);

  tld->name = sstrdup(name);

  cnt.tld++;

  return tld;
}

void tld_delete(char *name)
{
  tld_t *tld = tld_find(name);
  node_t *n;

  if (!tld)
  {
    slog(LG_DEBUG, "tld_delete(): called for nonexistant tld: %s", name);

    return;
  }

  slog(LG_DEBUG, "tld_delete(): %s", tld->name);

  n = node_find(tld, &tldlist);
  node_del(n, &tldlist);
  node_free(n);

  free(tld->name);
  BlockHeapFree(tld_heap, tld);

  cnt.tld--;
}

tld_t *tld_find(char *name)
{
  tld_t *tld;
  node_t *n;

  LIST_FOREACH(n, tldlist.head)
  {
    tld = (tld_t *)n->data;

    if (!strcasecmp(name, tld->name))
      return tld;
  }

  return NULL;
}

/***************
 * K L I N E S *
 ***************/

kline_t *kline_add(char *user, char *host, char *reason, long duration)
{
  kline_t *k;
  node_t *n = node_create();
  static uint32_t kcnt = 0;

  slog(LG_DEBUG, "kline_add(): %s@%s -> %s (%ld)", user, host, reason,
       duration);

  k = BlockHeapAlloc(kline_heap);

  node_add(k, n, &klnlist);

  k->user = sstrdup(user);
  k->host = sstrdup(host);
  k->reason = sstrdup(reason);
  k->duration = duration;
  k->settime = CURRTIME;
  k->expires = CURRTIME + duration;
  k->number = ++kcnt;

  cnt.kline++;

  kline_sts("*", user, host, duration, reason);

  return k;
}

void kline_delete(char *user, char *host)
{
  kline_t *k = kline_find(user, host);
  node_t *n;

  if (!k)
  {
    slog(LG_DEBUG, "kline_delete(): called for nonexistant kline: %s@%s",
         user, host);

    return;
  }

  slog(LG_DEBUG, "kline_delete(): %s@%s -> %s", k->user, k->host, k->reason);

  unkline_sts("*", k->user, k->host);

  n = node_find(k, &klnlist);
  node_del(n, &klnlist);
  node_free(n);

  free(k->user);
  free(k->host);
  free(k->reason);
  free(k->setby);

  BlockHeapFree(kline_heap, k);

  cnt.kline--;
}

kline_t *kline_find(char *user, char *host)
{
  kline_t *k;
  node_t *n;

  LIST_FOREACH(n, klnlist.head)
  {
    k = (kline_t *)n->data;

    if ((!match(k->user, user)) && (!match(k->host, host)))
      return k;
  }

  return NULL;
}

kline_t *kline_find_num(long number)
{
  kline_t *k;
  node_t *n;

  LIST_FOREACH(n, klnlist.head)
  {
    k = (kline_t *)n->data;

    if (k->number == number)
      return k;
  }

  return NULL;
}

void kline_expire(void *arg)
{
  kline_t *k;
  node_t *n, *tn;

  LIST_FOREACH_SAFE(n, tn, klnlist.head)
  {
    k = (kline_t *)n->data;

    if (k->duration == 0)
      continue;

    if (k->expires <= CURRTIME)
    {
      snoop("KLINE:EXPIRE: \2%s@%s\2 set \2%s\2 ago by \2%s\2", k->user,
            k->host, time_ago(k->settime), k->setby);

      kline_delete(k->user, k->host);
    }
  }
}

/*****************
 * S E R V E R S *
 *****************/

server_t *server_add(char *name, uint8_t hops, char *desc)
{
  server_t *s;
  node_t *n = node_create();
  char *tld;

  slog(LG_DEBUG, "server_add(): %s", name);

  s = BlockHeapAlloc(serv_heap);

  s->hash = shash(name);

  node_add(s, n, &servlist[s->hash]);

  s->name = sstrdup(name);
  s->desc = sstrdup(desc);
  s->hops = hops;
  s->connected_since = CURRTIME;

  /* check to see if it's hidden */
  if ((desc[0] == '(') && (desc[1] == 'H') && (desc[2] == ')'))
    s->flags |= SF_HIDE;

  /* tld list for global noticer */
  tld = strrchr(name, '.');

  if (!tld_find(tld))
    tld_add(tld);

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
    slog(LG_DEBUG, "server_delete(): called for nonexistant server: %s", name);

    return;
  }

  slog(LG_DEBUG, "server_delete(): %s", s->name);

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
  BlockHeapFree(serv_heap, s);

  cnt.server--;
}

server_t *server_find(char *name)
{
  server_t *s;
  node_t *n;

  LIST_FOREACH(n, servlist[shash(name)].head)
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

  slog(LG_DEBUG, "user_add(): %s (%s@%s) -> %s", nick, user, host,
       server->name);

  u = BlockHeapAlloc(user_heap);

  u->hash = shash(nick);

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
    slog(LG_DEBUG, "user_delete(): called for nonexistant user: %s", nick);
    return;
  }

  slog(LG_DEBUG, "user_delete(): removing user: %s -> %s", u->nick,
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

  BlockHeapFree(user_heap, u);

  cnt.user--;
}

user_t *user_find(char *nick)
{
  user_t *u;
  node_t *n;

  LIST_FOREACH(n, userlist[shash(nick)].head)
  {
    u = (user_t *)n->data;

    if (!irccasecmp(nick, u->nick))
      return u;
  }

  slog(LG_DEBUG, "user_find(): called for nonexistant user `%s'", nick);

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
    slog(LG_DEBUG, "channel_add(): got non #channel: %s", name);
    return NULL;
  }

  c = channel_find(name);

  if (c)
  {
    slog(LG_DEBUG, "channel_add(): channel already exists: %s", name);
    return c;
  }

  slog(LG_DEBUG, "channel_add(): %s", name);

  n = node_create();
  c = BlockHeapAlloc(chan_heap);

  c->name = sstrdup(name);
  c->ts = ts;
  c->hash = shash(name);

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
    slog(LG_DEBUG, "channel_delete(): called for nonexistant channel: %s",
         name);
    return;
  }

  slog(LG_DEBUG, "channel_delete(): %s", c->name);

  /* we assume all lists should be null */

  n = node_find(c, &chanlist[c->hash]);
  node_del(n, &chanlist[c->hash]);
  node_free(n);

  if ((mc = mychan_find(c->name)))
    mc->chan = NULL;

  free(c->name);
  BlockHeapFree(chan_heap, c);

  cnt.chan--;
}

channel_t *channel_find(char *name)
{
  channel_t *c;
  node_t *n;

  LIST_FOREACH(n, chanlist[shash(name)].head)
  {
    c = (channel_t *)n->data;

    if (!irccasecmp(name, c->name))
      return c;
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
  char *hostbuf;

  if (*chan->name != '#')
  {
    slog(LG_DEBUG, "chanuser_add(): got non #channel: %s", chan->name);
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

      if (!u)
      {
        slog(LG_DEBUG, "chanuser_add(): user_find() failed for %s", nick);
        return NULL;
      }

      hostbuf = make_hostmask(u->nick, u->user, u->host);

      /* see if we need to deop them */
      if ((mc = mychan_find(chan->name)))
      {
        mc->used = CURRTIME; /* without this channels randomly expire :( */

        if (MC_SECURE & mc->flags)
        {
          /* a very ugly check that makes sure they shouldn't be an op */
          if (!u->myuser || (!is_founder(mc, u->myuser) && u != svs.svs &&
             !is_xop(mc, u->myuser, (CA_AOP | CA_SOP) ||
             !should_op_host(mc, hostbuf))))
          {
            cmode(svs.nick, mc->name, "-o", u->nick);
            flags &= ~CMODE_OP;
          }
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
    slog(LG_DEBUG, "chanuser_add(): nonexistent user: %s", nick);
    return NULL;
  }

  tcu = chanuser_find(chan, u);
  if (tcu != NULL)
  {
    slog(LG_DEBUG, "chanuser_add(): user is already present: %s -> %s",
         chan->name, u->nick);

    /* could be an OPME or other desyncher... */
    tcu->modes |= flags;

    return tcu;
  }

  slog(LG_DEBUG, "chanuser_add(): %s -> %s", chan->name, u->nick);

  n1 = node_create();
  n2 = node_create();

  cu = BlockHeapAlloc(chanuser_heap);

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

  if (mc)
  {
    /* XXX - for some reason make_hostmask doesn't work here */
    char newhostbuf[BUFSIZE];

    newhostbuf[0] = '\0';

    strlcat(newhostbuf, u->nick, BUFSIZE);
    strlcat(newhostbuf, "!", BUFSIZE);
    strlcat(newhostbuf, u->user, BUFSIZE);
    strlcat(newhostbuf, "@", BUFSIZE);
    strlcat(newhostbuf, u->host, BUFSIZE);

    if (should_voice_host(mc, newhostbuf))
    {
      if (!(cu->modes & CMODE_VOICE))
      {
        cmode(svs.nick, chan->name, "+v", u->nick);
        cu->modes |= CMODE_VOICE;
      }
    }
    if (should_op_host(mc, newhostbuf))
    {
      if (!(cu->modes & CMODE_OP))
      {
        cmode(svs.nick, chan->name, "+o", u->nick);
        cu->modes |= CMODE_OP;
      }
    }
  }

  return cu;
}

void chanuser_delete(channel_t *chan, user_t *user)
{
  chanuser_t *cu;
  node_t *n, *tn, *n2;

  LIST_FOREACH_SAFE(n, tn, chan->members.head)
  {
    cu = (chanuser_t *)n->data;

    if (cu->user == user)
    {
      slog(LG_DEBUG, "chanuser_delete(): %s -> %s (%d)", cu->chan->name,
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
        slog(LG_DEBUG, "chanuser_delete(): `%s' is empty, removing",
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
    slog(LG_DEBUG, "myuser_add(): myuser already exists: %s", name);
    return mu;
  }

  slog(LG_DEBUG, "myuser_add(): %s -> %s", name, email);

  n = node_create();
  mu = BlockHeapAlloc(myuser_heap);

  mu->name = sstrdup(name);
  mu->pass = sstrdup(pass);
  mu->email = sstrdup(email);
  mu->registered = CURRTIME;
  mu->hash = shash(name);

  node_add(mu, n, &mulist[mu->hash]);

  cnt.myuser++;

  return mu;
}

void myuser_delete(char *name)
{
  myuser_t *mu = myuser_find(name);
  mychan_t *mc;
  chanacs_t *ca;
  node_t *n, *tn;
  uint32_t i;

  if (!mu)
  {
    slog(LG_DEBUG, "myuser_delete(): called for nonexistant myuser: %s", name);
    return;
  }

  slog(LG_DEBUG, "myuser_delete(): %s", mu->name);

  /* remove their chanacs shiz */
  LIST_FOREACH_SAFE(n, tn, mu->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    chanacs_delete(ca->mychan, ca->myuser, ca->level);
  }

  /* remove them as successors */
  for (i = 0; i < HASHSIZE; i++)
  {
    LIST_FOREACH(n, mclist[i].head)
    {
      mc = (mychan_t *)n->data;

      if ((mc->successor) && (mc->successor == mu))
        mc->successor = NULL;
    }
  }

  /* remove them from the sra list */
  if (sra_find(mu))
    sra_delete(mu);

  n = node_find(mu, &mulist[mu->hash]);
  node_del(n, &mulist[mu->hash]);
  node_free(n);

  free(mu->name);
  free(mu->pass);
  free(mu->email);
  BlockHeapFree(myuser_heap, mu);

  cnt.myuser--;
}

myuser_t *myuser_find(char *name)
{
  myuser_t *mu;
  node_t *n;

  LIST_FOREACH(n, mulist[shash(name)].head)
  {
    mu = (myuser_t *)n->data;

    if (!irccasecmp(name, mu->name))
      return mu;
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
    slog(LG_DEBUG, "mychan_add(): mychan already exists: %s", name);
    return mc;
  }

  slog(LG_DEBUG, "mychan_add(): %s", name);

  n = node_create();
  mc = BlockHeapAlloc(mychan_heap);

  mc->name = sstrdup(name);
  mc->pass = sstrdup(pass);
  mc->founder = NULL;
  mc->successor = NULL;
  mc->registered = CURRTIME;
  mc->chan = channel_find(name);
  mc->hash = shash(name);

  node_add(mc, n, &mclist[mc->hash]);

  cnt.mychan++;

  return mc;
}

void mychan_delete(char *name)
{
  mychan_t *mc = mychan_find(name);
  chanacs_t *ca;
  node_t *n, *tn;

  if (!mc)
  {
    slog(LG_DEBUG, "mychan_delete(): called for nonexistant mychan: %s", name);
    return;
  }

  slog(LG_DEBUG, "mychan_delete(): %s", mc->name);

  /* remove the chanacs shiz */
  LIST_FOREACH_SAFE(n, tn, mc->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if (ca->host)
      chanacs_delete_host(ca->mychan, ca->host, ca->level);
    else
      chanacs_delete(ca->mychan, ca->myuser, ca->level);
  }

  n = node_find(mc, &mclist[mc->hash]);
  node_del(n, &mclist[mc->hash]);
  node_free(n);

  free(mc->name);
  free(mc->pass);
  BlockHeapFree(mychan_heap, mc);

  cnt.mychan--;
}

mychan_t *mychan_find(char *name)
{
  mychan_t *mc;
  node_t *n;

  LIST_FOREACH(n, mclist[shash(name)].head)
  {
    mc = (mychan_t *)n->data;

    if (!irccasecmp(name, mc->name))
      return mc;
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
    slog(LG_DEBUG, "chanacs_add(): got non #channel: %s", mychan->name);
    return NULL;
  }

  slog(LG_DEBUG, "chanacs_add(): %s -> %s", mychan->name, myuser->name);

  n1 = node_create();
  n2 = node_create();

  ca = BlockHeapAlloc(chanacs_heap);

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
    slog(LG_DEBUG, "chanacs_add_host(): got non #channel: %s", mychan->name);
    return NULL;
  }

  slog(LG_DEBUG, "chanacs_add_host(): %s -> %s", mychan->name, host);

  n = node_create();

  ca = BlockHeapAlloc(chanacs_heap);

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
  node_t *n, *tn, *n2;

  LIST_FOREACH_SAFE(n, tn, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->myuser == myuser) && (ca->level == level))
    {
      slog(LG_DEBUG, "chanacs_delete(): %s -> %s", ca->mychan->name,
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
  node_t *n, *tn;

  LIST_FOREACH_SAFE(n, tn, mychan->chanacs.head)
  {
    ca = (chanacs_t *)n->data;

    if ((ca->host) && (!irccasecmp(host, ca->host)) && (ca->level == level))
    {
      slog(LG_DEBUG, "chanacs_delete_host(): %s -> %s", ca->mychan->name,
           ca->host);

      free(ca->host);
      node_del(n, &mychan->chanacs);
      node_free(n);

      BlockHeapFree(chanacs_heap, ca);

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

/*********
 * SENDQ *
 *********/

void sendq_add(char *buf, int len, int pos)
{
  node_t *n = node_create();
  struct sendq *sq = smalloc(sizeof(struct sendq));

  slog(LG_DEBUG, "sendq_add(): triggered");

  sq->buf = sstrdup(buf);
  sq->len = len - pos;
  sq->pos = pos;
  node_add(sq, n, &sendq);
}

int sendq_flush(void)
{
  node_t *n, *tn;
  struct sendq *sq;
  int l;

  LIST_FOREACH_SAFE(n, tn, sendq.head)
  {
    sq = (struct sendq *)n->data;

    if ((l = write(servsock, sq->buf + sq->pos, sq->len)) == -1)
    {
      if (errno != EAGAIN)
        return -1;

      return 0;
    }

    if (l == sq->len)
    {
      node_del(n, &sendq);
      free(sq->buf);
      free(sq);
      node_free(n);
    }
    else
    {
      sq->pos += l;
      sq->len -= l;
      return 0;
    }
  }

  return 1;
}
