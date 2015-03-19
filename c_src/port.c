#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>

#ifdef WIN32
#include <windows.h> 
#else
#include <unistd.h>
#endif

#include <sodium.h> // just as an example
#include <ei.h>
#include "port.h"

#define HEADER_SIZE 4 // Can be 1, 2 or 4, set whiile opening port in Erlang

#ifdef WIN32
#  define DO_EXIT(code) do { ExitProcess((code)); exit((code));} while (0)
/* exit() called only to avoid a warning */
#else
#  define DO_EXIT(code) exit((code))
#endif

static int read_exact(byte *buf, int len);
static byte * read_port_msg(void);
static int write_exact(byte *buf, int len);
static void write_port_msg(ei_x_buff result);
static ei_x_buff process(byte *msg);
static void make_error(ei_x_buff result, const char *text);
static void * safe_malloc(int size);


int main(void) {
  byte *msg = NULL;
  ei_x_buff result;
  
#ifdef WIN32
  setmode(fileno(stdout), O_BINARY);
  setmode(fileno(stdin), O_BINARY);
#endif
  
  if (sodium_init() == -1) {
    DO_EXIT(EXIT_OPEN_LIB);
  }
  
  fprintf(stderr, "Driver started\n");
  
  for (;;) {
    msg = read_port_msg();
    fprintf(stderr, "Got input\n");
    result = process(msg);
    write_port_msg(result);
    ei_x_free(&result);
    free(msg);
  }
  
  return 0;
}

static ei_x_buff process(byte *msg) {
  ei_x_buff result;

  ei_x_new_with_version(&result);
  ei_x_encode_tuple_header(&result, 2);
  ei_x_encode_atom(&result, "ok");
  ei_x_encode_atom(&result, "done");
  return result;
}

static void make_error(ei_x_buff result, const char *text) {
  ei_x_free(&result);
  ei_x_new_with_version(&result);
  ei_x_encode_tuple_header(&result, 2);
  ei_x_encode_atom(&result, "error");
  ei_x_encode_string(&result, text);
}

/* read from stdin */ 
#ifdef WIN32
static int read_exact(byte *buffer, int len) {
  HANDLE standard_input = GetStdHandle(STD_INPUT_HANDLE);
    
  unsigned read_result;
  unsigned sofar = 0;
    
  if (!len) { /* Happens for "empty packages */
    return 0;
  }
  for (;;) {
    if (!ReadFile(standard_input, buffer + sofar,
		  len - sofar, &read_result, NULL)) {
      return -1; /* EOF */
    }
    if (!read_result) {
      return -2; /* Interrupted while reading? */
    }
    sofar += read_result;
    if (sofar == len) {
      return len;
    }
  }
} 
#else
static int read_exact(byte *buffer, int len) {
  int i, got = 0;
    
  do {
    if ((i = read(0, buffer + got, len - got)) <= 0)
      return(i);
    got += i;
  } while (got < len);
  return len;
}
#endif

/* write to stdin */ 
#ifdef WIN32
static int write_exact(byte *buffer, int len) {
  HANDLE standard_output = GetStdHandle(STD_OUTPUT_HANDLE);
    
  unsigned write_result;
  unsigned wrote = 0;
    
  if (!len) { /* Happens for "empty packages */
    return 0;
  }
  for (;;) {
    if (!WriteFile(standard_output, buffer + wrote, len - wrote, &wrote_result, NULL)) {
      return -1;
    }
    if (!write_result) {
      return -2; /* Interrupted while writing? */
    }
    wrote += write_result;
    if (wrote == len) {
      return len;
    }
  }
} 
#else
static int write_exact(byte *buffer, int len) {
  int i, wrote = 0;

  do {
    if ((i = write(1, buffer + wrote, len - wrote)) <= 0)
      return (i);
    wrote += i;
  } while (wrote < len);
  return len;
}
#endif

/* Recieive (read) data from erlang on stdin */
static byte * read_port_msg(void) {
  size_t len = 0;
  int i;
  byte *buffer;
  byte hd[HEADER_SIZE];
  
  if(read_exact(hd, HEADER_SIZE) != HEADER_SIZE)
    {
      DO_EXIT(EXIT_STDIN_HEADER);
    }
  for (i = 0; i < HEADER_SIZE; ++i) {
    len <<= 8;
    len |= (unsigned char )hd[i];
  }
  if (len <= 0 || len > INT_MAX) {
    DO_EXIT(EXIT_PACKET_SIZE);
  }
  buffer = (byte *)safe_malloc(len);
  if (read_exact(buffer, len) <= 0) {
    DO_EXIT(EXIT_STDIN_BODY);
  }
  return buffer;
}

static void write_port_msg(ei_x_buff result) {
  byte hd[HEADER_SIZE];
  int i, s = result.buffsz;

  for (i = HEADER_SIZE - 1; i >= 0; --i) {
    hd[i] = s & 0xff;
    s >>= 8;
  }
  write_exact(hd, HEADER_SIZE);
  write_exact(result.buff, result.buffsz);
}

static void *safe_malloc(int size) {
  void *memory;
  
  memory = (void *)malloc(size);
  if (memory == NULL) 
    DO_EXIT(EXIT_ALLOC);

  return memory;
}
