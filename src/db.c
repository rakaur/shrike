/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains database routines.
 */

#include "../inc/shrike.h"

/* write shrike.db */
void db_save(void *arg)
{
  myuser_t *mu;
  mychan_t *mc;
  chanacs_t *ca;
  kline_t *k;
  node_t *n, *tn;
  FILE *f;
  uint32_t i, muout = 0, mcout = 0, caout = 0, kout = 0;

  /* back them up first */
  if ((rename("etc/shrike.db", "etc/shrike.db.save")) < 0)
  {
    slog(LG_ERROR, "db_save(): cannot backup shrike.db");
    return;
  }

  if (!(f = fopen("etc/shrike.db", "w")))
  {
    slog(LG_ERROR, "db_save(): cannot write to shrike.db");
    return;
  }

  /* write the database version */
  fprintf(f, "DBV 1\n");

  slog(LG_DEBUG, "db_save(): saving myusers");

  for (i = 0; i < USER_HASH_SIZE; i++)
  {
    LIST_FOREACH(n, mulist[i].head)
    {
      mu = (myuser_t *)n->data;

      /* MU <name> <pass> <email> <registered> [lastlogin] [failnum] [lastfail]
       * [lastfailon] [flags] [key]
       */
      fprintf(f, "MU %s %s %s %ld",
              mu->name, mu->pass, mu->email, (long)mu->registered);

      if (mu->lastlogin)
        fprintf(f, " %ld", (long)mu->lastlogin);
      else
        fprintf(f, " 0");

      if (mu->failnum)
      {
        fprintf(f, " %d %s %ld", mu->failnum, mu->lastfail,
                (long)mu->lastfailon);
      }
      else
        fprintf(f, " 0 0 0");

      if (mu->flags)
        fprintf(f, " %d", mu->flags);
      else
        fprintf(f, " 0");

      if (mu->key)
        fprintf(f, " %ld\n", mu->key);
      else
        fprintf(f, "\n");

      muout++;
    }
  }

  slog(LG_DEBUG, "db_save(): saving mychans");

  for (i = 0; i < CHANNEL_HASH_SIZE; i++)
  {
    LIST_FOREACH(n, mclist[i].head)
    {
      mc = (mychan_t *)n->data;

      /* MC <name> <pass> <founder> <registered> [used] [flags]
       * [mlock_on] [mlock_off] [mlock_limit] [mlock_key]
       */
      fprintf(f, "MC %s %s %s %ld %ld", mc->name, mc->pass, mc->founder->name,
              (long)mc->registered, (long)mc->used);

      if (mc->flags)
        fprintf(f, " %d", mc->flags);
      else
        fprintf(f, " 0");

      if (mc->mlock_on)
        fprintf(f, " %d", mc->mlock_on);
      else
        fprintf(f, " 0");

      if (mc->mlock_off)
        fprintf(f, " %d", mc->mlock_off);
      else
        fprintf(f, " 0");

      if (mc->mlock_limit)
        fprintf(f, " %d", mc->mlock_limit);
      else
        fprintf(f, " 0");

      if (mc->mlock_key)
        fprintf(f, " %s\n", mc->mlock_key);
      else
        fprintf(f, "\n");

      mcout++;
    }
  }

  slog(LG_DEBUG, "db_save(): saving chanacs");

  for (i = 0; i < USER_HASH_SIZE; i++)
  {
    LIST_FOREACH(n, mclist[i].head)
    {
      mc = (mychan_t *)n->data;

      LIST_FOREACH(tn, mc->chanacs.head)
      {
        ca = (chanacs_t *)tn->data;

        /* CA <mychan> <myuser|hostmask> <level> */
        fprintf(f, "CA %s %s %ld\n", ca->mychan->name,
                (ca->host) ? ca->host : ca->myuser->name, (long)ca->level);

        caout++;
      }
    }
  }

  slog(LG_DEBUG, "db_save(): saving klines");

  LIST_FOREACH(n, klnlist.head)
  {
    k = (kline_t *)n->data;

    /* KL <user> <host> <duration> <settime> <setby> <reason> */
    fprintf(f, "KL %s %s %ld %ld %s %s\n", k->user, k->host, k->duration,
            (long)k->settime, k->setby, k->reason);

    kout++;
  }

  /* DE <muout> <mcout> <caout> <kout> */
  fprintf(f, "DE %d %d %d %d\n", muout, mcout, caout, kout);

  fclose(f);
  remove("etc/shrike.db.save");
}

