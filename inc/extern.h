/*
 * Copyright (c) 2003-2004 E. Will et al.
 * Rights to this code are documented in doc/LICENSE.
 *
 * This header file contains all of the extern's needed.
 *
 * $Id$
 */

#ifndef EXTERN_H
#define EXTERN_H

/* save some space/typing */
#define E extern

/* balloc.c */
E BlockHeap *strdup_heap;

E int BlockHeapFree(BlockHeap *bh, void *ptr);
E void *BlockHeapAlloc(BlockHeap *bh);

E BlockHeap *BlockHeapCreate(size_t elemsize, int elemsperblock);
E int BlockHeapDestroy(BlockHeap *bh);

E void initBlockHeap(void);
E void BlockHeapUsage(BlockHeap *bh, size_t * bused, size_t * bfree,
                      size_t * bmemusage);

/* cmode.c */
E void channel_mode(channel_t *chan, uint8_t parc, char *parv[]);
E void user_mode(user_t *user, char *modes);
E void flush_cmode_callback(void *arg);
E void cmode(char *sender, ...);
E void check_modes(mychan_t *mychan);

/* conf.c */
E void conf_parse(void);
E void conf_init(void);
E boolean_t conf_rehash(void);
E boolean_t conf_check(void);

/* confp.c */
E void config_free(CONFIGFILE *cfptr);
E CONFIGFILE *config_load(char *filename);
E CONFIGENTRY *config_find(CONFIGENTRY *ceptr, char *name);

/* db.c */
void db_save(void *arg);
void db_load(void);

/* event.c */
E struct ev_entry event_table[MAX_EVENTS];
E const char *last_event_ran;

E uint32_t event_add(const char *name, EVH *func, void *arg, time_t when);
E uint32_t event_add_once(const char *name, EVH *func, void *arg, time_t when);
E void event_run(void);
E time_t event_next_time(void);
E void event_init(void);
E void event_delete(EVH *func, void *arg);
E uint32_t event_find(EVH *func, void *arg);

/* function.c */
E FILE *log_file;

#if !HAVE_STRLCAT
E size_t strlcat(char *dst, const char *src, size_t siz);
#endif
#if !HAVE_STRLCPY
E size_t strlcpy(char *dst, const char *src, size_t siz);
#endif

#if HAVE_GETTIMEOFDAY
E void s_time(struct timeval *sttime);
E void e_time(struct timeval sttime, struct timeval *ttime);
E int32_t tv2ms(struct timeval *tv);
#endif

E void tb2sp(char *line);

E char *strscpy(char *d, const char *s, size_t len);
E void *smalloc(size_t size);
E void *scalloc(long elsize, long els);
E void *srealloc(void *oldptr, long newsize);
E char *sstrdup(const char *s);
E void strip(char *line);
E char *make_hostmask(char *nick, char *user, char *host);
E void log_open(void);
E void slog(uint32_t level, const char *fmt, ...);
E uint32_t time_msec(void);
E uint8_t regex_match(regex_t * preg, char *pattern, char *string,
                      size_t nmatch, regmatch_t pmatch[], int eflags);
E uint32_t shash(const unsigned char *text);
E char *replace(char *s, int32_t size, const char *old, const char *new);
E char *itoa(int num);
E char *flags_to_string(int32_t flags);
E int32_t mode_to_flag(char c);
E char *time_ago(time_t event);
E char *timediff(time_t seconds);
E unsigned long makekey(void);
E int validemail(char *email);
E boolean_t validhostmask(char *host);
E void sendemail(char *what, const char *param, int type);

E boolean_t is_founder(mychan_t *mychan, myuser_t *myuser);
E boolean_t is_successor(mychan_t *mychan, myuser_t *myuser);
E boolean_t is_xop(mychan_t *mychan, myuser_t *myuser, uint8_t level);
E boolean_t is_on_mychan(mychan_t *mychan, myuser_t *myuser);
E boolean_t should_op(mychan_t *mychan, myuser_t *myuser);
E boolean_t should_op_host(mychan_t *mychan, char *host);
E boolean_t should_voice(mychan_t *mychan, myuser_t *myuser);
E boolean_t should_voice_host(mychan_t *mychan, char *host);
E boolean_t is_sra(myuser_t *myuser);
E boolean_t is_ircop(user_t *user);
E boolean_t is_admin(user_t *user);

E int token_to_value(struct Token token_table[], char *token);
E char *sbytes(float x);
E float bytes(float x);

/* help.c */
E void do_help(char *origin);

E struct help_command_ help_commands[];
E struct help_command_ *help_cmd_find(char *origin, char *command);

/* irc.c */
E void irc_parse(char *line);
E struct message_ messages[];
E struct message_ *msg_find(const char *name);

/* match.c */
#define MATCH_RFC1459   0
#define MATCH_ASCII     1

E int match_mapping;

#define IsLower(c)  ((unsigned char)(c) > 0x5f)
#define IsUpper(c)  ((unsigned char)(c) < 0x60)

