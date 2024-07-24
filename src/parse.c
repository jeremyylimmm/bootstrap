#include <ctype.h>
#include <stdio.h>
#include <string.h>

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

static int check_keyword(char* start, char* cur, char* string, int kind) {
    size_t length = cur-start;

    if (length == strlen(string) && memcmp(start, string, length) == 0) {
        return kind;
    }
    else {
        return TOKEN_IDENT;
    }
}

static int ident_kind(char* start, char* cur) {
    switch (start[0]) {
        case 'i':
            return check_keyword(start, cur, "if", TOKEN_KW_IF);
        case 'e':
            return check_keyword(start, cur, "else", TOKEN_KW_ELSE);
        case 'w':
            return check_keyword(start, cur, "while", TOKEN_KW_WHILE);
        case 'r':
            return check_keyword(start, cur, "return", TOKEN_KW_RETURN);
    }

    return TOKEN_IDENT;
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

                kind = ident_kind(start, p->lexer_char);
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

static Statement parse_if(Parser* p);
static Statement parse_while(Parser* p);
static Statement parse_return(Parser* p);

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

            case TOKEN_KW_IF: {
                Statement stmt = parse_if(p);
                if (stmt.failure) { return ERR(); }
            } break;

            case TOKEN_KW_WHILE: {
                Statement stmt = parse_while(p);
                if (stmt.failure) { return ERR(); }
            } break;

            case TOKEN_KW_RETURN: {
                Statement stmt = parse_return(p);
                if (stmt.failure) { return ERR(); }
            } break;
        }
    }

    REQUIRE_STMT(p, '}', "missing a closing } here");

    return OK(block_expr);
}

static Statement parse_if(Parser* p) {
    REQUIRE_STMT(p, TOKEN_KW_IF, "expecting an if statement here");

    // Parse the predicate
    HIR_Node* predicate = parse_expr(p);
    if (!predicate) { return ERR(); }
    // Insert branch
    HIR_Node* branch = new_node(p, HIR_OP_BRANCH);

    Statement stmt;

    // Parse then body
    HIR_Block* loc_then = new_block(p);
    stmt = parse_block(p);
    if (stmt.failure) { return ERR(); }
    // Insert jump to end
    HIR_Node* jump_then = new_node(p, HIR_OP_JUMP);

    // New block
    HIR_Block* loc_else = new_block(p);
    HIR_Block* loc_end = loc_else;

    if (peek(p).kind == TOKEN_KW_ELSE) {
        lex(p);

        stmt = parse_block(p);
        if (stmt.failure) { return ERR(); }

        // Insert jump to end
        HIR_Node* jump_else = new_node(p, HIR_OP_JUMP);
        loc_end = new_block(p);

        jump_else->as.jump.loc = loc_end;
    }

    // Specify jump locations

    branch->as.branch.predicate = predicate;
    branch->as.branch.loc_then = loc_then;
    branch->as.branch.loc_else = loc_else;

    jump_then->as.jump.loc = loc_end;

    return OK(0);
}

static Statement parse_while(Parser* p) {
    REQUIRE_STMT(p, TOKEN_KW_WHILE, "expecting a while loop here");

    // Insert jump to start
    HIR_Node* init_jump = new_node(p, HIR_OP_JUMP);
    HIR_Block* start = new_block(p);

    // Parse predicate
    HIR_Node* predicate = parse_expr(p);
    if (!predicate) { return ERR(); }
    // Insert branch
    HIR_Node* branch = new_node(p, HIR_OP_BRANCH);

    // Parse loop body
    HIR_Block* loc_then = new_block(p);
    Statement stmt = parse_block(p);
    if (stmt.failure) { return ERR(); }
    // Insert jump to top
    HIR_Node* loop_jump = new_node(p, HIR_OP_JUMP);

    HIR_Block* end = new_block(p);

    // Specify jump locations

    init_jump->as.jump.loc = start;

    branch->as.branch.predicate = predicate;
    branch->as.branch.loc_then = loc_then;
    branch->as.branch.loc_else = end;

    loop_jump->as.jump.loc = start;

    return OK(0);
}

static Statement parse_return(Parser* p) {
    REQUIRE_STMT(p, TOKEN_KW_RETURN, "expected a return statement here");

    HIR_Node* value = 0;

    if (peek(p).kind != ';') {
        value = parse_expr(p);
        if (!value) { return ERR(); }
    }

    REQUIRE_STMT(p, ';', "ill-formed return statement");

    HIR_Node* ret = new_node(p, HIR_OP_RET);
    ret->as.ret.value = value;

    new_block(p);

    return OK(0);
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

    if (stmt.expr) {
        HIR_Node* ret = new_node(&p, HIR_OP_RET);
        ret->as.ret.value = stmt.expr;
    }

    HIR_Proc* proc = arena_type(arena, HIR_Proc);
    proc->control_flow_head = control_flow_head;

    return proc;
}