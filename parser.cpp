#include <iostream>
#include <string>
#include <vector>
#include <map>

//-----------------------------------------------------------------------------------
// Lexer
//-----------------------------------------------------------------------------------
enum Token {
    tok_eof = -1,

    // commands
    tok_def = -2,
    tok_extern = -3,

    // primary
    tok_identifier = -4,
    tok_number = -5,
};

static std::string identifier_str; // filled in if tok_identifier
static double num_val;             // filled in if tok_number


// GetTok - return the next token from standard input.
static int getTok() { 
    static int last_char = ' ';

    // Skip any whitespace.
    while (isspace(last_char))
        last_char = getchar();
    

    // identifier: [a-zA-Z][a-zA-Z0-9]
    if (isalpha(last_char)) {
        identifier_str = last_char;
        while (isalnum((last_char = getchar())))
            identifier_str += last_char;
        
        if (identifier_str == "def")
            return tok_def;
        if (identifier_str == "extern")
            return tok_extern;
        return tok_identifier;
    }

    // number: [0-9.]+
    if (isdigit(last_char) || last_char == '.') {
        std::string num_str;
        do {
            num_str += last_char;
            last_char = getchar();
        } while (isdigit(last_char) || last_char == '.');

        // fill in num_val
        num_val = strtod(num_str.c_str(), 0);
        return tok_number;
    }

    // Check for EOF & Don't eat the EOF
    if (last_char == EOF)
        return tok_eof;

    // Otherwise, return the character as its ASCII value.
    int this_char = last_char;
    last_char = getchar();
    return this_char;
}


//-----------------------------------------------------------------------------------
// Abstract Syntax Tree
//-----------------------------------------------------------------------------------

// ExprAST - Base class for all expression nodes.
class ExprAST {
public:
    virtual ~ExprAST() = default;
};


// NumberExprAST - Expression class for numeric literals like "1.0".
class NumberExprAST : public ExprAST {
    double val;

public:
    NumberExprAST(double val) : val(val) {}
};


// VaraibleExprAST - Expression class for referencing a varaible, like "a".
class VariableExprAST : public ExprAST {
    std::string name;

public:
    VariableExprAST(const std::string &name) : name(name) {}
};


// BinaryExprAST - Expression class for a binary operator.
class BinaryExprAST : public ExprAST {
    char op;
    std::unique_ptr<ExprAST> lhs, rhs;

public:
    BinaryExprAST(char op, std::unique_ptr<ExprAST> lhs, std::unique_ptr<ExprAST> rhs) : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}
};


// CallExprAST - Expression class for function calls.
class CallExprAST : public ExprAST {
    std::string callee;
    std::vector<std::unique_ptr<ExprAST>> args;

public:
    CallExprAST(const std::string &callee, std::vector<std::unique_ptr<ExprAST>> args) : callee(callee), args(std::move(args)) {}
};


// PrototypeAST - represents the "prototype" for a function,
// which captures its name, and its argument names (thus implicitly the number of arguments the function takes)
class PrototypeAST {
    std::string name;
    std::vector<std::string> args;

public:
    PrototypeAST(const std::string &name, std::vector<std::string> args) : name(name), args(std::move(args)) {}

    const std::string &getName() const { return name; }
};


// FunctionAST - represents a function definition itself.
class FunctionAST {
    std::unique_ptr<PrototypeAST> proto;
    std::unique_ptr<ExprAST> body;

public:
    FunctionAST(std::unique_ptr<PrototypeAST> proto, std::unique_ptr<ExprAST> body) : proto(std::move(proto)), body(std::move(body)) {}
};



//-------------------------------------------------------------------------------------------------------------------------------------------------
// Parser
//-------------------------------------------------------------------------------------------------------------------------------------------------

/*
cur_tok / getNextToken : provide a simple token buffer.
cur_tok - current token the parser is now looking at.
getNextToken() - reads another token from the 'lexer' & updates cur_tok with its results.
*/
static int cur_tok;
static int getNextToken() {
    return cur_tok = getTok();
}

