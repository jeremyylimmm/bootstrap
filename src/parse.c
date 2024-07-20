#include <ctype.h>
#include <stdio.h>

#include "frontend.h"

typedef struct {
    Arena* arena;
    char* source;
    char* source_path;

    char* lexer_char;
    int lexer_line;
    Token lexer_cache;
} Parser;

static bool isident(char c) {
    return isalpha(c) || c == '_';
}

static Token lex(Parser* p) {
    if (p->lexer_cache.start) {
        Token token = p->lexer_cache;
        p->lexer_cache.start = 0;
        return token;
    }

    while (isspace(*p->lexer_char)) {
        if (*p->lexer_char == '\n') {
            ++p->lexer_line;
        }

        ++p->lexer_char;
    }

    char* start = p->lexer_char++;
    int start_line = p->lexer_line;
    int kind = *start;

    switch (*start) {
        case '\0':
            --p->lexer_char;
            kind = TOKEN_EOF;
            break;
        default:
            if (isdigit(*start)) {
                while (isdigit(*p->lexer_char)) {
                    ++p->lexer_char;
                }

                kind = TOKEN_INT_LITERAL;
            }
            else if (isident(*start)) {
                while (isident(*start)) {
                    ++p->lexer_char;
                }

                kind = TOKEN_IDENT;
            }
            break;
    }

    return (Token) {
        .kind = kind,
        .line = start_line,
        .length = (int)(p->lexer_char - start),
        .start = start,
    };
}

static Token peek(Parser* p) {
    if (!p->lexer_cache.start) {
        p->lexer_cache = lex(p);
    }

    return p->lexer_cache;
}

static AST* new_ast(Parser* p, AST_Kind kind) {
    assert("illegal ast kind" && kind);
    AST* node = arena_type(p->arena, AST);
    node->kind = kind;
    return node;
}

static AST* parse_primary(Parser* p) {
    Token tok = peek(p);

    switch (tok.kind) {
        case TOKEN_INT_LITERAL: {
            lex(p);

            int128_t value = int128_zero();

            for (int i = 0; i < tok.length; ++i) {
                value = int128_mul(value, int128_from_int64(10));
                value = int128_add(value, int128_from_int64(tok.start[i] - '0'));
            }

            AST* node = new_ast(p, AST_INT_CONST);
            node->as.int_const = value;

            return node;
        } break;

        default: {
            report_error_token(p->source, p->source_path, tok, "expected an expression");
            return 0;
        }
    }
}

static int binary_prec(Token op) {
    switch (op.kind) {
        default:
            return 0;
        case '*':
        case '/':
            return 20;
        case '+':
        case '-':
            return 10;
    }
}

static AST* parse_binary(Parser* p, int outer_prec) {
    AST* left = parse_primary(p);
    if (!left) { return 0; }

    while (binary_prec(peek(p)) > outer_prec) {
        Token op = lex(p);

        AST* right = parse_binary(p, binary_prec(op));
        if (!right) { return 0; }

        AST_Kind kind = AST_ILLEGAL;
        switch (op.kind) {
            case '*':
                kind = AST_MUL;
                break;
            case '/':
                kind = AST_DIV;
                break;
            case '+':
                kind = AST_ADD;
                break;
            case '-':
                kind = AST_SUB;
                break;
        }

        AST* bin = new_ast(p, kind);
        bin->as.binary[0] = left;
        bin->as.binary[1] = right;

        left = bin;
    }

    return left;
}

static AST* parse_any(Parser* p) {
    return parse_binary(p, 0);
}

AST* parse_source(Arena* arena, char* source, char* source_path) {
    Parser p = {
        .arena = arena,
        .source = source,
        .source_path = source_path,
        
        .lexer_char = source,
        .lexer_line = 1
    };

    AST* result = parse_any(&p);
    return result;
}