/* Based on src/http/ngx_http_parse.c from NGINX copyright Igor Sysoev
 *
 * Additional changes are licensed under the same terms as NGINX and
 * copyright Joyent, Inc. and other Node contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "httpparser.h"
#include <assert.h>
#include <stddef.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#ifndef ULLONG_MAX
#define ULLONG_MAX ((uint64_t) -1) /* 2^64-1 */
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif

#ifndef BIT_AT
#define BIT_AT(a, i)                                                \
  (!!((unsigned int) (a)[(unsigned int) (i) >> 3] &                  \
   (1 << ((unsigned int) (i) & 7))))
#endif

#ifndef ELEM_AT
#define ELEM_AT(a, i, v) ((unsigned int) (i) < ARRAY_SIZE(a) ? (a)[(i)] : (v))
#endif

#define SET_ERRNO(e)                                                 \
do {                                                                 \
  http_errno = (e);                                          \
} while(0)


/* Run the notify callback FOR, returning ER if it fails */
#define CALLBACK_NOTIFY_(FOR, ER)                                    \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(this) == HPE_OK);                       \
                                                                     \
  if (listener) {                                          \
    if (0 != listener->on_##FOR(this)) {                           \
      SET_ERRNO(HPE_CB_##FOR);                                       \
    }                                                                \
                                                                     \
    /* We either errored above or got paused; get out */             \
    if (HTTP_PARSER_ERRNO(this) != HPE_OK) {                       \
      return (ER);                                                   \
    }                                                                \
  }                                                                  \
} while (0)

/* Run the notify callback FOR and consume the current byte */
#define CALLBACK_NOTIFY(FOR)            CALLBACK_NOTIFY_(FOR, p - data + 1)

/* Run the notify callback FOR and don't consume the current byte */
#define CALLBACK_NOTIFY_NOADVANCE(FOR)  CALLBACK_NOTIFY_(FOR, p - data)

