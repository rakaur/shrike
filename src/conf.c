/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains the routines that deal with the configuration.
 *
 * $$Id$
 */

#include "../inc/shrike.h"

/*
 * Loads the configuration file.
 * No checking is done in this routine.
 */
void conf_parse(void)
{
  uint32_t linecnt = 0;
  FILE *f = fopen(config_file, "r");
  char *item, dBuf[BUFSIZE], tBuf[BUFSIZE];

  char *pattern, *lvalue, *rvalue;
  regex_t *preg = NULL;
  regmatch_t pmatch[5];

  pattern = (char *)malloc(2046);

  slog(0, LG_DEBUG, "conf_parse(): loading configuration file");

  if (!f)
  {
    fprintf(stderr, "shrike: config file `%s' was not found\n", config_file);
    exit(EXIT_FAILURE);
  }

  /* start reading the file one line at a time */
  while (fgets(dBuf, BUFSIZE, f))
  {
    linecnt++;

    /* check for unimportant lines */
    strcpy(tBuf, dBuf);
    item = strtok(tBuf, " ");
    strip(item);
    strip(dBuf);

    if (*item == '#' || !*item)
      continue;

    /* collapse tabs */
    tb2sp(dBuf);

    slog(0, LG_DEBUG, "conf_parse(): checking `%s' on line %d", dBuf, linecnt);

    /* check for the serverinfo{} block */
    strcpy(pattern, "^[[:space:]]*serverinfo[[:space:]]*\\{");

    if (regex_match(preg, pattern, dBuf, 5, pmatch, 0) == 0)
    {
      slog(0, LG_DEBUG, "conf_parse(): parsing serverinfo{} block on line %d",
           linecnt);

      /* this gets ugly, oh well */
      while (1)
      {
        fgets(dBuf, BUFSIZE, f);

        linecnt++;

        /* check for unimportant lines */
        strcpy(tBuf, dBuf);
        item = strtok(tBuf, " ");
        strip(item);
        strip(dBuf);

        if (*item == '#' || !*item)
          continue;

        /* collapse tabs */
        tb2sp(dBuf);

        slog(0, LG_DEBUG, "conf_parse(): checking severinfo:`%s' on line %d",
             dBuf, linecnt);

        if (!strcmp("}", dBuf))
        {
          slog(0, LG_DEBUG,
               "conf_parse(): finished parsing serverinfo{} block");
          break;
        }

        /* check for key = value entries */
        strcpy(pattern, "([^ ]+)[ \t]*=[ \t]*(.+)[ \t]*;");

        if (regex_match(preg, pattern, dBuf, 5, pmatch, 0) == 0)
        {
          lvalue = (dBuf + pmatch[1].rm_so);
          *(dBuf + pmatch[1].rm_eo) = '\0';

          rvalue = (dBuf + pmatch[2].rm_so);
          *(dBuf + pmatch[2].rm_eo) = '\0';

          slog(0, LG_DEBUG,
               "conf_parse(): parsing lvalue `%s' with rvalue `%s'", lvalue,
               rvalue);

          if (!strcasecmp("name", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              me.name = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("desc", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              me.desc = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("uplink", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              me.uplink = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("port", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              me.port = atoi(rvalue);
            continue;
          }
          else if (!strcasecmp("pass", lvalue))
          {
            me.pass = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("vhost", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              me.vhost = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("recontime", lvalue))
          {
            me.recontime = atoi(rvalue);
            continue;
          }
          else if (!strcasecmp("restarttime", lvalue))
          {
            me.restarttime = atoi(rvalue);
            continue;
          }
          else if (!strcasecmp("expire", lvalue))
          {
            me.expire = (atoi(rvalue) * 60 * 60 * 24);
            continue;
          }
          else if (!strcasecmp("netname", lvalue))
          {
            me.netname = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("adminname", lvalue))
          {
            me.adminname = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("adminemail", lvalue))
          {
            me.adminemail = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("mta", lvalue))
          {
            me.mta = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("loglevel", lvalue))
          {
            if (!strcasecmp("DEBUG", rvalue))
              me.loglevel |= LG_DEBUG;
            else if (!strcasecmp("CRITICAL", rvalue))
              me.loglevel |= LG_CRIT;
            else if (!strcasecmp("ERROR", rvalue))
              me.loglevel |= LG_ERR;
            else if (!strcasecmp("WARNING", rvalue))
              me.loglevel |= LG_WARN;
            else if (!strcasecmp("INFO", rvalue))
              me.loglevel |= LG_INFO;
            else if (!strcasecmp("NONE", rvalue))
              me.loglevel |= LG_NONE;
            else
              me.loglevel |= LG_NOTICE;
          }
          else if (!strcasecmp("maxusers", lvalue))
          {
            me.maxusers = atoi(rvalue);
            continue;
          }
          else if (!strcasecmp("maxchans", lvalue))
          {
            me.maxchans = atoi(rvalue);
            continue;
          }
          else if (!strcasecmp("auth", lvalue))
          {
            if (!strcasecmp("email", rvalue))
              me.auth = AUTH_EMAIL;
            else
              me.auth = AUTH_NONE;
          }
          else if (!strcasecmp("casemapping", lvalue))
          {
            if (!strcasecmp("ascii", rvalue))
              set_match_mapping(MATCH_ASCII);
            else
              set_match_mapping(MATCH_RFC1459);
          }
          else
            continue;
        }
        else
          continue;
      }
    }

    /* check for the clientinfo{} block */
    strcpy(pattern, "^[[:space:]]*clientinfo[[:space:]]*\\{");

    if (regex_match(preg, pattern, dBuf, 5, pmatch, 0) == 0)
    {
      slog(0, LG_DEBUG, "conf_parse(): parsing clientinfo{} block on line %d",
           linecnt);

      /* this gets ugly, oh well */
      while (1)
      {
        fgets(dBuf, BUFSIZE, f);

        linecnt++;

        /* check for unimportant lines */
        strcpy(tBuf, dBuf);
        item = strtok(tBuf, " ");
        strip(item);
        strip(dBuf);

        if (*item == '#' || !*item)
          continue;

        /* collapse tabs */
        tb2sp(dBuf);

        slog(0, LG_DEBUG, "conf_parse(): checking clientinfo:`%s' on line %d",
             dBuf, linecnt);

        if (!strcmp("}", dBuf))
        {
          slog(0, LG_DEBUG,
               "conf_parse(): finished parsing clientinfo{} block");
          break;
        }

        /* check for key = value entries */
        strcpy(pattern, "([^ ]+)[ \t]*=[ \t]*(.+)[ \t]*;");

        if (regex_match(preg, pattern, dBuf, 5, pmatch, 0) == 0)
        {
          lvalue = (dBuf + pmatch[1].rm_so);
          *(dBuf + pmatch[1].rm_eo) = '\0';

          rvalue = (dBuf + pmatch[2].rm_so);
          *(dBuf + pmatch[2].rm_eo) = '\0';

          slog(0, LG_DEBUG,
               "conf_parse(): parsing lvalue `%s' with rvalue `%s'", lvalue,
               rvalue);

          if (!strcasecmp("nick", lvalue))
          {
            svs.nick = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("user", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              svs.user = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("host", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              svs.host = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("real", lvalue))
          {
            if (!(runflags & RF_REHASHING))
              svs.real = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("chan", lvalue))
          {
            svs.chan = sstrdup(rvalue);
            continue;
          }
          else if (!strcasecmp("join_chans", lvalue))
          {
            if (!strcasecmp("yes", rvalue) || !strcasecmp("true", rvalue))
              svs.join_chans = TRUE;
            else
              svs.join_chans = FALSE;

            continue;
          }
          else if (!strcasecmp("leave_chans", lvalue))
          {
            if (!strcasecmp("yes", rvalue) || !strcasecmp("true", rvalue))
              svs.leave_chans = TRUE;
            else
              svs.leave_chans = FALSE;

            continue;
          }
          else if (!strcasecmp("sra", lvalue))
          {
            /* we only make a temp list of sra's names.  after db_load() does
             * its thing and we actually have myuser it will update the list
             * properly.
             */
            sra_add(rvalue);
            continue;
          }
          else
            continue;
        }
        else
          continue;
      }
    }
  }

  /* free the stuff we made */
  free(pattern);

  /* close the file */
  fclose(f);

  slog(0, LG_DEBUG, "conf_parse(): finished loading configuration file");
}

static void copy_me(struct me *src, struct me *dst)
{
  dst->name = sstrdup(src->name);
  dst->desc = sstrdup(src->desc);
  dst->uplink = sstrdup(src->uplink);
  dst->port = src->port;
  dst->pass = sstrdup(src->pass);
  if (src->vhost)
    dst->vhost = sstrdup(src->vhost);
  dst->recontime = src->recontime;
  dst->restarttime = src->restarttime;
  dst->expire = src->expire;
  dst->netname = sstrdup(src->netname);
  dst->adminname = sstrdup(src->adminname);
  dst->adminemail = sstrdup(src->adminemail);
  dst->mta = sstrdup(src->mta);
  dst->loglevel = src->loglevel;
  dst->maxfd = src->maxfd;
  dst->start = src->start;
  dst->me = src->me;
  dst->uplinkpong = src->uplinkpong;
  dst->connected = src->connected;
  dst->maxusers = src->maxusers;
  dst->maxchans = src->maxchans;
  dst->auth = src->auth;
}

static void copy_svs(struct svs *src, struct svs *dst)
{
  dst->nick = sstrdup(src->nick);
  dst->user = sstrdup(src->user);
  dst->host = sstrdup(src->host);
  dst->real = sstrdup(src->real);
  dst->chan = sstrdup(src->chan);
  dst->join_chans = src->join_chans;
  dst->leave_chans = src->leave_chans;
}

static void free_cstructs(struct me *mesrc, struct svs *svssrc)
{
  free(mesrc->name);
  free(mesrc->desc);
  free(mesrc->uplink);
  free(mesrc->pass);
  free(mesrc->vhost);
  free(mesrc->netname);
  free(mesrc->adminname);
  free(mesrc->adminemail);
  free(mesrc->mta);

  free(svssrc->nick);
  free(svssrc->user);
  free(svssrc->host);
  free(svssrc->real);
  free(svssrc->chan);
}

boolean_t conf_rehash(void)
{
  struct me *hold_me = scalloc(sizeof(struct me), 1);   /* and keep_me_warm; */
  struct svs *hold_svs = scalloc(sizeof(struct svs), 1);
  int i;
  sra_t *sra;
  node_t *n;

  /* we're rehashing */
  slog(0, LG_INFO, "conf_rehash(): rehashing");
  runflags |= RF_REHASHING;

  copy_me(&me, hold_me);
  copy_svs(&svs, hold_svs);

  /* reset everything */
  free(me.pass);
  me.recontime = 0;
  me.restarttime = 0;
  me.expire = 0;
  free(me.netname);
  free(me.adminname);
  free(me.adminemail);
  free(me.mta);
  me.loglevel &= LG_DEBUG;      /* default loglevel */
  me.maxusers = 0;
  me.maxchans = 0;
  me.auth = 0;
  free(svs.nick);
  free(svs.chan);
  svs.join_chans = ERROR;
  svs.leave_chans = ERROR;

  me.pass = me.netname = me.adminname = me.adminemail = me.mta = NULL;
  svs.nick = svs.chan = NULL;

  LIST_FOREACH(n, sralist.head)
  {
    sra = (sra_t *)n->data;

    sra_delete(sra->myuser);
  }

  /* now reload */
  conf_parse();

  /* now recheck */
  if (!conf_check())
  {
    slog(0, LG_INFO,
         "conf_rehash(): conf file was malformed, aborting rehash");

    /* freeing the new conf strings */
    free_cstructs(&me, &svs);

    /* return everything to the way it was before */
    copy_me(hold_me, &me);
    copy_svs(hold_svs, &svs);

    free(hold_me);
    free(hold_svs);

    return FALSE;
  }

  /* may change */
  if (irccmp(hold_svs->chan, svs.chan))
  {
    part(hold_svs->chan, svs.nick);
    join(svs.chan, svs.nick);
  }

  if (irccmp(hold_svs->nick, svs.nick))
  {
    user_t *u;

    sts(":%s NICK %s :%ld", hold_svs->nick, svs.nick, CURRTIME);

    u = user_find(hold_svs->nick);

    /* remove the current one from the list */
    n = node_find(u, &userlist[u->hash]);
    node_del(n, &userlist[u->hash]);
    node_free(n);

    /* change the nick */
    free(u->nick);
    u->nick = sstrdup(svs.nick);

    /* readd with new nick (so the hash works) */
    n = node_create();
    u->hash = UHASH((unsigned char *)u->nick);
    node_add(u, n, &userlist[u->hash]);
  }

  if (hold_svs->join_chans != svs.join_chans)
  {
    mychan_t *mc;
    
    for (i = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mclist[i].head)
      {
        mc = (mychan_t *)n->data;

        if (mc->chan->nummembers >= 1)
        {
          if ((!chanuser_find(mc->chan, user_find(svs.nick))) &&
              (svs.join_chans))
            join(mc->chan->name, svs.nick);
          else
            part(mc->chan->name, svs.nick);
        }
      }
    }
  }

  if (hold_svs->leave_chans != svs.leave_chans)
  {
    mychan_t *mc;

    for (i = 0; i < HASHSIZE; i++)
    {
      LIST_FOREACH(n, mclist[i].head)
      {
        mc = (mychan_t *)n->data;

        if ((mc->chan->nummembers == 1) &&
            (chanuser_find(mc->chan, user_find(svs.nick))))
        {
          if (svs.leave_chans)
            part(mc->chan->name, svs.nick);
          else if (svs.join_chans)
            join(mc->chan->name, svs.nick);
        }
      }
    }
  }

  free_cstructs(hold_me, hold_svs);
  free(hold_me);
  free(hold_svs);

  /* no longer rehashing */
  runflags &= ~RF_REHASHING;
  slog(0, LG_DEBUG, "conf_rehash(): finished rehashing");

  return TRUE;
}

boolean_t conf_check(void)
{
  if (!me.name)
  {
    slog(0, LG_INFO, "conf_check(): no `name' set in %s", config_file);
    return FALSE;
  }

  if (!me.desc)
    me.desc = sstrdup("Shrike IRC Services");

  if (!me.uplink)
  {
    slog(0, LG_INFO, "conf_check(): no `uplink' set in %s", config_file);
    return FALSE;
  }

  if (!me.port)
  {
    slog(0, LG_INFO, "conf_check(): no `port' set in %s; defaulting to 6667",
         config_file);
    me.port = 6667;
  }

  if (!me.pass)
  {
    slog(0, LG_INFO, "conf_check(): no `pass' set in %s", config_file);
    return FALSE;
  }

  if ((!me.recontime) || (me.recontime < 10))
  {
    slog(0, LG_INFO, "conf_check(): invalid `recontime' set in %s; "
         "defaulting to 10", config_file);
    me.recontime = 10;
  }

  if ((!me.restarttime) || (me.restarttime < 10))
  {
    slog(0, LG_INFO, "conf_check(): invalid `restarttime' set in %s; "
         "defaulting to 10", config_file);
    me.restarttime = 10;
  }

  if (!me.netname)
  {
    slog(0, LG_INFO, "conf_check(): no `netname' set in %s", config_file);
    return FALSE;
  }

  if (!me.adminname)
  {
    slog(0, LG_INFO, "conf_check(): no `adminname' set in %s", config_file);
    return FALSE;
  }

  if (!me.adminemail)
  {
    slog(0, LG_INFO, "conf_check(): no `adminemail' set in %s", config_file);
    return FALSE;
  }

  if (!me.mta)
  {
    slog(0, LG_INFO, "conf_check(): no `mta' set in %s", config_file);
    return FALSE;
  }

  if (!me.maxusers)
  {
    slog(0, LG_INFO, "conf_check(): no `maxusers' set in %s; "
         "defaulting to 5", config_file);
    me.maxusers = 5;
  }

  if (!me.maxchans)
  {
    slog(0, LG_INFO, "conf_check(): no `maxchans' set in %s; "
         "defaulting to 5", config_file);
    me.maxchans = 5;
  }

  if (me.auth != 0 && me.auth != 1)
  {
    slog(0, LG_INFO, "conf_check(): no `auth' set in %s; "
         "defaulting to NONE", config_file);
    me.auth = AUTH_NONE;
  }

  if (!svs.nick || !svs.user || !svs.host || !svs.real)
  {
    slog(0, LG_INFO, "check_conf(): invalid clientinfo{} block in %s",
         config_file);
    return FALSE;
  }

  return TRUE;
}
