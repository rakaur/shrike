/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This file contains routines to handle the HELP command.
 */

#include "../inc/shrike.h"

/* HELP <command> [params] */
void do_help(char *origin)
{
  user_t *u = user_find(origin);
  char *command = strtok(NULL, "");
  char buf[BUFSIZE];
  struct help_command_ *c;
  FILE *help_file;

  if (!command)
  {
    notice(origin, "The following core commands are available.");
    notice(origin, "\2HELP\2          Displays help information.");
    notice(origin, "\2REGISTER\2      Registers a username or channel.");
    notice(origin, "\2DROP\2          Drops a registered user or channel.");
    notice(origin, "\2LOGIN\2         Logs you into services.");
    notice(origin, "\2LOGOUT\2        Logs you out of services.");
    notice(origin, "\2SET\2           Sets various control flags.");
    notice(origin, "\2STATUS\2        Displays your status in services.");
    notice(origin, "\2INFO\2          Displays information on registrations.");
    notice(origin, " ");
    notice(origin, "The following additional commands are available.");
    notice(origin, "\2SOP\2           Manipulates a channel's SOP list.");
    notice(origin, "\2AOP\2           Manipulates a channel's AOP list.");
    notice(origin, "\2VOP\2           Manipulates a channel's VOP list.");
    notice(origin, "\2OP|DEOP\2       Manipulates ops on a channel.");
    notice(origin, "\2VOICE|DEVOICE\2 Manipulates voices on a channel.");
    notice(origin, "\2INVITE\2        Invites a nickname to a channel.");
    notice(origin, "\2UNBANME\2       Unbans you from a channel.");
    notice(origin, "\2RECOVER\2       Regain control of your channel.");
    notice(origin, " ");

    if (is_sra(u->myuser))
    {
      notice(origin, "The following SRA commands are available.");
      notice(origin, "\2UPDATE\2      Flush the database to disk.");
      notice(origin, "\2REHASH\2      Reload the configuration file.");

      if (svs.raw)
      {
        notice(origin, "\2RAW\2         Send data to the uplink.");
        notice(origin, "\2INJECT\2      Fake incoming data from the uplink.");
      }

      notice(origin, "\2RESTART\2     Restart services.");
      notice(origin, "\2SHUTDOWN\2    Shuts down services.");
      notice(origin, " ");
    }

    if ((is_sra(u->myuser)) || (is_ircop(u)))
    {
      notice(origin, "The following IRCop commands are available.");
      notice(origin, "\2SENDPASS\2      Email registration passwords.");
      notice(origin, "\2GLOBAL\2        Send a global notice.");
      notice(origin, "\2KLINE\2         Manage global KLINE's.");
      notice(origin, " ");
    }

    notice(origin, "For more specific help use \2HELP \37command\37\2.");

    return;
  }

  if (!strcasecmp("SET", command))
  {
    notice(origin, "Help for \2SET\2:");
    notice(origin, " ");
    notice(origin, "SET allows you to set various control flags");
    notice(origin, "for usernames and channels that change the");
    notice(origin, "way certain operations are performed on them.");
    notice(origin, " ");
    notice(origin, "The following commands are available.");
    notice(origin, "\2EMAIL\2         Changes the email address associated "
           "with a username.");
    notice(origin, "\2FOUNDER\2       Sets you as founder of a channel.");
    notice(origin, "\2HIDEMAIL\2      Hides a username's email address");
    notice(origin, "\2MLOCK\2         Sets channel mode lock.");
    notice(origin, "\2NEVEROP\2       Prevents services from automatically "
           "setting modes associated with access lists.");
    notice(origin, "\2NOOP\2          Prevents you from being added to "
           "access lists.");
    notice(origin, "\2PASSWORD\2      Change the password of a username or "
           "channel.");
    notice(origin, "\2SECURE\2        Prevents unauthorized people from "
           "gaining operator status.");
    notice(origin, "\2SUCCESSOR\2     Sets a channel successor.");
    notice(origin, "\2VERBOSE\2       Notifies channel about access list "
           "modifications.");
    notice(origin, " ");

    if (is_sra(u->myuser))
    {
      notice(origin, "The following SRA commands are available.");
      notice(origin, "\2HOLD\2        Prevents services from expiring a "
             "username or channel.");
      notice(origin, " ");
    }

    notice(origin, "For more specific help use \2HELP SET \37command\37\2.");

    return;
  }

  /* take the command through the hash table */
  if ((c = help_cmd_find(origin, command)))
  {
    if (c->file)
    {
      help_file = fopen(c->file, "r");

      if (!help_file)
      {
        notice(origin, "No help available for \2%s\2.", command);
        return;
      }

      while (fgets(buf, BUFSIZE, help_file))
      {
        strip(buf);

        replace(buf, sizeof(buf), "&nick&", svs.nick);

        if (buf[0])
          notice(origin, "%s", buf);
        else
          notice(origin, " ");
      }

      fclose(help_file);
    }
    else
      notice(origin, "No help available for \2%s\2.", command);
  }
}

