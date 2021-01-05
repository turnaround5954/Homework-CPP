/*
 * File: parser.cpp
 * ----------------
 * This file implements the parser.h interface.
 */

#include "parser.h"
#include "error.h"
#include "exp.h"
#include "strlib.h"
#include "tokenscanner.h"
#include <iostream>
#include <string>
using namespace std;

static Expression *readE(TokenScanner &scanner, SSModel &context,
                         Vector<Expression *> &track, int prec = 0);
static Expression *readT(TokenScanner &scanner, SSModel &context,
                         Vector<Expression *> &track);
static int precedence(const string &token);
static Expression *readRangeExp(TokenScanner &scanner, const string &token,
                                SSModel &context);

/**
 * Implementation notes: parseExp
 * ------------------------------
 * This code just reads an expression and then checks for extra tokens.
 */

Expression *parseExp(TokenScanner &scanner, SSModel &context) {
    Vector<Expression *> track;
    try {
        Expression *exp = readE(scanner, context, track);
        if (scanner.hasMoreTokens()) {
            error("Unexpected token \"" + scanner.nextToken() + "\"");
        }
        return exp;
    } catch (ErrorException ex) {
        cout << ex.getMessage() << endl;
        for (Expression *e : track) {
            delete e;
            e = nullptr;
        }
        error("Error while parsing expression.");
    }
}

/**
 * Implementation notes: readE
 * Usage: exp = readE(scanner, prec);
 * ----------------------------------
 * The implementation of readE uses precedence to resolve the ambiguity in
 * the grammar.  At each level, the parser reads operators and subexpressions
 * until it finds an operator whose precedence is greater than the prevailing
 * one.  When a higher-precedence operator is found, readE calls itself
 * recursively to read that subexpression as a unit.
 */

Expression *readE(TokenScanner &scanner, SSModel &context,
                  Vector<Expression *> &track, int prec) {
    Expression *exp = readT(scanner, context, track);
    string token;
    while (true) {
        token = scanner.nextToken();
        int tprec = precedence(token);
        if (tprec <= prec)
            break;
        Expression *rhs = readE(scanner, context, track, tprec);
        exp = new CompoundExp(token, exp, rhs);
        track.add(exp);
    }
    scanner.saveToken(token);
    return exp;
}

/**
 * Implementation notes: readT
 * ---------------------------
 * This function scans a term, which is either an integer, an identifier,
 * or a parenthesized subexpression.
 */
Expression *readT(TokenScanner &scanner, SSModel &context,
                  Vector<Expression *> &track) {
    string token = scanner.nextToken();
    TokenScanner::TokenType type = scanner.getTokenType(token);

    Expression *tracked = nullptr;

    if (type == TokenScanner::WORD) {
        // identifier expression
        if (context.nameIsValid(token)) {
            tracked = new IdentifierExp(token);
        } else {
            // range function expression
            tracked = readRangeExp(scanner, token, context);
        }
        track.add(tracked);
        return tracked;
    }
    if (type == TokenScanner::NUMBER) {
        tracked = new DoubleExp(stringToReal(token));
        track.add(tracked);
        return tracked;
    }
    if (type == TokenScanner::STRING) {
        tracked = new TextStringExp(token.substr(1, token.length() - 2));
        track.add(tracked);
        return tracked;
    }
    if (token != "(")
        error("Unexpected token \"" + token + "\"");
    Expression *exp = readE(scanner, context, track, 0);
    if (scanner.nextToken() != ")") {
        error("Unbalanced parentheses");
    }
    return exp;
}

/**
 * Implementation notes: precedence
 * --------------------------------
 * This function checks the token against each of the defined operators
 * and returns the appropriate precedence value.
 */
int precedence(const string &token) {
    if (token == "+" || token == "-")
        return 1;
    if (token == "*" || token == "/")
        return 2;
    return 0;
}

Expression *readRangeExp(TokenScanner &scanner, const string &token,
                         SSModel &context) {

    string tokenUpper = toUpperCase(token);

    if (!RangeExp::containsFunc(tokenUpper))
        error("Unexpected token \"" + token + "\"");
    if (scanner.nextToken() != "(")
        error("Missing ( after range function");
    string st = scanner.nextToken();
    if (!context.nameIsValid(st)) {
        error("Unexpected token \"" + st + "\"");
    }
    if (scanner.nextToken() != ":")
        error("Missing : in the middle of range function");
    string ed = scanner.nextToken();
    if (!context.nameIsValid(ed)) {
        error("Unexpected token \"" + ed + "\"");
    }
    if (!context.rangeIsValid(st, ed)) {
        error("Invalid range \"" + st + ":" + ed + "\"");
    }
    if (scanner.nextToken() != ")")
        error("Missing ) at the end of range function");
    return new RangeExp(tokenUpper, st, ed, context);
}
