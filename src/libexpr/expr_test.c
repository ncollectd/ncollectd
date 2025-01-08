// SPDX-License-Identifier: GPL-2.0-only
// SPDX-FileCopyrightText: Copyright (C) 2022-2024 Manuel Sanmartín
// SPDX-FileContributor: Manuel Sanmartín <manuel.luis at gmail.com>

#include "ncollectd.h"
#include "libexpr/expr.h"
#include "libtest/testing.h"

typedef struct {
    char *expr;
    expr_value_t *result;
} expr_test_t;

expr_test_t expr_test[] = {
    {"1",                   &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=1}},
    {"1+1",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=2}},
    {"(1)",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=1}},
    {"pi",                  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.141592653589793116}},
    {"atan(1)*4 - pi",      &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0}},
    {"e",                   &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=2.7182818284590450908}},
    {"2+1",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=2+1}},
    {"(((2+(1))))",         &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=2+1}},
    {"3+2",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3+2}},
    {"3+2+4",               &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3+2+4}},
    {"(3+2)+4",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3+2+4}},
    {"3+(2+4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3+2+4}},
    {"(3+2+4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3+2+4}},
    {"3*2*4",               &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3*2*4}},
    {"(3*2)*4",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3*2*4}},
    {"3*(2*4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3*2*4}},
    {"(3*2*4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3*2*4}},
    {"3-2-4",               &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3-2-4}},
    {"(3-2)-4",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=(3-2)-4}},
    {"3-(2-4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3-(2-4)}},
    {"(3-2-4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3-2-4}},
    {"3/2/4",               &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0/2.0/4.0}},
    {"(3/2)/4",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=(3.0/2.0)/4.0}},
    {"3/(2/4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0/(2.0/4.0)}},
    {"(3/2/4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0/2.0/4.0}},
    {"(3*2/4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0*2.0/4.0}},
    {"(3/2*4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0/2.0*4.0}},
    {"3*(2/4)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3.0*(2.0/4.0)}},
    {"asin(sin(.5))",       &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.5}},
    {"sin(asin(.5))",       &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.5}},
    {"log(exp(.5))",        &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.5}},
    {"exp(log(.5))",        &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.5}},
    {"asin(sin(-.5))",      &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=-0.5}},
    {"asin(sin(-0.5))",     &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=-0.5}},
    {"(asin(sin(-0.5)))",   &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=-0.5}},
    {"log10(1000)",         &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3}},
    {"log10(1e3)",          &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3}},
    {"log10(1.0e3)",        &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=3}},
    {"pow(10,5)*5e-5",      &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=5}},
    {"log(1000)",           &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=6.9077552789821368151}},
    {"log(e)",              &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=1}},
    {"log(pow(e,10))",      &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=10}},
    {"pow(100,.5)+1",       &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=11}},
    {"pow(100,--.5)+1",     &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=11}},
    {"sqrt(100) + 7",       &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=17}},
    {"sqrt(100) * 7",       &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=70}},
    {"sqrt(100 * 100)",     &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=100}},
    {"pow(2,2)",            &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=4}},
    {"atan2(1,1)",          &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.785398163397448279}},
    {"atan2(1,2)",          &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.46364760900080609352}},
    {"atan2(2,1)",          &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=1.10714871779409040897}},
    {"atan2(3,4)",          &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.64350110879328437097}},
    {"atan2(3+3,4*2)",      &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.64350110879328437097}},
    {"atan2(3+3,(4*2))",    &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.64350110879328437097}},
    {"atan2((3+3),4*2)",    &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.64350110879328437097}},
    {"atan2((3+3),(4*2))",  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.64350110879328437097}},
    {"0/0",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=NAN}},
    {"1%0",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=NAN}},
//    {"1%(1%0)",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=NAN}},
//    {"(1%0)%1",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=NAN}},
//    {"1/0",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=INFINITY}},
    {"log(0)",              &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=-INFINITY}},
    {"pow(2,10000000)",     &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=INFINITY}},
    {".5",                  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0.5}},
    {"0xaf",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0xaf}},
    {"0022",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=0022}},
    {"0755 | 0001 != 0",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"5>4",                 &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"true",                &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"false",               &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"true && true",        &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"true && false",       &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"false && true",       &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"false && false",      &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"true || true",        &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"false || true",       &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"true || false",       &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"false || false",      &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"\"foo\" == \"foo\"",  &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"\"foo\" == 'foo'",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"\"foo\" == 'bar'",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"\"foo\" != 'bar'",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"2.5 == 5/2",          &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"\"Word\" =~ \"[Ww]o.*\"", &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"'Word' =~ '[Ww]o.*'", &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
    {"'Word' =~ 'Word'",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=true}},
