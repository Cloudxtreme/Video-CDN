#include "mydns.h"

int init_mydns(const char *dns_ip, unsigned int dns_port, const char *local_ip)
{
  return 0;
}

int resolve(const char *node, const char *service,
            const struct addrinfo *hints, struct addrinfo **res)
{
  return 0;
}

/**
 * converts the binary char string str to ascii format. the length of 
 * ascii should be 2 times that of str
 */
void binary2hex(uint8_t *buf, int len, char *hex) {
	int i=0;
	for(i=0;i<len;i++) {
		sprintf(hex+(i*2), "%.2x", buf[i]);
	}
	hex[len*2] = 0;
}
  
/**
 *Ascii to hex conversion routine
 */
static uint8_t _hex2binary(char hex)
{
     hex = toupper(hex);
     uint8_t c = ((hex <= '9') ? (hex - '0') : (hex - ('A' - 0x0A)));
     return c;
}

/**
 * converts the ascii character string in "ascii" to binary string in "buf"
 * the length of buf should be atleast len / 2
 */
void hex2binary(char *hex, int len, uint8_t*buf) {
	int i = 0;
	for(i=0;i<len;i+=2) {
		buf[i/2] = 	_hex2binary(hex[i]) << 4 
				| _hex2binary(hex[i+1]);
	}
}

/*******************************************************************************/
/* @brief Creates a bytebuf struct. The bytebuf's buffer has bufsize bytes     */
/*        allocated. Note that the bytebuf must be freed outside the function, */
/*        using delete_bytebuf.                                                */
/* @param bufsize: Length of the buf argument.                                 */
/*******************************************************************************/
struct byte_buf* create_bytebuf(size_t bufsize)
{
  struct byte_buf *b;
  b = malloc(sizeof(struct byte_buf));
  if (!b) return NULL;

  b->buf = malloc(bufsize + 1);
  if (!b->buf) {
    free(b);
    return NULL;
  }

  b->pos = 0;
  bzero(b->buf, bufsize+1);

  b->bufsize = bufsize;

  return b;
}

/**************************************************/
/* @brief Free a bytebuf and all of its contents. */
/* @param b - The bytebuf to free                 */
/**************************************************/
void delete_bytebuf(struct byte_buf* b)
{
  free(b->buf);
  free(b);
}

/* @brief Moves <size> bytes, starting from <tempRequest>'s pos argument, into
 *        the argument binaryNumber.
 * @param binaryNumber: Destination buffer. Must be at least <size> bytes.
 * @param tempRequest: Source. Copying from tempRequest->buf
 * @param size: Number of bytes to copy.
 */
void mmemmove(uint8_t *binaryNumber, byte_buf *tempRequest, int size){
  memmove(binaryNumber, tempRequest->buf + tempRequest->pos, size);
  tempRequest->pos += size;
}

/* @brief Concatenates the contents of <binaryNumber> to <tempRequest>'s buf
 *        argument.
 * @param tempRequest: tempRequest->buf is the destination buffer.
 * @param binaryNumber: Source buffer.
 * @param size: Number of bytes to copy from binaryNumber.
 */
void mmemcat(byte_buf *tempRequest, uint8_t *binaryNumber, int size){
  memmove(tempRequest->buf + tempRequest->pos, binaryNumber, size);
  tempRequest->pos += size;
}

/* Sets a byte_buf's contents to its default values. */
void mmemclear(byte_buf *b)
{
  b->pos = 0;
  bzero(b->buf, b->bufsize);
}

/* @brief Converts a given binary number to an int.
 * @param binaryNumber: Number to convert.
 * @param len: Length of the buffer.
 */
int binary2int(uint8_t *buf, int len){
  char temp[2*len];
  bzero(temp, 2*len);
  binary2hex(buf, len, temp);
  int ret = (int)strtol(temp, NULL, 16);
  return ret;
}

void parse_other_half(uint8_t* other_half, dns_message* info){
	int other_int = binary2int(other_half);
	info->QR = other_int & 0x8000;
	info->OPCODE = other_int & 0x7800;
	info->AA = other_int & 0x400; 
	info->TC = other_int & 0x200;
	info->RD = other_int & 0x100; //Always 0
	info->RA = other_int & 0x80; //Always 0
	info->Z =  other_int & 0x40; //Always 0
	info->AD = other_int & 0x20;
	info->CD = other_int & 0x10;
	info->RCODE = other_int & 0xF;
}

dns_message* parse_message(uint8_t* message){
	int qd_count;
	int an_count;
	int counter = 0;
	dns_message* info = calloc(1, sizeof(dns_message));
	byte_buf *temp_message = create_bytebuf(MAX_MESSAGE_SIZE);

	mmemclear(temp_message);
	mmemmove(info->ID,   	   temp_message,     2);
    mmemmove(info->OTHER_HALF, temp_message,     2);
    parse_other_half(info->OTHER_HALF, info);
    mmemmove(info->QDCOUNT,    temp_message,     2);
    mmemmove(info->ANCOUNT,    temp_message,     2);
    mmemmove(info->NSCOUNT,    temp_message,     2);
    mmemmove(info->ARCOUNT,    temp_message,     2);

    qd_count = binary2int(info->QDCOUNT, 2);
    an_count = binary2int(info->ANCOUNT, 2);

    //TYPE and CLASS are always 1, but for the sake of generality.
    if(qd_count){
    	info->questions = calloc(1, sizeof(question_answer*));
    	while(counter < qd_count){
    		(info->questions)[counter] = calloc(1, sizeof(question_answer));
    		mmemmove(((info->questions)[counter])->NAME, temp_message, 2);
    		mmemmove(((info->questions)[counter])->TYPE, temp_message, 2);
    		mmemmove(((info->questions)[counter])->CLASS, temp_message, 2);
    		counter++;
    	}
    } else {
    	info->questions = NULL;
    }

    counter = 0;
    if(an_count){
    	info->answers = malloc(an_count * sizeof(question_answer));
    	while(counter < an_count){
    		(info->answers)[counter] = calloc(1, sizeof(question_answer));
    		mmemmove(((info->answers)[counter])->NAME, temp_message, 2);
    		mmemmove(((info->answers)[counter])->TYPE, temp_message, 2);
    		mmemmove(((info->answers)[counter])->CLASS, temp_message, 2);
    		counter++;
    	}
    } else {
    	info->answers = NULL;
    }

    delete_bytebuf(temp_message);
    return info;
}
