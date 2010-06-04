/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains channel mode tracking routines.
 */

#include "../inc/shrike.h"

#define MTYPE_NUL 0
#define MTYPE_ADD 1
#define MTYPE_DEL 2

struct cmode_
{
  char mode;
  uint32_t value;
};

/* *INDENT-OFF* */

static struct cmode_ mode_list[] = {
  { 'i', CMODE_INVITE },
  { 'm', CMODE_MOD    },
  { 'n', CMODE_NOEXT  },
  { 'p', CMODE_PRIV   },
  { 's', CMODE_SEC    },
  { 't', CMODE_TOPIC  },
  { '\0', 0 }
};

static struct cmode_ ignore_mode_list[] = {
  { 'b', CMODE_BAN    },
  { 'e', CMODE_EXEMPT },
  { 'I', CMODE_INVEX  },
  { '\0', 0 }
};

static struct cmode_ status_mode_list[] = {
  { 'o', CMODE_OP    },
  { 'v', CMODE_VOICE },
  { '\0', 0 }
};

/* *INDENT-ON* */

/* yeah, this should be fun. */
void channel_mode(channel_t *chan, uint8_t parc, char *parv[])
{
  boolean_t matched = FALSE;
  int i, parpos = 0, whatt = MTYPE_NUL;
  char *pos = parv[0];
  mychan_t *mc;
  chanuser_t *cu = NULL;

  if ((!pos) || (*pos == '\0'))
    return;

  if (!chan)
    return;

  /* SJOIN modes of 0 means no change */
  if (*pos == '0')
    return;

  /* assumed that the parc is correct.  ircd does, too. */
  for (; *pos != '\0'; pos++)
  {
    matched = FALSE;

    if (*pos == '+')
    {
      whatt = MTYPE_ADD;
      continue;
    }
    if (*pos == '-')
    {
      whatt = MTYPE_DEL;
      continue;
    }

    for (i = 0; mode_list[i].mode != '\0'; i++)
    {
      if (*pos == mode_list[i].mode)
      {
        matched = TRUE;

        if (whatt == MTYPE_ADD)
          chan->modes |= mode_list[i].value;
        else
          chan->modes &= ~mode_list[i].value;

        break;
      }
    }

    if (matched == TRUE)
      continue;

    for (i = 0; ignore_mode_list[i].mode != '\0'; i++)
    {
      if (*pos == ignore_mode_list[i].mode)
      {
        matched = TRUE;
        parpos++;
        break;
      }
    }

    if (matched == TRUE)
      continue;

    for (i = 0; ignore_mode_list[i].mode != '\0'; i++)
    {
      if (*pos == ignore_mode_list[i].mode)
      {
        matched = TRUE;
        parpos++;
        break;
      }
    }

    if (matched == TRUE)
      continue;

    if (*pos == 'l')
    {
      if (whatt == MTYPE_ADD)
      {
        chan->modes |= CMODE_LIMIT;
        chan->limit = atoi(parv[++parpos]);
      }
      else
      {
        chan->modes &= ~CMODE_LIMIT;
        chan->limit = 0;
      }
      continue;
    }

    if (*pos == 'k')
    {
      if (whatt == MTYPE_ADD)
      {
        chan->modes |= CMODE_KEY;
        chan->key = sstrdup(parv[++parpos]);
      }
      else
      {
        chan->modes &= ~CMODE_KEY;
        free(chan->key);
        chan->key = NULL;
        /* ratbox typically sends either the key or a `*' on -k, so you
         * should eat a parameter
         */
        parpos++;
      }
      continue;
    }

    for (i = 0; status_mode_list[i].mode != '\0'; i++)
    {
      if (*pos == status_mode_list[i].mode)
      {
        /* for some reason we apparently lose count.. */
        if (*parv[parpos] == '+' || *parv[parpos] == '-')
          parpos++;

        cu = chanuser_find(chan, user_find(parv[parpos]));

        if (cu == NULL)
        {
          slog(LG_ERROR, "channel_mode(): MODE %s %c%c %s", chan->name,
               (whatt == MTYPE_ADD) ? '+' : '-', status_mode_list[i].value,
               parv[parpos]);

          parpos++;
          matched = TRUE;
        }

        if (matched == TRUE)
          break;

        matched = TRUE;

        if (whatt == MTYPE_ADD)
        {
          cu->modes |= status_mode_list[i].value;

          /* see if they did something we have to undo */
          if ((mc = mychan_find(cu->chan->name)))
          {
            myuser_t *mu = cu->user->myuser;

            if ((MC_SECURE & mc->flags) && (status_mode_list[i].mode == 'o'))
            {
              char *hostbuf = make_hostmask(cu->user->nick, cu->user->user,
                                            cu->user->host);

              if ((!is_founder(mc, mu)) && (cu->user != svs.svs) &&
                  (!is_xop(mc, mu, (CA_AOP | CA_SOP))) &&
                  (!chanacs_find_host(mc, hostbuf, CA_AOP)))
              {
                /* they were opped and aren't on the list, deop them */
                cmode(svs.nick, mc->name, "-o", cu->user->nick);
                cu->modes &= ~status_mode_list[i].value;
              }
            }
          }
        }
        else
        {
          if (cu->user == svs.svs)
          {
            slog(LG_DEBUG, "channel_mode(): deopped on %s, rejoining",
                 cu->chan->name);

            /* we were deopped, part and join */
            part(cu->chan->name, svs.nick);
            join(cu->chan->name, svs.nick);

            continue;
          }

          cu->modes &= ~status_mode_list[i].value;
        }

        parpos++;
        break;
      }
    }
    if (matched == TRUE)
      continue;

    slog(LG_DEBUG, "channel_mode(): mode %c not matched", *pos);
  }

  check_modes(mychan_find(chan->name));
}

