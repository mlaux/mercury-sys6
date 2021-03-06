#ifndef IRC_H
#define IRC_H

#define PRIVMSG 0
#define NOTICE 1

#define IRC_LINE_MAX 512
#define IRC_PARAM_MAX 15

void send_userinfo(StreamPtr stream, const char *nick, const char *ident, const char *realname);
void send_pong(StreamPtr stream, const char *token);
void send_join(StreamPtr stream, const char *channel);
void send_part(StreamPtr stream, const char *channel);
void send_message(StreamPtr stream, int type, const char *target, const char *message);

#endif