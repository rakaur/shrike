/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the SET command.
 */

#include "../inc/shrike.h"

/* SET <username|#channel> <setting> <parameters> */
void do_set(char *origin)
{
  char *name = strtok(NULL, " ");
  char *setting = strtok(NULL, " ");
  char *params = strtok(NULL, " ");
  struct set_command_ *c;

  if (!name || !setting || !params)
  {
    notice(origin, "Insufficient parameters specificed for \2SET\2.");
    notice(origin, "Syntax: SET <username|#channel> <setting> <parameters>");
    return;
  }

  /* take the command through the hash table */
  if ((c = set_cmd_find(origin, setting)))
  {
    if (c->func)
      c->func(origin, name, params);
    else
      notice(origin, "Invalid setting.  Please use \2HELP\2 for help.");
  }
}

static void do_set_email(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;

  if (*name == '#')
  {
    notice(origin, "Invalid parameters specified for \2EMAIL\2.");
    return;
  }

  if (!(mu = myuser_find(name)))
  {
    notice(origin, "No such username: \2%s\2.", name);
    return;
  }

  if (mu != u->myuser)
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }

  if (mu->temp)
  {
    if (mu->key == atoi(params))
    {
      free(mu->email);
      mu->email = sstrdup(mu->temp);
      free(mu->temp);
      mu->temp = NULL;
      mu->key = 0;

      snoop("SET:EMAIL:VS: \2%s\2 by \2%s\2", mu->email, origin);

      notice(origin, "\2%s\2 has now been verified.", mu->email);

      return;
    }

    snoop("SET:EMAIL:VF: \2%s\2 by \2%s\2", mu->temp, origin);

    notice(origin, "Verification failed. Invalid key for \2%s\2.", mu->temp);

    return;
  }

  if (!validemail(params))
  {
    notice(origin, "\2%s\2 is not a valid email address.", params);
    return;
  }

  if (!strcasecmp(mu->email, params))
  {
    notice(origin, "The email address for \2%s\2 is already set to \2%s\2.",
           mu->name, mu->email);
    return;
  }

  snoop("SET:EMAIL: \2%s\2 (\2%s\2 -> \2%s\2)", mu->name, mu->email, params);

  if (me.auth == AUTH_EMAIL)
  {
    if (mu->temp)
      free(mu->temp);

    mu->temp = sstrdup(params);
    mu->key = makekey();

    notice(origin, "An email containing email changing instructions "
           "has been sent to \2%s\2.", mu->temp);
    notice(origin, "Your email address will not be changed until you follow "
           "these instructions.");

    sendemail(mu->name, itoa(mu->key), 3);

    return;
  }

  free(mu->email);
  mu->email = sstrdup(params);

  notice(origin, "The email address for \2%s\2 has been changed to \2%s\2.",
         mu->name, mu->email);
}

static void do_set_founder(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  mychan_t *mc;

  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if (*name != '#')
  {
    notice(origin, "Invalid parameters specified for \2FOUNDER\2.");
    return;
  }

  /* only other people that supply the password can set themselves as founder.
   * this prevents people from registering #kidsex and setting the founder to
   * someone else.
   */
  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  if (is_founder(mc, u->myuser))
  {
    notice(origin, "You are already founder on \2%s\2.", mc->name);
    return;
  }

  if (strcmp(mc->pass, params))
  {
    notice(origin, "Authorization failed. Invalid password for \2%s\2.",
           mc->name);
    return;
  }

  chanacs_delete(mc, mc->founder, CA_FOUNDER);
  chanacs_delete(mc, u->myuser, CA_VOP);
  chanacs_delete(mc, u->myuser, CA_AOP);
  chanacs_delete(mc, u->myuser, CA_SOP);
  mc->founder = u->myuser;
  chanacs_add(mc, u->myuser, CA_FOUNDER);

  snoop("SET:FOUNDER: \2%s\2 -> \2%s\2", mc->name, u->nick);

  notice(origin, "You have been set as founder for \2%s\2.", mc->name);
}