/* i'm putting usermode in here too */
void user_mode(user_t *user, char *modes)
{
  boolean_t toadd = FALSE;

  if (!user)
  {
    slog(LG_DEBUG, "user_mode(): called for nonexistant user");
    return;
  }

  while (*modes != '\0')
  {
    switch (*modes)
    {
      case '+':
        toadd = TRUE;
        break;
      case '-':
        toadd = FALSE;
        break;
      case 'o':
        if (toadd)
        {
          if (!is_ircop(user))
          {
            user->flags |= UF_IRCOP;
            slog(LG_DEBUG, "user_mode(): %s is now an IRCop", user->nick);
            snoop("OPER: %s (%s)", user->nick, user->server->name);
          }
        }
        else
        {
          if (is_ircop(user))
          {
            user->flags &= ~UF_IRCOP;
            slog(LG_DEBUG, "user_mode(): %s is no longer an IRCop",
                 user->nick);
            snoop("DEOPER: %s (%s)", user->nick, user->server->name);
          }
        }
      default:
        break;
    }
    modes++;
  }
}

/* mode stacking code was borrowed from cygnus.
 * credit to darcy grexton and andrew church.
 */

/* mode stacking struct */
struct modedata_
{
  time_t used;
  int last_add;
  char channel[64];
  char sender[32];
  int32_t binmodes_on;
  int32_t binmodes_off;
  char opmodes[MAXMODES * 2 + 1];
  char params[BUFSIZE];
  int nparams;
  int paramslen;
  uint32_t event;
} modedata[3];

/* flush stacked and waiting cmodes */
static void flush_cmode(struct modedata_ *md)
{
  char buf[BUFSIZE];
  int len = 0;
  char lastc = 0;

  if (!md->binmodes_on && !md->binmodes_off && !*md->opmodes)
  {
    memset(md, 0, sizeof(*md));
    md->last_add = -1;
    return;
  }

  if (md->binmodes_off)
  {
    len += snprintf(buf + len, sizeof(buf) - len, "-%s",
                    flags_to_string(md->binmodes_off));
    lastc = '-';
  }

  if (md->binmodes_on)
  {
    len += snprintf(buf + len, sizeof(buf) - len, "+%s",
                    flags_to_string(md->binmodes_on));
    lastc = '+';
  }

  if (*md->opmodes)
  {
    if (*md->opmodes == lastc)
      memmove(md->opmodes, md->opmodes + 1, strlen(md->opmodes + 1) + 1);
    len += snprintf(buf + len, sizeof(buf) - len, "%s", md->opmodes);
  }

  if (md->paramslen)
    snprintf(buf + len, sizeof(buf) - len, " %s", md->params);

  sts(":%s MODE %s %s", md->sender, md->channel, buf);

  memset(md, 0, sizeof(*md));
  md->last_add = -1;
}

void flush_cmode_callback(void *arg)
{
  flush_cmode((struct modedata_ *)arg);
}

