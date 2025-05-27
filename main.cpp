#include <iostream>
#include <vector>
#include <map>
#include <stdio.h>
#include <stdint.h>
#include <cstring>
#include <cassert>

using namespace std;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

void repl();

#define EC_8BITCOLOR(colorstr, boldstr) \
    "\x1B[" boldstr ";38;5;" colorstr "m"
const char* g_red_color = EC_8BITCOLOR("196", "0");
const char* g_reset_color = "\x1B[0m";

enum TokenKind {
    TK_IDENT,
    TK_NUMBER,
    TK_STRING,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_FSLASH,
    TK_SEMICOLON,
    TK_LPAREN,
    TK_RPAREN,
    TK_EQUAL,
    TK_EQ_EQ,
    TK_NOT_EQ,
    TK_NOT,
    TK_EOF,
};

class Token {
public:
    TokenKind kind;
    int pos;
    union {
        double number;
        char* str;
        char* ident;
    };

    Token(TokenKind kind, int pos) {
        this->kind = kind;
        this->pos = pos+1;
    }
};

enum NodeKind {
    NK_EXPR_STMT,
    NK_ASSIGN_STMT,
    NK_BINARY,
    NK_UNARY,
    NK_NUMBER,
    NK_STRING,
    NK_IDENT,
};

enum ValueKind {
    VK_NUMBER,
    VK_STRING,
    VK_BOOL,
    VK_NULL,
};

class Value {
public:
    ValueKind kind;
    union {
        double number;
        char* str;
        bool boolean;
    };

    static Value make_number(double num) {
        Value v;
        v.kind = VK_NUMBER;
        v.number = num;
        return v;
    }

    static Value make_string(const char* str) {
        Value v;
        v.kind = VK_STRING;
        v.str = new char[strlen(str)+1];
        strcpy(v.str, str);
        return v;
    }

    static Value make_bool(bool b) {
        Value v;
        v.kind = VK_BOOL;
        v.boolean = b;
        return v;
    }

    static Value make_null() {
        Value v;
        v.kind = VK_NULL;
        return v;
    }

    void print() {
        switch (kind) {
            case VK_NUMBER:
                cout << number;
                break;
            case VK_STRING:
                cout << '"' << str << '"';
                break;
            case VK_BOOL:
                cout << (boolean ? "true" : "false");
                break;
            case VK_NULL:
                cout << "(null)";
                break;
        }
    }

    std::string stringify_kind() {
        switch (kind) {
            case VK_NUMBER: return "number";
            case VK_STRING: return "string";
            case VK_BOOL: return "boolean";
        }
        assert(0);
        return "";
    }
};

class AstNode {
public:
    NodeKind kind;
    Token* mark;
    union {
        struct {
            AstNode* left, *right;
            char op;
        } bin;

        struct {
            AstNode* child;
            char op;
        } un;

        AstNode* expr_stmt;

        struct {
            AstNode* left, *right;
        } assign;

        double num;
        char* str;
        char* ident;
    };

    static AstNode* make_expr_stmt(AstNode* child, Token* mark) {
        AstNode* n = new AstNode;
        n->kind = NK_EXPR_STMT;
        n->mark = mark;
        n->expr_stmt = child;
        return n;
    }

    static AstNode* make_assign_stmt(AstNode* left, AstNode* right, Token* mark) {
        AstNode* n = new AstNode;
        n->kind = NK_ASSIGN_STMT;
        n->mark = mark;
        n->assign.left = left;
        n->assign.right = right;
        return n;
    }

    static AstNode* make_binary(AstNode* left, AstNode* right, char op, Token* mark) {
        AstNode* n = new AstNode;
        n->kind = NK_BINARY;
        n->mark = mark;
        n->bin.left = left;
        n->bin.right = right;
        n->bin.op = op;
        return n;
    }

    static AstNode* make_unary(AstNode* child, char op, Token* mark) {
        AstNode* n = new AstNode;
        n->kind = NK_UNARY;
        n->mark = mark;
        n->un.child = child;
        n->un.op = op;
        return n;
    }

    static AstNode* make_number(Token* token) {
        AstNode* n = new AstNode;
        n->kind = NK_NUMBER;
        n->mark = token;
        n->num = token->number;
        return n;
    }

