/*
 * Copyright (C) 2015 Texas Instruments Inc
 *
 * Aneesh V <aneesh@ti.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "bqt.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

enum op_type_t {
	INVALID_OPERATOR = 0,
	OPERATOR_EXPONENT,
	OPERATOR_MULT,
	OPERATOR_DIV,
	OPERATOR_ADD,
	OPERATOR_SUBTRACT,
};

enum op_precedence_t {
	OP_PREC_EXPONENT = 6,
	OP_PREC_MULT_DIV = 5,
	OP_PREC_ADD_SUB = 4,

};

enum op_assoc_t {
	LEFT_ASSOCIATIVE,
	RIGHT_ASSOCIATIVE,
};

/*
 * Not supporting multi-character operators as of now.
 * Not used in any bqz files I have seen and it doesn't
 * look like bqStudio supports any operators other than
 * the basic ones. So, no need to complicate things by
 * supporting the full array of C operators.
 */
struct operator_t {
	char op_char;
	enum op_type_t op_num;
	enum op_precedence_t prec;
	enum op_assoc_t assoc;
	double (*fp)(double op1, double op2);
};

static double mult(double op1, double op2)
{
	return op1 * op2;
}

static double my_div(double op1, double op2)
{
	return op1 / op2;
}

static double add(double op1, double op2)
{
	return op1 + op2;
}

static double sub(double op1, double op2)
{
	return op1 - op2;
}

static double my_pow(double op1, double op2)
{
	return pow(op1, op2);
}

const struct operator_t operators[] = {
	{'^',	OPERATOR_EXPONENT,	OP_PREC_EXPONENT, RIGHT_ASSOCIATIVE, my_pow},
	{'*',	OPERATOR_MULT,		OP_PREC_MULT_DIV, LEFT_ASSOCIATIVE,  mult},
	{'/',	OPERATOR_DIV,		OP_PREC_MULT_DIV, LEFT_ASSOCIATIVE,  my_div},
	{'+',	OPERATOR_ADD,		OP_PREC_ADD_SUB,  LEFT_ASSOCIATIVE,  add},
	{'-',	OPERATOR_SUBTRACT,	OP_PREC_ADD_SUB,  LEFT_ASSOCIATIVE,  sub},
};

enum tok_type_t {
	OPERATOR,
	LEFT_PARENTHESIS,
	RIGHT_PARENTHESIS,
	OPERAND_CONST,
	OPERAND_X,
};


struct token_t {
	double val;
	enum tok_type_t type;
	const struct operator_t *op;
};

struct queue_t *create_queue()
{
	struct queue_t *q = (struct queue_t *) malloc(sizeof(struct queue_t));

	q->head = NULL;
	q->tail = NULL;
	q->curr = NULL;

	return q;
}

void delete_queue(struct queue_t *q)
{
	struct node_t *node = q->head;
	struct node_t *next;

	while (node) {
		next = node->next;
		free(node->val_p);
		free(node);
		node = next;
	}

	free(q);
}


static void push_front(struct queue_t *q, void *val_p)
{
	struct node_t *node = (struct node_t *) malloc(sizeof(struct node_t));

	node->val_p = val_p;
	if (q->head)
		q->head->prev = node;
	node->next = q->head;
	node->prev = NULL;
	q->head = node;
	if (!q->tail)
		q->tail = node;
}

static void push_back(struct queue_t *q, void *val_p)
{
	struct node_t *node = (struct node_t *) malloc(sizeof(struct node_t));

	node->val_p = val_p;
	node->next = NULL;
	if (q->tail)
		q->tail->next = node;
	node->prev = q->tail;
	q->tail = node;
	if (!q->head)
		q->head = node;
}

static void* pop_front(struct queue_t* q)
{
	if (!q->head)
		return NULL;

	struct node_t *front = q->head;
	struct node_t *next = q->head->next;

	void *val_p = front->val_p;
	q->head = next;
	if (next)
		next->prev = NULL;

	free(front);

	return val_p;
}

static void reset_iterator(struct queue_t* q)
{
	q->curr = q->head;
}

static void *get_next(struct queue_t *q)
{
	if (!q->curr)
		return NULL;

	void *val_p = q->curr->val_p;;

	q->curr = q->curr->next;

	return val_p;
}

static void *front(struct queue_t* q)
{
	if (q->head)
		return q->head->val_p;
	else
		return NULL;
}

#if 0
static void *back(struct queue_t* q)
{
	if (q->tail)
		return q->tail->val_p;
	else
		return NULL;
}
#endif

static int get_operator(const char c, struct token_t *tok)
{
	unsigned int i;

	for (i = 0; i < sizeof(operators)/sizeof(operators[0]); i++) {
		if (operators[i].op_char == c) {
			tok->op = &operators[i];
			return operators[i].op_num;
		}
	}

	tok->op = NULL;

	return INVALID_OPERATOR;
}

struct token_t *get_expr_token(const char *expr, const char **endptr)
{
	if (!expr || !(*expr))
		return NULL;

	struct token_t tok;
	char c = *expr;
	char *tmp;

	/* */
	tmp = (char *) expr + 1;

	if(isdigit(c)) {
		//d = strtod(expr, &tmp);
		tok.val = strtod(expr, &tmp);
		if (expr == tmp) {
			pr_err("Not a valid number at :%s\n", expr);
			goto error;
		}
		tok.type = OPERAND_CONST;
	} else if (c == 'x' || c == 'X') {
		tok.type = OPERAND_X;
	} else if (c == '(') {
		tok.type = LEFT_PARENTHESIS;
	} else if (c == ')') {
		tok.type = RIGHT_PARENTHESIS;
	} else {
		if (get_operator(c, &tok) == INVALID_OPERATOR) {
			pr_err("Not a valid token at :%s\n", expr);
			goto error;
		}
		tok.type = OPERATOR;
	}