/* stacks channel modes to be applied to a channel */
void cmode(char *sender, ...)
{
  va_list args;
  char *channel, *modes;
  struct modedata_ *md;
  int which = -1, add;
  int32_t flag;
  int i;
  char c, *s;

  if (!sender)
  {
    for (i = 0; i < 3; i++)
    {
      if (modedata[i].used)
        flush_cmode(&modedata[i]);
    }
  }

  va_start(args, sender);
  channel = va_arg(args, char *);
  modes = va_arg(args, char *);

  for (i = 0; i < 3; i++)
  {
    if ((modedata[i].used) && (!strcasecmp(modedata[i].channel, channel)))
    {
      if (strcasecmp(modedata[i].sender, sender))
        flush_cmode(&modedata[i]);

      which = i;
      break;
    }
  }

  if (which < 0)
  {
    for (i = 0; i < 3; i++)
    {
      if (!modedata[i].used)
      {
        which = i;
        modedata[which].last_add = -1;
        break;
      }
    }
  }

  if (which < 0)
  {
    int oldest = 0;
    time_t oldest_time = modedata[0].used;

    for (i = 1; i < 3; i++)
    {
      if (modedata[i].used < oldest_time)
      {
        oldest_time = modedata[i].used;
        oldest = i;
      }
    }

    flush_cmode(&modedata[oldest]);
    which = oldest;
    modedata[which].last_add = -1;
  }

  md = &modedata[which];
  strscpy(md->sender, sender, 32);
  strscpy(md->channel, channel, 64);

  add = -1;

  while ((c = *modes++))
  {
    if (c == '+')
    {
      add = 1;
      continue;
    }
    else if (c == '-')
    {
      add = 0;
      continue;
    }
    else if (add < 0)
      continue;

    switch (c)
    {
      case 'l':
      case 'k':
      case 'o':
      case 'v':
        if (md->nparams >= MAXMODES || md->paramslen >= MAXPARAMSLEN)
        {
          flush_cmode(&modedata[which]);
          strscpy(md->sender, sender, 32);
          strscpy(md->channel, channel, 64);
          md->used = CURRTIME;
        }

        s = md->opmodes + strlen(md->opmodes);

        if (add != md->last_add)
        {
          *s++ = add ? '+' : '-';
          md->last_add = add;
        }

        *s++ = c;

        if (!add && c == 'l')
          break;

        s = va_arg(args, char *);

        md->paramslen += snprintf(md->params + md->paramslen,
                                  MAXPARAMSLEN + 1 - md->paramslen, "%s%s",
                                  md->paramslen ? " " : "", s);

        md->nparams++;
        break;

      default:
        flag = mode_to_flag(c);

        if (add)
        {
          md->binmodes_on |= flag;
          md->binmodes_off &= ~flag;
        }
        else
        {
          md->binmodes_off |= flag;
          md->binmodes_on &= ~flag;
        }
    }
  }

  va_end(args);
  md->used = CURRTIME;

  if (!md->event)
    md->event = event_add_once("flush_cmode_callback", flush_cmode_callback,
                               md, 1);
}

void check_modes(mychan_t *mychan)
{
  char newmodes[40], *newkey = NULL;
  char *end = newmodes;
  int32_t newlimit = 0;
  int modes, set_limit = 0, set_key = 0;

  if (!mychan || !mychan->chan)
    return;

  modes = ~mychan->chan->modes & mychan->mlock_on;

  end += snprintf(end, sizeof(newmodes) - (end - newmodes) - 2,
                  "+%s", flags_to_string(modes));

  mychan->chan->modes |= modes;

  if (mychan->mlock_limit && mychan->mlock_limit != mychan->chan->limit)
  {
    *end++ = 'l';
    newlimit = mychan->mlock_limit;
    mychan->chan->limit = newlimit;
    set_limit = 1;
  }

  if (mychan->mlock_key)
  {
    if (mychan->chan->key && strcmp(mychan->chan->key, mychan->mlock_key))
    {
      sts(":%s MODE %s -k %s", svs.nick, mychan->name, mychan->chan->key);
      free(mychan->chan->key);
      mychan->chan->key = NULL;
    }

    if (!mychan->chan->key)
    {
      newkey = mychan->mlock_key;
      mychan->chan->key = sstrdup(newkey);
      sts(":%s MODE %s +k %s", svs.nick, mychan->name, newkey);
      set_key = 0;
    }
  }

  if (end[-1] == '+')
    end--;

  modes = mychan->chan->modes & mychan->mlock_off;
  modes &= ~(CMODE_KEY | CMODE_LIMIT);

  end += snprintf(end, sizeof(newmodes) - (end - newmodes) - 1,
                  "-%s", flags_to_string(modes));

  mychan->chan->modes &= ~modes;

  if (mychan->chan->limit && (mychan->mlock_off & CMODE_LIMIT))
  {
    *end++ = 'l';
    mychan->chan->limit = 0;
  }

  if (mychan->chan->key && (mychan->mlock_off & CMODE_KEY))
  {
    *end++ = 'k';
    newkey = sstrdup(mychan->chan->key);
    free(mychan->chan->key);
    mychan->chan->key = NULL;
    set_key = 1;
  }

  if (end[-1] == '-')
    end--;

  if (end == newmodes)
    return;

  *end = 0;

  cmode(svs.nick, mychan->name, newmodes,
        (set_limit) ? itoa(newlimit) : (set_key) ? newkey : "");

  if (newkey && !mychan->chan->key)
    free(newkey);
}