    static AstNode* make_string(Token* token) {
        AstNode* n = new AstNode;
        n->kind = NK_STRING;
        n->mark = token;
        n->str = token->str;
        return n;
    }

    static AstNode* make_ident(Token* token) {
        AstNode* n = new AstNode;
        n->kind = NK_IDENT;
        n->mark = token;
        n->ident = token->ident;
        return n;
    }
};

void error(const std::string& msg, int at) {
    cout << g_red_color << "(input):1:" << at << ": error: " << g_reset_color << msg << endl;
    repl();
}

void error(const std::string& msg, Token* token) {
    error(msg, token->pos);
}

class Interpreter {
public:
    void interpret_nodes(std::vector<AstNode*> nodes);
    Value interpret(AstNode* node);

    std::map<std::string, Value> lookup;
};

void Interpreter::interpret_nodes(std::vector<AstNode*> nodes) {
    for (size_t i = 0; i < nodes.size(); i++) {
        Value v = interpret(nodes[i]);
        v.print();
        cout << endl;
    }
}

Value Interpreter::interpret(AstNode* node) {
    switch (node->kind) {
        case NK_EXPR_STMT: {
            Value v = interpret(node->expr_stmt);
            return v;
        };

        case NK_ASSIGN_STMT: {
            Value child = interpret(node->assign.right);
            lookup[std::string(node->assign.left->ident)] = child;
            return Value::make_null();
        } break;

        case NK_BINARY: {
            Value a = interpret(node->bin.left);
            Value b = interpret(node->bin.right);
            if (a.kind != b.kind) {
                error("type mismatch: " + a.stringify_kind() + " and " + b.stringify_kind(), node->mark);
            }

            if (node->bin.op == '=' || node->bin.op == '!') {
                bool res;
                switch (a.kind) {
                    case VK_NUMBER: {
                        switch (node->bin.op) {
                            case '=': res = a.number == b.number; break;
                            case '!': res = a.number != b.number; break;
                        }
                    } break;

                    case VK_BOOL: {
                        switch (node->bin.op) {
                            case '=': res = a.boolean == b.boolean; break;
                            case '!': res = a.boolean != b.boolean; break;
                        }
                    } break;

                    case VK_STRING: {
                        if (strcmp(a.str, b.str) == 0) res = true;
                        else res = false;
                    } break;
                }
                return Value::make_bool(res);
            } else {
                switch (a.kind) {
                    case VK_NUMBER: {
                        double res;
                        switch (node->bin.op) {
                            case '+': res = a.number + b.number; break;
                            case '-': res = a.number - b.number; break;
                            case '*': res = a.number * b.number; break;
                            case '/': {
                                if (b.number == 0.0) {
                                    error("Division by zero", node->mark);
                                }
                                res = a.number / b.number;
                            } break;
                        }
                        return Value::make_number(res);
                    } break;

                    case VK_STRING: {
                        if (node->bin.op != '+') {
                            error(std::string("invalid operation with strings: ") + "`" + node->bin.op + "`", node->mark);
                        }
                        return Value::make_string((std::string(a.str) + std::string(b.str)).c_str());
                    } break;

                    case VK_BOOL: {
                        error("cannot apply arithmetic to booleans", node->mark);
                    } break;
                }
            }
        } break;

        case NK_UNARY: {
            Value child = interpret(node->un.child);
            if (child.kind != VK_NUMBER) {
                error("unary `-` cannot be applied to " + child.stringify_kind(), node->mark);
            }
            return Value::make_number(-child.number);
        } break;

        case NK_NUMBER: {
            return Value::make_number(node->num);
        } break;

        case NK_STRING: {
            return Value::make_string(node->str);
        } break;

        case NK_IDENT: {
            std::string ident = std::string(node->ident);
            if (ident == "true") return Value::make_bool(true);
            else if (ident == "false") return Value::make_bool(false);

            auto it = lookup.find(ident);
            if (it == lookup.end()) {
                error("unresolved symbol `" + ident + '`', node->mark);
            }
            return it->second;
        } break;
    }
    assert(0);
    return Value();
}

class Parser {
public:
    void parse(std::vector<Token*>* tokens);

    std::vector<AstNode*> nodes;

private:
    std::vector<Token*>* tokens;
    Token* current;
    Token* prev;
    size_t token_idx;

