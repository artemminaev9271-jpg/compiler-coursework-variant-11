%{
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "ast.hpp"

int yylex();
void yyerror(const char *message);
extern int yylineno;

#define MC_SET_LOC(node, at) \
    do { (node)->loc = mc::SourceLocation{(at).first_line, (at).first_column}; } while (0)

namespace mc {
std::unique_ptr<Program> g_parsed_program;
std::vector<std::string> g_parse_errors;
}
%}

%code requires {
#include <cstdint>
#include <cstdlib>
#include "ast.hpp"
}

%define parse.error verbose
%locations

%union {
    std::int64_t int_value;
    char *str;
    int type_kind;
    mc::Expr *expr;
    mc::Stmt *stmt;
    mc::BlockStmt *block;
    mc::FunctionDecl *func;
    mc::Program *program;
    mc::ExprList *exprs;
    mc::ParamList *params;
    mc::StmtList *stmts;
    mc::FunctionList *funcs;
}

%token RETURN IF ELSE FOR
%token INT_KW
%token EQ NE LE GE AND OR
%token <int_value> INTEGER
%token <str> IDENT

%type <type_kind> type
%type <program> program
%type <funcs> function_list
%type <func> function
%type <params> param_list_opt param_list
%type <exprs> argument_list_opt argument_list
%type <block> block
%type <stmts> statement_list
%type <stmt> statement var_decl for_init_opt for_init
%type <expr> expression assignment ternary logical_or logical_and equality relational additive multiplicative unary primary expression_opt

%destructor { std::free($$); } <str>
%destructor { delete $$; } <expr>
%destructor { delete $$; } <stmt>
%destructor { delete $$; } <block>
%destructor { delete $$; } <func>
%destructor { delete $$; } <program>
%destructor { delete $$; } <exprs>
%destructor { delete $$; } <params>
%destructor { delete $$; } <stmts>
%destructor { delete $$; } <funcs>

%start program

%%

program
    : function_list
      {
          auto *program = new mc::Program();
          program->functions = std::move(*$1);
          delete $1;
          mc::g_parsed_program.reset(program);
          $$ = nullptr;
      }
    ;

function_list
    : function
      {
          $$ = new mc::FunctionList();
          $$->push_back(std::unique_ptr<mc::FunctionDecl>($1));
      }
    | function_list function
      {
          $1->push_back(std::unique_ptr<mc::FunctionDecl>($2));
          $$ = $1;
      }
    ;

function
    : type IDENT '(' param_list_opt ')' block
      {
          std::string name($2);
          std::free($2);
          auto params = std::move(*$4);
          delete $4;
          $$ = new mc::FunctionDecl(static_cast<mc::TypeKind>($1), name, std::move(params), std::unique_ptr<mc::BlockStmt>($6));
          MC_SET_LOC($$, @1);
      }
    ;

type
    : INT_KW { $$ = static_cast<int>(mc::TypeKind::Int); }
    ;

param_list_opt
    : /* empty */ { $$ = new mc::ParamList(); }
    | param_list { $$ = $1; }
    ;

param_list
    : type IDENT
      {
          $$ = new mc::ParamList();
          $$->push_back({static_cast<mc::TypeKind>($1), std::string($2)});
          std::free($2);
      }
    | param_list ',' type IDENT
      {
          $1->push_back({static_cast<mc::TypeKind>($3), std::string($4)});
          std::free($4);
          $$ = $1;
      }
    ;

block
    : '{' statement_list '}'
      {
          $$ = new mc::BlockStmt();
          MC_SET_LOC($$, @1);
          $$->statements = std::move(*$2);
          delete $2;
      }
    ;

statement_list
    : /* empty */ { $$ = new mc::StmtList(); }
    | statement_list statement
      {
          $1->push_back(std::unique_ptr<mc::Stmt>($2));
          $$ = $1;
      }
    ;

statement
    : var_decl ';' { $$ = $1; }
    | expression ';'
      {
          $$ = new mc::ExprStmt(std::unique_ptr<mc::Expr>($1));
          MC_SET_LOC($$, @1);
      }
    | RETURN expression ';'
      {
          $$ = new mc::ReturnStmt(std::unique_ptr<mc::Expr>($2));
          MC_SET_LOC($$, @1);
      }
    | IF '(' expression ')' block ELSE block
      {
          $$ = new mc::IfStmt(std::unique_ptr<mc::Expr>($3), std::unique_ptr<mc::BlockStmt>($5), std::unique_ptr<mc::BlockStmt>($7));
          MC_SET_LOC($$, @1);
      }
    | IF '(' expression ')' block
      {
          $$ = new mc::IfStmt(std::unique_ptr<mc::Expr>($3), std::unique_ptr<mc::BlockStmt>($5), nullptr);
          MC_SET_LOC($$, @1);
      }
    | FOR '(' for_init_opt ';' expression_opt ';' expression_opt ')' block
      {
          $$ = new mc::ForStmt(std::unique_ptr<mc::Stmt>($3), std::unique_ptr<mc::Expr>($5), std::unique_ptr<mc::Expr>($7), std::unique_ptr<mc::BlockStmt>($9));
          MC_SET_LOC($$, @1);
      }
    | block { $$ = $1; }
    ;