	struct token_t *ret_tok = (struct token_t *) malloc(sizeof(struct token_t));
	if (ret_tok) {
		*ret_tok = tok;
		*endptr = tmp;
	}

	return ret_tok;

error:
	*endptr = expr;
	return NULL;
}

/*
Logic:
- Read a token.
- If the token is a number, then add it to the output queue.
- If the token is an operator, o1, then:
	. while there is an operator token, o2, at the top of the operator stack, and either
		o1 is left-associative and its precedence is less than or equal to that of o2, or
		o1 is right associative, and has precedence less than that of o2,
	. then pop o2 off the operator stack, onto the output queue;
	. push o1 onto the operator stack.
- If the token is a left parenthesis (i.e. "("), then push it onto the stack.
- If the token is a right parenthesis (i.e. ")"):
	. Until the token at the top of the stack is a left parenthesis, pop operators off the stack onto the output queue.
	. Pop the left parenthesis from the stack, but not onto the output queue.
	. If the stack runs out without finding a left parenthesis, then there are mismatched parentheses.
- When there are no more tokens to read:
	. While there are still operator tokens in the stack:
		If the operator token on the top of the stack is a parenthesis, then there are mismatched parentheses.
		Pop the operator onto the output queue.
- Exit.
*/
struct queue_t *parse_expression(const char *expr)
{
	struct queue_t *outq = create_queue();
	struct queue_t *op_stack = create_queue();
	struct token_t *tok, *tmp_tok;

	while ((tok = get_expr_token(expr, &expr))) {
	switch(tok->type) {
	case OPERAND_CONST:
	case OPERAND_X:
		push_back(outq, tok);
		break;
	case OPERATOR:
		while ((tmp_tok = (struct token_t *) front(op_stack))) {
			if ((tok->op->assoc == LEFT_ASSOCIATIVE && tok->op->prec <= tmp_tok->op->prec) ||
			    (tok->op->assoc == RIGHT_ASSOCIATIVE && tok->op->prec < tmp_tok->op->prec)) {
				push_back(outq, tmp_tok);
				pop_front(op_stack);
			} else {
				break;
			}
		}
		push_front(op_stack, tok);
		//push_front(op_stack, tok);
		break;
	case LEFT_PARENTHESIS:
		push_front(op_stack, tok);
		break;
	case RIGHT_PARENTHESIS:
		while ((tmp_tok = (struct token_t *) pop_front(op_stack)) &&
			(tmp_tok->type != LEFT_PARENTHESIS)) {
			push_back(outq, tmp_tok);
		}
		free(tok);
		if (tmp_tok) {
			/* Left parenthesis, we don't need it anymore either */
			free(tmp_tok);
		} else {
			pr_err("Unmatched parenthesis, no left parenthesis matching right parenthesis at:%s\n", expr - 1);
			goto error;
		}
		break;
	default:
		pr_err("Unexpected token just before :%s", expr);
		goto error;
		break;
	}
	}

	/* End of stream, pop out the remaining opertors from operator stack */
	while ((tmp_tok = (struct token_t *) pop_front(op_stack)) &&
		(tmp_tok->type != LEFT_PARENTHESIS)) {
		push_back(outq, tmp_tok);
	}

	if (tmp_tok) {
		pr_err("Unmatched parenthesis, unmatched left parenthesis\n");
		goto error;
	}

	delete_queue(op_stack);
	return outq;

error:
	delete_queue(op_stack);
	delete_queue(outq);
	pr_err("Parsing failed!\n");
	return NULL;
}

/*
 * Evaluate reverse polish notation expression:
 * rpn_q - Expression in Reverse Polish(postfix) notation
 * Logic: https://en.wikipedia.org/wiki/Reverse_Polish_notation#Postfix_algorithm
 * limited to binary operators
 */
double evaluate_expr(struct queue_t *rpn_q, double val)
{
	double result;
	struct token_t *tok, *tok_tmp, *opnd1, *opnd2;
	struct queue_t *opnd_stack = create_queue();

	reset_iterator(rpn_q);
	while((tok = (struct token_t *) get_next(rpn_q))) {
		switch(tok->type) {
		case OPERAND_CONST:
		case OPERAND_X:
			tok_tmp = (struct token_t *) malloc(sizeof(struct token_t));
			*tok_tmp = *tok;
			if (tok->type == OPERAND_X)
				tok_tmp->val = val;
			push_front(opnd_stack, tok_tmp);
			break;
		case OPERATOR:
			opnd2 = (struct token_t *) pop_front(opnd_stack);
			opnd1 = (struct token_t *) front(opnd_stack);
			if (!opnd1 || !opnd2) {
				pr_err("Not enough operands for operator :%c\n", tok->op->op_char)
				return 0;
			}
			opnd1->val = tok->op->fp(opnd1->val, opnd2->val);
			free(opnd2);
			break;
		default:
			break;
		}
	}

	reset_iterator(opnd_stack);
	opnd1 = (struct token_t *) get_next(opnd_stack);
	opnd2 = (struct token_t *) get_next(opnd_stack);
	result = opnd1->val;
	delete_queue(opnd_stack);
	if (opnd2) {
		pr_err("More than one operand remaining in the stack, but no more operators remainig\n");
		return 0;
	}

	return result;
}