    AstNode* parse_stmt();
    AstNode* parse_expr();
    AstNode* parse_cmp_binop();
    AstNode* parse_add_binop();
    AstNode* parse_mul_binop();
    AstNode* parse_unop();
    AstNode* parse_atom();

    void goto_next_token() {
        if (token_idx < (*tokens).size()) {
            token_idx++;
            prev = current;
            current = (*tokens)[token_idx];
        }
    }

    void check_eof() {
        if (current->kind == TK_EOF) {
            error("unexpected end of line", current);
        }
    }

    bool match(TokenKind kind) {
        if (current->kind == kind) {
            goto_next_token();
            return true;
        }
        return false;
    }

    bool expect(TokenKind kind, const std::string& thing) {
        if (!match(kind)) {
            error("expected " + thing, current);
            return false;
        }
        return true;
    }
};

void Parser::parse(std::vector<Token*>* tokens) {
    this->tokens = tokens;
    current = (*tokens)[0];
    prev = nullptr;
    token_idx = 0;

    while (current->kind != TK_EOF) {
        AstNode* node = parse_stmt();
        if (node) nodes.push_back(node);
    }
}

AstNode* Parser::parse_stmt() {
    AstNode* node = parse_expr();
    if (match(TK_EQUAL)) {
        if (node->kind != NK_IDENT) {
            error("only identifiers can be assigned to", prev);
        }
        Token* mark = prev;
        AstNode* right = parse_expr();
        //expect(TK_SEMICOLON, "semicolon");
        return AstNode::make_assign_stmt(node, right, mark);
    } else {
        //expect(TK_SEMICOLON, "semicolon");
        return AstNode::make_expr_stmt(node, prev);
    }
    assert(0);
    return nullptr;
}

AstNode* Parser::parse_expr() {
    return parse_cmp_binop();
}

AstNode* Parser::parse_cmp_binop() {
    AstNode* left = parse_add_binop();
    while (match(TK_EQ_EQ) || match(TK_NOT_EQ)) {
        char op;
        Token* op_tok = prev;
        switch (op_tok->kind) {
            case TK_EQ_EQ:   op = '='; break;
            case TK_NOT_EQ:  op = '!'; break;
        }
        AstNode* right = parse_add_binop();
        left = AstNode::make_binary(left, right, op, op_tok);
    }
    return left;
}

AstNode* Parser::parse_add_binop() {
    AstNode* left = parse_mul_binop();
    while (match(TK_PLUS) || match(TK_MINUS)) {
        char op;
        Token* op_tok = prev;
        switch (op_tok->kind) {
            case TK_PLUS:  op = '+'; break;
            case TK_MINUS: op = '-'; break;
        }
        AstNode* right = parse_mul_binop();
        left = AstNode::make_binary(left, right, op, op_tok);
    }
    return left;
}

AstNode* Parser::parse_mul_binop() {
    AstNode* left = parse_unop();
    while (match(TK_STAR) || match(TK_FSLASH)) {
        char op;
        Token* op_tok = prev;
        switch (op_tok->kind) {
            case TK_STAR:   op = '*'; break;
            case TK_FSLASH: op = '/'; break;
        }
        AstNode* right = parse_unop();
        left = AstNode::make_binary(left, right, op, op_tok);
    }
    return left;
}

AstNode* Parser::parse_unop() {
    if (match(TK_MINUS)) {
        char op;
        Token* op_tok = prev;
        switch (op_tok->kind) {
            case TK_MINUS: op = '-'; break;
        }
        AstNode* child = parse_unop();
        return AstNode::make_unary(child, op, op_tok);
    }
    return parse_atom();
}

AstNode* Parser::parse_atom() {
    if (match(TK_NUMBER)) {
        return AstNode::make_number(prev);
    } else if (match(TK_STRING)) {
        return AstNode::make_string(prev);
    } else if (match(TK_LPAREN)) {
        AstNode* child = parse_expr();
        expect(TK_RPAREN, "closing parenthesis");
        return child;
    } else if (match(TK_IDENT)) {
        return AstNode::make_ident(prev);
    }
    error("invalid expression", current);
    return nullptr;
}

class Lexer {
public:
    std::vector<Token*> tokens;
    void lex(std::string& input);
};