// BinopPrecedence - this holds the precedence for each binary operator that is defined.
static std::map<char, int> BinopPrecedence;

// getTokPrecendence
static int getTokPrecedence() {
    if (!isascii(cur_tok)) // if tok is not binary operator
        return -1;
    
    // make sure it's a declared binary operator.
    int tok_prec = BinopPrecedence[cur_tok];
    if (tok_prec <= 0) return -1;
    return tok_prec;
}

auto lhs = std::make_unique<VariableExprAST>("x");
auto rhs = std::make_unique<VariableExprAST>("y");
auto result = std::make_unique<BinaryExprAST>('+', std::move(lhs), std::move(rhs));





// logError* - these are little helper functions for error handling.
std::unique_ptr<ExprAST> logError(const char *str) {
    fprintf(stderr, "Error: %s\n", str);
    return nullptr;
}
std::unique_ptr<PrototypeAST> logErrorP(const char *str) {
    logError(str);
    return nullptr;
}


//------------------------------------------------------------------
//Basic Expression Parsing


// numberexpr ::= number
static std::unique_ptr<ExprAST> parseNumberExpr() {
    auto result = std::make_unique<NumberExprAST>(num_val);
    getNextToken();
    return std::move(result);
}

// parenexpr ::= '(' expression ')'
static std::unique_ptr<ExprAST> parseParenExpr() {
    getNextToken(); // eat (
    auto v = parseExpression();
    if (!v)
        return nullptr;
    
    if (cur_tok != ')')
        return logError("expected ')'");
    getNextToken(); // eat )
    return v;
}

/*
identifierexpr
    ::= identifier
    ::= identifier '(' expression * ')'
*/
static std::unique_ptr<ExprAST> parseIdentifierExpr() {
    std::string id_name = identifier_str;

    getNextToken(); // eat identifier
    
    if (cur_tok != '(') // simple varaible ref
        return std::make_unique<VariableExprAST>(id_name);

    // call
    getNextToken(); // eat (
    std::vector<std::unique_ptr<ExprAST>> args;
    if (cur_tok != ')') {
        while (true) {
            if (auto arg = parseExpression())
                args.push_back(std::move(arg));
            else
                return nullptr;

            if (cur_tok == ')')
                break;
            
            if (cur_tok != ',')
                return logError("Expected ')' or ',' in argument list");
            getNextToken();
        }
    }

    // eat the ')'
    getNextToken();

    return std::make_unique<CallExprAST>(id_name, std::move(args));
}

/*
primary
    ::= identifierexpr
    ::= numberexpr
    ::= parenexpr
*/
static std::unique_ptr<ExprAST> parsePrimary() {
    switch (cur_tok) {
        default:
            return logError("unknown token when expecting an expression");
        case tok_identifier:
            return parseIdentifierExpr();
        case tok_number:
            return parseNumberExpr();
        case '(':
            return parseParenExpr();
    }
}

/*
binoprhs
    ::= ('+' primary)*
*/
static std::unique_ptr<ExprAST> parseBinOpRHS(int expr_prec, std::unique_ptr<ExprAST> lhs) {
    // if this is a binop, find its precedence.
    while (true) {
        int tok_prec = getTokPrecedence();

        // if this is a binop that binds at least as tightly as the current binop,
        // consume it, otherwise we are done.
        if (tok_prec < expr_prec)
            return lhs;
        
        // now know this is a binop.
        int bin_op = cur_tok;
        getNextToken(); // eat binop

        // parse the primary expression after the binary operator.
        auto rhs = parsePrimary();
        if (!rhs)
            return nullptr;

        // if bin_op binds less tightly with rhs than the operator after rhs,
        // let the pending operator take rhs as its lhs.
        int next_prec = getTokPrecedence();
        if (tok_prec < next_prec) {
            rhs = parseBinOpRHS(tok_prec + 1, std::move(rhs));
            if (!rhs)
                return nullptr;
        }

        // merge lhs/rhs
        lhs = std::make_unique<BinaryExprAST>(bin_op, std::move(lhs), std::move(rhs));
    }
}