//    {"'Word' !~ 'Word'",    &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
    {"64Ki",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=65536}},
    {"64Mi",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=67108864}},
    {"64Gi",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=68719476736}},
    {"64Ti",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=70368744177664}},
    {"64k",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=64000}},
    {"64M",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=64000000}},
    {"64G",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=64000000000}},
    {"64T",                 &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=64000000000000}},
    {"6.4Ki",               &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=6553.6}},
    {"6.4k",                &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=6400}},
    {"\"1\"+1",             &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=2}},
    {"\"1x\"+1",            &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=NAN}},
    { NULL,                 &(expr_value_t){.type=EXPR_VALUE_BOOLEAN, .boolean=false}},
};

static int eval_boolean(char *expr)
{
    expr_node_t *node = expr_parse(expr, NULL);
    if (node == NULL) {
#ifdef DEBUG
        fprintf(stderr, "FAIL %s PARSE FAIL\n", expr);
#endif
        return -1;
    }

    expr_value_t *value = expr_eval(node);
    if (value == NULL) {
        expr_node_free(node);
#ifdef DEBUG
        fprintf(stderr, "FAIL %s EVAL FAIL\n", expr);
#endif
        return -1;
    }

    int ret = -1;
    if (value->type == EXPR_VALUE_BOOLEAN)
        ret = value->boolean;

    expr_value_free(value);
    expr_node_free(node);
    return ret;
}

static double eval_double(char *expr)
{
    expr_node_t *node = expr_parse(expr, NULL);
    if (node == NULL) {
#ifdef DEBUG
        fprintf(stderr, "FAIL %s PARSE FAIL\n", expr);
#endif
        return -NAN;
    }

    expr_value_t *value = expr_eval(node);
    if (value == NULL) {
        expr_node_free(node);
#ifdef DEBUG
        fprintf(stderr, "FAIL %s EVAL FAIL\n", expr);
#endif
        return -NAN;
    }

    double ret = -NAN;
    if (value->type == EXPR_VALUE_NUMBER)
        ret = value->number;

    expr_value_free(value);
    expr_node_free(node);
    return ret;
}

static double eval_double_arg1(char *expr,  char *name, double value)
{
    expr_symtab_t *symtab = expr_symtab_alloc();

    expr_symtab_append_name_value(symtab, name,
                                  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=value});

    expr_node_t *node = expr_parse(expr, symtab);
    if (node == NULL) {
#ifdef DEBUG
        fprintf(stderr, "FAIL %s PARSE FAIL\n", expr);
#endif
        return -NAN;
    }

    expr_value_t *rvalue = expr_eval(node);
    if (rvalue == NULL) {
        expr_symtab_free(symtab);
        expr_node_free(node);
#ifdef DEBUG
        fprintf(stderr, "FAIL %s EVAL FAIL\n", expr);
#endif
        return -NAN;
    }

    double ret = -NAN;
    if (rvalue->type == EXPR_VALUE_NUMBER)
        ret = rvalue->number;

    expr_symtab_free(symtab);
    expr_node_free(node);
    expr_value_free(rvalue);
    return ret;
}

static double eval_double_arg2(char *expr, char *name1, double value1, char *name2,double value2)
{
    expr_symtab_t *symtab = expr_symtab_alloc();

    expr_symtab_append_name_value(symtab, name1,
                                  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=value1});
    expr_symtab_append_name_value(symtab, name2,
                                  &(expr_value_t){.type=EXPR_VALUE_NUMBER, .number=value2});

    expr_node_t *node = expr_parse(expr, symtab);
    if (node == NULL) {
#ifdef DEBUG
        fprintf(stderr, "FAIL %s PARSE FAIL\n", expr);
#endif
        expr_symtab_free(symtab);
        return -NAN;
    }

    expr_value_t *rvalue = expr_eval(node);
    if (rvalue == NULL) {
#ifdef DEBUG
        fprintf(stderr, "FAIL %s EVAL FAIL\n", expr);
#endif
        expr_symtab_free(symtab);
        expr_node_free(node);
        return -NAN;
    }

    double ret = -NAN;
    if (rvalue->type == EXPR_VALUE_NUMBER)
        ret = rvalue->number;

    expr_symtab_free(symtab);
    expr_node_free(node);
    expr_value_free(rvalue);
    return ret;
}