/* loads shrike.db */
void db_load(void)
{
  sra_t *sra;
  myuser_t *mu;
  mychan_t *mc;
  kline_t *k;
  node_t *n;
  uint32_t i = 0, linecnt = 0, muin = 0, mcin = 0, cain = 0, kin = 0;
  FILE *f = fopen("etc/shrike.db", "r");
  char *item, *s, dBuf[BUFSIZE];

  if (!f)
  {
    slog(LG_ERROR, "db_load(): can't open shrike.db for reading");

    /* restore the backup */
    if ((rename("etc/shrike.db.save", "etc/shrike.db")) < 0)
      slog(LG_ERROR, "db_load(): unable to restore backup");
    else
    {
      slog(LG_ERROR, "db_load(): restored from backup");
      db_load();
    }

    return;
  }

  slog(LG_DEBUG,
       "db_load(): ----------------------- loading ------------------------");

  /* start reading it, one line at a time */
  while (fgets(dBuf, BUFSIZE, f))
  {
    linecnt++;

    /* check for unimportant lines */
    item = strtok(dBuf, " ");
    strip(item);

    if (*item == '#' || *item == '\n' || *item == '\t' || *item == ' ' ||
        *item == '\0' || *item == '\r' || !*item)
      continue;

    /* database version */
    if (!strcmp("DBV", item))
    {
      i = atoi(strtok(NULL, " "));
      if (i > 1)
      {
        slog(LG_INFO,
             "db_load(): database version is %d; i only understand "
             "1 or below", i);
        exit(EXIT_FAILURE);
      }
    }

    /* myusers */
    if (!strcmp("MU", item))
    {
      char *muname, *mupass, *muemail;

      if ((s = strtok(NULL, " ")))
      {
        if ((mu = myuser_find(s)))
          continue;

        muin++;

        muname = s;
        mupass = strtok(NULL, " ");
        muemail = strtok(NULL, " ");

        mu = myuser_add(muname, mupass, muemail);

        mu->registered = atoi(strtok(NULL, " "));

        mu->lastlogin = atoi(strtok(NULL, " "));

        mu->failnum = atoi(strtok(NULL, " "));
        mu->lastfail = sstrdup(strtok(NULL, " "));
        mu->lastfailon = atoi(strtok(NULL, " "));
        mu->flags = atol(strtok(NULL, " "));

        if ((s = strtok(NULL, " ")))
          mu->key = atoi(s);
      }
    }

    /* mychans */
    if (!strcmp("MC", item))
    {
      char *mcname, *mcpass;

      if ((s = strtok(NULL, " ")))
      {
        if ((mc = mychan_find(s)))
          continue;

        mcin++;

        mcname = s;
        mcpass = strtok(NULL, " ");

        mc = mychan_add(mcname, mcpass);

        mc->founder = myuser_find(strtok(NULL, " "));
        mc->registered = atoi(strtok(NULL, " "));
        mc->used = atoi(strtok(NULL, " "));
        mc->flags = atoi(strtok(NULL, " "));

        mc->mlock_on = atoi(strtok(NULL, " "));
        mc->mlock_off = atoi(strtok(NULL, " "));
        mc->mlock_limit = atoi(strtok(NULL, " "));

        if ((s = strtok(NULL, " ")))
        {
          strip(s);
          mc->mlock_key = sstrdup(s);
        }
      }
    }

    /* chanacs */
    if (!strcmp("CA", item))
    {
      chanacs_t *ca;
      char *cachan, *causer;

      cachan = strtok(NULL, " ");
      causer = strtok(NULL, " ");

      if (cachan && causer)
      {
        mc = mychan_find(cachan);
        mu = myuser_find(causer);

        cain++;

        if ((!mu) && (validhostmask(causer)))
          ca = chanacs_add_host(mc, causer, atol(strtok(NULL, " ")));
        else
          ca = chanacs_add(mc, mu, atol(strtok(NULL, " ")));

        if (ca->level & CA_SUCCESSOR)
          ca->mychan->successor = ca->myuser;
      }
    }

    /* klines */
    if (!strcmp("KL", item))
    {
      char *user, *host, *reason, *setby, *tmp;
      time_t settime;
      long duration;

      user = strtok(NULL, " ");
      host = strtok(NULL, " ");
      tmp = strtok(NULL, " ");
      duration = atol(tmp);
      tmp = strtok(NULL, " ");
      settime = atol(tmp);
      setby = strtok(NULL, " ");
      reason = strtok(NULL, "");

      strip(reason);

      k = kline_add(user, host, reason, duration);
      k->settime = settime;
      k->setby = sstrdup(setby);

      kin++;
    }

    /* end */
    if (!strcmp("DE", item))
    {
      i = atoi(strtok(NULL, " "));
      if (i != muin)
        slog(LG_ERROR, "db_load(): got %d myusers; expected %d", muin, i);

      i = atoi(strtok(NULL, " "));
      if (i != mcin)
        slog(LG_ERROR, "db_load(): got %d mychans; expected %d", mcin, i);

      i = atoi(strtok(NULL, " "));
      if (i != cain)
        slog(LG_ERROR, "db_load(): got %d chanacs; expected %d", cain, i);

      if ((s = strtok(NULL, " ")))
        if ((i = atoi(s)) != kin)
          slog(LG_ERROR, "db_load(): got %d klines; expected %d", kin, i);
    }
  }

  /* now we update the sra list */
  LIST_FOREACH(n, sralist.head)
  {
    sra = (sra_t *)n->data;

    if (!sra->myuser)
    {
      sra->myuser = myuser_find(sra->name);

      if (sra->myuser)
      {
        slog(LG_DEBUG, "db_load(): updating %s to SRA", sra->name);
        sra->myuser->sra = sra;
      }
    }
  }

  fclose(f);

  slog(LG_DEBUG,
       "db_load(): ------------------------- done -------------------------");
}