/*
expression
    ::= primary binoprhs
*/
static std::unique_ptr<ExprAST> parseExpression() {
    auto lhs = parsePrimary();
    if (!lhs)
        return nullptr;

    return parseBinOpRHS(0, std::move(lhs));
}

/*
prototype
    ::= id '(' id* ')'
*/
static std::unique_ptr<PrototypeAST> parsePrototype() {
    if (cur_tok != tok_identifier)
        return logErrorP("Expected function name in prototype");
    
    std::string fn_name = identifier_str;
    getNextToken();

    if (cur_tok != '(')
        return logErrorP("Expected '(' in prototype");
    
    // read the list of argument names.
    std::vector<std::string> arg_names;
    while (getNextToken() == tok_identifier)
        arg_names.push_back(identifier_str);
    if (cur_tok != ')')
        return logErrorP("Expected ')' in prototype");

    // success
    getNextToken(); // eat )

    return std::make_unique<PrototypeAST>(fn_name, std::move(arg_names));
}


/// definition ::= 'def' prototype expression
static std::unique_ptr<FunctionAST> parseDefinition() {
    getNextToken(); // eat def
    auto proto = parsePrototype();
    if (!proto)
        return nullptr;
    
    if (auto e = parseExpression())
        return std::make_unique<FunctionAST>(std::move(proto), std::move(e));
    
    return nullptr;
}


/// topLevelexpr ::= expression
static std::unique_ptr<FunctionAST> parseTopLevelExpr() {
    if (auto e = parseExpression()) {
        // make an anonymous proto.
        auto proto = std::make_unique<PrototypeAST>("", std::vector<std::string>());
        return std::make_unique<FunctionAST>(std::move(proto), std::move(e));
    }
    return nullptr;
}


/// external ::= 'extern' prototype
static std::unique_ptr<PrototypeAST> parseExtern() {
    getNextToken(); // eat extern.
    return parsePrototype();
}

//---------------------------------------------------------------------
// Top-Level Parsing
//---------------------------------------------------------------------

static void handleDefinition() {
    if (parseDefinition()) {
        fprintf(stderr, "Parsed a function definition.\n");
    }
    else {
        // skip token for error recovery.
        getNextToken();
    }
}

static void handleExtern() {
    if (parseExtern()) {
        fprintf(stderr, "Parsed an extern\n");
    }
    else {
        // skip token for error recovery.
        getNextToken();
    }
}

static void handleTopLevelExpression() {
    // evaluate a top-level expression into an anonymous function.
    if (parseTopLevelExpr()) {
        fprintf(stderr, "Parsed a top-level expr\n");
    }
    else {
        // skip token for error recovery.
        getNextToken();
    }
}

/// top ::= definition | external | expression | ';'
static void mainLoop() {
    while (true) {
        fprintf(stderr, "ready> ");
        switch (cur_tok) {
            case tok_eof:
                return;
            case ';': // ignore top-level semicolons.
                getNextToken();
                break;
            case tok_def:
                handleDefinition();
                break;
            case tok_extern:
                handleExtern();
                break;
            default:
                handleTopLevelExpression();
                break;
        }
    }
}


//--------------------------------------------------------------
// Main driver
//--------------------------------------------------------------

int main() {
    // Install standard binary operators.
    // 1 is lowest precedence.
    BinopPrecedence['<'] = 10;
    BinopPrecedence['+'] = 20;
    BinopPrecedence['-'] = 20;
    BinopPrecedence['*'] = 40; // highest

    // prime the first token.
    fprintf(stderr, "ready> ");
    getNextToken();

    // run the main "Interpreter Loop" now
    mainLoop();

    return 0;
}