/* *INDENT-OFF* */

/* help commands we understand */
struct help_command_ help_commands[] = {
  { "LOGIN",    AC_NONE,  "help/login"    },
  { "LOGOUT",   AC_NONE,  "help/logout"   },
  { "HELP",     AC_NONE,  "help/help"     },
  { "SOP",      AC_NONE,  "help/xop"      },
  { "AOP",      AC_NONE,  "help/xop"      },
  { "VOP",      AC_NONE,  "help/xop"      },
  { "OP",       AC_NONE,  "help/op_voice" },
  { "DEOP",     AC_NONE,  "help/op_voice" },
  { "VOICE",    AC_NONE,  "help/op_voice" },
  { "DEVOICE",  AC_NONE,  "help/op_voice" },
  { "INVITE",   AC_NONE,  "help/invite"   },
  { "INFO",     AC_NONE,  "help/info"     },
  { "RECOVER",  AC_NONE,  "help/recover"  },
  { "REGISTER", AC_NONE,  "help/register" },
  { "DROP",     AC_NONE,  "help/drop"     },
  { "UNBANME",  AC_NONE,  "help/unbanme"  },
  { "KLINE",    AC_IRCOP, "help/kline"    },
  { "UPDATE",   AC_SRA,   "help/update"   },
  { "STATUS",   AC_NONE,  "help/status"   },
  { "SENDPASS", AC_IRCOP, "help/sendpass" },
  { "GLOBAL",   AC_IRCOP, "help/global"   },
  { "RESTART",  AC_SRA,   "help/restart"  },
  { "SHUTDOWN", AC_SRA,   "help/shutdown" },
  { "RAW",      AC_SRA,   "help/raw"      },
  { "REHASH",   AC_SRA,   "help/rehash"   },
  { "INJECT",   AC_SRA,   "help/inject"   },

  { "SET EMAIL",     AC_NONE, "help/set_email"     },
  { "SET FOUNDER",   AC_NONE, "help/set_founder"   },
  { "SET HIDEMAIL",  AC_NONE, "help/set_hidemail"  },
  { "SET HOLD",      AC_SRA,  "help/set_hold"      },
  { "SET MLOCK",     AC_NONE, "help/set_mlock"     },
  { "SET NEVEROP",   AC_NONE, "help/set_neverop"   },
  { "SET NOOP",      AC_NONE, "help/set_noop"      },
  { "SET PASSWORD",  AC_NONE, "help/set_password"  },
  { "SET SECURE",    AC_NONE, "help/set_secure"    },
  { "SET SUCCESSOR", AC_NONE, "help/set_successor" },
  { "SET VERBOSE",   AC_NONE, "help/set_verbose"   },

  { NULL }
};

/* *INDENT-ON* */

struct help_command_ *help_cmd_find(char *origin, char *command)
{
  user_t *u = user_find(origin);
  struct help_command_ *c;

  for (c = help_commands; c->name; c++)
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
  notice(origin, "No help available for \2%s\2.", command);
  return NULL;
}
