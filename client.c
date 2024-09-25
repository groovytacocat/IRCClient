#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/times.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include<regex.h>

#define PORT "6665"
#define HOSTNAME "irc.libera.chat"

// Courtesy of Beej's Guide to Network Programming
int sendall(int s, char *buf, int *len) {
  int total = 0;
  int bytesleft = *len;
  int n;

  while (total < *len) {
    n = send(s, buf + total, bytesleft, 0);
    if (n == -1) {
      break;
    }
    total += n;
    bytesleft -= n;
  }

  *len = total;

  return n == -1 ? -1 : 0;
}

void parse_user_message(const char* in_message, char* out_message, size_t out_size) {
  regex_t regex_nick, regex_msg;
  const char *nick_pattern = ":([^!]+)";
  const char *msg_pattern = "^:[^ ]+ PRIVMSG [^ ]+ :(.+)";
  if(regcomp(&regex_nick, nick_pattern, REG_EXTENDED) != 0) {
    perror("regex compile fail");
    exit(1);
  }
  regmatch_t matches[2];
  if(regexec(&regex_nick, in_message, 2, matches, 0) == 0)
  {
    char name[50];
    snprintf(name, sizeof(name), "%.*s", (int)(matches[1].rm_eo - matches[1].rm_so), in_message + matches[1].rm_so);

    regmatch_t msgMatch[2];
    if(regcomp(&regex_msg, msg_pattern, REG_EXTENDED) != 0) {
      perror("regex msg compile fail");
      exit(1);
    }
    if(regexec(&regex_msg, in_message, 2, msgMatch, 0) == 0) {
      char output[4096];
      snprintf(output, sizeof(output), "%.*s", (int)(msgMatch[1].rm_eo - msgMatch[1].rm_so), in_message + msgMatch[1].rm_so);

      snprintf(out_message, out_size, "%s: %s\r\n", name, output);
    }
  }
  regfree(&regex_nick);
  regfree(&regex_msg);
}





int main() {
  struct addrinfo hints, *peer_address;

  memset(&hints, 0, sizeof(hints));
  hints.ai_socktype = SOCK_STREAM;

  if (getaddrinfo(HOSTNAME, PORT, &hints, &peer_address)) {
    perror("getaddrinfo");
    exit(1);
  }

  int clientSocket;

  clientSocket = socket(peer_address->ai_family, peer_address->ai_socktype,
                        peer_address->ai_protocol);

  if (clientSocket <= 0) {
    perror("socket() failed");
    exit(1);
  }

  if (connect(clientSocket, peer_address->ai_addr, peer_address->ai_addrlen)) {
    perror("connect() failed");
    exit(1);
  }
  freeaddrinfo(peer_address);

  printf("Connected.\n");
  char* userNick = "NICK groovytest\r\n";
  char* userUser = "USER groovytest 0 * :groovy\r\n";
  char* userJoin = "JOIN ##groovytest GROOVYPASS\r\n";
  int nickLen = strlen(userNick);
  int userLen = strlen(userUser);
  int joinLen = strlen(userJoin);

  if(sendall(clientSocket, userNick, &nickLen) == -1) {
    perror("nick send");
    exit(1);
  }
  if(sendall(clientSocket, userUser, &userLen) == -1) {
    perror("user send");
    exit(1);
  }
  if(sendall(clientSocket, userJoin, &joinLen) == -1) {
    perror("user join");
    exit(1);
  }

  while (1) {
    fd_set master;
    fd_set read_fds;
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);
    FD_SET(clientSocket, &master);

    read_fds = master;

    struct timeval timeout;
    timeout.tv_sec = 15;

    if (select(clientSocket + 1, &read_fds, 0, 0, &timeout) < 0) {
      perror("select() failed");
      exit(1);
    }

    if (FD_ISSET(clientSocket, &read_fds)) {
      char read[4096];
      int bytes_received = recv(clientSocket, read, 4096, 0);
      if (bytes_received < 1) {
        printf("Connection closed by peer.\n");
        break;
      }
      if(strstr(read, "PRIVMSG"))
      {
        char formatted_message[5000];
        parse_user_message(read, formatted_message, sizeof(formatted_message));
        printf("%s", formatted_message);
      }
      else {
        printf("%.*s", bytes_received, read);
      }
    }
    else if (FD_ISSET(0, &read_fds)) {
      char read[4096];
      int len;

      if (!fgets(read, sizeof(read), stdin))
        break;
/*     
      len = strlen(read);

      if(read[len - 1] == '\n'){
        read[len - 1] = '\r';
        read[len] = '\n';
        len++;
      }
*/
      char msg[4121];

      snprintf(msg, sizeof(msg), "PRIVMSG ##groovytest :%s\r\n", read);

      len = strlen(msg);

      if (sendall(clientSocket, msg, &len) == -1) {
        perror("sendall");
        printf("Only sent %d bytes because of error\n", len);
        exit(1);
      }
    }
  }

  return 0;
}