void Lexer::lex(std::string& input) {
    size_t i = 0;
    for (i = 0; i < input.size(); i++) {
        switch (input[i]) {
            case '+': {
                tokens.push_back(new Token(TK_PLUS, i));
            } break;

            case '-': {
                tokens.push_back(new Token(TK_MINUS, i));
            } break;

            case '*': {
                tokens.push_back(new Token(TK_STAR, i));
            } break;

            case '/': {
                tokens.push_back(new Token(TK_FSLASH, i));
            } break;

            case ';': {
                tokens.push_back(new Token(TK_SEMICOLON, i));
            } break;

            case '(': {
                tokens.push_back(new Token(TK_LPAREN, i));
            } break;

            case ')': {
                tokens.push_back(new Token(TK_RPAREN, i));
            } break;

            case '=': {
                if (input[i+1] == '=') {
                    tokens.push_back(new Token(TK_EQ_EQ, i));
                    i++;
                } else {
                    tokens.push_back(new Token(TK_EQUAL, i));
                }
            } break;

            case '!': {
                if (input[i+1] == '=') {
                    tokens.push_back(new Token(TK_NOT_EQ, i));
                    i++;
                } else {
                    tokens.push_back(new Token(TK_NOT, i));
                }
            } break;

            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': case '0': {
                size_t start = i;
                while (isdigit(input[i])) i++;
                if (input[i] == '.') {
                    i++;
                    if (!isdigit(input[i])) {
                        error("Expected number after `.`", i);
                    }
                    while (isdigit(input[i])) i++;
                }
                std::string numstr = input.substr(start, i-start);
                double num = atof(numstr.c_str());
                Token* t = new Token(TK_NUMBER, start);
                t->number = num;
                tokens.push_back(t);
                i--; // to counteract i++ in for loop
            } break;

            case 'a': case 'b': case 'c': case 'd': case 'e':
            case 'f': case 'g': case 'h': case 'i': case 'j':
            case 'k': case 'l': case 'm': case 'n': case 'o':
            case 'p': case 'q': case 'r': case 's': case 't':
            case 'u': case 'v': case 'w': case 'x': case 'y':
            case 'z': case 'A': case 'B': case 'C': case 'D':
            case 'E': case 'F': case 'G': case 'H': case 'I':
            case 'J': case 'K': case 'L': case 'M': case 'N':
            case 'O': case 'P': case 'Q': case 'R': case 'S':
            case 'T': case 'U': case 'V': case 'W': case 'X':
            case 'Y': case 'Z': case '_': {
                size_t start = i;
                i++;
                while (isalpha(input[i]) || isdigit(input[i])) i++;
                std::string substr = input.substr(start, i-start);
                char* str = new char[substr.size()+1];
                strcpy(str, substr.c_str());
                str[substr.size()] = '\0';
                Token* t = new Token(TK_IDENT, start);
                t->ident = str;
                tokens.push_back(t);
                i--; // to counteract i++ in for loop
            } break;

            case '"': {
                size_t start = i;
                i++;
                while (input[i] != '"') {
                    if (input[i] == '\n' || input[i] == '\0') {
                        error("Unexpected end of line in string", i);
                    }
                    i++;
                }
                std::string substr = input.substr(start+1, i-start-1);
                char* str = new char[substr.size()+1];
                strcpy(str, substr.c_str());
                str[substr.size()] = '\0';
                Token* t = new Token(TK_STRING, start);
                t->str = str;
                tokens.push_back(t);
            } break;

            default: break;
        }
    }
    tokens.push_back(new Token(TK_EOF, i));
}

Interpreter i;

void repl() {
    cout << "> ";
    std::string input;
    char ch;
    while ((ch = getchar()) != '\n' && ch != EOF) {
        input.push_back(ch);
    }
    input.push_back('\0');

    Lexer l;
    l.lex(input);
    /*
    for (int i = 0; i < l.tokens.size(); i++) {
        cout << "TYPE: " << l.tokens[i]->kind << ", POS: " << l.tokens[i]->pos << endl;
    }
    */

    Parser p;
    p.parse(&l.tokens);

    i.interpret_nodes(p.nodes);
}

int main() {
    while (true) {
        repl();
    }
}
