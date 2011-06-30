#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "parser.h"

typedef struct log_entry_s log_entry_t;

struct log_entry_s {
    redis_protocol_t obj;

    /* string_t specifics */
    const char *string_buf;
    size_t string_len;

    /* array_t specifics */
    size_t array_len;

    /* integer_t specifics */
    int64_t integer_value;
};

#define CB_LOG_SIZE 10
static log_entry_t cb_log[CB_LOG_SIZE];
static int cb_log_idx = 0;

#define reset_cb_log() do {              \
    memset(cb_log, 0, sizeof(cb_log));     \
    cb_log_idx = 0;                      \
} while(0)

#define copy_cb_log(dst) do {            \
    memcpy(dst, cb_log, sizeof(cb_log));   \
} while(0)

int on_string(redis_parser_t *parser, redis_protocol_t *obj, const char *buf, size_t len) {
    log_entry_t tmp = {
        .obj = *obj,
        .string_buf = buf,
        .string_len = len
    };
    cb_log[cb_log_idx++] = tmp;
    return 1;
}

int on_array(redis_parser_t *parser, redis_protocol_t *obj, size_t len) {
    log_entry_t tmp = {
        .obj = *obj,
        .array_len = len
    };
    cb_log[cb_log_idx++] = tmp;
    return 1;
}

int on_integer(redis_parser_t *parser, redis_protocol_t *obj, int64_t value) {
    log_entry_t tmp = {
        .obj = *obj,
        .integer_value = value
    };
    cb_log[cb_log_idx++] = tmp;
    return 1;
}

int on_nil(redis_parser_t *parser, redis_protocol_t *obj) {
    log_entry_t tmp = {
        .obj = *obj
    };
    cb_log[cb_log_idx++] = tmp;
    return 1;
}

static redis_parser_cb_t callbacks = {
    &on_string,
    &on_array,
    &on_integer,
    &on_nil
};

redis_parser_t *new_parser(void) {
    redis_parser_t *parser = malloc(sizeof(redis_parser_t));
    redis_parser_init(parser, &callbacks);
    return parser;
}

void free_parser(redis_parser_t *parser) {
    free(parser);
}

void test_char_by_char(redis_protocol_t *ref, const char *buf, size_t len) {
    redis_parser_t *p = new_parser();
    redis_protocol_t *res;
    size_t i;

    for (i = 1; i < (len-1); i++) {
        reset_cb_log();
        redis_parser_init(p, &callbacks);

        /* Slice 1 */
        assert(i == redis_parser_execute(p, &res, buf, i));
        assert(NULL == res); /* no result */

        /* Slice 2 */
        assert(len-i == redis_parser_execute(p, &res, buf+i, len-i));
        assert(NULL != res);

        /* Compare result with reference */
        assert(memcmp(ref, res, sizeof(redis_protocol_t)) == 0);
    }

    free_parser(p);
}

void test_string(void) {
    redis_parser_t *p = new_parser();
    redis_protocol_t *res;

    const char *buf = "$5\r\nhello\r\n";
    size_t len = 11;

    /* Parse and check resulting protocol_t */
    reset_cb_log();
    assert(redis_parser_execute(p, &res, buf, len) == len);
    assert(res != NULL);
    assert(res->type == REDIS_STRING_T);
    assert(res->poff == 0);
    assert(res->plen == 11);
    assert(res->coff == 4);
    assert(res->clen == 5);

    /* Check callbacks */
    assert(cb_log_idx == 1);
    assert(cb_log[0].string_buf == buf+4);
    assert(cb_log[0].string_len == 5);

    /* Chunked check */
    test_char_by_char(res, buf, len);

    free_parser(p);
}

void test_array(void) {
    redis_parser_t *p = new_parser();
    redis_protocol_t *res;

    const char *buf =
        "*2\r\n"
        "$5\r\nhello\r\n"
        "$5\r\nworld\r\n";
    size_t len = 26;

    /* Parse and check resulting protocol_t */
    reset_cb_log();
    assert(redis_parser_execute(p, &res, buf, len) == len);
    assert(res != NULL);
    assert(res->type == REDIS_ARRAY_T);
    assert(res->poff == 0);
    assert(res->plen == 26);
    assert(res->coff == 0);
    assert(res->clen == 0);

    /* Check callbacks */
    assert(cb_log_idx == 3);

    assert(cb_log[0].obj.poff == 0);
    assert(cb_log[0].obj.plen == 4);
    assert(cb_log[0].array_len == 2);

    assert(cb_log[1].obj.poff == 4);
    assert(cb_log[1].obj.plen == 4+5+2);
    assert(cb_log[1].obj.coff == 4+4);
    assert(cb_log[1].obj.clen == 5);
    assert(cb_log[1].string_buf == buf+4+4);
    assert(cb_log[1].string_len == 5);

    assert(cb_log[2].obj.poff == 4+11);
    assert(cb_log[2].obj.plen == 4+5+2);
    assert(cb_log[2].obj.coff == 4+11+4);
    assert(cb_log[2].obj.clen == 5);
    assert(cb_log[2].string_buf == buf+4+11+4);
    assert(cb_log[2].string_len == 5);

    /* Chunked check */
    test_char_by_char(res, buf, len);

    free_parser(p);
}

int main(int argc, char **argv) {
    test_string();
    test_array();
    return 0;
}