DEF_TEST(expr_eval_functions)
{
    char buffer[256];
    for (double x = -5; x < 5; x += .5) {
        snprintf(buffer, sizeof(buffer), "abs(%g)", x);
        EXPECT_EQ_DOUBLE_STR(fabs(x), eval_double_arg1("abs(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "acos(%g)", x);
        EXPECT_EQ_DOUBLE_STR(acos(x), eval_double_arg1("acos(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "asin(%g)", x);
        EXPECT_EQ_DOUBLE_STR(asin(x), eval_double_arg1("asin(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "atan(%g)", x);
        EXPECT_EQ_DOUBLE_STR(atan(x), eval_double_arg1("atan(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "ceil(%g)", x);
        EXPECT_EQ_DOUBLE_STR(ceil(x), eval_double_arg1("ceil(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "cos(%g)", x);
        EXPECT_EQ_DOUBLE_STR(cos(x), eval_double_arg1("cos(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "cosh(%g)", x);
        EXPECT_EQ_DOUBLE_STR(cosh(x), eval_double_arg1("cosh(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "exp(%g)", x);
        EXPECT_EQ_DOUBLE_STR(exp(x), eval_double_arg1("exp(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "floor(%g)", x);
        EXPECT_EQ_DOUBLE_STR(floor(x), eval_double_arg1("floor(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "log(%g)", x);
        EXPECT_EQ_DOUBLE_STR(log(x), eval_double_arg1("log(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "log10(%g)", x);
        EXPECT_EQ_DOUBLE_STR(log10(x), eval_double_arg1("log10(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "sin(%g)", x);
        EXPECT_EQ_DOUBLE_STR(sin(x), eval_double_arg1("sin(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "sinh(%g)", x);
        EXPECT_EQ_DOUBLE_STR(sinh(x), eval_double_arg1("sinh(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "sqrt(%g)", x);
        EXPECT_EQ_DOUBLE_STR(sqrt(x), eval_double_arg1("sqrt(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "tan(%g)", x);
        EXPECT_EQ_DOUBLE_STR(tan(x), eval_double_arg1("tan(x)", "x", x), buffer);
        snprintf(buffer, sizeof(buffer), "tanh(%g)", x);
        EXPECT_EQ_DOUBLE_STR(tanh(x), eval_double_arg1("tanh(x)", "x", x), buffer);

        for (double y = -2; y < 2; y += .5) {
            if (fabs(x) < 0.01) break;
            snprintf(buffer, sizeof(buffer), "atan2(%g, %g)", x, y);
            EXPECT_EQ_DOUBLE_STR(atan2(x, y), eval_double_arg2("atan2(x,y)", "x", x, "y", y), buffer);
            snprintf(buffer, sizeof(buffer), "pow(%g, %g)", x, y);
            EXPECT_EQ_DOUBLE_STR(pow(x, y), eval_double_arg2("pow(x,y)", "x", x, "y", y), buffer);
        }
    }

    return 0;
}

DEF_TEST(expr_eval)
{
    for (size_t i = 0; expr_test[i].expr != NULL; i++) {
        switch(expr_test[i].result->type) {
        case EXPR_VALUE_NUMBER:
            EXPECT_EQ_DOUBLE_STR(expr_test[i].result->number,
                                 eval_double(expr_test[i].expr), expr_test[i].expr);
            break;
        case EXPR_VALUE_BOOLEAN:
            EXPECT_EQ_INT_STR(expr_test[i].result->boolean,
                              eval_boolean(expr_test[i].expr), expr_test[i].expr);
            break;
        default:
            break;
        }
    }

    return 0;
}

int main(void)
{
    RUN_TEST(expr_eval);
    RUN_TEST(expr_eval_functions);

    END_TEST;
}
