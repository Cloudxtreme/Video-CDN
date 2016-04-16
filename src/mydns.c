#include "mydns.h"
#include <time.h>
#include <stdlib.h>

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

/* @brief First converts decimalNumber from decimal to hex. Next, it passes the
 *        result to the given hex2binary function.
 * @param decimalNumber: Number to be converted.
 * @param bytesNeeded: The maximum possible number of hex bytes needed to
 *        represent the number.
 * @param binaryNumber: buffer where the binary number is stored
 */
void dec2hex2binary(int decimalNumber, int bytesNeeded, uint8_t* binaryNumber){

  int quotient;
  int i=bytesNeeded-1, temp;
  char hexadecimalNumber[bytesNeeded];
  memset(hexadecimalNumber, 0, bytesNeeded);
  quotient = decimalNumber;

  while(quotient!=0){
    temp = quotient % 16;

    if(temp < 10){
      temp = temp + 48;
    } else {
      temp = temp + 55;
    }

    hexadecimalNumber[i] = temp;
    quotient = quotient / 16;
    i--;
  }

  i = 0;

  while(i < bytesNeeded){
    if(hexadecimalNumber[i] != '\0'){
      break;
    }
    hexadecimalNumber[i] = '0';
    i++;
  }

  hex2binary(hexadecimalNumber, bytesNeeded, binaryNumber);
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

void mmemtransfer(byte_buf *dest, byte_buf *src, int size){
	memmove(dest->buf + dest->pos, src->buf + src->pos, size);
	dest->pos += size;
	src->pos  += size;
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

void free_dns(dns_message* info){
	int qcount = info->QDCOUNT - 1;
	int acount = info->ANCOUNT - 1;

	if(info->questions != NULL){
		while(qcount <= 0){
			free(((info->questions)[qcount])->NAME);
			free(((info->questions)[qcount]))
			qcount--;
		}
		free(info->questions);
	}

	if(info->answers != NULL){
		while(acount <= 0){
			free(((info->answers)[acount])->NAME);
			free(((info->answers)[acount]))
			acount--;
		}
		free(info->answers);
	}

	free(info);
	return;
}

//0X2E = "."
//quora = 0 if question, 1 if answer
void parse_name(byte_buf *temp_message, dns_message* info, int index,
														  int quora){
	int total = 1;
	byte_buf* name_help = create_bytebuf(MAX_MESSAGE_SIZE);
	uint8_t length[1] = {0};
	uint8_t period[1] = {0x2E};
	int length = 0;
	mmemclear(name_help);
	
	mmemmove(length, temp_message, 1);
	length = binary2int(length, 1);

	while(length != 0){
		total += length;
		mmemtransfer(name_help, temp_message, length);
		mmemcat(name_help, period, 1);
		memset(length, 0, 1);
		mmemmove(length, temp_message, 1);
		length = binary2int(length, 1);
	}

	if(quora == 0){
		((info->questions)[index])->name_size = name_help->pos;
		((info->questions)[index])->NAME = calloc(1, total);
		length = name_help->pos;
		name_help->pos = 0;
		mmemmove(((info->questions)[index])->NAME, name_help, length);
	} else {
		((info->answers)[index])->name_size = name_help->pos;
		((info->answers)[index])->NAME = calloc(1, total);
		length = name_help->pos;
		name_help->pos = 0;
		mmemmove(((info->answers)[index])->NAME, name_help, length);
	}

	delete_bytebuf(name_help);
}

//User responsible for freeing info.
dns_message* parse_message(uint8_t* message){
	int qd_count;
	int an_count;
	int counter = 0;
	dns_message* info = calloc(1, sizeof(dns_message));
	byte_buf* temp_message = create_bytebuf(MAX_MESSAGE_SIZE);
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
    		parse_name(temp_message, info, counter, 0)
    		mmemmove(((info->questions)[counter])->TYPE,  temp_message, 2);
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
    		parse_name(temp_message, info, counter, 1)
    		mmemmove(((info->answers)[counter])->TYPE, 		 temp_message, 2);
    		mmemmove(((info->answers)[counter])->CLASS, 	 temp_message, 2);
    		mmemmove(((info->answers)[counter])->TTL, 		 temp_message, 2);
    		mmemmove(((info->answers)[counter])->RDLENGTH, 	 temp_message, 2);
    		mmemmove(((info->answers)[counter])->RDATA,		 temp_message, 4);
    		counter++;
    	}
    } else {
    	info->answers = NULL;
    }

    delete_bytebuf(temp_message);
    return info;
}