static void do_set_hidemail(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;

  if (*name == '#')
  {
    notice(origin, "Invalid parameters specified for \2HIDEMAIL\2.");
    return;
  }

  if (!(mu = myuser_find(name)))
  {
    notice(origin, "No such username: \2%s\2.", name);
    return;
  }

  if (mu != u->myuser)
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }


  if (!strcasecmp("ON", params))
  {
    if (MU_HIDEMAIL & mu->flags)
    {
      notice(origin, "The \2HIDEMAIL\2 flag is already set for \2%s\2.",
             mu->name);
      return;
    }

    snoop("SET:HIDEMAIL:ON: for \2%s\2", mu->name);

    mu->flags |= MU_HIDEMAIL;

    notice(origin, "The \2HIDEMAIL\2 flag has been set for \2%s\2.", mu->name);

    return;
  }

  else if (!strcasecmp("OFF", params))
  {
    if (!(MU_HIDEMAIL & mu->flags))
    {
      notice(origin, "The \2HIDEMAIL\2 flag is not set for \2%s\2.", mu->name);
      return;
    }

    snoop("SET:HIDEMAIL:OFF: for \2%s\2", mu->name);

    mu->flags &= ~MU_HIDEMAIL;

    notice(origin, "The \2HIDEMAIL\2 flag has been removed for \2%s\2.",
           mu->name);

    return;
  }

  else
  {
    notice(origin, "Invalid parameters specified for \2HIDEMAIL\2.");
    return;
  }
}

