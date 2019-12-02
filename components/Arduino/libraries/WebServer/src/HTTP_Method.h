#ifndef _HTTP_Method_H_
#define _HTTP_Method_H_

/*
This to avoid a conflict with esp_http_client
While they give the impression of being bitwise friendly,
I didn't find code using them that way, we should be fine.

typedef enum {
  HTTP_GET     = 0b00000001,
  HTTP_POST    = 0b00000010,
  HTTP_DELETE  = 0b00000100,
  HTTP_PUT     = 0b00001000,
  HTTP_PATCH   = 0b00010000,
  HTTP_HEAD    = 0b00100000,
  HTTP_OPTIONS = 0b01000000,
  HTTP_ANY     = 0b01111111,
} HTTPMethod;
*/

#include "http_parser.h"
#define HTTP_ANY 0xFFFF
typedef int HTTPMethod;

#endif /* _HTTP_Method_H_ */