//THIS FUNCTION NEEDS SOME TESTING
void gen_other_half(dns_message* info){
	int final = (info->QR << 15) | (info->OPCODE << 11) |
				(info->AA << 10) | (info->TC 	 << 9)  | 
				(info->RD << 8)  | (info->RA 	 << 7)  | 
				(info->Z  << 6)  | (info->AD 	 << 5)  |
				(info->CD << 4)  | (info->OPCODE)  & 0xFF;
	dec2hex2binary(final, 4, info->OTHER_HALF);
}

/* I know, we agreed that some of these values should be 0/1 by default,
 * but I like to be consistent. I take care of any fields explicitly mentioned
 * in the write-up. The rest is up to you.
 *
 * For the question_answer** fields, create an array of allocated questions
 * and answers. It'll make my life easier...
 */
byte_buf* gen_message(int QR, int OPCODE, int AA, int TC, int AD, int CD, 
			int RCODE, int QDCOUNT, int ANCOUNT, question** questions,
			answer** answers){

	srand(time(NULL));
	int id = (rand()) & 0xFFFF;
	int qcount = QDCOUNT;
	int acount = ANCOUNT;
	int counter = 0;
	int name_size = 0;
	dns_message* info = calloc(1, sizeof(dns_message)); 
	byte_buf *temp_message = create_bytebuf(MAX_MESSAGE_SIZE);
	mmemclear(temp_message);

	dec2hex2binary(id, 4, info->id);
	info->QR = QR;
	info->OPCODE = OPCODE;
	info->AA = AA;
	info->TC = TC;
	info->RD = 0;
	info->RA = 0;
	info->Z = 0;
	info->AD = AD;
	info->CD = CD;
	info->QDCOUNT = QDCOUNT;
	info->ANCOUNT = ANCOUNT;
	info->NSCOUNT = 0;
	info->ARCOUNT = 0;
	info->questions = questions;
	info->answers = answer;

	gen_other_half(info);

	mmemcat(temp_message, info->ID, 		2);
	mmemcat(temp_message, info->OTHER_HALF, 2);
	mmemcat(temp_message, info->QDCOUNT, 	2);
	mmemcat(temp_message, info->ANCOUNT, 	2);
	mmemcat(temp_message, info->NSCOUNT, 	2);
	mmemcat(temp_message, info->ARCOUNT, 	2);

	while(counter < qcount){
		name_size = ((info->questions)[counter])->name_size
		mmemcat(temp_message, ((info->questions)[counter])->NAME, 	name_size);
		mmemcat(temp_message, ((info->questions)[counter])->TYPE, 	2);
		mmemcat(temp_message, ((info->questions)[counter])->CLASS, 	2);
		counter++;
	}

	counter = 0;

	while(counter < acount){
		name_size = ((info->questions)[counter])->name_size
		mmemcat(temp_message, ((info->answers)[counter])->NAME, 	name_size);
		mmemcat(temp_message, ((info->answers)[counter])->TYPE, 	2);
		mmemcat(temp_message, ((info->answers)[counter])->CLASS, 	2);
		mmemcat(temp_message, ((info->answers)[counter])->TTL, 		2);
		mmemcat(temp_message, ((info->answers)[counter])->RDLENGTH, 2);
		mmemcat(temp_message, ((info->answers)[counter])->RDATA, 	4);
		counter++;
	}

	free_dns(info);
	return temp_message;
}