static void do_set_hold(char *origin, char *name, char *params)
{
  myuser_t *mu;
  mychan_t *mc;

  if (*name == '#')
  {
    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    if (!strcasecmp("ON", params))
    {
      if (MC_HOLD & mc->flags)
      {
        notice(origin, "The \2HOLD\2 flag is already set for \2%s\2.",
               mc->name);
        return;
      }

      snoop("SET:HOLD:ON: for \2%s\2 by \2%s\2", mc->name, origin);

      mc->flags |= MC_HOLD;

      notice(origin, "The \2HOLD\2 flag has been set for \2%s\2.", mc->name);

      return;
    }

    else if (!strcasecmp("OFF", params))
    {
      if (!(MC_HOLD & mc->flags))
      {
        notice(origin, "The \2HOLD\2 flag is not set for \2%s\2.", mc->name);
        return;
      }

      snoop("SET:HOLD:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

      mc->flags &= ~MC_HOLD;

      notice(origin, "The \2HOLD\2 flag has been removed for \2%s\2.",
             mc->name);

      return;
    }

    else
    {
      notice(origin, "Invalid parameters specified for \2HOLD\2.");
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

    if (!strcasecmp("ON", params))
    {
      if (MU_HOLD & mu->flags)
      {
        notice(origin, "The \2HOLD\2 flag is already set for \2%s\2.",
               mu->name);
        return;
      }

      snoop("SET:HOLD:ON: for \2%s\2 by \2%s\2", mu->name, origin);

      mu->flags |= MU_HOLD;

      notice(origin, "The \2HOLD\2 flag has been set for \2%s\2.", mu->name);

      return;
    }

    else if (!strcasecmp("OFF", params))
    {
      if (!(MU_HOLD & mu->flags))
      {
        notice(origin, "The \2HOLD\2 flag is not set for \2%s\2.", mu->name);
        return;
      }

      snoop("SET:HOLD:OFF: for \2%s\2 by \2%s\2", mu->name, origin);

      mu->flags &= ~MU_HOLD;

      notice(origin, "The \2HOLD\2 flag has been removed for \2%s\2.",
             mu->name);

      return;
    }

    else
    {
      notice(origin, "Invalid parameters specified for \2HOLD\2.");
      return;
    }
  }
}

static void do_set_mlock(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  mychan_t *mc;
  char *s, modebuf[32], *end, c;
  int add = -1;
  int32_t newlock_on = 0, newlock_off = 0, newlock_limit = 0;
  char *newlock_key = NULL;

  if (*name != '#')
  {
    notice(origin, "Invalid parameters specified for \2MLOCK\2.");
    return;
  }

  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }

  while (*params)
  {
    if (*params != '+' && *params != '-' && add < 0)
    {
      params++;
      continue;
    }

    switch ((c = *params++))
    {
      case '+':
        add = 1;
        break;

      case '-':
        add = 0;
        break;

      case 'k':
        if (add)
        {
          if (!(s = strtok(NULL, " ")))
          {
            notice(origin, "You need to specify which key to MLOCK.");
            return;
          }

          if (newlock_key)
            free(newlock_key);

          newlock_key = sstrdup(s);
          newlock_off &= ~CMODE_KEY;
        }
        else
        {
          if (newlock_key)
          {
            free(newlock_key);
            newlock_key = NULL;
          }

          newlock_off |= CMODE_KEY;
        }

        break;

      case 'l':
        if (add)
        {
          if (!(s = strtok(NULL, " ")))
          {
            notice(origin, "You need to specify what limit to MLOCK.");
            return;
          }

          if (atol(s) <= 0)
          {
            notice(origin, "You must specify a positive integer for limit.");
            return;
          }

          newlock_limit = atol(s);
          newlock_off &= ~CMODE_LIMIT;
        }
        else
        {
          newlock_limit = 0;
          newlock_off |= CMODE_LIMIT;
        }

        break;

      default:
      {
        int32_t flag = mode_to_flag(c);

        if (flag)
        {
          if (add)
            newlock_on |= flag, newlock_off &= ~flag;
          else
            newlock_off |= flag, newlock_on &= ~flag;
        }
      }
    }
  }

  /* save it to mychan */
  mc->mlock_on = newlock_on;
  mc->mlock_off = newlock_off;
  mc->mlock_limit = newlock_limit;

  if (mc->mlock_key)
    free(mc->mlock_key);

  mc->mlock_key = newlock_key;

  end = modebuf;
  *end = 0;

  if (mc->mlock_on || mc->mlock_key || mc->mlock_limit)
    end += snprintf(end, sizeof(modebuf) - (end - modebuf), "+%s%s%s",
                    flags_to_string(mc->mlock_on),
                    mc->mlock_key ? "k" : "", mc->mlock_limit ? "l" : "");

  if (mc->mlock_off)
    end += snprintf(end, sizeof(modebuf) - (end - modebuf), "-%s",
                    flags_to_string(mc->mlock_off));

  if (*modebuf)
  {
    notice(origin, "The MLOCK for \2%s\2 has been set to \2%s\2.",
           mc->name, modebuf);
    snoop("SET:MLOCK: \2%s\2 to \2%s\2 by \2%s\2", mc->name, modebuf, origin);
  }
  else
  {
    notice(origin, "The MLOCK for \2%s\2 has been removed.", mc->name);
    snoop("SET:MLOCK:OFF: \2%s\2 by \2%s\2", mc->name, origin);
  }

  check_modes(mc);

  return;
}

static void do_set_neverop(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;

  if (*name == '#')
  {
    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
    {
      notice(origin, "You are not authorized to perform this command.");
      return;
    }

    if (!strcasecmp("ON", params))
    {
      if (MC_NEVEROP & mc->flags)
      {
        notice(origin, "The \2NEVEROP\2 flag is already set for \2%s\2.",
               mc->name);
        return;
      }

      snoop("SET:NEVEROP:ON: for \2%s\2 by \2%s\2", mc->name, origin);

      mc->flags |= MC_NEVEROP;

      notice(origin, "The \2NEVEROP\2 flag has been set for \2%s\2.",
             mc->name);

      return;
    }

    else if (!strcasecmp("OFF", params))
    {
      if (!(MC_NEVEROP & mc->flags))
      {
        notice(origin, "The \2NEVEROP\2 flag is not set for \2%s\2.",
               mc->name);
        return;
      }

      snoop("SET:NEVEROP:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

      mc->flags &= ~MC_NEVEROP;

      notice(origin, "The \2NEVEROP\2 flag has been removed for \2%s\2.",
             mc->name);

      return;
    }

    else
    {
      notice(origin, "Invalid parameters specified for \2NEVEROP\2.");
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

    if (u->myuser != mu)
    {
      notice(origin, "You are not authorized to perform this command.");
      return;
    }

    if (!strcasecmp("ON", params))
    {
      if (MU_NEVEROP & mu->flags)
      {
        notice(origin, "The \2NEVEROP\2 flag is already set for \2%s\2.",
               mu->name);
        return;
      }

      snoop("SET:NEVEROP:ON: for \2%s\2 by \2%s\2", mu->name, origin);

      mu->flags |= MU_NEVEROP;

      notice(origin, "The \2NEVEROP\2 flag has been set for \2%s\2.",
             mu->name);

      return;
    }

    else if (!strcasecmp("OFF", params))
    {
      if (!(MU_NEVEROP & mu->flags))
      {
        notice(origin, "The \2NEVEROP\2 flag is not set for \2%s\2.",
               mu->name);
        return;
      }

      snoop("SET:NEVEROP:OFF: for \2%s\2 by \2%s\2", mu->name, origin);

      mu->flags &= ~MU_NEVEROP;

      notice(origin, "The \2NEVEROP\2 flag has been removed for \2%s\2.",
             mu->name);

      return;
    }

    else
    {
      notice(origin, "Invalid parameters specified for \2NEVEROP\2.");
      return;
    }
  }
}

static void do_set_noop(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;

  if (*name == '#')
  {
    notice(origin, "Invalid parameters specified for \2NEVEROP\2.");
    return;
  }

  if (!(mu = myuser_find(name)))
  {
    notice(origin, "No such username: \2%s\2.", name);
    return;
  }

  if (mu != u->myuser)
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }


  if (!strcasecmp("ON", params))
  {
    if (MU_NOOP & mu->flags)
    {
      notice(origin, "The \2NOOP\2 flag is already set for \2%s\2.", mu->name);
      return;
    }

    snoop("SET:NOOP:ON: for \2%s\2", mu->name);

    mu->flags |= MU_NOOP;

    notice(origin, "The \2NOOP\2 flag has been set for \2%s\2.", mu->name);

    return;
  }

  else if (!strcasecmp("OFF", params))
  {
    if (!(MU_NOOP & mu->flags))
    {
      notice(origin, "The \2NOOP\2 flag is not set for \2%s\2.", mu->name);
      return;
    }

    snoop("SET:NOOP:OFF: for \2%s\2", mu->name);

    mu->flags &= ~MU_NOOP;

    notice(origin, "The \2NOOP\2 flag has been removed for \2%s\2.", mu->name);

    return;
  }

  else
  {
    notice(origin, "Invalid parameters specified for \2NOOP\2.");
    return;
  }
}

static void do_set_password(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;

  if (*name == '#')
  {
    if (!(mc = mychan_find(name)))
    {
      notice(origin, "No such channel: \2%s\2.", name);
      return;
    }

    if (!is_founder(mc, u->myuser))
    {
      notice(origin, "You are not authorized to perform this command.");
      return;
    }

    snoop("SET:PASSWORD: \2%s\2 as \2%s\2 for \2%s\2", u->nick,
          u->myuser->name, mc->name);

    free(mc->pass);
    mc->pass = sstrdup(params);

    notice(origin, "The password for \2%s\2 has been changed to \2%s\2. "
           "Please write this down for future reference.", mc->name, mc->pass);

    return;
  }

  else
  {
    if (!(mu = myuser_find(name)))
    {
      notice(origin, "No such username: \2%s\2.", name);
      return;
    }

    if (u->myuser != mu)
    {
      notice(origin, "You are not authorized to perform this command.");
      return;
    }

    snoop("SET:PASSWORD: \2%s\2 as \2%s\2 for \2%s\2", u->nick,
          mu->name, mu->name);

    free(mu->pass);
    mu->pass = sstrdup(params);

    notice(origin, "The password for \2%s\2 has been changed to \2%s\2. "
           "Please write this down for future reference.", mu->name, mu->pass);

    return;
  }
}

static void do_set_secure(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  mychan_t *mc;

  if (*name != '#')
  {
    notice(origin, "Invalid parameters specified for \2SECURE\2.");
    return;
  }

  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }

  if (!strcasecmp("ON", params))
  {
    if (MC_SECURE & mc->flags)
    {
      notice(origin, "The \2SECURE\2 flag is already set for \2%s\2.",
             mc->name);
      return;
    }

    snoop("SET:SECURE:ON: for \2%s\2 by \2%s\2", mc->name, origin);

    mc->flags |= MC_SECURE;

    notice(origin, "The \2SECURE\2 flag has been set for \2%s\2.", mc->name);

    return;
  }

  else if (!strcasecmp("OFF", params))
  {
    if (!(MC_SECURE & mc->flags))
    {
      notice(origin, "The \2SECURE\2 flag is not set for \2%s\2.", mc->name);
      return;
    }

    snoop("SET:SECURE:OFF: for \2%s\2 by \2%s\2", mc->name, origin);

    mc->flags &= ~MC_SECURE;

    notice(origin, "The \2SECURE\2 flag has been removed for \2%s\2.",
           mc->name);

    return;
  }

  else
  {
    notice(origin, "Invalid parameters specified for \2SECURE\2.");
    return;
  }
}

static void do_set_successor(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  myuser_t *mu;
  mychan_t *mc;

  if (*name != '#')
  {
    notice(origin, "Invalid parameters specified for \2SUCCESSOR\2.");
    return;
  }

  if (!u->myuser)
  {
    notice(origin, "You are not logged in.");
    return;
  }

  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such chanel: \2%s\2.", name);
    return;
  }

  if (!is_founder(mc, u->myuser))
  {
    notice(origin, "You are not authorized to perform this operation.");
    return;
  }

  if (!strcasecmp("OFF", params) || !strcasecmp("NONE", params))
  {
    if (!mc->successor)
    {
      notice(origin, "There is no successor set for \2%s\2.", mc->name);
      return;
    }

    snoop("SET:SUCCESSOR:NONE: \2%s\2", mc->name);

    chanacs_delete(mc, mc->successor, CA_SUCCESSOR);

    notice(origin, "\2%s\2 is no longer the successor of \2%s\2.",
           mc->successor->name, mc->name);

    /* if they're online, let them know about it */
    if (mc->successor->user != NULL)
      notice(mc->successor->user->nick,
             "You are no longer the successor of \2%s\2.", mc->name);

    mc->successor = NULL;

    return;
  }

  if (!(mu = myuser_find(params)))
  {
    notice(origin, "No such username: \2%s\2.", params);
    return;
  }

  if (is_founder(mc, u->myuser) && !strcasecmp(u->nick, params))
  {
    notice(origin, "You cannot be the successor if you are also the founder.");
    return;
  }

  if (mu->flags & MU_NOOP)
  {
    notice(origin, "\2%s\2 does not wish to be added to access lists.",
           mu->name);
    return;
  }

  if (is_successor(mc, mu))
  {
    notice(origin, "\2%s\2 is already the successor of \2%s\2.", mu->name,
           mc->name);
    return;
  }

  /* Is there a different successor? */
  if (mc->successor)
  {
    /* Let them know they've been replaced if they're online. */
    notice(origin, "\2%s\2 is no longer the successor of \2%s\2.",
           mc->successor->name, mc->name);

    if (mc->successor->user != NULL)
      notice(mc->successor->user->nick,
             "You are no longer the successor of \2%s\2.", mc->name);

    chanacs_delete(mc, mc->successor, CA_SUCCESSOR);
    mc->successor = NULL;
  }

  chanacs_delete(mc, mu, CA_VOP);
  chanacs_delete(mc, mu, CA_AOP);
  chanacs_delete(mc, mu, CA_SOP);
  mc->successor = mu;
  chanacs_add(mc, mu, CA_SUCCESSOR);

  snoop("SET:SUCCESSOR: \2%s\2 -> \2%s\2", mc->name, mu->name);

  notice(origin, "\2%s\2 is now the successor of \2%s\2.", mu->name, mc->name);

  /* if they're online, let them know about it */
  if (mu->user != NULL)
    notice(mu->user->nick, "\2%s\2 has set you as the successor of \2%s\2.",
           u->myuser->name, mc->name);
}