#define C_ALPHA 0x00000001
#define C_DIGIT 0x00000002

E const unsigned int charattrs[];

#define IsAlpha(c)      (charattrs[(unsigned char) (c)] & C_ALPHA)
#define IsDigit(c)      (charattrs[(unsigned char) (c)] & C_DIGIT)
#define IsAlphaNum(c)   (IsAlpha((c)) || IsDigit((c)))
#define IsNon(c)        (!IsAlphaNum((c)))

E const unsigned char ToLowerTab[];
E const unsigned char ToUpperTab[];

void set_match_mapping(int);

E int ToLower(int);
E int ToUpper(int);

E int irccmp(char *, char *);
E int irccasecmp(char *, char *);
E int ircncmp(char *, char *, int);
E int ircncasecmp(char *, char *, int);

E int match(char *, char *);
E char *collapse(char *);

/* node.c */
E list_t sralist;
E list_t tldlist;
E list_t klnlist;
E list_t servlist[HASHSIZE];
E list_t userlist[HASHSIZE];
E list_t chanlist[HASHSIZE];
E list_t mulist[HASHSIZE];
E list_t mclist[HASHSIZE];

E list_t sendq;

E void init_nodes(void);
E node_t *node_create(void);
E void node_free(node_t *n);
E void node_add(void *data, node_t *n, list_t *l);
E void node_del(node_t *n, list_t *l);
E node_t *node_find(void *data, list_t *l);
E void node_move(node_t *m, list_t *oldlist, list_t *newlist);

E sra_t *sra_add(char *name);
E void sra_delete(myuser_t *myuser);
E sra_t *sra_find(myuser_t *myuser);

E tld_t *tld_add(char *name);
E void tld_delete(char *name);
E tld_t *tld_find(char *name);

E kline_t *kline_add(char *user, char *host, char *reason, long duration);
E void kline_delete(char *user, char *host);
E kline_t *kline_find(char *user, char *host);
E kline_t *kline_find_num(long number);
E void kline_expire(void *arg);

E server_t *server_add(char *name, uint8_t hops, char *desc);
E void server_delete(char *name);
E server_t *server_find(char *name);

E user_t *user_add(char *nick, char *user, char *host, server_t *server);
E void user_delete(char *nick);
E user_t *user_find(char *nick);

E channel_t *channel_add(char *name, uint32_t ts);
E void channel_delete(char *name);
E channel_t *channel_find(char *name);

E chanuser_t *chanuser_add(channel_t *chan, char *user);
E void chanuser_delete(channel_t *chan, user_t *user);
E chanuser_t *chanuser_find(channel_t *chan, user_t *user);

E myuser_t *myuser_add(char *name, char *pass, char *email);
E void myuser_delete(char *name);
E myuser_t *myuser_find(char *name);

E mychan_t *mychan_add(char *name, char *pass);
E void mychan_delete(char *name);
E mychan_t *mychan_find(char *name);

E chanacs_t *chanacs_add(mychan_t *mychan, myuser_t *myuser, uint8_t level);
E chanacs_t *chanacs_add_host(mychan_t *mychan, char *host, uint8_t level);
E void chanacs_delete(mychan_t *mychan, myuser_t *myuser, uint8_t level);
E void chanacs_delete_host(mychan_t *mychan, char *host, uint8_t level);
E chanacs_t *chanacs_find(mychan_t *mychan, myuser_t *myuser, uint8_t level);
E chanacs_t *chanacs_find_host(mychan_t *mychan, char *host, uint8_t level);

E void sendq_add(char *buf, int len, int pos);
E int sendq_flush(void);

/* set.c */
E void do_set(char *origin);

E struct set_command_ set_commands[];
E struct set_command_ *set_cmd_find(char *origin, char *command);

/* services.c */
E user_t *introduce_nick(char *nick, char *user, char *host, char *real,
                         char *modes);
E void join(char *chan, char *nick);
E void expire_check(void *arg);
E void services_init(void);
E void msg(char *target, char *fmt, ...);
E void notice(char *target, char *fmt, ...);
E void wallops(char *fmt, ...);
E void skill(char *nick, char *reason);
E void kline_sts(char *server, char *user, char *host, long duration,
                 char *reason);
E void unkline_sts(char *server, char *user, char *host);
E void verbose(mychan_t *mychan, char *fmt, ...);
E void snoop(char *fmt, ...);
E void part(char *chan, char *nick);
E void services(char *origin, uint8_t parc, char *parv[]);

E struct command_ commands[];
E struct command_ *cmd_find(char *origin, char *command);

/* shrike.c */
E char *config_file;

/* socket.c */
E int servsock;
E time_t CURRTIME;

E int8_t sts(char *fmt, ...);
E int conn(char *host, uint32_t port);
E void reconn(void *arg);
E void io_loop(void);

/* version.c */
E const char *generation;
E const char *creation;
E const char *platform;
E const char *version;
E const char *infotext[];

#endif /* EXTERN_H */
