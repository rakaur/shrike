/*
 * Copyright (c) 2003-2010 Eric Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * General documentation.
 */

Shrike -- a set of services for TSora networks
==============================================

This program is free but copyrighted software; see the LICENSE file for
details.

Information about Shrike may be found at http://shrike.malkier.net/. Development
information and repositories can be found at http://github.com/rakaur/shrike/

TABLE OF CONTENTS
-----------------
  1. Credits
  2. Presentation
  3. Installation
  4. Command line options
  5. Contact and support

1. CREDITS
----------

While Shrike isn't directly based on any other program it borrows a decent
amount of code from Cygnus and Sentinel.

Information about Sentinel can be found at http://ircd.botbay.net/sentinel/

Currently Shrike consists of the following developers:

- rakaur, Eric Will <rakaur@malkier.net>
- dKingston, Michael Rodriguez <dkingston02@gmail.com>

Thanks to testers, contributors, etc:

- sycobuny, Stephen Belcher <sycobuny@malkier.net>
- rintaun, Matt Lanigan <rintaun@projectxero.net>
- Hwy, W. Campbell <wcampbel@botbay.net>
- naki, Adam Walls <naki@coreag.net>
- Rob_X, Mikael <mickek15@hotmail.com>
- madragoran, Mike Campbell <madragoran@avendesora.net>

Special thanks to:

- Hwy, for your continued patience while I badger you endlessly.
- sycobuny, for your continued not-killing-me while I badger your code.
- tanka, for the book I will someday repay you for.
- dbn, for the original Website.

Files that we didn't write most of:

- src/balloc.c, this was stolen from ircd-ratbox.
- src/confp.c, this was stolen from Sentinel (from csircd).
- src/event.c, this was stolen from ircd-ratbox.
- src/match.c, this was stolen from Sentinel (from IRCnet ircd).

2. PRESENTATION
---------------

Shrike is a set of services for TSora-based IRC networks that allows users to
manage their channels in a secure and efficient way and allows operators to
manage various things about their networks.

Shrike currently works with:

- ircd-ratbox 1.3 and 2.x in TS5
- ircd-hybrid 7.0 or later
- ircd-hybrid 6.0 or later
  csircd 1.3 or later

Shrike may *possibly* work with other TSora IRCd's running at least TS3.

3. INSTALLATION
---------------

See the INSTALL file.

4. COMMAND LINE OPTIONS
-----------------------

Normally, Shrike can be run simply by invoking the "shrike" executable.  Shrike
will then use the defaults specified in the configuraiton file and connect
to the specified uplink server. Alternatively, any of the following
command-line options can be specified to change the default behavior:

       -c </path/to/file> - specify the configuration file
       -d                 - start in debugging mode
       -h                 - print the help message and exit
       -n                 - do not fork into the background
       -v                 - print the version information and exit

Shrike also handles the following signals:

       HUP  - force a REHASH
       TERM - force a SHUTDOWN
       USR2 - force a RESTART

Upon starting, Shrike will parse its command-line arguments, open its log file,
parse its configuration file, load its database, connect to the uplink,
and (assuming-n is not specified) detach into the background.  If Shrike
encounters an error while parsing the configuration or database files it will
terminate immediately. Otherwise, it will run until killed or otherwise stopped.

5. CONTACT AND SUPPORT
----------------------

Okay, here's the deal. We enjoy coding this program, and we do it for free. You
didn't pay anything for this, did you? No. Our time and our effort went into
making this program, and now you want us to help you with it, too. Well, that's
fine, but do us a favor: READ THE DOCS. Unlike many other programs we've
included some fairly extensive documentation. The configuration file itself has
a description of every option as well as examples of: how it's used, what it
does, and what happens if you turn it on or off. *Please* don't waste our time
by asking asinine questions which are clearly answered in the documentation.
You'll just make us regret making this program so easy to use that it requires
zero intelligence.

That said, if you've skimmed the docs and can't find your answer feel free to
ask. I've been unable to find the answer I was looking for in many programs
myself so I don't mind. Just don't ask us to set it up for you. It's too easy.

For bug reports, please use this reporting mechanism:
http://github.com/rakaur/shrike/issues

If you're reporting a bug, here's some advice. Be sure to include information
on how to reproduce the problem. If you can't reproduce it, you're likely out
of luck. You can go ahead and report the problem but chances are if we can't
find the cause then we can't fix it. If Shrike crashed (with a core file) be
sure to include a backtrace for us. You can do this by running something along
the lines of "gdb bin/shrike shrike.core" and typing "bt" at the "(gdb)"
prompt. If you do all of these things and still manage to keep your report
short and concise you will be loved.

If your problem requires extensive debugging in a real-time situation we may
ask that you find us on IRC:
  rakaur @ irc.malkier.net
  dKingston @ irc.malkier.net

If you've read this far, congratulations. You are among the few elite people
that actually read documentation. Thank you.