static void do_set_verbose(char *origin, char *name, char *params)
{
  user_t *u = user_find(origin);
  mychan_t *mc;

  if (*name != '#')
  {
    notice(origin, "Invalid parameters specified for \2VERBOSE\2.");
    return;
  }

  if (!(mc = mychan_find(name)))
  {
    notice(origin, "No such channel: \2%s\2.", name);
    return;
  }

  if ((!is_founder(mc, u->myuser)) && (!is_successor(mc, u->myuser)))
  {
    notice(origin, "You are not authorized to perform this command.");
    return;
  }

  if (!strcasecmp("ON", params))
  {
    if (MC_VERBOSE & mc->flags)
    {
      notice(origin, "The \2VERBOSE\2 flag is already set for \2%s\2.",
             mc->name);
      return;
    }

    snoop("SET:VERBOSE:ON: for \2%s\2", mc->name);

    mc->flags |= MC_VERBOSE;

    notice(origin, "The \2VERBOSE\2 flag has been set for \2%s\2.", mc->name);

    return;
  }

  else if (!strcasecmp("OFF", params))
  {
    if (!(MC_VERBOSE & mc->flags))
    {
      notice(origin, "The \2VERBOSE\2 flag is not set for \2%s\2.", mc->name);
      return;
    }

    snoop("SET:VERBOSE:OFF: for \2%s\2", mc->name);

    mc->flags &= ~MC_VERBOSE;

    notice(origin, "The \2VERBOSE\2 flag has been removed for \2%s\2.",
           mc->name);

    return;
  }

  else
  {
    notice(origin, "Invalid parameters specified for \2VERBOSE\2.");
    return;
  }
}

/* *INDENT-OFF* */

/* commands we understand */
struct set_command_ set_commands[] = {
  { "EMAIL",      AC_NONE, do_set_email      },
  { "FOUNDER",    AC_NONE, do_set_founder    },
  { "HIDEMAIL",   AC_NONE, do_set_hidemail   },
  { "HOLD",       AC_SRA,  do_set_hold       },
  { "MLOCK",      AC_NONE, do_set_mlock      },
  { "NEVEROP",    AC_NONE, do_set_neverop    },
  { "NOOP",       AC_NONE, do_set_noop       },
  { "PASSWORD",   AC_NONE, do_set_password   },
  { "SECURE",     AC_NONE, do_set_secure     },
  { "SUCCESSOR",  AC_NONE, do_set_successor  }, 
  { "VERBOSE",    AC_NONE, do_set_verbose    },
  { NULL }
};

/* *INDENT-ON* */

struct set_command_ *set_cmd_find(char *origin, char *command)
{
  user_t *u = user_find(origin);
  struct set_command_ *c;

  for (c = set_commands; c->name; c++)
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
