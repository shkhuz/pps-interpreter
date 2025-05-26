#include <iostream>
#include <vector>
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

#define EC_8BITCOLOR(colorstr, boldstr) \
    "\x1B[" boldstr ";38;5;" colorstr "m"
const char* g_red_color = EC_8BITCOLOR("196", "0");
const char* g_reset_color = "\x1B[0m";

enum TokenKind {
    TK_NUMBER,
    TK_STRING,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_FSLASH,
    TK_SEMICOLON,
    TK_LPAREN,
    TK_RPAREN,
    TK_EOF,
};

class Token {
public:
    TokenKind kind;
    int pos;
    union {
        double number;
        char* str;
    };

    Token(TokenKind kind, int pos) {
        this->kind = kind;
        this->pos = pos+1;
    }
};

enum NodeKind {
    NK_EXPR_STMT,
    NK_BINARY,
    NK_UNARY,
    NK_NUMBER,
    NK_STRING,
};

enum ValueKind {
    VK_NUMBER,
    VK_STRING,
    VK_NULL,
};

class Value {
public:
    ValueKind kind;
    union {
        double number;
        char* str;
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
            case VK_NULL:
                cout << "(null)";
                break;
        }
    }

    std::string stringify_kind() {
        switch (kind) {
            case VK_NUMBER: return "number";
            case VK_STRING: return "string";
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

        double num;
        char* str;
    };

    static AstNode* make_expr_stmt(AstNode* child, Token* mark) {
        AstNode* n = new AstNode;
        n->kind = NK_EXPR_STMT;
        n->mark = mark;
        n->expr_stmt = child;
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
};

void error(const std::string& msg, int at) {
    cout << g_red_color << "(input):1:" << at << ": error: " << g_reset_color << msg << endl;
    exit(-1);
}

void error(const std::string& msg, Token* token) {
    error(msg, token->pos);
}

class Interpreter {
public:
    void interpret_nodes(std::vector<AstNode*> nodes);
    Value interpret(AstNode* node);
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

        case NK_BINARY: {
            Value a = interpret(node->bin.left);
            Value b = interpret(node->bin.right);
            if (a.kind != b.kind) {
                error("Type mismatch: " + a.stringify_kind() + " and " + b.stringify_kind(), node->mark);
            }

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
                        error(std::string("Invalid operation with strings: ") + "`" + node->bin.op + "`", node->mark);
                    }
                    return Value::make_string((std::string(a.str) + std::string(b.str)).c_str());
                } break;
            }
        } break;

        case NK_UNARY: {
            Value child = interpret(node->un.child);
            if (child.kind != VK_NUMBER) {
                error("Unary `-` cannot be applied to " + child.stringify_kind(), node->mark);
            }
            return Value::make_number(-child.number);
        } break;

        case NK_NUMBER: {
            return Value::make_number(node->num);
        } break;

        case NK_STRING: {
            return Value::make_string(node->str);
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
    expect(TK_SEMICOLON, "semicolon");
    return AstNode::make_expr_stmt(node, prev);
}

AstNode* Parser::parse_expr() {
    return parse_add_binop();
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

int main() {
    Interpreter i;

    while (true) {
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
}
