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

    HIR_Block* cur_block;
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
                while (isident(*p->lexer_char)) {
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

static bool match(Parser* p, int kind, char* message) {
    Token tok = peek(p);
    
    if (tok.kind != kind) {
        report_error_token(p->source, p->source_path, tok, message);
        return false;
    }

    lex(p);
    return true;
}

#define REQUIRE(p, kind, message) do { if (!match(p, kind, message)) { return 0; } } while (false)

static bool until(Parser* p, int kind) {
    return peek(p).kind != kind && peek(p).kind != TOKEN_EOF;
}

static HIR_Node* new_node(Parser* p, HIR_Op op) {
    assert(op);
    HIR_Node* node = arena_type(p->arena, HIR_Node);
    node->op = op;
    hir_append(p->cur_block, node);
    return node;
}

static HIR_Block* new_block(Parser* p) {
    HIR_Block* block = arena_type(p->arena, HIR_Block);
    if (p->cur_block) { p->cur_block->next = block; }
    p->cur_block = block;
    return block;
}

static HIR_Node* parse_expr(Parser* p);

static HIR_Node* parse_primary(Parser* p) {
    Token tok = peek(p);

    switch (tok.kind) {
        case TOKEN_INT_LITERAL: {
            lex(p);
            int128_t value = int128_zero();
            for (int i = 0; i < tok.length; ++i) {
                int digit = tok.start[i] - '0';
                value = int128_mul(value, int128_from_uint64(10));
                value = int128_add(value, int128_from_uint64(digit));
            }
            HIR_Node* node = new_node(p, HIR_OP_INT_CONST);
            node->as.int_const = value;
            return node;
        }
        case '{':
            return parse_expr(p);
    }

    report_error_token(p->source, p->source_path, tok, "expected an expression here");
    return 0;
}

static int binary_prec(Token op) {
    switch (op.kind) {
        default: return 0;
        case '*':
        case '/':
            return 20;
        case '+':
        case '-':
            return 10;
    }
}

static HIR_Op binary_op(Token op) {
    switch (op.kind) {
        default: return HIR_OP_ILLEGAL;
        case '*': return HIR_OP_MUL;
        case '/': return HIR_OP_DIV;
        case '+': return HIR_OP_ADD;
        case '-': return HIR_OP_SUB;
    }
}

static HIR_Node* parse_binary(Parser* p, int caller_prec) {
    HIR_Node* left = parse_primary(p);
    if (!left) { return 0; }

    while (binary_prec(peek(p)) > caller_prec) {
        Token op = lex(p);

        HIR_Node* right = parse_binary(p, binary_prec(op));
        if (!right) { return 0; }

        HIR_Node* bin = new_node(p, binary_op(op));
        bin->as.binary[0] = left;
        bin->as.binary[1] = right;

        left = bin;
    }

    return left;
}

static HIR_Node* parse_natural_expr(Parser* p) {
    return parse_binary(p, 0);
}

typedef struct {
    HIR_Node* expr;
    bool failure;
} Statement;

#define OK(e) ((Statement){.expr=(e)})
#define ERR() ((Statement){.failure=true})

#define REQUIRE_STMT(p, kind, msg) do { if (!match(p, kind, msg)) { return ERR(); } } while (false)

static Statement parse_statement(Parser* p);

static Statement parse_block(Parser* p) {
    REQUIRE_STMT(p, '{', "expected a {} block here");

    HIR_Node* block_expr = 0;

    while(until(p, '}')) {
        Token tok = peek(p);

        switch (tok.kind) {
            default: {
                HIR_Node* expr = parse_natural_expr(p);
                if (!expr) { return ERR(); }

                switch (peek(p).kind) {
                    case ';':
                        lex(p);
                        break;
                    case '}':
                        block_expr = expr;
                        break;
                    default:
                        report_error_token(p->source, p->source_path, peek(p), "ill-formed expression");
                        return ERR();
                }
            } break;

            case '{': {
                Statement stmt = parse_block(p);
                if(stmt.failure) { return ERR(); }
                if (stmt.expr && peek(p).kind == '}') {
                    block_expr = stmt.expr;
                }
            } break;
        }
    }

    REQUIRE_STMT(p, '}', "missing a closing } here");

    return OK(block_expr);
}

static HIR_Node* parse_expr(Parser* p) {
    Token tok = peek(p);

    switch (tok.kind) {
        default:
            return parse_natural_expr(p);
        case '{': {
            Statement stmt = parse_block(p);
            if (stmt.failure) { return 0; }
            if (!stmt.expr) {
                report_error_token(p->source, p->source_path, tok, "block does not produce a value");
                return 0;
            }
            return stmt.expr;
        } break;
    }
}

#undef OK
#undef ERR

HIR_Proc* parse_source(Arena* arena, char* source, char* source_path) {
    Parser p = {
        .arena = arena,
        .source = source,
        .source_path = source_path,
        
        .lexer_char = source,
        .lexer_line = 1
    };

    HIR_Block* control_flow_head = new_block(&p);

    Statement stmt = parse_block(&p);
    if (stmt.failure) { return 0; }

    HIR_Proc* proc = arena_type(arena, HIR_Proc);
    proc->control_flow_head = control_flow_head;

    return proc;
}