var_decl
    : type IDENT
      {
          std::string name($2);
          std::free($2);
          $$ = new mc::VarDeclStmt(static_cast<mc::TypeKind>($1), name, nullptr);
          MC_SET_LOC($$, @1);
      }
    | type IDENT '=' expression
      {
          std::string name($2);
          std::free($2);
          $$ = new mc::VarDeclStmt(static_cast<mc::TypeKind>($1), name, std::unique_ptr<mc::Expr>($4));
          MC_SET_LOC($$, @1);
      }
    ;

for_init_opt
    : /* empty */ { $$ = nullptr; }
    | for_init { $$ = $1; }
    ;

for_init
    : var_decl { $$ = $1; }
    | expression { $$ = new mc::ExprStmt(std::unique_ptr<mc::Expr>($1)); }
    ;

expression_opt
    : /* empty */ { $$ = nullptr; }
    | expression { $$ = $1; }
    ;

expression
    : assignment { $$ = $1; }
    ;

assignment
    : IDENT '=' assignment
      {
          std::string name($1);
          std::free($1);
          $$ = new mc::AssignExpr(name, std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | ternary { $$ = $1; }
    ;

ternary
    : logical_or '?' expression ':' ternary
      {
          $$ = new mc::TernaryExpr(std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3), std::unique_ptr<mc::Expr>($5));
          MC_SET_LOC($$, @1);
      }
    | logical_or { $$ = $1; }
    ;

logical_or
    : logical_or OR logical_and
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Or, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | logical_and { $$ = $1; }
    ;

logical_and
    : logical_and AND equality
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::And, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | equality { $$ = $1; }
    ;

equality
    : equality EQ relational
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Eq, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | equality NE relational
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Ne, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | relational { $$ = $1; }
    ;

relational
    : relational '<' additive
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Lt, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | relational LE additive
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Le, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | relational '>' additive
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Gt, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | relational GE additive
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Ge, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | additive { $$ = $1; }
    ;

additive
    : additive '+' multiplicative
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Add, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | additive '-' multiplicative
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Sub, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | multiplicative { $$ = $1; }
    ;

multiplicative
    : multiplicative '*' unary
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Mul, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | multiplicative '/' unary
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Div, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | multiplicative '%' unary
      {
          $$ = new mc::BinaryExpr(mc::BinaryOp::Mod, std::unique_ptr<mc::Expr>($1), std::unique_ptr<mc::Expr>($3));
          MC_SET_LOC($$, @1);
      }
    | unary { $$ = $1; }
    ;

unary
    : '-' unary
      {
          $$ = new mc::UnaryExpr(mc::UnaryOp::Neg, std::unique_ptr<mc::Expr>($2));
          MC_SET_LOC($$, @1);
      }
    | '!' unary
      {
          $$ = new mc::UnaryExpr(mc::UnaryOp::Not, std::unique_ptr<mc::Expr>($2));
          MC_SET_LOC($$, @1);
      }
    | primary { $$ = $1; }
    ;

primary
    : INTEGER
      {
          $$ = new mc::IntLiteral($1);
          MC_SET_LOC($$, @1);
      }
    | IDENT
      {
          std::string name($1);
          std::free($1);
          $$ = new mc::VarExpr(name);
          MC_SET_LOC($$, @1);
      }
    | IDENT '(' argument_list_opt ')'
      {
          std::string name($1);
          std::free($1);
          auto args = std::move(*$3);
          delete $3;
          $$ = new mc::CallExpr(name, std::move(args));
          MC_SET_LOC($$, @1);
      }
    | '(' expression ')' { $$ = $2; }
    ;

argument_list_opt
    : /* empty */ { $$ = new mc::ExprList(); }
    | argument_list { $$ = $1; }
    ;

argument_list
    : expression
      {
          $$ = new mc::ExprList();
          $$->push_back(std::unique_ptr<mc::Expr>($1));
      }
    | argument_list ',' expression
      {
          $1->push_back(std::unique_ptr<mc::Expr>($3));
          $$ = $1;
      }
    ;

%%

void yyerror(const char *message) {
    mc::g_parse_errors.push_back("line " + std::to_string(yylloc.first_line) +
                                 ", column " + std::to_string(yylloc.first_column) +
                                 ": " + message);
}
