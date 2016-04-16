#include <netdb.h>
#define MAX_MESSAGE_SIZE  512

struct byte_buf {
  uint8_t *buf;
  int pos;
  size_t bufsize;
};

typedef struct question_answer{
  uint8_t NAME[2];
  uint8_t TYPE[2];
  uint8_t CLASS[2];
} question_answer;

typedef struct dns_message{
  uint8_t ID[2];
  int QR;
  int OPCODE; //4 bits
  int AA;
  int TC;
  int RD;
  int RA;
  int Z;
  int AD;
  int CD;
  int RCODE; //4 bits
  uint8_t OTHER_HALF[2]; //Second half of first line of DNS message (see the image)
  uint8_t QDCOUNT[2];
  uint8_t ANCOUNT[2];
  uint8_t NSCOUNT[2];
  uint8_t ARCOUNT[2];
  question_answer** questions; //Pointers to an array of questions
  question_answer** answers; //Pointers to an array of answers
} dns_message;

/**
 * Initialize your client DNS library with the IP address and port number of
 * your DNS server.
 *
 * @param  dns_ip    The IP address of the DNS server.
 * @param  dns_port  The port number of the DNS server.
 * @param  local_ip  The local IP address client sockets should bind to.
 *
 * @return 0 on success, -1 otherwise
 */
int init_mydns(const char *dns_ip, unsigned int dns_port, const char *local_ip);


/**
 * Resolve a DNS name using your custom DNS server.
 *
 * Whenever your proxy needs to open a connection to a web server, it calls
 * resolve() as follows:
 *
 * struct addrinfo *result;
 * int rc = resolve("video.cs.cmu.edu", "8080", null, &result);
 * if (rc != 0) {
 *     // handle error
 * }
 * // connect to address in result
 * free(result);
 *
 *
 * @param  node  The hostname to resolve.
 * @param  service  The desired port number as a string.
 * @param  hints  Should be null. resolve() ignores this parameter.
 * @param  res  The result. resolve() should allocate a struct addrinfo, which
 * the caller is responsible for freeing.
 *
 * @return 0 on success, -1 otherwise
 */

int resolve(const char *node, const char *service,
            const struct addrinfo *hints, struct addrinfo **res);


void usage();
