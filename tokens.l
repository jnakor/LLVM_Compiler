%{
#include <string>
#include "node.h"
#include "parser.hpp"

#define SAVE_TOKEN  yylval.string = new std::string(yytext, yyleng)
#define TOKEN(t)    (yylval.token = t)
%}

%option noyywrap

%%

[ \t\n]					        ;
"for"                           return TOKEN(TFOR);
"to"                            return TOKEN(TTO);
"do"                            return TOKEN(TDO);
"begin"                         return TOKEN(TBEGIN);
"end"                           return TOKEN(TEND);
"if"                            return TOKEN(IF);
"else"                          return TOKEN(ELSE);
"then"                          return TOKEN(THEN);
"program"                       return TOKEN(PROGRAM);
"function"                      return TOKEN(FUNCTION);
"var"                           return TOKEN(VAR);
"extern"                        return TOKEN(TEXTERN);
"return"				        return TOKEN(TRETURN);
"array"                         return TOKEN(ARRAY);
"of"                            return TOKEN(OF);
"foreach"                       return TOKEN(TFOREACH);
"in"                            return TOKEN(IN);
[a-zA-Z_][a-zA-Z0-9_]*  SAVE_TOKEN; return TIDENTIFIER;
[0-9]+\.[0-9]* 			    SAVE_TOKEN; return TDOUBLE;
[0-9]+					        SAVE_TOKEN; return TINTEGER;

"="						          return TOKEN(TEQUAL);
"=="				          	return TOKEN(TCEQ);
"!="			          		return TOKEN(TCNE);
"<"				          		return TOKEN(TCLT);
"<="	          				return TOKEN(TCLE);
">"				          		return TOKEN(TCGT);
">="					          return TOKEN(TCGE);

"("	          					return TOKEN(TLPAREN);
")"					          	return TOKEN(TRPAREN);
"{"         						return TOKEN(TLBRACE);
"}"					          	return TOKEN(TRBRACE);

"."         						return TOKEN(TDOT);
","				          		return TOKEN(TCOMMA);

"+"				          		return TOKEN(TPLUS);
"-"		          				return TOKEN(TMINUS);
"*"		          				return TOKEN(TMUL);
"/"				          		return TOKEN(TDIV);
":"                             return TOKEN(COLON);
";"                             return TOKEN(SEMICOLON);
"["                             return TOKEN(SQLBRACE);
"]"                             return TOKEN(SQRBRACE);
.                       printf("Unknown!! token! %s %zu\n", yytext, strlen(yytext)); //yyterminate();

%%