/* Run data callback FOR with LEN bytes, returning ER if it fails */
#define CALLBACK_DATA_(FOR, LEN, ER)                                 \
do {                                                                 \
  assert(HTTP_PARSER_ERRNO(this) == HPE_OK);                       \
                                                                     \
  if (FOR##_mark) {                                                  \
    if (listener) {						     \
      if (0 != listener->on_##FOR(this, FOR##_mark, (LEN))) {      \
        SET_ERRNO(HPE_CB_##FOR);                                     \
      }                                                              \
                                                                     \
      /* We either errored above or got paused; get out */           \
      if (HTTP_PARSER_ERRNO(this) != HPE_OK) {                     \
        return (ER);                                                 \
      }                                                              \
    }                                                                \
    FOR##_mark = NULL;                                               \
  }                                                                  \
} while (0)

/* Run the data callback FOR and consume the current byte */
#define CALLBACK_DATA(FOR)                                           \
    CALLBACK_DATA_(FOR, p - FOR##_mark, p - data + 1)

/* Run the data callback FOR and don't consume the current byte */
#define CALLBACK_DATA_NOADVANCE(FOR)                                 \
    CALLBACK_DATA_(FOR, p - FOR##_mark, p - data)

/* Set the mark FOR; non-destructive if mark is already set */
#define MARK(FOR)                                                    \
do {                                                                 \
  if (!FOR##_mark) {                                                 \
    FOR##_mark = p;                                                  \
  }                                                                  \
} while (0)


#define PROXY_CONNECTION	"proxy-connection"
#define CONNECTION		"connection"
#define CONTENT_LENGTH		"content-length"
#define TRANSFER_ENCODING	"transfer-encoding"
#define UPGRADE			"upgrade"
#define CHUNKED			"chunked"
#define KEEP_ALIVE		"keep-alive"
#define CLOSE			"close"

static const char *method_strings[] ={
#define XX(num, name, string) #string,
	HTTP_METHOD_MAP(XX)
#undef XX
};


/* Tokens as defined by rfc 2616. Also lowercases them.
 *        token       = 1*<any CHAR except CTLs or separators>
 *     separators     = "(" | ")" | "<" | ">" | "@"
 *                    | "," | ";" | ":" | "\" | <">
 *                    | "/" | "[" | "]" | "?" | "="
 *                    | "{" | "}" | SP | HT
 */
static const char tokens[256] = {
	/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
	0, 0, 0, 0, 0, 0, 0, 0,
	/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
	0, 0, 0, 0, 0, 0, 0, 0,
	/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
	0, 0, 0, 0, 0, 0, 0, 0,
	/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
	0, 0, 0, 0, 0, 0, 0, 0,
	/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
	0, '!', 0, '#', '$', '%', '&', '\'',
	/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
	0, 0, '*', '+', 0, '-', '.', 0,
	/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
	'0', '1', '2', '3', '4', '5', '6', '7',
	/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
	'8', '9', 0, 0, 0, 0, 0, 0,
	/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
	0, 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
	'x', 'y', 'z', 0, 0, 0, '^', '_',
	/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
	'`', 'a', 'b', 'c', 'd', 'e', 'f', 'g',
	/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
	'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
	/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w',
	/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
	'x', 'y', 'z', 0, '|', 0, '~', 0
};


static const int8_t unhex[256] ={-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, -1, -1, -1, -1, -1, -1
	, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1
	, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};


#if HTTP_PARSER_STRICT
#define T(v) 0
#else
#define T(v) v
#endif


static const uint8_t normal_url_char[32] = {
	/*   0 nul    1 soh    2 stx    3 etx    4 eot    5 enq    6 ack    7 bel  */
	0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
	/*   8 bs     9 ht    10 nl    11 vt    12 np    13 cr    14 so    15 si   */
	0 | T(2) | 0 | 0 | T(16) | 0 | 0 | 0,
	/*  16 dle   17 dc1   18 dc2   19 dc3   20 dc4   21 nak   22 syn   23 etb */
	0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
	/*  24 can   25 em    26 sub   27 esc   28 fs    29 gs    30 rs    31 us  */
	0 | 0 | 0 | 0 | 0 | 0 | 0 | 0,
	/*  32 sp    33  !    34  "    35  #    36  $    37  %    38  &    39  '  */
	0 | 2 | 4 | 0 | 16 | 32 | 64 | 128,
	/*  40  (    41  )    42  *    43  +    44  ,    45  -    46  .    47  /  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  48  0    49  1    50  2    51  3    52  4    53  5    54  6    55  7  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  56  8    57  9    58  :    59  ;    60  <    61  =    62  >    63  ?  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,
	/*  64  @    65  A    66  B    67  C    68  D    69  E    70  F    71  G  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  72  H    73  I    74  J    75  K    76  L    77  M    78  N    79  O  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  80  P    81  Q    82  R    83  S    84  T    85  U    86  V    87  W  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  88  X    89  Y    90  Z    91  [    92  \    93  ]    94  ^    95  _  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/*  96  `    97  a    98  b    99  c   100  d   101  e   102  f   103  g  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/* 104  h   105  i   106  j   107  k   108  l   109  m   110  n   111  o  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/* 112  p   113  q   114  r   115  s   116  t   117  u   118  v   119  w  */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 128,
	/* 120  x   121  y   122  z   123  {   124  |   125  }   126  ~   127 del */
	1 | 2 | 4 | 8 | 16 | 32 | 64 | 0,
};

#undef T

enum state
{
	s_dead = 1 /* important that this is > 0 */

	, s_start_req_or_res
	, s_res_or_resp_H
	, s_start_res
	, s_res_H
	, s_res_HT
	, s_res_HTT
	, s_res_HTTP
	, s_res_first_http_major
	, s_res_http_major
	, s_res_first_http_minor
	, s_res_http_minor
	, s_res_first_status_code
	, s_res_status_code
	, s_res_status
	, s_res_line_almost_done

	, s_start_req

	, s_req_method
	, s_req_spaces_before_url
	, s_req_schema
	, s_req_schema_slash
	, s_req_schema_slash_slash
	, s_req_server_start
	, s_req_server
	, s_req_server_with_at
	, s_req_path
	, s_req_query_string_start
	, s_req_query_string
	, s_req_fragment_start
	, s_req_fragment
	, s_req_http_start
	, s_req_http_H
	, s_req_http_HT
	, s_req_http_HTT
	, s_req_http_HTTP
	, s_req_first_http_major
	, s_req_http_major
	, s_req_first_http_minor
	, s_req_http_minor
	, s_req_line_almost_done

	, s_header_field_start
	, s_header_field
	, s_header_value_start
	, s_header_value
	, s_header_value_lws

	, s_header_almost_done

	, s_chunk_size_start
	, s_chunk_size
	, s_chunk_parameters
	, s_chunk_size_almost_done

	, s_headers_almost_done
	, s_headers_done

	/* Important: 's_headers_done' must be the last 'header' state. All
	 * states beyond this must be 'body' states. It is used for overflow
	 * checking. See the PARSING_HEADER() macro.
	 */

	, s_chunk_data
	, s_chunk_data_almost_done
	, s_chunk_data_done

	, s_body_identity
	, s_body_identity_eof

	, s_message_done
};


#define PARSING_HEADER(state) (state <= s_headers_done)

enum header_states
{
	h_general = 0
	, h_C
	, h_CO
	, h_CON

	, h_matching_connection
	, h_matching_proxy_connection
	, h_matching_content_length
	, h_matching_transfer_encoding
	, h_matching_upgrade

	, h_connection
	, h_content_length
	, h_transfer_encoding
	, h_upgrade

	, h_matching_transfer_encoding_chunked
	, h_matching_connection_keep_alive
	, h_matching_connection_close

	, h_transfer_encoding_chunked
	, h_connection_keep_alive
	, h_connection_close
};

enum http_host_state
{
	s_http_host_dead = 1
	, s_http_userinfo_start
	, s_http_userinfo
	, s_http_host_start
	, s_http_host_v6_start
	, s_http_host
	, s_http_host_v6
	, s_http_host_v6_end
	, s_http_host_port_start
	, s_http_host_port
};

/* Macros for character classes; depends on strict-mode  */
#define CR                  '\r'
#define LF                  '\n'
#define LOWER(c)            (unsigned char)(c | 0x20)
#define IS_ALPHA(c)         (LOWER(c) >= 'a' && LOWER(c) <= 'z')
#define IS_NUM(c)           ((c) >= '0' && (c) <= '9')
#define IS_ALPHANUM(c)      (IS_ALPHA(c) || IS_NUM(c))
#define IS_HEX(c)           (IS_NUM(c) || (LOWER(c) >= 'a' && LOWER(c) <= 'f'))
#define IS_MARK(c)          ((c) == '-' || (c) == '_' || (c) == '.' || \
  (c) == '!' || (c) == '~' || (c) == '*' || (c) == '\'' || (c) == '(' || \
  (c) == ')')
#define IS_USERINFO_CHAR(c) (IS_ALPHANUM(c) || IS_MARK(c) || (c) == '%' || \
  (c) == ';' || (c) == ':' || (c) == '&' || (c) == '=' || (c) == '+' || \
  (c) == '$' || (c) == ',')

#if HTTP_PARSER_STRICT
#define TOKEN(c)            (tokens[(unsigned char)c])
#define IS_URL_CHAR(c)      (BIT_AT(normal_url_char, (unsigned char)c))
#define IS_HOST_CHAR(c)     (IS_ALPHANUM(c) || (c) == '.' || (c) == '-')
#else
#define TOKEN(c)            ((c == ' ') ? ' ' : tokens[(unsigned char)c])
#define IS_URL_CHAR(c)                                                         \
  (BIT_AT(normal_url_char, (unsigned char)c) || ((c) & 0x80))
#define IS_HOST_CHAR(c)                                                        \
  (IS_ALPHANUM(c) || (c) == '.' || (c) == '-' || (c) == '_')
#endif


#define start_state (type == HTTP_REQUEST ? s_start_req : s_start_res)


#if HTTP_PARSER_STRICT
#define STRICT_CHECK(cond)                                          \
do {                                                                 \
  if (cond) {                                                        \
    SET_ERRNO(HPE_STRICT);                                           \
    goto error;                                                      \
  }                                                                  \
} while (0)
#define NEW_MESSAGE() (ShouldKeepAlive() ? start_state : s_dead)
#else
#define STRICT_CHECK(cond)
#define NEW_MESSAGE() start_state
#endif


/* Map errno values to strings for human-readable output */
#define HTTP_STRERROR_GEN(n, s) { "HPE_" #n, s },

static struct
{
	const char *name;
	const char *description;
} http_strerror_tab[] = {
	HTTP_ERRNO_MAP(HTTP_STRERROR_GEN)
};
#undef HTTP_STRERROR_GEN

/* Our URL parser.
 *
 * This is designed to be shared by http_parser_execute() for URL validation,
 * hence it has a state transition + byte-for-byte interface. In addition, it
 * is meant to be embedded in http_parser_parse_url(), which does the dirty
 * work of turning state transitions URL components for its API.
 *
 * This function should only be invoked with non-space characters. It is
 * assumed that the caller cares about (and can detect) the transition between
 * URL and non-URL states by looking for these.
 */
static enum state
parse_url_char(enum state s, const char ch)
{
	if (ch == ' ' || ch == '\r' || ch == '\n')
	{
		return s_dead;
	}

#if HTTP_PARSER_STRICT
	if (ch == '\t' || ch == '\f')
	{
		return s_dead;
	}
#endif

	switch (s)
	{
		case s_req_spaces_before_url:
			/* Proxied requests are followed by scheme of an absolute URI (alpha).
			 * All methods except CONNECT are followed by '/' or '*'.
			 */

			if (ch == '/' || ch == '*')
			{
				return s_req_path;
			}

			if (IS_ALPHA(ch))
			{
				return s_req_schema;
			}

			break;

		case s_req_schema:
			if (IS_ALPHA(ch))
			{
				return s;
			}

			if (ch == ':')
			{
				return s_req_schema_slash;
			}

			break;

		case s_req_schema_slash:
			if (ch == '/')
			{
				return s_req_schema_slash_slash;
			}

			break;

		case s_req_schema_slash_slash:
			if (ch == '/')
			{
				return s_req_server_start;
			}

			break;

		case s_req_server_with_at:
			if (ch == '@')
			{
				return s_dead;
			}

			/* FALLTHROUGH */
		case s_req_server_start:
		case s_req_server:
			if (ch == '/')
			{
				return s_req_path;
			}

			if (ch == '?')
			{
				return s_req_query_string_start;
			}

			if (ch == '@')
			{
				return s_req_server_with_at;
			}

			if (IS_USERINFO_CHAR(ch) || ch == '[' || ch == ']')
			{
				return s_req_server;
			}

			break;

		case s_req_path:
			if (IS_URL_CHAR(ch))
			{
				return s;
			}

			switch (ch)
			{
				case '?':
					return s_req_query_string_start;

				case '#':
					return s_req_fragment_start;
			}

			break;

		case s_req_query_string_start:
		case s_req_query_string:
			if (IS_URL_CHAR(ch))
			{
				return s_req_query_string;
			}

			switch (ch)
			{
				case '?':
					/* allow extra '?' in query string */
					return s_req_query_string;

				case '#':
					return s_req_fragment_start;
			}

			break;

		case s_req_fragment_start:
			if (IS_URL_CHAR(ch))
			{
				return s_req_fragment;
			}

			switch (ch)
			{
				case '?':
					return s_req_fragment;

				case '#':
					return s;
			}

			break;

		case s_req_fragment:
			if (IS_URL_CHAR(ch))
			{
				return s;
			}

			switch (ch)
			{
				case '?':
				case '#':
					return s;
			}

			break;

		default:
			break;
	}

	/* We should never fall out of the switch above unless there's an error */
	return s_dead;
}

DWORD HTTPParser::Execute(const char *data, DWORD len)
{
	char c, ch;
	int8_t unhex_val;
	const char *p = data;
	const char *header_field_mark = 0;
	const char *header_value_mark = 0;
	const char *url_mark = 0;
	const char *body_mark = 0;

	/* We're in an error state. Don't bother doing anything. */
	if (HTTP_PARSER_ERRNO(this) != HPE_OK)
	{
		return 0;
	}

	if (len == 0)
	{
		switch (state)
		{
			case s_body_identity_eof:
				/* Use of CALLBACK_NOTIFY() here would erroneously return 1 byte read if
				 * we got paused.
				 */
				CALLBACK_NOTIFY_NOADVANCE(message_complete);
				return 0;

			case s_dead:
			case s_start_req_or_res:
			case s_start_res:
			case s_start_req:
				return 0;

			default:
				SET_ERRNO(HPE_INVALID_EOF_STATE);
				return 1;
		}
	}


	if (state == s_header_field)
		header_field_mark = data;
	if (state == s_header_value)
		header_value_mark = data;
	switch (state)
	{
		case s_req_path:
		case s_req_schema:
		case s_req_schema_slash:
		case s_req_schema_slash_slash:
		case s_req_server_start:
		case s_req_server:
		case s_req_server_with_at:
		case s_req_query_string_start:
		case s_req_query_string:
		case s_req_fragment_start:
		case s_req_fragment:
			url_mark = data;
			break;
	}

	for (p = data; p != data + len; p++)
	{
		ch = *p;

		if (PARSING_HEADER(state))
		{
			++nread;
			/* Don't allow the total size of the HTTP headers (including the status
			 * line) to exceed HTTP_MAX_HEADER_SIZE.  This check is here to protect
			 * embedders against denial-of-service attacks where the attacker feeds
			 * us a never-ending header that the embedder keeps buffering.
			 *
			 * This check is arguably the responsibility of embedders but we're doing
			 * it on the embedder's behalf because most won't bother and this way we
			 * make the web a little safer.  HTTP_MAX_HEADER_SIZE is still far bigger
			 * than any reasonable request or response so this should never affect
			 * day-to-day operation.
			 */
			if (nread > HTTP_MAX_HEADER_SIZE)
			{
				SET_ERRNO(HPE_HEADER_OVERFLOW);
				goto error;
			}
		}

reexecute_byte:
		switch (state)
		{

			case s_dead:
				/* this state is used after a 'Connection: close' message
				 * the parser will error out if it reads another message
				 */
				if (ch == CR || ch == LF)
					break;

				SET_ERRNO(HPE_CLOSED_CONNECTION);
				goto error;

			case s_start_req_or_res:
			{
				if (ch == CR || ch == LF)
					break;
				flags = 0;
				content_length = ULLONG_MAX;

				if (ch == 'H')
				{
					state = s_res_or_resp_H;

					CALLBACK_NOTIFY(message_begin);
				} else
				{
					type = HTTP_REQUEST;
					state = s_start_req;
					goto reexecute_byte;
				}

				break;
			}

			case s_res_or_resp_H:
				if (ch == 'T')
				{
					type = HTTP_RESPONSE;
					state = s_res_HT;
				} else
				{
					if (ch != 'E')
					{
						SET_ERRNO(HPE_INVALID_CONSTANT);
						goto error;
					}

					type = HTTP_REQUEST;
					method = HTTP_HEAD;
					index = 2;
					state = s_req_method;
				}
				break;

			case s_start_res:
			{
				flags = 0;
				content_length = ULLONG_MAX;

				switch (ch)
				{
					case 'H':
						state = s_res_H;
						break;

					case CR:
					case LF:
						break;

					default:
						SET_ERRNO(HPE_INVALID_CONSTANT);
						goto error;
				}

				CALLBACK_NOTIFY(message_begin);
				break;
			}

			case s_res_H:
				STRICT_CHECK(ch != 'T');
				state = s_res_HT;
				break;

			case s_res_HT:
				STRICT_CHECK(ch != 'T');
				state = s_res_HTT;
				break;

			case s_res_HTT:
				STRICT_CHECK(ch != 'P');
				state = s_res_HTTP;
				break;

			case s_res_HTTP:
				STRICT_CHECK(ch != '/');
				state = s_res_first_http_major;
				break;

			case s_res_first_http_major:
				if (ch < '0' || ch > '9')
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_major = ch - '0';
				state = s_res_http_major;
				break;

				/* major HTTP version or dot */
			case s_res_http_major:
			{
				if (ch == '.')
				{
					state = s_res_first_http_minor;
					break;
				}

				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_major *= 10;
				http_major += ch - '0';

				if (http_major > 999)
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				break;
			}

				/* first digit of minor HTTP version */
			case s_res_first_http_minor:
				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_minor = ch - '0';
				state = s_res_http_minor;
				break;

				/* minor HTTP version or end of request line */
			case s_res_http_minor:
			{
				if (ch == ' ')
				{
					state = s_res_first_status_code;
					break;
				}

				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_minor *= 10;
				http_minor += ch - '0';

				if (http_minor > 999)
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				break;
			}

			case s_res_first_status_code:
			{
				if (!IS_NUM(ch))
				{
					if (ch == ' ')
					{
						break;
					}

					SET_ERRNO(HPE_INVALID_STATUS);
					goto error;
				}
				status_code = ch - '0';
				state = s_res_status_code;
				break;
			}

			case s_res_status_code:
			{
				if (!IS_NUM(ch))
				{
					switch (ch)
					{
						case ' ':
							state = s_res_status;
							break;
						case CR:
							state = s_res_line_almost_done;
							break;
						case LF:
							state = s_header_field_start;
							break;
						default:
							SET_ERRNO(HPE_INVALID_STATUS);
							goto error;
					}
					break;
				}

				status_code *= 10;
				status_code += ch - '0';

				if (status_code > 999)
				{
					SET_ERRNO(HPE_INVALID_STATUS);
					goto error;
				}

				break;
			}

			case s_res_status:
				/* the human readable status. e.g. "NOT FOUND"
				 * we are not humans so just ignore this */
				if (ch == CR)
				{
					state = s_res_line_almost_done;
					break;
				}

				if (ch == LF)
				{
					state = s_header_field_start;
					break;
				}
				break;

			case s_res_line_almost_done:
				STRICT_CHECK(ch != LF);
				state = s_header_field_start;
				CALLBACK_NOTIFY(status_complete);
				break;

			case s_start_req:
			{
				if (ch == CR || ch == LF)
					break;
				flags = 0;
				content_length = ULLONG_MAX;

				if (!IS_ALPHA(ch))
				{
					SET_ERRNO(HPE_INVALID_METHOD);
					goto error;
				}

				method = (enum http_method) 0;
				index = 1;
				switch (ch)
				{
					case 'C': method = HTTP_CONNECT;
						/* or COPY, CHECKOUT */ break;
					case 'D': method = HTTP_DELETE;
						break;
					case 'G': method = HTTP_GET;
						break;
					case 'H': method = HTTP_HEAD;
						break;
					case 'L': method = HTTP_LOCK;
						break;
					case 'M': method = HTTP_MKCOL;
						/* or MOVE, MKACTIVITY, MERGE, M-SEARCH */ break;
					case 'N': method = HTTP_NOTIFY;
						break;
					case 'O': method = HTTP_OPTIONS;
						break;
					case 'P': method = HTTP_POST;
						/* or PROPFIND|PROPPATCH|PUT|PATCH|PURGE */
						break;
					case 'R': method = HTTP_REPORT;
						break;
					case 'S': method = HTTP_SUBSCRIBE;
						/* or SEARCH */ break;
					case 'T': method = HTTP_TRACE;
						break;
					case 'U': method = HTTP_UNLOCK;
						/* or UNSUBSCRIBE */ break;
					default:
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
				}
				state = s_req_method;

				CALLBACK_NOTIFY(message_begin);

				break;
			}

			case s_req_method:
			{
				const char *matcher;
				if (ch == '\0')
				{
					SET_ERRNO(HPE_INVALID_METHOD);
					goto error;
				}

				matcher = method_strings[method];
				if (ch == ' ' && matcher[index] == '\0')
				{
					state = s_req_spaces_before_url;
				} else if (ch == matcher[index])
				{
					; /* nada */
				} else if (method == HTTP_CONNECT)
				{
					if (index == 1 && ch == 'H')
					{
						method = HTTP_CHECKOUT;
					} else if (index == 2 && ch == 'P')
					{
						method = HTTP_COPY;
					} else
					{
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
					}
				} else if (method == HTTP_MKCOL)
				{
					if (index == 1 && ch == 'O')
					{
						method = HTTP_MOVE;
					} else if (index == 1 && ch == 'E')
					{
						method = HTTP_MERGE;
					} else if (index == 1 && ch == '-')
					{
						method = HTTP_MSEARCH;
					} else if (index == 2 && ch == 'A')
					{
						method = HTTP_MKACTIVITY;
					} else
					{
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
					}
				} else if (method == HTTP_SUBSCRIBE)
				{
					if (index == 1 && ch == 'E')
					{
						method = HTTP_SEARCH;
					} else
					{
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
					}
				} else if (index == 1 && method == HTTP_POST)
				{
					if (ch == 'R')
					{
						method = HTTP_PROPFIND; /* or HTTP_PROPPATCH */
					} else if (ch == 'U')
					{
						method = HTTP_PUT; /* or HTTP_PURGE */
					} else if (ch == 'A')
					{
						method = HTTP_PATCH;
					} else
					{
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
					}
				} else if (index == 2)
				{
					if (method == HTTP_PUT)
					{
						if (ch == 'R')
						{
							method = HTTP_PURGE;
						} else
						{
							SET_ERRNO(HPE_INVALID_METHOD);
							goto error;
						}
					} else if (method == HTTP_UNLOCK)
					{
						if (ch == 'S')
						{
							method = HTTP_UNSUBSCRIBE;
						} else
						{
							SET_ERRNO(HPE_INVALID_METHOD);
							goto error;
						}
					} else
					{
						SET_ERRNO(HPE_INVALID_METHOD);
						goto error;
					}
				} else if (index == 4 && method == HTTP_PROPFIND && ch == 'P')
				{
					method = HTTP_PROPPATCH;
				} else
				{
					SET_ERRNO(HPE_INVALID_METHOD);
					goto error;
				}

				++index;
				break;
			}

			case s_req_spaces_before_url:
			{
				if (ch == ' ') break;

				MARK(url);
				if (method == HTTP_CONNECT)
				{
					state = s_req_server_start;
				}

				state = parse_url_char((enum state)state, ch);
				if (state == s_dead)
				{
					SET_ERRNO(HPE_INVALID_URL);
					goto error;
				}

				break;
			}

			case s_req_schema:
			case s_req_schema_slash:
			case s_req_schema_slash_slash:
			case s_req_server_start:
			{
				switch (ch)
				{
						/* No whitespace allowed here */
					case ' ':
					case CR:
					case LF:
						SET_ERRNO(HPE_INVALID_URL);
						goto error;
					default:
						state = parse_url_char((enum state)state, ch);
						if (state == s_dead)
						{
							SET_ERRNO(HPE_INVALID_URL);
							goto error;
						}
				}

				break;
			}

			case s_req_server:
			case s_req_server_with_at:
			case s_req_path:
			case s_req_query_string_start:
			case s_req_query_string:
			case s_req_fragment_start:
			case s_req_fragment:
			{
				switch (ch)
				{
					case ' ':
						state = s_req_http_start;
						CALLBACK_DATA(url);
						break;
					case CR:
					case LF:
						http_major = 0;
						http_minor = 9;
						state = (ch == CR) ?
							s_req_line_almost_done :
							s_header_field_start;
						CALLBACK_DATA(url);
						break;
					default:
						state = parse_url_char((enum state)state, ch);
						if (state == s_dead)
						{
							SET_ERRNO(HPE_INVALID_URL);
							goto error;
						}
				}
				break;
			}

			case s_req_http_start:
				switch (ch)
				{
					case 'H':
						state = s_req_http_H;
						break;
					case ' ':
						break;
					default:
						SET_ERRNO(HPE_INVALID_CONSTANT);
						goto error;
				}
				break;

			case s_req_http_H:
				STRICT_CHECK(ch != 'T');
				state = s_req_http_HT;
				break;

			case s_req_http_HT:
				STRICT_CHECK(ch != 'T');
				state = s_req_http_HTT;
				break;

			case s_req_http_HTT:
				STRICT_CHECK(ch != 'P');
				state = s_req_http_HTTP;
				break;

			case s_req_http_HTTP:
				STRICT_CHECK(ch != '/');
				state = s_req_first_http_major;
				break;

				/* first digit of major HTTP version */
			case s_req_first_http_major:
				if (ch < '1' || ch > '9')
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_major = ch - '0';
				state = s_req_http_major;
				break;

				/* major HTTP version or dot */
			case s_req_http_major:
			{
				if (ch == '.')
				{
					state = s_req_first_http_minor;
					break;
				}

				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_major *= 10;
				http_major += ch - '0';

				if (http_major > 999)
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				break;
			}

				/* first digit of minor HTTP version */
			case s_req_first_http_minor:
				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_minor = ch - '0';
				state = s_req_http_minor;
				break;

				/* minor HTTP version or end of request line */
			case s_req_http_minor:
			{
				if (ch == CR)
				{
					state = s_req_line_almost_done;
					break;
				}

				if (ch == LF)
				{
					state = s_header_field_start;
					break;
				}

				/* XXX allow spaces after digit? */

				if (!IS_NUM(ch))
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				http_minor *= 10;
				http_minor += ch - '0';

				if (http_minor > 999)
				{
					SET_ERRNO(HPE_INVALID_VERSION);
					goto error;
				}

				break;
			}

				/* end of request line */
			case s_req_line_almost_done:
			{
				if (ch != LF)
				{
					SET_ERRNO(HPE_LF_EXPECTED);
					goto error;
				}

				state = s_header_field_start;
				break;
			}

			case s_header_field_start:
			{
				if (ch == CR)
				{
					state = s_headers_almost_done;
					break;
				}

				if (ch == LF)
				{
					/* they might be just sending \n instead of \r\n so this would be
					 * the second \n to denote the end of headers*/
					state = s_headers_almost_done;
					goto reexecute_byte;
				}

				c = TOKEN(ch);

				if (!c)
				{
					SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
					goto error;
				}

				MARK(header_field);

				index = 0;
				state = s_header_field;

				switch (c)
				{
					case 'c':
						header_state = h_C;
						break;

					case 'p':
						header_state = h_matching_proxy_connection;
						break;

					case 't':
						header_state = h_matching_transfer_encoding;
						break;

					case 'u':
						header_state = h_matching_upgrade;
						break;

					default:
						header_state = h_general;
						break;
				}
				break;
			}

			case s_header_field:
			{
				c = TOKEN(ch);

				if (c)
				{
					switch (header_state)
					{
						case h_general:
							break;

						case h_C:
							index++;
							header_state = (c == 'o' ? h_CO : h_general);
							break;

						case h_CO:
							index++;
							header_state = (c == 'n' ? h_CON : h_general);
							break;

						case h_CON:
							index++;
							switch (c)
							{
								case 'n':
									header_state = h_matching_connection;
									break;
								case 't':
									header_state = h_matching_content_length;
									break;
								default:
									header_state = h_general;
									break;
							}
							break;

							/* connection */

						case h_matching_connection:
							index++;
							if (index > sizeof (CONNECTION) - 1
								|| c != CONNECTION[index])
							{
								header_state = h_general;
							} else if (index == sizeof (CONNECTION) - 2)
							{
								header_state = h_connection;
							}
							break;

							/* proxy-connection */

						case h_matching_proxy_connection:
							index++;
							if (index > sizeof (PROXY_CONNECTION) - 1
								|| c != PROXY_CONNECTION[index])
							{
								header_state = h_general;
							} else if (index == sizeof (PROXY_CONNECTION) - 2)
							{
								header_state = h_connection;
							}
							break;

							/* content-length */

						case h_matching_content_length:
							index++;
							if (index > sizeof (CONTENT_LENGTH) - 1
								|| c != CONTENT_LENGTH[index])
							{
								header_state = h_general;
							} else if (index == sizeof (CONTENT_LENGTH) - 2)
							{
								header_state = h_content_length;
							}
							break;

							/* transfer-encoding */

						case h_matching_transfer_encoding:
							index++;
							if (index > sizeof (TRANSFER_ENCODING) - 1
								|| c != TRANSFER_ENCODING[index])
							{
								header_state = h_general;
							} else if (index == sizeof (TRANSFER_ENCODING) - 2)
							{
								header_state = h_transfer_encoding;
							}
							break;

							/* upgrade */

						case h_matching_upgrade:
							index++;
							if (index > sizeof (UPGRADE) - 1
								|| c != UPGRADE[index])
							{
								header_state = h_general;
							} else if (index == sizeof (UPGRADE) - 2)
							{
								header_state = h_upgrade;
							}
							break;

						case h_connection:
						case h_content_length:
						case h_transfer_encoding:
						case h_upgrade:
							if (ch != ' ') header_state = h_general;
							break;

						default:
							assert(0 && "Unknown header_state");
							break;
					}
					break;
				}

				if (ch == ':')
				{
					state = s_header_value_start;
					CALLBACK_DATA(header_field);
					break;
				}

				if (ch == CR)
				{
					state = s_header_almost_done;
					CALLBACK_DATA(header_field);
					break;
				}

				if (ch == LF)
				{
					state = s_header_field_start;
					CALLBACK_DATA(header_field);
					break;
				}

				SET_ERRNO(HPE_INVALID_HEADER_TOKEN);
				goto error;
			}

			case s_header_value_start:
			{
				if (ch == ' ' || ch == '\t') break;

				MARK(header_value);

				state = s_header_value;
				index = 0;

				if (ch == CR)
				{
					header_state = h_general;
					state = s_header_almost_done;
					CALLBACK_DATA(header_value);
					break;
				}

				if (ch == LF)
				{
					state = s_header_field_start;
					CALLBACK_DATA(header_value);
					break;
				}

				c = LOWER(ch);

				switch (header_state)
				{
					case h_upgrade:
						flags |= F_UPGRADE;
						header_state = h_general;
						break;

					case h_transfer_encoding:
						/* looking for 'Transfer-Encoding: chunked' */
						if ('c' == c)
						{
							header_state = h_matching_transfer_encoding_chunked;
						} else
						{
							header_state = h_general;
						}
						break;

					case h_content_length:
						if (!IS_NUM(ch))
						{
							SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
							goto error;
						}

						content_length = ch - '0';
						break;

					case h_connection:
						/* looking for 'Connection: keep-alive' */
						if (c == 'k')
						{
							header_state = h_matching_connection_keep_alive;
							/* looking for 'Connection: close' */
						} else if (c == 'c')
						{
							header_state = h_matching_connection_close;
						} else
						{
							header_state = h_general;
						}
						break;

					default:
						header_state = h_general;
						break;
				}
				break;
			}

			case s_header_value:
			{

				if (ch == CR)
				{
					state = s_header_almost_done;
					CALLBACK_DATA(header_value);
					break;
				}

				if (ch == LF)
				{
					state = s_header_almost_done;
					CALLBACK_DATA_NOADVANCE(header_value);
					goto reexecute_byte;
				}

				c = LOWER(ch);

				switch (header_state)
				{
					case h_general:
						break;

					case h_connection:
					case h_transfer_encoding:
						assert(0 && "Shouldn't get here.");
						break;

					case h_content_length:
					{
						uint64_t t;

						if (ch == ' ') break;

						if (!IS_NUM(ch))
						{
							SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
							goto error;
						}

						t = content_length;
						t *= 10;
						t += ch - '0';

						/* Overflow? */
						if (t < content_length || t == ULLONG_MAX)
						{
							SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
							goto error;
						}

						content_length = t;
						break;
					}

						/* Transfer-Encoding: chunked */
					case h_matching_transfer_encoding_chunked:
						index++;
						if (index > sizeof (CHUNKED) - 1
							|| c != CHUNKED[index])
						{
							header_state = h_general;
						} else if (index == sizeof (CHUNKED) - 2)
						{
							header_state = h_transfer_encoding_chunked;
						}
						break;

						/* looking for 'Connection: keep-alive' */
					case h_matching_connection_keep_alive:
						index++;
						if (index > sizeof (KEEP_ALIVE) - 1
							|| c != KEEP_ALIVE[index])
						{
							header_state = h_general;
						} else if (index == sizeof (KEEP_ALIVE) - 2)
						{
							header_state = h_connection_keep_alive;
						}
						break;

						/* looking for 'Connection: close' */
					case h_matching_connection_close:
						index++;
						if (index > sizeof (CLOSE) - 1 || c != CLOSE[index])
						{
							header_state = h_general;
						} else if (index == sizeof (CLOSE) - 2)
						{
							header_state = h_connection_close;
						}
						break;

					case h_transfer_encoding_chunked:
					case h_connection_keep_alive:
					case h_connection_close:
						if (ch != ' ') header_state = h_general;
						break;

					default:
						state = s_header_value;
						header_state = h_general;
						break;
				}
				break;
			}

			case s_header_almost_done:
			{
				STRICT_CHECK(ch != LF);

				state = s_header_value_lws;

				switch (header_state)
				{
					case h_connection_keep_alive:
						flags |= F_CONNECTION_KEEP_ALIVE;
						break;
					case h_connection_close:
						flags |= F_CONNECTION_CLOSE;
						break;
					case h_transfer_encoding_chunked:
						flags |= F_CHUNKED;
						break;
					default:
						break;
				}

				break;
			}

			case s_header_value_lws:
			{
				if (ch == ' ' || ch == '\t')
					state = s_header_value_start;
				else
				{
					state = s_header_field_start;
					goto reexecute_byte;
				}
				break;
			}

			case s_headers_almost_done:
			{
				STRICT_CHECK(ch != LF);

				if (flags & F_TRAILING)
				{
					/* End of a chunked request */
					state = NEW_MESSAGE();
					CALLBACK_NOTIFY(message_complete);
					break;
				}

				state = s_headers_done;

				/* Set this here so that on_headers_complete() callbacks can see it */
				upgrade =
					(flags & F_UPGRADE || method == HTTP_CONNECT);

				/* Here we call the headers_complete callback. This is somewhat
				 * different than other callbacks because if the user returns 1, we
				 * will interpret that as saying that this message has no body. This
				 * is needed for the annoying case of recieving a response to a HEAD
				 * request.
				 *
				 * We'd like to use CALLBACK_NOTIFY_NOADVANCE() here but we cannot, so
				 * we have to simulate it by handling a change in errno below.
				 */
				if (listener)
				{
					switch (listener->on_headers_complete(this))
					{
						case 0:
							break;

						case 1:
							flags |= F_SKIPBODY;
							break;

						default:
							SET_ERRNO(HPE_CB_headers_complete);
							return p - data; /* Error */
					}
				}
				
				if (HTTP_PARSER_ERRNO(this) != HPE_OK)
				{
					return p - data;
				}

				goto reexecute_byte;
			}

			case s_headers_done:
			{
				STRICT_CHECK(ch != LF);

				nread = 0;

				/* Exit, the rest of the connect is in a different protocol. */
				if (upgrade)
				{
					state = NEW_MESSAGE();
					CALLBACK_NOTIFY(message_complete);
					return (p - data) + 1;
				}

				if (flags & F_SKIPBODY)
				{
					state = NEW_MESSAGE();
					CALLBACK_NOTIFY(message_complete);
				} else if (flags & F_CHUNKED)
				{
					/* chunked encoding - ignore Content-Length header */
					state = s_chunk_size_start;
				} else
				{
					if (content_length == 0)
					{
						/* Content-Length header given but zero: Content-Length: 0\r\n */
						state = NEW_MESSAGE();
						CALLBACK_NOTIFY(message_complete);
					} else if (content_length != ULLONG_MAX)
					{
						/* Content-Length header given and non-zero */
						state = s_body_identity;
					} else
					{
						if (type == HTTP_REQUEST ||
							!MessageNeedsEOF())
						{
							/* Assume content-length 0 - read the next */
							state = NEW_MESSAGE();
							CALLBACK_NOTIFY(message_complete);
						} else
						{
							/* Read body until EOF */
							state = s_body_identity_eof;
						}
					}
				}

				break;
			}

			case s_body_identity:
			{
				uint64_t to_read = MIN(content_length,
					(uint64_t) ((data + len) - p));

				assert(content_length != 0
					&& content_length != ULLONG_MAX);

				/* The difference between advancing content_length and p is because
				 * the latter will automaticaly advance on the next loop iteration.
				 * Further, if content_length ends up at 0, we want to see the last
				 * byte again for our message complete callback.
				 */
				MARK(body);
				content_length -= to_read;
				p += to_read - 1;

				if (content_length == 0)
				{
					state = s_message_done;

					/* Mimic CALLBACK_DATA_NOADVANCE() but with one extra byte.
					 *
					 * The alternative to doing this is to wait for the next byte to
					 * trigger the data callback, just as in every other case. The
					 * problem with this is that this makes it difficult for the test
					 * harness to distinguish between complete-on-EOF and
					 * complete-on-length. It's not clear that this distinction is
					 * important for applications, but let's keep it for now.
					 */
					CALLBACK_DATA_(body, p - body_mark + 1, p - data);
					goto reexecute_byte;
				}

				break;
			}

				/* read until EOF */
			case s_body_identity_eof:
				MARK(body);
				p = data + len - 1;

				break;

			case s_message_done:
				state = NEW_MESSAGE();
				CALLBACK_NOTIFY(message_complete);
				break;

			case s_chunk_size_start:
			{
				assert(nread == 1);
				assert(flags & F_CHUNKED);

				unhex_val = unhex[(unsigned char) ch];
				if (unhex_val == -1)
				{
					SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
					goto error;
				}

				content_length = unhex_val;
				state = s_chunk_size;
				break;
			}

			case s_chunk_size:
			{
				uint64_t t;

				assert(flags & F_CHUNKED);

				if (ch == CR)
				{
					state = s_chunk_size_almost_done;
					break;
				}

				unhex_val = unhex[(unsigned char) ch];

				if (unhex_val == -1)
				{
					if (ch == ';' || ch == ' ')
					{
						state = s_chunk_parameters;
						break;
					}

					SET_ERRNO(HPE_INVALID_CHUNK_SIZE);
					goto error;
				}

				t = content_length;
				t *= 16;
				t += unhex_val;

				/* Overflow? */
				if (t < content_length || t == ULLONG_MAX)
				{
					SET_ERRNO(HPE_INVALID_CONTENT_LENGTH);
					goto error;
				}

				content_length = t;
				break;
			}

			case s_chunk_parameters:
			{
				assert(flags & F_CHUNKED);
				/* just ignore this shit. TODO check for overflow */
				if (ch == CR)
				{
					state = s_chunk_size_almost_done;
					break;
				}
				break;
			}

			case s_chunk_size_almost_done:
			{
				assert(flags & F_CHUNKED);
				STRICT_CHECK(ch != LF);

				nread = 0;

				if (content_length == 0)
				{
					flags |= F_TRAILING;
					state = s_header_field_start;
				} else
				{
					state = s_chunk_data;
				}
				break;
			}

			case s_chunk_data:
			{
				uint64_t to_read = MIN(content_length,
					(uint64_t) ((data + len) - p));

				assert(flags & F_CHUNKED);
				assert(content_length != 0
					&& content_length != ULLONG_MAX);

				/* See the explanation in s_body_identity for why the content
				 * length and data pointers are managed this way.
				 */
				MARK(body);
				content_length -= to_read;
				p += to_read - 1;

				if (content_length == 0)
				{
					state = s_chunk_data_almost_done;
				}

				break;
			}

			case s_chunk_data_almost_done:
				assert(flags & F_CHUNKED);
				assert(content_length == 0);
				STRICT_CHECK(ch != CR);
				state = s_chunk_data_done;
				CALLBACK_DATA(body);
				break;

			case s_chunk_data_done:
				assert(flags & F_CHUNKED);
				STRICT_CHECK(ch != LF);
				nread = 0;
				state = s_chunk_size_start;
				break;

			default:
				assert(0 && "unhandled state");
				SET_ERRNO(HPE_INVALID_INTERNAL_STATE);
				goto error;
		}
	}

	/* Run callbacks for any marks that we have leftover after we ran our of
	 * bytes. There should be at most one of these set, so it's OK to invoke
	 * them in series (unset marks will not result in callbacks).
	 *
	 * We use the NOADVANCE() variety of callbacks here because 'p' has already
	 * overflowed 'data' and this allows us to correct for the off-by-one that
	 * we'd otherwise have (since CALLBACK_DATA() is meant to be run with a 'p'
	 * value that's in-bounds).
	 */

	assert(((header_field_mark ? 1 : 0) +
		(header_value_mark ? 1 : 0) +
		(url_mark ? 1 : 0) +
		(body_mark ? 1 : 0)) <= 1);

	CALLBACK_DATA_NOADVANCE(header_field);
	CALLBACK_DATA_NOADVANCE(header_value);
	CALLBACK_DATA_NOADVANCE(url);
	CALLBACK_DATA_NOADVANCE(body);

	return len;

error:
	if (HTTP_PARSER_ERRNO(this) == HPE_OK)
	{
		SET_ERRNO(HPE_UNKNOWN);
	}

	return (p - data);
}

/* Does the parser need to see an EOF to find the end of the message? */
int HTTPParser::MessageNeedsEOF()
{
	if (type == HTTP_REQUEST)
	{
		return 0;
	}

	/* See RFC 2616 section 4.4 */
	if (status_code / 100 == 1 || /* 1xx e.g. Continue */
		status_code == 204 || /* No Content */
		status_code == 304 || /* Not Modified */
		flags & F_SKIPBODY)
	{ /* response to a HEAD request */
		return 0;
	}

	if ((flags & F_CHUNKED) || content_length != ULLONG_MAX)
	{
		return 0;
	}

	return 1;
}

int HTTPParser::ShouldKeepAlive()
{
	if (http_major > 0 && http_minor > 0)
	{
		/* HTTP/1.1 */
		if (flags & F_CONNECTION_CLOSE)
		{
			return 0;
		}
	} else
	{
		/* HTTP/1.0 or earlier */
		if (!(flags & F_CONNECTION_KEEP_ALIVE))
		{
			return 0;
		}
	}

	return !MessageNeedsEOF();
}

const char *
http_method_str(enum http_method m)
{
	return ELEM_AT(method_strings, m, "<unknown>");
}

void HTTPParser::Init(Listener *listener, Type type)
{
	this->listener = listener;
	this->type = type;
	state = (type == HTTP_REQUEST ? s_start_req : (type == HTTP_RESPONSE ? s_start_res : s_start_req_or_res));
	http_errno = HPE_OK;
}

const char *
http_errno_name(enum http_errno err)
{
	assert(err < (sizeof (http_strerror_tab) / sizeof (http_strerror_tab[0])));
	return http_strerror_tab[err].name;
}

const char *
http_errno_description(enum http_errno err)
{
	assert(err < (sizeof (http_strerror_tab) / sizeof (http_strerror_tab[0])));
	return http_strerror_tab[err].description;
}

static enum http_host_state
http_parse_host_char(enum http_host_state s, const char ch)
{
	switch (s)
	{
		case s_http_userinfo:
		case s_http_userinfo_start:
			if (ch == '@')
			{
				return s_http_host_start;
			}

			if (IS_USERINFO_CHAR(ch))
			{
				return s_http_userinfo;
			}
			break;

		case s_http_host_start:
			if (ch == '[')
			{
				return s_http_host_v6_start;
			}

			if (IS_HOST_CHAR(ch))
			{
				return s_http_host;
			}

			break;

		case s_http_host:
			if (IS_HOST_CHAR(ch))
			{
				return s_http_host;
			}

			/* FALLTHROUGH */
		case s_http_host_v6_end:
			if (ch == ':')
			{
				return s_http_host_port_start;
			}

			break;

		case s_http_host_v6:
			if (ch == ']')
			{
				return s_http_host_v6_end;
			}

			/* FALLTHROUGH */
		case s_http_host_v6_start:
			if (IS_HEX(ch) || ch == ':' || ch == '.')
			{
				return s_http_host_v6;
			}

			break;

		case s_http_host_port:
		case s_http_host_port_start:
			if (IS_NUM(ch))
			{
				return s_http_host_port;
			}

			break;

		default:
			break;
	}
	return s_http_host_dead;
}

static int
http_parse_host(const char * buf, struct http_parser_url *u, int found_at)
{
	enum http_host_state s;

	const char *p;
	DWORD buflen = u->field_data[UF_HOST].off + u->field_data[UF_HOST].len;

	u->field_data[UF_HOST].len = 0;

	s = found_at ? s_http_userinfo_start : s_http_host_start;

	for (p = buf + u->field_data[UF_HOST].off; p < buf + buflen; p++)
	{
		enum http_host_state new_s = http_parse_host_char(s, *p);

		if (new_s == s_http_host_dead)
		{
			return 1;
		}

		switch (new_s)
		{
			case s_http_host:
				if (s != s_http_host)
				{
					u->field_data[UF_HOST].off = p - buf;
				}
				u->field_data[UF_HOST].len++;
				break;

			case s_http_host_v6:
				if (s != s_http_host_v6)
				{
					u->field_data[UF_HOST].off = p - buf;
				}
				u->field_data[UF_HOST].len++;
				break;

			case s_http_host_port:
				if (s != s_http_host_port)
				{
					u->field_data[UF_PORT].off = p - buf;
					u->field_data[UF_PORT].len = 0;
					u->field_set |= (1 << UF_PORT);
				}
				u->field_data[UF_PORT].len++;
				break;

			case s_http_userinfo:
				if (s != s_http_userinfo)
				{
					u->field_data[UF_USERINFO].off = p - buf;
					u->field_data[UF_USERINFO].len = 0;
					u->field_set |= (1 << UF_USERINFO);
				}
				u->field_data[UF_USERINFO].len++;
				break;

			default:
				break;
		}
		s = new_s;
	}

	/* Make sure we don't end somewhere unexpected */
	switch (s)
	{
		case s_http_host_start:
		case s_http_host_v6_start:
		case s_http_host_v6:
		case s_http_host_port_start:
		case s_http_userinfo:
		case s_http_userinfo_start:
			return 1;
		default:
			break;
	}

	return 0;
}

int
http_parser_parse_url(const char *buf, DWORD buflen, int is_connect,
	struct http_parser_url *u)
{
	enum state s;
	const char *p;
	enum http_parser_url_fields uf, old_uf;
	int found_at = 0;

	u->port = u->field_set = 0;
	s = is_connect ? s_req_server_start : s_req_spaces_before_url;
	uf = old_uf = UF_MAX;

	for (p = buf; p < buf + buflen; p++)
	{
		s = parse_url_char(s, *p);

		/* Figure out the next field that we're operating on */
		switch (s)
		{
			case s_dead:
				return 1;

				/* Skip delimeters */
			case s_req_schema_slash:
			case s_req_schema_slash_slash:
			case s_req_server_start:
			case s_req_query_string_start:
			case s_req_fragment_start:
				continue;

			case s_req_schema:
				uf = UF_SCHEMA;
				break;

			case s_req_server_with_at:
				found_at = 1;

				[[fallthrough]];
			case s_req_server:
				uf = UF_HOST;
				break;

			case s_req_path:
				uf = UF_PATH;
				break;

			case s_req_query_string:
				uf = UF_QUERY;
				break;

			case s_req_fragment:
				uf = UF_FRAGMENT;
				break;

			default:
				assert(!"Unexpected state");
				return 1;
		}

		/* Nothing's changed; soldier on */
		if (uf == old_uf)
		{
			u->field_data[uf].len++;
			continue;
		}

		u->field_data[uf].off = p - buf;
		u->field_data[uf].len = 1;

		u->field_set |= (1 << uf);
		old_uf = uf;
	}

	/* host must be present if there is a schema */
	/* parsing http:///toto will fail */
	if ((u->field_set & ((1 << UF_SCHEMA) | (1 << UF_HOST))) != 0)
	{
		if (http_parse_host(buf, u, found_at) != 0)
		{
			return 1;
		}
	}

	/* CONNECT requests can only contain "hostname:port" */
	if (is_connect && u->field_set != ((1 << UF_HOST) | (1 << UF_PORT)))
	{
		return 1;
	}

	if (u->field_set & (1 << UF_PORT))
	{
		/* Don't bother with endp; we've already validated the string */
		unsigned long v = strtoul(buf + u->field_data[UF_PORT].off, NULL, 10);

		/* Ports have a max value of 2^16 */
		if (v > 0xffff)
		{
			return 1;
		}

		u->port = (uint16_t) v;
	}

	return 0;
}

void HTTPParser::Pause(int paused)
{
	/* Users should only be pausing/unpausing a parser that is not in an error
	 * state. In non-debug builds, there's not much that we can do about this
	 * other than ignore it.
	 */
	if (HTTP_PARSER_ERRNO(this) == HPE_OK ||
		HTTP_PARSER_ERRNO(this) == HPE_PAUSED)
	{
		SET_ERRNO((paused) ? HPE_PAUSED : HPE_OK);
	} else
	{
		assert(0 && "Attempting to pause parser in error state");
	}
}

int HTTPParser::IsFinal()
{
	return state == s_message_done;
}
unsigned char HTTPParser::GetUpgrade() const
{
	return upgrade;
}
unsigned char HTTPParser::GetHttpErrno() const
{
	return http_errno;
}
unsigned char HTTPParser::GetMethod() const
{
	return method;
}
unsigned short HTTPParser::GetHttpMinor() const
{
	return http_minor;
}
unsigned short HTTPParser::GetHttpMajor() const
{
	return http_major;
}

unsigned long HTTPParser::Version()
{
	return HTTP_PARSER_VERSION_MAJOR * 0x10000 |
		HTTP_PARSER_VERSION_MINOR * 0x00100 |
		HTTP_PARSER_VERSION_PATCH * 0x00001;
}
