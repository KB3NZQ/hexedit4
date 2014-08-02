// expr.cpp : Handles C-like expression evaluation
//
// Copyright (c) 2012 by Andrew W. Phillips.
//
// This file is distributed under the MIT license, which basically says
// you can do what you want with it but I take no responsibility for bugs.
// See http://www.opensource.org/licenses/mit-license.php for full details.
//

// Expressions support 4 data types: int (64 bit), real (double = 64 bit), boolean, date.
// Type checking is stricter than C/C++.  Variables (including
// multi-dimensional arrays) are supported through a map.  Arrays are allocated on
// an element by element basis and so may be slow.  All integer arithmetic is done
// in 64-bit so this also may be slow.  Integer (including characters like 'a'),
// real, and boolean (TRUE/FALSE) constants are handled.
//
// The precedence of operators is:
// , (comma operator)
// = (assignment)
// ?:
// ||
// &&
// ==  !=  <  <=  >  >=
// |
// ^
// &
// <<  >>
// +  -
// *  /
// unary operators, ( expr ), [ expr ], etc
//

#include "stdafx.h"
#undef _MAX
#include <algorithm>
#include <cctype>
#include <cstring>
#include <cerrno>
#include <cassert>
#include <clocale>

// These are needed for date string parsing
#include <locale>
#include <iostream>
#include <sstream>
#include <time.h>

#include "HexEdit.h"
#include "expr.h"
#include "dialog.h"
#include "misc.h"

expr_eval::expr_eval(int mr /*=10*/, bool csa /*=false*/)
{
	p_ = NULL;
	psymbol_ = NULL;
	var_.clear();
	var_changed_ = clock();
	max_radix_ = mr;                // Max radix used in integer literals
	const_sep_allowed_ = csa;       // Are separators allowed in int constants

	dec_sep_char_ = theApp.dec_sep_char_;
	dec_point_    = theApp.dec_point_;

	changes_on_ = true;
}

// expr is the expression to evaluate, possibly including names (symbols)
// reference is used as a starting reference point in the resolution of symbols
// ref_ac returns the most recent symbol accessed (used for checking for var/fixed arrays)
// radix is the used to interpret integer literals, the default (10) means decimal

// Note: expr is single byte (ANSI) string so cannot include any Unicode string
// literals, however string expressions can include Unicode strings.
expr_eval::value_t expr_eval::evaluate(const char *expr, int reference, int &ref_ac,
									   int radix /* = 10 */, bool side_effects /* = true */)
{
	value_t retval;                     // Returned value of the expression
	tok_t next_tok;                     // Last token found (TOK_EOL if no error)

	assert(p_ == NULL);
	p_ = expr;
	ref_ = reference;
	pac_ = &ref_ac;
	changes_on_ = side_effects;

	ASSERT(radix > 1 && radix <= max_radix_);  // Ensure we don't erroneously allow hex consts

	const_radix_ = radix;

	*pac_ = -1;

	error_buf_[0] = '\0';
	if ((next_tok = prec_lowest(retval)) != TOK_EOL)
	{
		if (error_buf_[0] == '\0')
			strcpy(error_buf_, "Unexpected character");
		retval.typ = TYPE_NONE;
		error_pos_ = saved_ - expr;
	}
	else if (retval.error)
	{
		if (error_buf_[0] == '\0')
			strcpy(error_buf_, "Invalid value(s)");
		retval.typ = TYPE_NONE;
		error_pos_ = saved_ - expr;
	}


	p_ = NULL; pac_ = NULL;             // Set back to NULL so we can detect bad accesses
	return retval;
}

expr_eval::tok_t expr_eval::prec_comma(value_t &val)
{
	tok_t next_tok = prec_assign(val);

	while (next_tok == TOK_COMMA)
	{
		if (error(next_tok = prec_assign(val), "Expected another expression after comma"))
			return TOK_NONE;
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_assign(value_t &val)
{
	CString vname;                      // Variable name is just an assignment
	tok_t next_tok = prec_ternary(val, vname);
	if (next_tok == TOK_ASSIGN)
	{
		vname.MakeUpper();
		if (vname.IsEmpty())
		{
			strcpy(error_buf_, "Assignment requires a variable name on the left of the equals sign");
			return TOK_NONE;
		}
		//if (func_token(vname) != TOK_NONE)
		//{
		//    strcpy(error_buf_, "Assignment attempted to reserved name:       ");
		//    strncpy(error_buf_ + 39, vname, 200);
		//    return TOK_NONE;
		//}

		if (error(next_tok = prec_assign(val), "Expected expression for assignment"))
			return TOK_NONE;

		// Only assign the value if side effects allowed
		if (changes_on_)
		{
			var_[vname] = val;
			var_changed_ = clock();
		}
	}
	else if (val.typ == TYPE_NONE)
	{
		if (error_buf_[0] == '\0')
		{
			if (vname.IsEmpty())
				sprintf(error_buf_, "Invalid expression");
			else
				sprintf(error_buf_, "Unknown variable \"%.200s\"", vname);
		}
		return TOK_NONE;
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_ternary(value_t &val, CString &vname)
{
	tok_t next_tok = prec_or(val, vname);

	while (next_tok == TOK_QUESTION)
	{
		if (val.typ != TYPE_BOOLEAN)
		{
			strcpy(error_buf_, "Condition for ternary operator must be BOOLEAN");
			return TOK_NONE;
		}
		// Evaluate both branches
		value_t v1, v2;

		bool saved_changes_on;

		// Parse/evaluate "if" part
		if (!val.boolean)
		{
			// Turn off any side-effects (see description where changes_on_ is declared)
			saved_changes_on = changes_on_;     // remember old value
			changes_on_ = false;
		}
		if (error(next_tok = prec_or(v1, vname), "Expected \"if\" expression for conditional (?:) operator"))
			if (val.boolean)
				return TOK_NONE;        // we needed a valid expression
			else
				next_tok = get_next();  // we didn't need it as we are only using the other (else) part

		if (!val.boolean)
			changes_on_ = saved_changes_on;     // restore old value

		// Make sure "if" and "else" parts are separated by a colon
		if (next_tok != TOK_COLON)
		{
			strcpy(error_buf_, "Expected colon (:) for conditional (?:) operator");
			return TOK_NONE;
		}

		// Parse/evaluate "else" part
		if (val.boolean)
		{
			// Turn off any side-effects (see description where changes_on_ is declared)
			saved_changes_on = changes_on_;     // remember old value
			changes_on_ = false;
		}
		if (error(next_tok = prec_or(v2, vname), "Expected \"else\" expression for conditional (?:) operator"))
			if (!val.boolean)
				return TOK_NONE;        // we needed a valid expression
			else
				next_tok = get_next();  // we didn't need it as we are only using the above (if) part

		if (val.boolean)
			changes_on_ = saved_changes_on;     // restore old value

		if (val.boolean)
			val = v1;
		else
			val = v2;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_or(value_t &val, CString &vname)
{
	tok_t next_tok = prec_and(val, vname);

	while (next_tok == TOK_OR)
	{
		bool saved_changes_on;
		if (val.boolean)
		{
			// Turn off any side-effects (see description where changes_on_ is declared)
			saved_changes_on = changes_on_;     // remember old value
			changes_on_ = false;
		}
		value_t op2;
		if (error(next_tok = prec_and(op2, vname), "Expected 2nd operand for OR (||)"))
			return TOK_NONE;

		if (val.boolean)
		{
			// If left operand is true then result is true and we don't care about right operand
			// (even if it gets and eval error as long as it parses OK).
			op2.error = false;
			changes_on_ = saved_changes_on;     // restore old value
		}
		else if (val.typ != TYPE_BOOLEAN || op2.typ != TYPE_BOOLEAN)
		{
			strcpy(error_buf_, "Operands for OR (||) must be BOOLEAN");
			return TOK_NONE;
		}
		else
		{
			val.boolean = op2.boolean;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_and(value_t &val, CString &vname)
{
	tok_t next_tok = prec_comp(val, vname);

	while (next_tok == TOK_AND)
	{
		bool saved_changes_on;
		if (!val.boolean)
		{
			// Turn off any side-effects (see description where changes_on_ is declared)
			saved_changes_on = changes_on_;
			changes_on_ = false;
		}

		value_t op2;
		if (error(next_tok = prec_comp(op2, vname), "Expected 2nd operand for AND (&&)"))
			return TOK_NONE;

		if (!val.boolean)
		{
			// If left operand is false then result is false and we don't care about right operand
			// (even if it gets an eval error as long as it parses OK).
			op2.error = false;
			changes_on_ = saved_changes_on;
		}
		else if (val.typ != TYPE_BOOLEAN || op2.typ != TYPE_BOOLEAN)
		{
			strcpy(error_buf_, "Operands for AND (&&) must be BOOLEAN");
			return TOK_NONE;
		}
		else
		{
			val.boolean = op2.boolean;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_comp(value_t &val, CString &vname)
{
	tok_t next_tok = prec_bitor(val, vname);

	while (next_tok == TOK_EQ || next_tok == TOK_NE ||
		   next_tok == TOK_LT || next_tok == TOK_LE ||
		   next_tok == TOK_GT || next_tok == TOK_GE)
	{
		value_t op2;
		tok_t next_tok2;
		if (error(next_tok2 = prec_bitor(op2, vname), "Expected 2nd operand for comparison"))
			return TOK_NONE;

		if ((val.typ != TYPE_STRING || op2.typ != TYPE_STRING) &&
			(val.typ != TYPE_DATE   || op2.typ != TYPE_DATE) &&
			(val.typ != TYPE_REAL && val.typ != TYPE_INT || 
			 op2.typ != TYPE_REAL && op2.typ != TYPE_INT) )
		{
			strcpy(error_buf_, "Illegal operand types for comparison");
			return TOK_NONE;
		}

		// Possible order of evaluation problems since val.boolean is assigned
		// in the same expression as val is used??
		switch (next_tok)
		{
		case TOK_EQ:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) == *(op2.pstr);  // CString(W) operator==
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date == op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) == make_real(op2);
			}
			else
			{
				val.boolean = val.int64 == op2.int64;
			}
			break;
		case TOK_NE:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) != *(op2.pstr);  // CString operator!=
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date != op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) != make_real(op2);
			}
			else
			{
				val.boolean = val.int64 != op2.int64;
			}
			break;
		case TOK_LT:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) < *(op2.pstr);  // CString operator<
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date < op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) < make_real(op2);
			}
			else
			{
				val.boolean = val.int64 < op2.int64;
			}
			break;
		case TOK_LE:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) <= *(op2.pstr);  // CString operator<=
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date <= op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) <= make_real(op2);
			}
			else
			{
				val.boolean = val.int64 <= op2.int64;
			}
			break;
		case TOK_GT:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) > *(op2.pstr);  // CString operator>
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date > op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) > make_real(op2);
			}
			else
			{
				val.boolean = val.int64 > op2.int64;
			}
			break;
		case TOK_GE:
			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				bool retval = *(val.pstr) >= *(op2.pstr);  // CString operator>=
				delete val.pstr;
				val.boolean = retval;
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				val.boolean = val.date >= op2.date;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.boolean = make_real(val) >= make_real(op2);
			}
			else
			{
				val.boolean = val.int64 >= op2.int64;
			}
			break;
		default:
			assert(0);
			return TOK_NONE;
		}
		next_tok = next_tok2;
		val.typ = TYPE_BOOLEAN;
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_bitor(value_t &val, CString &vname)
{
	tok_t next_tok = prec_xor(val, vname);

	while (next_tok == TOK_BITOR)
	{
		value_t op2;
		if (error(next_tok = prec_xor(op2, vname), "Expected 2nd operand of bitwise OR (|)"))
			return TOK_NONE;

		if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Bitwise OR requires integer operands");
			return TOK_NONE;
		}
		else
		{
			val.int64 = val.int64 | op2.int64;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_xor(value_t &val, CString &vname)
{
	tok_t next_tok = prec_bitand(val, vname);

	while (next_tok == TOK_XOR)
	{
		value_t op2;
		if (error(next_tok = prec_bitand(op2, vname), "Expected 2nd operand of exclusive OR (^)"))
			return TOK_NONE;

		if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Exclusive OR (^) requires integer operands");
			return TOK_NONE;
		}
		else
		{
			val.int64 = val.int64 ^ op2.int64;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_bitand(value_t &val, CString &vname)
{
	tok_t next_tok = prec_shift(val, vname);

	while (next_tok == TOK_BITAND)
	{
		value_t op2;
		if (error(next_tok = prec_shift(op2, vname), "Expected 2nd operand of bitwise AND (&)"))
			return TOK_NONE;

		if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Bitwise AND requires integer operands");
			return TOK_NONE;
		}
		else
		{
			val.int64 = val.int64 & op2.int64;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_shift(value_t &val, CString &vname)
{
	tok_t next_tok = prec_add(val, vname);

	while (next_tok == TOK_SHL || next_tok == TOK_SHR)
	{
		value_t op2;
		switch (next_tok)
		{
		case TOK_SHL:
			if (error(next_tok = prec_add(op2, vname), "Expected 2nd operand of shift left"))
				return TOK_NONE;

			if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Left shift operands must be integers");
				return TOK_NONE;
			}
			else
			{
//                val.typ = TYPE_INT;
				val.int64 = val.int64 << op2.int64;
			}
			break;
		case TOK_SHR:
			if (error(next_tok = prec_add(op2, vname), "Expected 2nd operand of shift right"))
				return TOK_NONE;

			if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Right shift operands must be integers");
				return TOK_NONE;
			}
			else
			{
//                val.typ = TYPE_INT;
				val.int64 = val.int64 >> op2.int64;
			}
			break;
		default:
			assert(0);
			return TOK_NONE;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_add(value_t &val, CString &vname)
{
	tok_t next_tok = prec_mul(val, vname);

	while (next_tok == TOK_PLUS || next_tok == TOK_MINUS)
	{
		value_t op2;
		switch (next_tok)
		{
		case TOK_PLUS:
			if (error(next_tok = prec_mul(op2, vname), "Expected 2nd operand for addition"))
				return TOK_NONE;

			if (val.typ == TYPE_STRING && op2.typ == TYPE_STRING)
			{
				// "string" + "string"
				*(val.pstr) += *(op2.pstr);   // CString operator+=
			}
			else if (val.typ == TYPE_REAL && op2.typ == TYPE_REAL)
			{
				// eg 0.1 + 0.2
				val.real64 += op2.real64;
			}
			else if (val.typ == TYPE_REAL && op2.typ == TYPE_INT || 
					 val.typ == TYPE_INT  && op2.typ == TYPE_REAL)
			{
				// eg 2 + 0.1
				val.real64 = make_real(val) + make_real(op2);
				val.typ = TYPE_REAL;
			}
			else if (val.typ == TYPE_INT && op2.typ == TYPE_INT)
			{
				// eg 2 + 3
				val.int64 = val.int64 + op2.int64;
			}
			else if (val.typ == TYPE_DATE &&
					 (op2.typ == TYPE_INT || op2.typ == TYPE_REAL))
			{
				// eg now + 1.0
				val.date += make_real(op2);
			}
			else if (op2.typ == TYPE_DATE &&
					 (val.typ == TYPE_INT || val.typ == TYPE_REAL))
			{
				// eg 1.0 + now
				val.date = make_real(val) + op2.date;
				val.typ = TYPE_DATE;
			}
			else
			{
				strcpy(error_buf_, "Illegal operand types for addition");
				return TOK_NONE;
			}
			break;
		case TOK_MINUS:
			if (error(next_tok = prec_mul(op2, vname), "Expected 2nd operand of subtraction"))
				return TOK_NONE;

			if (val.typ == TYPE_REAL && op2.typ == TYPE_REAL)
			{
				val.real64 -= op2.real64;
			}
			else if (val.typ == TYPE_REAL && op2.typ == TYPE_INT || 
					 val.typ == TYPE_INT  && op2.typ == TYPE_REAL)
			{
				val.real64 = make_real(val) - make_real(op2);
				val.typ = TYPE_REAL;
			}
			else if (val.typ == TYPE_INT && op2.typ == TYPE_INT)
			{
				val.int64 -= op2.int64;
			}
			else if (val.typ == TYPE_DATE && (op2.typ == TYPE_INT || op2.typ == TYPE_REAL))
			{
				// Subtract number from date
				val.date -= make_real(op2);
			}
			else if (val.typ == TYPE_DATE && op2.typ == TYPE_DATE)
			{
				// Return date difference as a number
				val.real64 = make_real(val) - make_real(op2);
				val.typ = TYPE_REAL;
			}
			else
			{
				strcpy(error_buf_, "Illegal operand types for subtraction");
				return TOK_NONE;
			}
			break;
		default:
			assert(0);
			return TOK_NONE;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_mul(value_t &val, CString &vname)
{
	tok_t next_tok = prec_prim(val, vname);

	while (next_tok == TOK_MUL || next_tok == TOK_DIV || next_tok == TOK_MOD)
	{
		value_t op2;
		switch (next_tok)
		{
		case TOK_MUL:
			if (error(next_tok = prec_prim(op2, vname), "Expected 2nd operand for multiplication"))
				return TOK_NONE;

			if (val.typ != TYPE_REAL && val.typ != TYPE_INT || 
				op2.typ != TYPE_REAL && op2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Illegal operand types for multiplication");
				return TOK_NONE;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				val.real64 = make_real(val) * make_real(op2);
				val.typ = TYPE_REAL;
			}
			else
			{
				val.int64 = val.int64 * op2.int64;
			}
			break;
		case TOK_DIV:
			if (error(next_tok = prec_prim(op2, vname), "Expected 2nd operand of division"))
				return TOK_NONE;

			if (val.typ != TYPE_REAL && val.typ != TYPE_INT || 
				op2.typ != TYPE_REAL && op2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Illegal operand types for division");
				return TOK_NONE;
			}
			else if (val.typ == TYPE_REAL || op2.typ == TYPE_REAL)
			{
				double tmp = make_real(op2);
				if (tmp == 0.0)
				{
					strcpy(error_buf_, "Divide by zero");
					return TOK_NONE;
				}
				val.real64 = make_real(val) / tmp;
				val.typ = TYPE_REAL;
			}
			else if (op2.int64 == 0)
			{
				strcpy(error_buf_, "Integer divide by zero");
				return TOK_NONE;
			}
			else
			{
				val.int64 = val.int64 / op2.int64;
			}
			break;
		case TOK_MOD:
			if (error(next_tok = prec_prim(op2, vname), "Expected 2nd operand of modulus"))
				return TOK_NONE;

			if (val.typ != TYPE_INT || op2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Modulus operands must be integers");
				return TOK_NONE;
			}
			else if (op2.int64 == 0)
			{
				strcpy(error_buf_, "Divide by zero (modulus operation)");
				return TOK_NONE;
			}
			else
			{
				val.int64 = val.int64 % op2.int64;
			}
			break;
		default:
			assert(0);
			return TOK_NONE;
		}
		val.error = val.error || op2.error;

		vname.Empty();
	}

	return next_tok;
}

expr_eval::tok_t expr_eval::prec_prim(value_t &val, CString &vname)
{
	vname.Empty();
	std::map<CString, value_t>::const_iterator pv;
	tok_t next_tok = get_next();
	value_t tmp;
	__int64 sym_size, sym_address;
	CString sym_str;
	bool saved_const_sep_allowed;
	GetInt *pGetIntDlg = NULL;
	GetStr *pGetStrDlg = NULL;
	GetBool *pGetBoolDlg = NULL;
	int num_bits;                // how many bits are used in bit-wise operations (ROL, REVERSE, etc)

	switch (next_tok)
	{
	case TOK_CONST:
		val = last_val_;
		return get_next();
	case TOK_SIZEOF:
		// Get opening bracket and first symbol within
		if (error(next_tok = get_next(), "Expected opening parenthesis after SIZEOF"))
			return TOK_NONE;

		if (next_tok != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after SIZEOF");
			return TOK_NONE;
		}
		if (error(next_tok = get_next(), "Expected symbol name"))
			return TOK_NONE;

		// We handle special cases of sizeof(int) and sizeof(char) for TParser use of expressions
		tmp.typ = TYPE_NONE;
		tmp.int64 = ref_;
		if (next_tok == TOK_SYMBOL)
		{
			val = find_symbol(psymbol_, tmp, 0, pac_, sym_size, sym_address, sym_str);
		}
		else if (next_tok == TOK_INT)
		{
			val = find_symbol("int", tmp, 0, pac_, sym_size, sym_address, sym_str);
		}
		else if (next_tok == TOK_CHAR)
		{
			val = find_symbol("char", tmp, 0, pac_, sym_size, sym_address, sym_str);
		}
		else
		{
			strcpy(error_buf_, "Symbol expected");
			return TOK_NONE;
		}
		if (val.typ == TYPE_NONE)
		{
			// Not found in derived class so check our global vars (var_)
			CString ss(psymbol_);
			next_tok = get_var(val, ss);
			switch (val.typ)
			{
			case TYPE_INT:
				sym_size = sizeof(val.int64);
				break;
			case TYPE_BOOLEAN:
				sym_size = sizeof(val.boolean);
				break;
			case TYPE_REAL:
				sym_size = sizeof(val.real64);
				break;
			case TYPE_STRING:
				sym_size = val.pstr->GetLength() * 2;  // Unicode chars are 2 bytes
				break;
			case TYPE_DATE:
				sym_size = sizeof(val.date);
				break;
			default:
				sym_size = 0;
				val.typ = TYPE_NONE;
				break;
			}
		}
		else
			next_tok = get_next();

		while (next_tok == TOK_DOT || next_tok == TOK_LBRA)
		{
			switch (next_tok)
			{
			case TOK_DOT:
				if (val.typ != TYPE_STRUCT)
				{
					strcpy(error_buf_, "Dot operator (.) must only be used with struct");
					return TOK_NONE;
				}
				if (error(next_tok = get_next(), "Expected member name"))
					return TOK_NONE;

				if (next_tok != TOK_SYMBOL)
				{
					strcpy(error_buf_, "Symbol expected");
					return TOK_NONE;
				}
				val = find_symbol(psymbol_, val, 0, pac_, sym_size, sym_address, sym_str);
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Unrecognised member name for struct");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			case TOK_LBRA:
				if (val.typ != TYPE_ARRAY)
				{
					strcpy(error_buf_, "Array index on non-array symbol");
					return TOK_NONE;
				}
				if (error(next_tok = prec_lowest(tmp), "Expected array index"))
					return TOK_NONE;

				if (next_tok != TOK_RBRA)
				{
					strcpy(error_buf_, "Closing bracket (]) expected");
					return TOK_NONE;
				}
				else if (tmp.typ != TYPE_INT)
				{
					strcpy(error_buf_, "Array (for) index must be an integer");
					return TOK_NONE;
				}
				val = find_symbol(NULL, val, size_t(tmp.int64), pac_, sym_size, sym_address, sym_str);  // get value from array
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Invalid array (for) index");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			}
		}
		if (val.typ == TYPE_NONE)
		{
			sprintf(error_buf_, "Unknown symbol \"%.200s\"", psymbol_);
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Expected closing parenthesis after SIZEOF");
			return TOK_NONE;
		}
		val.typ = TYPE_INT;
		val.error = false;      // don't care about errors - just need size
		val.int64 = sym_size;
		return get_next();
	case TOK_DEFINED:
		if (error(next_tok = get_next(), "Expected identifier after DEFINED"))
			return TOK_NONE;
		else
		{
			bool in_paren = false;
			if (next_tok == TOK_LPAR)
			{
				in_paren = true;
				if (error(next_tok = get_next(), "Expected identifier for DEFINED"))
					return TOK_NONE;
			}
			if (next_tok != TOK_SYMBOL)
			{
				strcpy(error_buf_, "Symbol expected");
				return TOK_NONE;
			}
			tmp.typ = TYPE_NONE;
			tmp.int64 = ref_;
			val = find_symbol(psymbol_, tmp, 0, pac_, sym_size, sym_address, sym_str);
			if (val.typ == TYPE_NONE)
			{
				// Not found in derived class so check our global vars (var_)
				CString ss(psymbol_);
				next_tok = get_var(val, ss);
			}
			else
				next_tok = get_next();

			val.boolean = val.typ != TYPE_NONE;
			val.typ = TYPE_BOOLEAN;
			val.error = false;      // don't care about errors

			if (in_paren)
			{
				if (next_tok != TOK_RPAR)
				{
					strcpy(error_buf_, "Expected closing parenthesis after DEFINED");
					return TOK_NONE;
				}
				next_tok = get_next();
			}
		}
		return next_tok;
	case TOK_STR:
		// Get opening bracket and first symbol within
		if (error(next_tok = get_next(), "Expected opening parenthesis after STRING"))
			return TOK_NONE;

		if (next_tok != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after STRING");
			return TOK_NONE;
		}
		if (error(next_tok = get_next(), "Expected symbol name"))
			return TOK_NONE;

		if (next_tok != TOK_SYMBOL)
		{
			strcpy(error_buf_, "Symbol expected");
			return TOK_NONE;
		}

		tmp.typ = TYPE_NONE;
		tmp.int64 = ref_;
		val = find_symbol(psymbol_, tmp, 0, pac_, sym_size, sym_address, sym_str);
		if (val.typ == TYPE_NONE)
		{
			// Not found in derived class so check our global vars (var_)
			char buf[32];
			CString ss(psymbol_);
			next_tok = get_var(val, ss);

			switch (val.typ)
			{
			case TYPE_INT:
				sprintf(buf, "%I64d", val.int64);
				sym_str = buf;
				break;
			case TYPE_BOOLEAN:
				sym_str = val.boolean ? "TRUE" : "FALSE";
				break;
			case TYPE_REAL:
				sym_str.Format("%g", val.real64);
				break;
			case TYPE_STRING:
				sym_str = CString(*(val.pstr));  // xxx sym_str should be Unicode
				break;
			// xxx handle TYPE_DATE?
			default:
				sym_str.Empty();
				val.typ = TYPE_NONE;
				break;
			}
		}
		else
			next_tok = get_next();
		while (next_tok == TOK_DOT || next_tok == TOK_LBRA)
		{
			switch (next_tok)
			{
			case TOK_DOT:
				if (val.typ != TYPE_STRUCT)
				{
					strcpy(error_buf_, "Dot operator (.) must only be used with struct");
					return TOK_NONE;
				}
				if (error(next_tok = get_next(), "Expected member name"))
					return TOK_NONE;

				if (next_tok != TOK_SYMBOL)
				{
					strcpy(error_buf_, "Symbol expected");
					return TOK_NONE;
				}
				val = find_symbol(psymbol_, val, 0, pac_, sym_size, sym_address, sym_str);
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Unrecognised member name for struct");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			case TOK_LBRA:
				if (val.typ != TYPE_ARRAY)
				{
					strcpy(error_buf_, "Array index on non-array symbol");
					return TOK_NONE;
				}
				if (error(next_tok = prec_lowest(tmp), "Expected array index"))
					return TOK_NONE;

				if (next_tok != TOK_RBRA)
				{
					strcpy(error_buf_, "Closing bracket (]) expected");
					return TOK_NONE;
				}
				else if (tmp.typ != TYPE_INT)
				{
					strcpy(error_buf_, "Array (for) index must be an integer");
					return TOK_NONE;
				}
				val = find_symbol(NULL, val, size_t(tmp.int64), pac_, sym_size, sym_address, sym_str);
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Invalid array (for) index");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			}
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Expected closing parenthesis after STRING");
			return TOK_NONE;
		}
		if (val.typ == TYPE_NONE)
		{
			sprintf(error_buf_, "Unknown symbol \"%.200s\"", psymbol_);
			return TOK_NONE;
		}

		val.typ = TYPE_STRING;
		val.error = false;      // don't care about errors - just need size
		val.pstr = new ExprStringType(sym_str);
		return get_next();
	case TOK_GETINT:
		// Get opening bracket and first symbol within
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after GETINT");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(val), "Expected prompt string for GETINT"))
		{
			return TOK_NONE;
		}
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for GETINT must be a string");
			return TOK_NONE;
		}
		pGetIntDlg = new GetInt();
		pGetIntDlg->prompt_ = CString(*(val.pstr));
		pGetIntDlg->value_ = 0L;
		pGetIntDlg->min_ = LONG_MIN;
		pGetIntDlg->max_ = LONG_MAX;

		// Get optional parameters: default value, min, max
		if (next_tok == TOK_COMMA)
		{
			value_t tmp2;

			if (error(next_tok = prec_assign(tmp2), "Expected 2nd parameter to GETINT"))
			{
				delete pGetIntDlg;
				return TOK_NONE;
			}
			if (tmp2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "2nd parameter for GETINT must be an integer");
				delete pGetIntDlg;
				return TOK_NONE;
			}
			pGetIntDlg->value_ = long(tmp2.int64);

			if (next_tok == TOK_COMMA)
			{
				value_t tmp3;

				if (error(next_tok = prec_assign(tmp3), "Expected 3rd parameter to GETINT"))
				{
					delete pGetIntDlg;
					return TOK_NONE;
				}
				if (tmp3.typ != TYPE_INT)
				{
					strcpy(error_buf_, "3rd parameter for GETINT must be an integer");
					delete pGetIntDlg;
					return TOK_NONE;
				}
				pGetIntDlg->min_ = long(tmp3.int64);

				if (next_tok == TOK_COMMA)
				{
					value_t tmp4;

					if (error(next_tok = prec_assign(tmp4), "Expected 4th parameter to GETINT"))
					{
						delete pGetIntDlg;
						return TOK_NONE;
					}
					if (tmp4.typ != TYPE_INT)
					{
						strcpy(error_buf_, "4th parameter for GETINT must be an integer");
						delete pGetIntDlg;
						return TOK_NONE;
					}
					pGetIntDlg->max_ = long(tmp4.int64);
				}
			}
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for GETINT");
			delete pGetIntDlg;
			return TOK_NONE;
		}

		// Make sure the value is within range
		if (pGetIntDlg->value_ < pGetIntDlg->min_)
			pGetIntDlg->value_ = pGetIntDlg->min_;
		if (pGetIntDlg->value_ > pGetIntDlg->max_)
			pGetIntDlg->value_ = pGetIntDlg->max_;

		ASSERT(val.typ == TYPE_STRING);   // make sure its has a string before freeing the memory
		delete val.pstr;
		val.typ = TYPE_INT;
		if (changes_on_)
		{
			if (pGetIntDlg->DoModal() != IDOK)
			{
				strcpy(error_buf_, "GETINT cancelled");
				delete pGetIntDlg;
				return TOK_NONE;
			}
			val.int64 = pGetIntDlg->value_;
		}
		else
		{
			val.int64 = 0;
		}
		delete pGetIntDlg;
		return get_next();

	case TOK_GETSTR:
		// Get opening bracket and first symbol within
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after GETSTRING");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(val), "Expected prompt string for GETSTRING"))
		{
			return TOK_NONE;
		}
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for GETSTRING must be a string");
			return TOK_NONE;
		}

		pGetStrDlg = new GetStr();
		pGetStrDlg->prompt_ = CString(*(val.pstr));

		// Get optional parameter: default value
		if (next_tok == TOK_COMMA)
		{
			value_t tmp2;

			if (error(next_tok = prec_assign(tmp2), "Expected 2nd parameter to GETSTRING"))
			{
				delete pGetStrDlg;
				return TOK_NONE;
			}
			if (tmp2.typ != TYPE_STRING)
			{
				strcpy(error_buf_, "2nd parameter for GETSTRING must be a string");
				delete pGetStrDlg;
				return TOK_NONE;
			}
			pGetStrDlg->value_ = CString(*(tmp2.pstr));
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for GETSTRING");
			delete pGetStrDlg;
			return TOK_NONE;
		}

		ASSERT(val.typ == TYPE_STRING);   // make sure its has a string before freeing the memory
		if (changes_on_)
		{
			if (pGetStrDlg->DoModal() != IDOK)
			{
				strcpy(error_buf_, "GETSTRING cancelled");
				delete pGetStrDlg;
				return TOK_NONE;
			}
			*val.pstr = pGetStrDlg->value_;
		}
		else
		{
			*val.pstr = ExprStringType();
		}
		delete pGetStrDlg;
		return get_next();

	case TOK_GETBOOL:
		// Get opening bracket and first symbol within
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after GETBOOL");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(val), "Expected prompt string for GETBOOL"))
		{
			return TOK_NONE;
		}
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for GETBOOL must be a string");
			return TOK_NONE;
		}

		pGetBoolDlg = new GetBool();
		pGetBoolDlg->prompt_ = CString(*(val.pstr));

		// Get optional parameters: default value, min, max
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(val), "Expected 2nd parameter to GETBOOL"))
			{
				delete pGetBoolDlg;
				return TOK_NONE;
			}
			if (val.typ != TYPE_STRING)
			{
				strcpy(error_buf_, "2nd parameter for GETBOOL must be a string");
				delete pGetBoolDlg;
				return TOK_NONE;
			}
			pGetBoolDlg->yes_ = CString(*(val.pstr));

			if (next_tok == TOK_COMMA)
			{
				if (error(next_tok = prec_assign(val), "Expected 3rd parameter to GETBOOL"))
				{
					delete pGetBoolDlg;
					return TOK_NONE;
				}
				if (val.typ != TYPE_STRING)
				{
					strcpy(error_buf_, "3rd parameter for GETBOOL must be a string");
					delete pGetBoolDlg;
					return TOK_NONE;
				}
				pGetBoolDlg->no_ = CString(*(val.pstr));
			}
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for GETBOOL");
			delete pGetBoolDlg;
			return TOK_NONE;
		}

		ASSERT(val.typ == TYPE_STRING);   // make sure its has a string before freeing the memory
		delete val.pstr;
		val.typ = TYPE_BOOLEAN;
		if (changes_on_)
			val.boolean = pGetBoolDlg->DoModal() == IDOK;
		else
			val.boolean = false;

		delete pGetBoolDlg;
		return get_next();

	case TOK_ADDRESSOF:
		// Get opening bracket and first symbol within
		if (error(next_tok = get_next(), "Expected opening parenthesis after ADDRESSOF"))
			return TOK_NONE;

		if (next_tok != TOK_LPAR)
		{
			strcpy(error_buf_, "Expected opening parenthesis after ADDRESSOF");
			return TOK_NONE;
		}
		if (error(next_tok = get_next(), "Expected symbol name"))
			return TOK_NONE;

		if (next_tok != TOK_SYMBOL)
		{
			strcpy(error_buf_, "Symbol expected");
			return TOK_NONE;
		}

		tmp.typ = TYPE_NONE;
		tmp.int64 = ref_;
		val = find_symbol(psymbol_, tmp, 0, pac_, sym_size, sym_address, sym_str);
		next_tok = get_next();
		while (next_tok == TOK_DOT || next_tok == TOK_LBRA)
		{
			switch (next_tok)
			{
			case TOK_DOT:
				if (val.typ != TYPE_STRUCT)
				{
					strcpy(error_buf_, "Dot operator (.) must only be used with struct");
					return TOK_NONE;
				}
				if (error(next_tok = get_next(), "Expected member name"))
					return TOK_NONE;

				if (next_tok != TOK_SYMBOL)
				{
					strcpy(error_buf_, "Symbol expected");
					return TOK_NONE;
				}
				val = find_symbol(psymbol_, val, 0, pac_, sym_size, sym_address, sym_str);
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Unrecognised member name for struct");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			case TOK_LBRA:
				if (val.typ != TYPE_ARRAY)
				{
					strcpy(error_buf_, "Array index on non-array symbol");
					return TOK_NONE;
				}
				if (error(next_tok = prec_lowest(tmp), "Expected array index"))
					return TOK_NONE;

				if (next_tok != TOK_RBRA)
				{
					strcpy(error_buf_, "Closing bracket (]) expected");
					return TOK_NONE;
				}
				else if (tmp.typ != TYPE_INT)
				{
					strcpy(error_buf_, "Array (for) index must be an integer");
					return TOK_NONE;
				}
				val = find_symbol(NULL, val, size_t(tmp.int64), pac_, sym_size, sym_address, sym_str);
				if (val.typ == TYPE_NONE)
				{
					strcpy(error_buf_, "Invalid array (for) index");
					return TOK_NONE;
				}
				next_tok = get_next();
				break;
			}
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Expected closing parenthesis after ADDRESSOF");
			return TOK_NONE;
		}

		// Note for templates (uses CHexExpr::find_symbol) sym_address == -1 if symbol is not found
		// For other expressions (eg CJumpExpr::find_symbol) sym_address == 0 always (whether or
		// not the symbol exists) but you are only supposed to use addressof() in templates.
		if (sym_address < 0)
		{
			sprintf(error_buf_, "Symbol \"%.200s\" not in file", psymbol_);
			return TOK_NONE;
		}

		val.typ = TYPE_INT;
		val.error = false;      // don't care about errors - just need size
		val.int64 = sym_address;
		return get_next();
	case TOK_ABS:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"abs\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"abs\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"abs\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT)
		{
			val.int64 = val.int64 < 0 ? -val.int64 : val.int64;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = fabs(val.real64);
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"abs\" must be numeric");
			return TOK_NONE;
		}
		return get_next();
	case TOK_MIN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"min\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"min\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		if (val.typ != TYPE_REAL && val.typ != TYPE_INT)
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			strcpy(error_buf_, "Parameter for \"min\" must be numeric");
			return TOK_NONE;
		}

		while (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected next parameter for \"min\""))
			{
				const_sep_allowed_ = saved_const_sep_allowed;
				return TOK_NONE;
			}

			if (tmp.typ != TYPE_REAL && tmp.typ != TYPE_INT)
			{
				const_sep_allowed_ = saved_const_sep_allowed;
				strcpy(error_buf_, "Parameter for \"min\" must be numeric");
				return TOK_NONE;
			}
			else if (val.typ == TYPE_INT && tmp.typ == TYPE_INT)
			{
				if (tmp.int64 < val.int64)
					val.int64 = tmp.int64;
			}
			else
			{
				double dd = make_real(tmp);
				val.real64 = make_real(val);
				if (dd < val.real64) val.real64 = dd;
				val.typ = TYPE_REAL;
			}
			val.error = val.error || tmp.error;
		}
		const_sep_allowed_ = saved_const_sep_allowed;
		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"min\"");
			return TOK_NONE;
		}
		return get_next();
	case TOK_MAX:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"max\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"max\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		if (val.typ != TYPE_REAL && val.typ != TYPE_INT)
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			strcpy(error_buf_, "Parameter for \"max\" must be numeric");
			return TOK_NONE;
		}

		while (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected parameter for \"max\""))
			{
				const_sep_allowed_ = saved_const_sep_allowed;
				return TOK_NONE;
			}

			if (tmp.typ != TYPE_REAL && tmp.typ != TYPE_INT)
			{
				const_sep_allowed_ = saved_const_sep_allowed;
				strcpy(error_buf_, "Parameter for \"max\" must be numeric");
				return TOK_NONE;
			}
			else if (val.typ == TYPE_INT && tmp.typ == TYPE_INT)
			{
				if (tmp.int64 > val.int64)
					val.int64 = tmp.int64;
			}
			else
			{
				double dd = make_real(tmp);
				val.real64 = make_real(val);
				if (dd > val.real64) val.real64 = dd;
				val.typ = TYPE_REAL;
			}
			val.error = val.error || tmp.error;
		}
		const_sep_allowed_ = saved_const_sep_allowed;
		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"max\"");
			return TOK_NONE;
		}
		return get_next();
	case TOK_FACT:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"fact\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"fact\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"fact\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT)
		{
			if (val.int64 < 100)
			{
				__int64 temp = 1;
				for (int ii = 1; ii < val.int64; ++ii)
					temp *= ii + 1;
				val.int64 = temp;
			}
			else
			{
				val.int64 = 0;
				strcpy(error_buf_, "Parameter for \"fact\" is too big");
				return TOK_NONE;
			}
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"fact\" must be an integer");
			return TOK_NONE;
		}
		return get_next();

	case TOK_FLIP:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"flip\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"flip\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"flip\"");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"flip\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"flip\"");
			return TOK_NONE;
		}
		else if (val.typ != TYPE_INT || tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Parameters for \"flip\" must be integers");
			return TOK_NONE;
		}
		else
		{
			// Since we are using little-endian its just a matter of flipping bytes from the start
			size_t bytes = size_t(tmp.int64);
			if (bytes > sizeof(val.int64)) bytes = 0;       // can't flip more than 8 bytes for int64
			flip_bytes((unsigned char *)&val.int64, bytes);
		}
		val.error = val.error || tmp.error;
		return get_next();

	case TOK_REVERSE:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"reverse\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"reverse\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		if (val.typ != TYPE_INT)
		{
			strcpy(error_buf_, "First parameter for \"reverse\" must be integer");
			return TOK_NONE;
		}

		// Get (optional) 2nd parameters = number of bits
		num_bits = 64;
		if (next_tok == TOK_COMMA)
		{
			// Get 2nd parameter (number of bits)
			if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"reverse\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Bits (2nd) parameter for \"reverse\" must be integer");
				return TOK_NONE;
			}

			num_bits = (int)tmp.int64;
			if (num_bits > 64) num_bits = 64;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"reverse\"");
			return TOK_NONE;
		}
		else
		{
			__int64 result = 0;
			for (int ii = 0; ii < num_bits; ++ii)
			{
				result <<= 1;
				if (val.int64 & 1)
					result |= 1;
				val.int64 >>= 1;
			}
			val.int64 = result;
		}
		return get_next();

	case TOK_ROL:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"rol\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"rol\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		// Get (optional) 2nd parameter = number of bits to rotate
		num_bits = 1;
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"rol\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "2nd parameter for \"rol\" must be integer");
				return TOK_NONE;
			}

			num_bits = (int)tmp.int64;
			if (num_bits > 64) num_bits = 0;
		}


		// Get (optional) 3nd parameter = size in bits
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 3rd parameter to \"rol\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "3rd parameter for \"rol\" must be integer");
				return TOK_NONE;
			}
			if (tmp.int64 > 64) tmp.int64 = 64;
		}
		else
			tmp.int64 = 64;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"rol\"");
			return TOK_NONE;
		}
		else
		{
			__int64 mask = (1LL << tmp.int64) - 1;
			val.int64 = ((val.int64 << num_bits) | (val.int64 >> (tmp.int64 - num_bits))) & mask;
		}
		return get_next();

	case TOK_ROR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"ror\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"ror\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		// Get (optional) 2nd parameter = number of bits to rotate
		num_bits = 1;
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"ror\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "2nd parameter for \"ror\" must be integer");
				return TOK_NONE;
			}

			num_bits = (int)tmp.int64;
			if (num_bits > 64) num_bits = 0;
		}


		// Get (optional) 3rd parameter = size in bits
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 3rd parameter to \"ror\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "3rd parameter for \"ror\" must be integer");
				return TOK_NONE;
			}
			if (tmp.int64 > 64) tmp.int64 = 64;
		}
		else
			tmp.int64 = 64;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"ror\"");
			return TOK_NONE;
		}
		else
		{
			__int64 mask = (1LL << tmp.int64) - 1;
			val.int64 = ((val.int64 >> num_bits) | (val.int64 << (tmp.int64 - num_bits))) & mask;
		}
		return get_next();

	case TOK_ASR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"asr\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"asr\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		// Get (optional) 2nd parameter = number of bits to shift
		num_bits = 1;
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"asr\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "2nd parameter for \"asr\" must be integer");
				return TOK_NONE;
			}

			num_bits = (int)tmp.int64;
			if (num_bits > 64) num_bits = 0;
		}


		// Get (optional) 3nd parameters = size in bits
		if (next_tok == TOK_COMMA)
		{
			if (error(next_tok = prec_assign(tmp), "Expected 3rd parameter to \"ror\""))
				return TOK_NONE;

			else if (tmp.typ != TYPE_INT)
			{
				strcpy(error_buf_, "3rd parameter for \"ror\" must be integer");
				return TOK_NONE;
			}
			if (tmp.int64 > 64) tmp.int64 = 64;
		}
		else
			tmp.int64 = 64;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"ror\"");
			return TOK_NONE;
		}
		else
		{
			__int64 mask = (1LL << tmp.int64) - 1;
			val.int64 = (val.int64 >> num_bits) & mask;   // xxx does this handle high-bit on?
		}
		return get_next();

	case TOK_POW:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"pow\"");
			return TOK_NONE;
		}
		// Turn off allowing of commas in ints to avoid confusion
		saved_const_sep_allowed = const_sep_allowed_;
		const_sep_allowed_ = false;
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"pow\""))
		{
			const_sep_allowed_ = saved_const_sep_allowed;
			return TOK_NONE;
		}
		const_sep_allowed_ = saved_const_sep_allowed;

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"pow\"");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"pow\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"pow\"");
			return TOK_NONE;
		}
		else if (val.typ != TYPE_REAL && val.typ != TYPE_INT || 
				 tmp.typ != TYPE_REAL && tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Parameter for \"pow\" must be numeric");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT && tmp.typ == TYPE_INT)
		{
			__int64 factor = val.int64;
			for (int ii = 1; ii < tmp.int64; ++ii)
				val.int64 *= factor;
		}
		else
		{
			errno = 0;
			val.real64 = pow(make_real(val), make_real(tmp));
			if (errno == EDOM || errno == ERANGE)
			{
				strcpy(error_buf_, "Invalid parameters to pow");
				val.typ = TYPE_NONE;
			}
			else
				val.typ = TYPE_REAL;
		}
		val.error = val.error || tmp.error;
		return get_next();

	case TOK_SQRT:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"sqrt\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"sqrt\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"sqrt\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			if (val.real64 < 0)
			{
				strcpy(error_buf_, "Parameter for \"sqrt\" must not be negative");
				val.real64 = 0.0;
				return TOK_NONE;
			}
			else
			val.real64 = sqrt(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			if (val.int64 < 0)
			{
				strcpy(error_buf_, "Parameter for \"sqrt\" must not be negative");
				val.int64 = 0;
				return TOK_NONE;
			}
			else
				val.int64 = (__int64)sqrt((double)val.int64);
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"sqrt\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_SIN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"sin\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"sin\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"sin\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = sin(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = sin((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"sin\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_COS:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"cos\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"cos\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"cos\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = cos(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = cos((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"cos\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_TAN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"tan\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"tan\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"tan\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = tan(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = tan((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"tan\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_ASIN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"asin\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"asin\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"asin\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = asin(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = asin((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"asin\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_ACOS:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"acos\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"acos\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"acos\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = acos(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = acos((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"acos\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_ATAN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"atan\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"atan\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"atan\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = atan(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = atan((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"atan\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_EXP:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"exp\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"exp\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"exp\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = exp(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = exp((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"exp\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_LOG:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"log\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"log\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"log\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.real64 = log(val.real64);
		}
		else if (val.typ == TYPE_INT)
		{
			val.real64 = log((double)val.int64);
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"log\" must be numeric");
			return TOK_NONE;
		}
		return get_next();

	case TOK_INT:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"int\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"int\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"int\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_REAL)
		{
			val.int64 = __int64(val.real64);
			val.typ = TYPE_INT;
		}
		else if (val.typ == TYPE_BOOLEAN)
		{
			val.int64 = __int64(val.boolean ? 1 : 0);
			val.typ = TYPE_INT;
		}
		else if (val.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Parameter for \"int\" must be numeric or boolean");
			return TOK_NONE;
		}
		return get_next();
	case TOK_DATE:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"date\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"date\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"date\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
			val.date = get_date(CString(*(val.pstr)));
			val.typ = TYPE_DATE;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"date\" must be a date string");
			return TOK_NONE;
		}
		return get_next();
	case TOK_TIME:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"time\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"time\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"time\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
			val.date = get_time(CString(*(val.pstr)));
			val.typ = TYPE_DATE;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"time\" must be a time string");
			return TOK_NONE;
		}
		return get_next();
	case TOK_NOW:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"now\"");
			return TOK_NONE;
		}
		if ((next_tok = get_next()) != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"now\"");
			return TOK_NONE;
		}
		val = value_t(COleDateTime::GetCurrentTime());
		return get_next();
	case TOK_YEAR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"year\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"year\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"year\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = fround(365.25*tmp);
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"year\" must be a number");
			return TOK_NONE;
		}
		return get_next();
	case TOK_MONTH:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"month\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"month\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"month\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = fround(365.25/12.0*tmp);
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"month\" must be a number");
			return TOK_NONE;
		}
		return get_next();
	case TOK_DAY:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"day\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"day\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"day\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = fround(tmp);
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"day\" must be a number");
			return TOK_NONE;
		}
		return get_next();
	case TOK_HOUR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"hour\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"hour\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"hour\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = tmp/24;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"hour\" must be a number");
			return TOK_NONE;
		}
		return get_next();
	case TOK_MINUTE:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"minute\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"minute\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"minute\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = tmp/24/60.0;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"minute\" must be a number");
			return TOK_NONE;
		}
		return get_next();
	case TOK_SECOND:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"second\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"second\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"second\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_INT || val.typ == TYPE_REAL)
		{
			double tmp = make_real(val);
			val.typ = TYPE_REAL;
			val.real64 = tmp/24/60.0/60;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"second\" must be a number");
			return TOK_NONE;
		}
		return get_next();

	case TOK_ATOI:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"atoi\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"atoi\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"atoi\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
#ifdef UNICODE_TYPE_STRING
			val.int64 = _wtoi64(*val.pstr);
#else
			val.int64 = _atoi64(*val.pstr);
#endif
			val.typ = TYPE_INT;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"atoi\" must be a string");
			return TOK_NONE;
		}
		return get_next();
	case TOK_ATOF:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"atof\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"atof\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"atof\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
#ifdef UNICODE_TYPE_STRING
			val.real64 = _wtof(*val.pstr);
#else
			val.real64 = atof(*val.pstr);
#endif
			val.typ = TYPE_REAL;
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"atof\" must be a string");
			return TOK_NONE;
		}
		return get_next();
	case TOK_RAND:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"rand\"");
			return TOK_NONE;
		}
		if ((next_tok = get_next()) != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"rand\"");
			return TOK_NONE;
		}
		// Create a 64 bit random number using two 32 bit ones
		val.int64 = (__int64(rand_good())<<32) | rand_good();
		val.typ = TYPE_INT;
		return get_next();
	case TOK_STRLEN:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"strlen\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected string parameter to \"strlen\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strlen\"");
			return TOK_NONE;
		}
		else if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Parameter for \"strlen\" must be a string");
			return TOK_NONE;
		}
		else
		{
			size_t tmp = val.pstr->GetLength();
			delete val.pstr;
			val.int64 = tmp;
			val.typ = TYPE_INT;
		}
		return get_next();
	case TOK_LEFT:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"left\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"left\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"left\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"left\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"left\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Second parameter for \"left\" must be an integer");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"left\"");
			return TOK_NONE;
		}
		else
		{
			*val.pstr = val.pstr->Left(int(tmp.int64));
		}
		return get_next();
	case TOK_RIGHT:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"right\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"right\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"right\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"right\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"right\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Second parameter for \"right\" must be an integer");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"right\"");
			return TOK_NONE;
		}
		else
		{
			*val.pstr = val.pstr->Right(int(tmp.int64));
		}
		return get_next();
	case TOK_MID:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"mid\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"mid\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"mid\" must be a string");
			return TOK_NONE;
		}

		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"mid\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Second parameter for \"mid\" must be an integer");
			return TOK_NONE;
		}

		if (next_tok == TOK_RPAR)
		{
			*val.pstr = val.pstr->Mid(int(tmp.int64));
		}
		else if (next_tok == TOK_COMMA)
		{
			value_t tmp2;

			if (error(next_tok = prec_assign(tmp2), "Expected 3rd parameter to \"mid\""))
				return TOK_NONE;
			if (tmp2.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Third parameter for \"mid\" must be an integer");
				return TOK_NONE;
			}

			if (next_tok != TOK_RPAR)
			{
				strcpy(error_buf_, "Closing parenthesis expected for \"mid\"");
				return TOK_NONE;
			}
			else
			{
				*val.pstr = val.pstr->Mid(int(tmp.int64), int(tmp2.int64));
			}
		}
		else
		{
			strcpy(error_buf_, "Closing parenthesis/3rd parameter expected for \"mid\"");
			return TOK_NONE;
		}
		return get_next();
	case TOK_LTRIM:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"ltrim\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"ltrim\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strlen\"");
			return TOK_NONE;
		}
		else if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Parameter for \"strlen\" must be a string");
			return TOK_NONE;
		}
		else
		{
			val.pstr->TrimLeft();
		}
		return get_next();
	case TOK_RTRIM:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"rtrim\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"rtrim\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strlen\"");
			return TOK_NONE;
		}
		else if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Parameter for \"strlen\" must be a string");
			return TOK_NONE;
		}
		else
		{
			val.pstr->TrimRight();
		}
		return get_next();
	case TOK_STRCHR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"strchr\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"strchr\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"strchr\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"strchr\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"strchr\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Second parameter for \"strchr\" must be an integer");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strchr\"");
			return TOK_NONE;
		}
		else
		{
#ifdef UNICODE_TYPE_STRING
			const wchar_t *pstart = val.pstr->GetBuffer(0);
			const wchar_t *pp = wcschr(pstart, wchar_t(tmp.int64));
#else
			const char *pstart = val.pstr->GetBuffer(0);
			const char *pp = strchr(pstart, int(tmp.int64));
#endif
			__int64 ii = -1;
			if (pp != NULL)
				ii = pp - pstart;
			delete val.pstr;
			val.int64 = ii;
			val.typ = TYPE_INT;
		}
		return get_next();
	case TOK_STRSTR:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"strstr\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"strstr\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"strstr\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"strstr\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"strstr\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Second parameter for \"strstr\" must be an string");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strstr\"");
			return TOK_NONE;
		}
		else
		{
#ifdef UNICODE_TYPE_STRING
			const wchar_t *pstart = val.pstr->GetBuffer(0);
			const wchar_t *pp = wcsstr(pstart, (const wchar_t *)(*tmp.pstr));
#else
			const char *pstart = val.pstr->GetBuffer(0);
			const char *pp = strstr(pstart, (const char *)(*tmp.pstr));
#endif
			__int64 ii = -1;
			if (pp != NULL)
				ii = pp - pstart;
			delete val.pstr;
			val.int64 = ii;
			val.typ = TYPE_INT;
		}
		return get_next();
	case TOK_STRCMP:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"strcmp\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"strcmp\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"strcmp\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"strcmp\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"strcmp\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Second parameter for \"strcmp\" must be an string");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"strcmp\"");
			return TOK_NONE;
		}
		else
		{
			__int64 retval = val.pstr->Compare(*(tmp.pstr));
			delete val.pstr;
			val.int64 = retval;
			val.typ = TYPE_INT;
		}
		return get_next();
	case TOK_STRICMP:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"stricmp\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected 1st parameter to \"stricmp\""))
			return TOK_NONE;
		if (val.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "First parameter for \"stricmp\" must be a string");
			return TOK_NONE;
		}

		if (next_tok != TOK_COMMA)
		{
			strcpy(error_buf_, "Expected comma and 2nd parameter for \"stricmp\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(tmp), "Expected 2nd parameter to \"stricmp\""))
			return TOK_NONE;
		if (tmp.typ != TYPE_STRING)
		{
			strcpy(error_buf_, "Second parameter for \"stricmp\" must be an string");
			return TOK_NONE;
		}

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"stricmp\"");
			return TOK_NONE;
		}
		else
		{
			__int64 retval = val.pstr->CompareNoCase(*(tmp.pstr));
			delete val.pstr;
			val.int64 = retval;
			val.typ = TYPE_INT;
		}
		return get_next();
	case TOK_A2E:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"asc2ebc\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"asc2ebc\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"asc2ebc\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
#ifdef UNICODE_TYPE_STRING
			wchar_t *pp = (*val.pstr).GetBuffer();
#else
			char *pp = (*val.pstr).GetBuffer();
#endif
			for (int ii = 0; ii < (*val.pstr).GetLength(); ++ii)
			{
				if (pp[ii] < sizeof(a2e_tab) && a2e_tab[pp[ii]] != 0)
					pp[ii] = a2e_tab[pp[ii]];
				else
					pp[ii] = a2e_tab['?'];
			}
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"asc2ebc\" must be a string");
			return TOK_NONE;
		}
		return get_next();
	case TOK_E2A:
		if ((next_tok = get_next()) != TOK_LPAR)
		{
			strcpy(error_buf_, "Opening parenthesis expected after \"ebc2asc\"");
			return TOK_NONE;
		}
		if (error(next_tok = prec_assign(val), "Expected parameter for \"ebc2asc\""))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected for \"ebc2asc\"");
			return TOK_NONE;
		}
		else if (val.typ == TYPE_STRING)
		{
#ifdef UNICODE_TYPE_STRING
			wchar_t *pp = (*val.pstr).GetBuffer();
#else
			char *pp = (*val.pstr).GetBuffer();
#endif
			for (int ii = 0; ii < (*val.pstr).GetLength(); ++ii)
			{
				if (pp[ii] < sizeof(e2a_tab) && e2a_tab[pp[ii]] != 0)
					pp[ii] = e2a_tab[pp[ii]];
				else
					pp[ii] = e2a_tab['?'];
			}
		}
		else
		{
			strcpy(error_buf_, "Parameter for \"ebc2asc\" must be a string");
			return TOK_NONE;
		}
		return get_next();

	case TOK_SYMBOL:
		tmp.typ = TYPE_NONE;
		tmp.int64 = ref_;
		if (_stricmp(psymbol_, "end") == 0 ||
			_stricmp(psymbol_, "next") == 0  ||
			_stricmp(psymbol_, "index") == 0  ||
			_stricmp(psymbol_, "member") == 0)
		{
			sprintf(error_buf_, "\"%s\" is a reserved symbol", psymbol_);
			return TOK_NONE;
		}
		// First do a quick check for a global var not array (or a string which can also be indexed)
		// then check the file (template), then finally check globals again but include arrays
		// Note that this could possibly be even faster if var_ was a hash table instead of a std::map (RB tree)
		vname = psymbol_;
		vname.MakeUpper();
		if ((pv = var_.find(vname)) != var_.end() &&
			pv->second.typ != TYPE_STRING &&            // ignore string in case we have to index into it
			pv->second.typ != TYPE_NONE)                // ignore bogus vars (should not happen?)
		{
			// Found the global var
			next_tok = get_next();
			val = pv->second;
		}
		else if (val = find_symbol(psymbol_, tmp, 0, pac_, sym_size, sym_address, sym_str),
				 val.typ != TYPE_NONE)
		{
			// The symbol was located in derived class (eg a field from the template)
			// Now check if it is an array [] or struct .
			next_tok = get_next();
			while (next_tok == TOK_DOT || next_tok == TOK_LBRA)
			{
				switch (next_tok)
				{
				case TOK_DOT:
					if (val.typ != TYPE_STRUCT)
					{
						strcpy(error_buf_, "Dot operator (.) must only be used with struct");
						return TOK_NONE;
					}
					if (error(next_tok = get_next(), "Expected member name"))
						return TOK_NONE;

					if (next_tok != TOK_SYMBOL)
					{
						strcpy(error_buf_, "Symbol expected");
						return TOK_NONE;
					}
					val = find_symbol(psymbol_, val, 0, pac_, sym_size, sym_address, sym_str);
					if (val.typ == TYPE_NONE)
					{
						strcpy(error_buf_, "Unrecognised member name for struct");
						return TOK_NONE;
					}
					next_tok = get_next();
					break;
				case TOK_LBRA:
					if (val.typ != TYPE_ARRAY && val.typ != TYPE_STRING && val.typ != TYPE_BLOB)
					{
						strcpy(error_buf_, "Unexpected array index");
						return TOK_NONE;
					}
					if (error(next_tok = prec_lowest(tmp), "Expected array index"))
						return TOK_NONE;

					if (next_tok != TOK_RBRA)
					{
						strcpy(error_buf_, "Closing bracket (]) expected");
						return TOK_NONE;
					}
					else if (tmp.typ != TYPE_INT)
					{
						strcpy(error_buf_, "Array (for) index must be an integer");
						return TOK_NONE;
					}
					if (val.typ == TYPE_ARRAY)
					{
						val = find_symbol(NULL, val, size_t(tmp.int64), pac_, sym_size, sym_address, sym_str);
						if (val.typ == TYPE_NONE)
						{
							strcpy(error_buf_, "Invalid array (for) index");
							return TOK_NONE;
						}
					}
					else if (val.typ == TYPE_BLOB)
					{
						val = find_symbol(NULL, val, size_t(tmp.int64), pac_, sym_size, sym_address, sym_str);
						if (val.typ == TYPE_NONE)
						{
							strcpy(error_buf_, "Invalid index into \"none\" DATA element");
							return TOK_NONE;
						}
					}
					else
					{
						ASSERT(val.typ == TYPE_STRING);
						if (tmp.int64 < 0 || tmp.int64 >= val.pstr->GetLength())
						{
							strcpy(error_buf_, "Index on string out of bounds");
							return TOK_NONE;
						}
						int value = (*val.pstr)[int(tmp.int64)];
						delete val.pstr;
						val.int64 = value;
						val.typ = TYPE_INT;
					}
					next_tok = get_next();
					break;
				}
			}
			if (val.typ == TYPE_STRUCT)
			{
				strcpy(error_buf_, "Dot operator (.) expected after struct");
				return TOK_NONE;
			}
			else if (val.typ == TYPE_ARRAY)
			{
				strcpy(error_buf_, "Array index expected after for");
				return TOK_NONE;
			}
		}
		else
		{
			// Check our assignable variables (includes arrays)
			next_tok = get_var(val, vname);
		}
		// At this point val.error may be set and we leave it on to
		// propagate through the evaluations

		// Check for post-increment and post-decrement
		if (next_tok == TOK_INC)
		{
			if (vname.IsEmpty() || val.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Post-increment (++) requires an integer variable");
				return TOK_NONE;
			}
			// Only change the value if side effects allowed
			if (changes_on_)
			{
				++val.int64;
				var_[vname] = val;
				val.int64--;   // restore value as this is *post* increment
				var_changed_ = clock();
			}
			vname.Empty();  // Clear vname since we can't assign after auto-inc
			next_tok = get_next();
		}
		else if (next_tok == TOK_DEC)
		{
			if (vname.IsEmpty() || val.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Post-decrement (++) requires an integer variable");
				return TOK_NONE;
			}
			// Only change the value if side effects allowed
			if (changes_on_)
			{
				val.int64--;
				var_[vname] = val;
				++val.int64;   // restore value as this is *post* decrement
				var_changed_ = clock();
			}
			vname.Empty();  // Clear vname since we can't assign after auto-dec
			next_tok = get_next();
		}
		return next_tok;
	case TOK_LPAR:
		if (error(next_tok = prec_lowest(val), "Expected expression after opening parenthesis"))
			return TOK_NONE;

		if (next_tok != TOK_RPAR)
		{
			strcpy(error_buf_, "Closing parenthesis expected");
			return TOK_NONE;
		}
		return get_next();

	case TOK_INC:  // pre-increment
		{
			CString vname2;  // Don't use vname as an incremented var cannot be later assigned to
			next_tok = prec_prim(val, vname2);
			vname2.MakeUpper();
			if (vname2.IsEmpty() || val.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Pre-increment (++) requires an integer variable");
				return TOK_NONE;
			}
			++val.int64;
			// Only change the value if side effects allowed
			if (changes_on_)
			{
				var_[vname2] = val;
				var_changed_ = clock();
			}
		}
		return next_tok;
	case TOK_DEC:  // pre-decrement
		{
			CString vname2;  // Don't use vname as an decremented var cannot be later assigned to
			next_tok = prec_prim(val, vname2);
			vname2.MakeUpper();
			if (vname2.IsEmpty() || val.typ != TYPE_INT)
			{
				strcpy(error_buf_, "Pre-decrement (--) requires an integer variable");
				return TOK_NONE;
			}
			val.int64--;
			// Only change the value if side effects allowed
			if (changes_on_)
			{
				var_[vname2] = val;
				var_changed_ = clock();
			}
		}
		return next_tok;

	case TOK_PLUS:  // unary +
		if (error(next_tok = prec_prim(val, vname), "Expected number after +"))
			return TOK_NONE;

		if (val.typ != TYPE_INT && val.typ != TYPE_REAL)
		{
			strcpy(error_buf_, "Invalid operand for unary +");
			return TOK_NONE;
		}
		return next_tok;
	case TOK_MINUS:
		if (error(next_tok = prec_prim(val, vname), "Expected number after minus"))
			return TOK_NONE;

		if (val.typ == TYPE_INT)
			val.int64 = -val.int64;
		else if (val.typ == TYPE_REAL)
			val.real64 = -val.real64;
		else
		{
			strcpy(error_buf_, "Invalid operand for negation");
			return TOK_NONE;
		}
		return next_tok;
	case TOK_NOT:
		if (error(next_tok = prec_prim(val, vname), "Expected BOOLEAN operand for NOT (!)"))
			return TOK_NONE;

		if (val.typ != TYPE_BOOLEAN)
		{
			strcpy(error_buf_, "NOT operator must have a BOOLEAN operand");
			return TOK_NONE;
		}
		val.boolean = !val.boolean;
		return next_tok;
	case TOK_BITNOT:
		if (error(next_tok = prec_prim(val, vname), "Expected integer for bitwise NOT (~)"))
			return TOK_NONE;

		if (val.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Bitwise NOT operator must have an INTEGER operand");
			return TOK_NONE;
		}
		val.int64 = ~val.int64;
		return next_tok;
	default:
        strcpy(error_buf_, "Not implemented");
		return TOK_NONE;
	}
	ASSERT(0);
	return TOK_NONE;
}

expr_eval::tok_t expr_eval::get_var(expr_eval::value_t & retval, CString &vname)
{
	value_t tmp;
	std::map<CString, value_t>::const_iterator pv;

	retval.typ = TYPE_NONE;
	tok_t next_tok = get_next();
	while (next_tok == TOK_LBRA)   // for each index [1][2]...
	{
		if (error(next_tok = prec_lowest(tmp), "Expected array index"))
			return TOK_NONE;

		if (next_tok != TOK_RBRA)
		{
			strcpy(error_buf_, "Closing bracket (]) expected");
			return TOK_NONE;
		}
		else if (tmp.typ != TYPE_INT)
		{
			strcpy(error_buf_, "Array index must be an integer");
			return TOK_NONE;
		}

		// Check if using index on a string
		vname.MakeUpper();
		if ((pv = var_.find(vname)) != var_.end() && pv->second.typ == TYPE_STRING)
		{
			if (tmp.int64 < 0 || tmp.int64 >= pv->second.pstr->GetLength())
			{
				strcpy(error_buf_, "Index on string out of bounds");
				return TOK_NONE;
			}
			retval.int64 = (*pv->second.pstr)[int(tmp.int64)];
			retval.typ = TYPE_INT;
			return get_next();
		}
		else
		{
			// Add index to vname
			char buf[32];
			sprintf(buf, "%I64d", tmp.int64);
			vname += CString('[') + buf + CString(']');
		}

		next_tok = get_next();
	}

	// Check if it already has a value
	vname.MakeUpper();
	if ((pv = var_.find(vname)) != var_.end())
		retval = pv->second;

	return next_tok;
}

DATE expr_eval::get_date(const char * ss)
{
	// Use date/time parsing from C++ std library
	std::locale loc;
	std::basic_stringstream<char> pszGetF;
	std::ios_base::iostate st = 0;
	pszGetF << ss;                                             // Place our date string into a string stream
	pszGetF.imbue(loc);
	std::basic_istream<char>::_Iter pp(pszGetF.rdbuf());       // Point an iterator to the start of it
	std::basic_istream<char>::_Iter eos(0);                    // "null" iterator representing end of string

	// Init tm structure to zeroes in case something is not filled in
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	// Skip leading whitespace
	while (*pp == ' ')
		++pp;

	// Parse out date part
	pp = std::use_facet<std::time_get<char> >(loc).get_date(pp, eos, pszGetF, st, &tm);

	// If we couldn't even parse out a date part then return an error value
	if (st & std::ios_base::failbit)
		return -1e30;

	// Skip white space and a comma if present
	while (pp != eos && *pp == ' ')
		++pp;
	if (pp != eos && *pp == ',')
		++pp;
	while (pp != eos && *pp == ' ')
		++pp;

	// If not at end of string also get the time part
	if (pp != eos)
		pp = std::use_facet<std::time_get<char> >(loc).get_time(pp, eos, pszGetF, st, &tm);

	// Convert tm structure to DATE (using COleDateTime constructor)
	COleDateTime odt(tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (odt.m_status == 0)
		return odt.m_dt;
	else
		return -1e30;
}

DATE expr_eval::get_time(const char * ss)
{
	// Use date/time parsing from C++ std library
	std::locale loc;
	std::basic_stringstream<char> pszGetF;
	std::ios_base::iostate st = 0;
	pszGetF << ss;                                             // Place our date string into a string stream
	pszGetF.imbue(loc);
	std::basic_istream<char>::_Iter pp(pszGetF.rdbuf());       // Point an iterator to the start of it
	std::basic_istream<char>::_Iter eos(0);                    // "null" iterator representing end of string

	// Init tm structure to zeroes in case something is not filled in
	struct tm tm;
	memset(&tm, 0, sizeof(tm));

	// set to year zero since we are just storing time
	tm.tm_year = -1;
	tm.tm_mon = 11;
	tm.tm_mday = 30;

	// Skip leading whitespace
	while (*pp == ' ')
		++pp;

	// Parse out time part
	pp = std::use_facet<std::time_get<char> >(loc).get_time(pp, eos, pszGetF, st, &tm);

	// Convert tm structure to DATE (using COleDateTime constructor)
	COleDateTime odt(tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
	if (odt.m_status == 0)
		return odt.m_dt;
	else
		return -1e30;
}

bool expr_eval::error(expr_eval::tok_t tt, const char *mess)
{
	if (tt == TOK_NONE)
	{
		if (error_buf_[0] == '\0')
			strcpy(error_buf_, mess);
		return true;
	}
	else
	{
		return false;
	}
}

double expr_eval::make_real(value_t &val)
{
	switch (val.typ)
	{
	case TYPE_REAL:
		return val.real64;
	case TYPE_INT:
		return double(val.int64);
	case TYPE_DATE:
		return double(val.date);
	default:
		ASSERT(0);
		return 0.0;
	}
}

__int64 expr_eval::make_int(value_t &val)
{
	switch (val.typ)
	{
	case TYPE_REAL:
		return __int64(val.real64);
	case TYPE_INT:
		return val.int64;
	default:
		ASSERT(0);
		// xxx error message?
		return 0;
	}
}

struct
{
	const char *name;
	expr_eval::tok_t tok;
} func_list[] =
{
	{"ABS",       expr_eval::TOK_ABS},
	{"ACOS",      expr_eval::TOK_ACOS},
	{"ADDRESSOF", expr_eval::TOK_ADDRESSOF},
	{"ASC2EBC",   expr_eval::TOK_A2E},
	{"ASIN",      expr_eval::TOK_ASIN},
	{"ASR",       expr_eval::TOK_ASR},
	{"ATAN",      expr_eval::TOK_ATAN},
	{"ATOF",      expr_eval::TOK_ATOF},
	{"ATOI",      expr_eval::TOK_ATOI},
	{"COS",       expr_eval::TOK_COS},
	{"DATE",      expr_eval::TOK_DATE},
	{"DAY",       expr_eval::TOK_DAY},
	{"DEFINED",   expr_eval::TOK_DEFINED},
	{"EBC2ASC",   expr_eval::TOK_E2A},
	{"EXP",       expr_eval::TOK_EXP},
	{"FACT",      expr_eval::TOK_FACT},
	{"FLIP",      expr_eval::TOK_FLIP},
	//{"GET",       expr_eval::TOK_GET},
	{"GETBOOL",   expr_eval::TOK_GETBOOL},
	{"GETINT",    expr_eval::TOK_GETINT},
	{"GETSTRING", expr_eval::TOK_GETSTR},
	{"HOUR",      expr_eval::TOK_HOUR},
	{"INT",       expr_eval::TOK_INT},
	{"LEFT",      expr_eval::TOK_LEFT},
	{"LOG",       expr_eval::TOK_LOG},
	{"LTRIM",     expr_eval::TOK_LTRIM},
	{"MAX",       expr_eval::TOK_MAX},
	{"MID",       expr_eval::TOK_MID},
	{"MIN",       expr_eval::TOK_MIN},
	{"MINUTE",    expr_eval::TOK_MINUTE},
	{"MONTH",     expr_eval::TOK_MONTH},
	{"NOW",       expr_eval::TOK_NOW},
	{"POW",       expr_eval::TOK_POW},
	{"RAND",      expr_eval::TOK_RAND},
	{"REVERSE",   expr_eval::TOK_REVERSE},
	{"RIGHT",     expr_eval::TOK_RIGHT},
	{"ROL",       expr_eval::TOK_ROL},
	{"ROR",       expr_eval::TOK_ROR},
	{"RTRIM",     expr_eval::TOK_RTRIM},
	{"SECOND",    expr_eval::TOK_SECOND},
	{"SIN",       expr_eval::TOK_SIN},
	{"SIZEOF",    expr_eval::TOK_SIZEOF},
	{"SQRT",      expr_eval::TOK_SQRT},
	{"STRCHR",    expr_eval::TOK_STRCHR},
	{"STRCMP",    expr_eval::TOK_STRCMP},
	{"STRICMP",   expr_eval::TOK_STRICMP},
	{"STRING",    expr_eval::TOK_STR},
	{"STRLEN",    expr_eval::TOK_STRLEN},
	{"STRSTR",    expr_eval::TOK_STRSTR},
	{"TAN",       expr_eval::TOK_TAN},
	{"TIME",      expr_eval::TOK_TIME},
	{"YEAR",      expr_eval::TOK_YEAR},

	// The above list should be in alphabetical order to allow a binary search

	{NULL,        expr_eval::TOK_NONE}  // marks end of list
};

// TBD This could be sped up using a binary search.
// (Most of the time no match is found as it is a var name.)
expr_eval::tok_t expr_eval::func_token(const char *buf)
{
	int ii;
#ifdef _DEBUG
	// Make sure it is uppercase
	char tmp[256];
	strcpy(tmp, buf);
	_strupr(tmp);

	// Make sure they are sorted
	for (ii = 1; func_list[ii].name != NULL; ++ii)
		ASSERT(strcmp(func_list[ii-1].name, func_list[ii].name) < 0);
#endif
	for (ii = 0; func_list[ii].name != NULL; ++ii)
		if (strcmp(buf, func_list[ii].name) == 0)
			break;

	return func_list[ii].tok;
}

expr_eval::tok_t expr_eval::get_next()
{
	skip_ws();
	saved_ = p_;
	assert(p_ != NULL);

	if (*p_ == '\0')
	{
		return TOK_EOL;
	}
	else if (*p_ == '@' || isalpha(*p_) || *p_ == '$' || *p_ == '_')
	{
		bool is_int = true;     // Used to check whether this is a valid number

		if (*p_ == '@')
		{
			// @ escapes anything to make it an identifier
			is_int = false;
			++p_;
			++saved_;
		}

		// Check whether this appears to be a valid int
		while (isalpha(*p_) || *p_ == '$' || *p_ == '_' || isdigit(*p_))
		{
			if (isalpha(*p_))
			{
				if (toupper(*p_) - 'A' + 10 >= const_radix_)
					is_int = false;                     // Greater than max digit for consts
			}
			else if (*p_ == '$' || *p_ == '_')
			{
				is_int = false;                         // Ints can't contain $ or _
			}
			++p_;
		}

		if (!is_int)
		{
			// Either a predefined symbol (const or function name) or a variable
			char buf[256], bufu[256];
			size_t len = p_ - saved_;
			if (len > sizeof(buf)-1) len = sizeof(buf)-1;
			strncpy(buf, saved_, len);
			buf[len] = '\0';
			strcpy(bufu, buf);
			_strupr(bufu);              // make uppercase so we can use strcmp instead of _stricmp

			tok_t token;
			if ((token = func_token(bufu)) != TOK_NONE)
			{
				return token;
			}
			else if (strcmp(bufu, "TRUE") == 0)
			{
				last_val_ = value_t(true);
				return TOK_CONST;
			}
			else if (strcmp(bufu, "FALSE") == 0)
			{
				last_val_ = value_t(false);
				return TOK_CONST;
			}
			else if (strcmp(bufu, "PI") == 0)
			{
				last_val_ = value_t((double)3.14159265358979323846);
				return TOK_CONST;
			}
			else
			{
				// Parse out any scope specifiers
				for (;;)
				{
					// name components may have whitespace (eg, "std  ::  vector")
					skip_ws();

					// Stop if not ::
					if (*p_ != ':' || *(p_+1) != ':')
						break;

					// Skip colons and whitespace
					++p_; ++p_;
					skip_ws();
					if (len < sizeof(buf)-3)
					{
						strcpy(buf + len, "::");
						len += 2;
					}

					// Get next part of name
					saved_ = p_;
					while (isalpha(*p_) || *p_ == '$' || *p_ == '_' || (isdigit(*p_) && p_ > saved_))
						++p_;

					size_t curr_offset = len;
					len += p_ - saved_;
					if (len > sizeof(buf)-1) len = sizeof(buf)-1;
					strncpy(buf + curr_offset, saved_, len - curr_offset);
					buf[len] = '\0';
				}
				free(psymbol_);                // Make sure any previously allocated memory is freed
				psymbol_ = _strdup(buf);
				return TOK_SYMBOL;
			}
			ASSERT(0);
		}
		// This appears to be a int literal so restore p_ to
		// start of token and continue (below) to get number
		p_ = saved_;
	}

	// Now check for numeric literals
	if (isalnum(*p_) /* || *p_ == dec_point_ */) // don't allow dec point as 1st char of real as it confused structure parsing
	{
		// Find first non-digit to try and work out what sort of number this is
		p_ += strspn(p_, "0123456789");

		// If we hit a decimal point this is a real or we hit an 'E' (unless 'E' is a valid digit)
		if (*p_ == dec_point_ || (const_radix_ < 0xE && (*p_ == 'd' || *p_ == 'D' || *p_ == 'e' || *p_ == 'E'))) // Floating point number
		{
			// skip past rest of float number
			if (*p_ == dec_point_)
			{
				++p_;
				while (isdigit(*p_))
					++p_;
			}

			if (*p_ == 'e' || *p_ == 'E')
			{
				++p_;
				if (*p_ == '+' || *p_ == '-') ++p_;
				while (isdigit(*p_))
					++p_;
			}

			char *endp;
			last_val_ = value_t(strtod(saved_, &endp));
			if (endp != p_)
			{
				sprintf(error_buf_, "Invalid number: %.*s", p_ - saved_, saved_);
				return TOK_NONE;
			}
		}
		else if ((*p_ == 'x' || *p_ == 'X') && *saved_ == '0' && p_ == saved_+1)
		{
			// Skip past rest of hex integer
			++p_;
			while (isxdigit(*p_))
				++p_;

			CString ss(saved_+2, p_ - (saved_+2));      // Get digits into string
			const char *endptr;
			last_val_ = value_t(::strtoi64(ss, 16, &endptr));
			if (*endptr != '\0')
			{
				sprintf(error_buf_, "Overflow: Hex integer \"%s\" too big", ss);
				return TOK_NONE;
			}
		}
		else if (const_radix_ == 10)
		{
			// Decimal integer
			// (At this point we hit a separator (comma?) or the end of the number.)
			if (const_sep_allowed_ && *p_ == dec_sep_char_ && isdigit(*(p_+1)))
			{
				// Find the end of the integer including separators
				while (isdigit(*p_) || *p_ == dec_sep_char_)
					++p_;

				// Remove the separator characters from the text number
				CString ss(saved_, p_ - saved_);
				ss.Remove(dec_sep_char_);

				last_val_ = value_t(_atoi64(ss));
			}
			else
				last_val_ = value_t(_atoi64(saved_));  // At this point _p points after the decimal integer
		}
		else
		{
			// Non-decimal integer
			// (At this point we may have hit a space or alpha or just the end of the number.)
			if (const_sep_allowed_)
			{
				// Find end of the hex (or other base) integer (possibly with space separators)
				while (isdigit(*p_) ||
					   *p_ == ' ' ||
					   (isalpha(*p_) && toupper(*p_) - 'A' + 10 < const_radix_))
				{
					++p_;
				}
			}
			else
			{
				// find end of hex (or other base) integer
				while (isdigit(*p_) ||
					   (isalpha(*p_) && toupper(*p_) - 'A' + 10 < const_radix_))
				{
					++p_;
				}
			}
			CString ss(saved_, p_ - saved_);
			ss.Remove(' ');                  // Remove any spaces copied (if const_sep_allowed_)

			const char *endptr;
			last_val_ = value_t(::strtoi64(ss, const_radix_, &endptr));
			if (*endptr != '\0')
			{
				sprintf(error_buf_, "Overflow: \"%s\" too big", ss);
				return TOK_NONE;
			}
		}

		return TOK_CONST;
	}
	else if (*p_ == '"')
	{
		CString ss;

		// Get string constant
		for (++p_; *p_ != '"' && *p_ != '\0' ; ++p_)
		{
			if (*p_ == '\\' && strchr("abtnvfrx\"", *(p_+1)) != NULL)
			{
				++p_;
				switch(*p_)
				{
				case 'a':
					ss += '\a';
					break;
				case 'b':
					ss += '\b';
					break;
				case 't':
					ss += '\t';
					break;
				case 'n':
					ss += '\n';
					break;
				case 'v':
					ss += '\v';
					break;
				case 'f':
					ss += '\f';
					break;
				case 'r':
					ss += '\r';
					break;
				case '0':
					ss += '\0';
					break;
				case 'x':
					if (isxdigit(*(p_+1)))
					{
						++p_;
						int cc = isdigit(*p_) ? (*p_ - '0') : (toupper(*p_) - 'A' + 10);
						if (isxdigit(*(p_+1)))
						{
							++p_;
							cc = cc*16 + (isdigit(*p_) ? (*p_ - '0') : (toupper(*p_) - 'A' + 10));
						}
						ss += (char)cc;
					}
					else
						ss += '\0';
					break;
				case '"':
					ss += '"';
					break;
				default:
					sprintf(error_buf_, "Unexpected character escape sequence \\%c in string", *p_);
					return TOK_NONE;
				}
			}
			else
				ss += *p_;
		}

		last_val_ = (const char *)ss;
		if (*p_ != '"')
		{
			sprintf(error_buf_, "Unterminated string constant");
			return TOK_NONE;
		}
		else
		{
			++p_;
			return TOK_CONST;
		}
	}
	else if (*p_ == '\'')
	{
		// Get character constant
		++p_;
		last_val_ = *p_;
		++p_;
		if (last_val_.int64 == '\\')
		{
			switch(*p_)
			{
			case 'a':
				last_val_ = '\a';
				break;
			case 'b':
				last_val_ = '\b';
				break;
			case 't':
				last_val_ = '\t';
				break;
			case 'n':
				last_val_ = '\n';
				break;
			case 'v':
				last_val_ = '\v';
				break;
			case 'f':
				last_val_ = '\f';
				break;
			case 'r':
				last_val_ = '\r';
				break;
			default:
				sprintf(error_buf_, "Unexpected character constant escape sequence \\%c", *p_);
				return TOK_NONE;
			}
			++p_;
		}
		if (*p_ != '\'')
		{
			sprintf(error_buf_, "Expected ' after character constant");
			return TOK_NONE;
		}
		else
		{
			++p_;
			return TOK_CONST;
		}
	}
	else if (*p_ == '.')
	{
		++p_;
		return TOK_DOT;
	}
	else if (*p_ == '[')
	{
		++p_;
		return TOK_LBRA;
	}
	else if (*p_ == ']')
	{
		++p_;
		return TOK_RBRA;
	}
	else if (*p_ == '(')
	{
		++p_;
		return TOK_LPAR;
	}
	else if (*p_ == ')')
	{
		++p_;
		return TOK_RPAR;
	}
	else if (strncmp(p_, "++", 2) == 0)
	{
		p_ += 2;
		return TOK_INC;
	}
	else if (strncmp(p_, "--", 2) == 0)
	{
		p_ += 2;
		return TOK_DEC;
	}
	else
	{
		++p_;
		// Get rest of other operator (one or more "punctuation" characters)
		// but stop at unary operator (ie +-!~ are not in the string below
		while (strchr("*/%&|^><=?:", *p_) != NULL && *p_ != '\0')
			++p_;
		char buf[8];
		size_t len = p_ - saved_;
		if (len > sizeof(buf)-1) len = sizeof(buf)-1;
		strncpy(buf, saved_, len);
		buf[len] = '\0';

		if (len == 1)
		{
			switch (*saved_)
			{
			case '+':
				return TOK_PLUS;
			case '-':
				return TOK_MINUS;
			case '*':
				return TOK_MUL;
			case '/':
				return TOK_DIV;
			case '%':
				return TOK_MOD;
			case '~':
				return TOK_BITNOT;
			case '&':
				return TOK_BITAND;
			case '|':
				return TOK_BITOR;
			case '^':
				return TOK_XOR;
			case '!':
				return TOK_NOT;
			case '<':
				return TOK_LT;
			case '>':
				return TOK_GT;
			case '?':
				return TOK_QUESTION;
			case ':':
				return TOK_COLON;
			case '=':
				return TOK_ASSIGN;
			case ',':
				return TOK_COMMA;
			default:
				sprintf(error_buf_, "Unexpected character \"%c\"", *saved_);
				return TOK_NONE;
			}
		}
		else if (strcmp(buf, "==") == 0)
			return TOK_EQ;
		else if (strcmp(buf, "!=") == 0)
			return TOK_NE;
		else if (strcmp(buf, "<=") == 0)
			return TOK_LE;
		else if (strcmp(buf, ">=") == 0)
			return TOK_GE;
		else if (strcmp(buf, "<<") == 0)
			return TOK_SHL;
		else if (strcmp(buf, ">>") == 0)
			return TOK_SHR;
		else if (strcmp(buf, "&&") == 0)
			return TOK_AND;
		else if (strcmp(buf, "||") == 0)
			return TOK_OR;
		else
		{
			sprintf(error_buf_, "Unexpected characters \"%.200s\"", buf);
			return TOK_NONE;    // Syntax error or not yet implemented
		}
	}
}

void expr_eval::skip_ws()
{
	assert(p_ != NULL);
	while(isspace(*p_))
		++p_;
}

// Formats a value as a string
// - strFormat is the format string that determines formating depending on the type of val
// - size/unsgned indicate expected size and type for integers
ExprStringType expr_eval::value_t::GetDataString(CString strFormat, int size /* = -1 */, bool unsgned /* = false */)
{
	// Get default format string if none given
	if (strFormat.IsEmpty())
	{
		switch (typ)
		{
		case TYPE_INT:
			strFormat = theApp.default_int_format_;
			break;
		case TYPE_DATE:
			strFormat = theApp.default_date_format_;
			break;
		case TYPE_REAL:
			strFormat = theApp.default_real_format_;
			break;
		case TYPE_STRING:
			strFormat = theApp.default_string_format_;
			break;
		}
	}

	CString ss;
#ifdef UNICODE_TYPE_STRING
	CStringW sw;
#endif
	char disp[128];     // Holds output of sprintf for %I64 (CString::Format can't handle it)

	switch (typ)
	{
	case TYPE_BOOLEAN:
		if (strFormat == "f")
			ss = boolean ? "t" : "f";
		else if (strFormat == "F")
			ss = boolean ? "T" : "F";
		else if (strFormat == "false")
			ss = boolean ? "true" : "false";
		else if (strFormat == "n")
			ss = boolean ? "y" : "n";
		else if (strFormat == "N")
			ss = boolean ? "Y" : "N";
		else if (strFormat == "no")
			ss = boolean ? "yes" : "no";
		else if (strFormat == "NO")
			ss = boolean ? "YES" : "NO";
		else if (strFormat == "off")
			ss = boolean ? "on" : "off";
		else if (strFormat == "OFF")
			ss = boolean ? "ON" : "OFF";
		else if (strFormat == "0")
			ss = boolean ? "1" : "0";
		else
			ss = boolean ? "TRUE" : "FALSE";
		break;

	case TYPE_INT:
		if (size == -1) size = 2;

		// We want to make sure we display the number of digits appropriate to the actual size of
		// the value (size of operands in expression?) but not lose bits if there was overflow.
		// So for each size make sure all unused bits are the same as the top-most used bit for
		// signed types OR unused bits are all zero for unsigned types.
		if (size == 1)
		{
			if (!unsgned && (int64 & ~0x7Fi64) != ~0x7Fi64 && (int64 & ~0x7Fi64) != 0 ||
				 unsgned && (int64 & ~0xFFi64) != 0)
			{
				size = 2;
			}
		}
		if (size == 2)
		{
			if (!unsgned && (int64 & ~0x7FFFi64) != ~0x7FFFi64 && (int64 & ~0x7FFFi64) != 0 ||
				 unsgned && (int64 & ~0xFFFFi64) != 0)
			{
				size = 4;
			}
		}
		if (size == 4)
		{
			if (!unsgned && (int64 & ~0x7fffFFFFi64) != ~0x7fffFFFFi64 && (int64 & ~0x7fffFFFFi64) != 0 ||
				 unsgned && (int64 & ~0xffffFFFFi64) != 0)
			{
				size = 8;
			}
		}

		ASSERT(size == 1 || size == 2 || size == 4 || size == 8);
		if (strFormat.Left(3).CompareNoCase("hex") == 0)
		{
			if (theApp.hex_ucase_)
				sprintf(disp, "%*.*I64X", size*2, size*2, int64);  // pass number of digits (4-bit nybbles) to sprintf
			else
				sprintf(disp, "%*.*I64x", size*2, size*2, int64);
			ss = disp;
			AddSpaces(ss);
		}
		else if (strFormat.Left(3).CompareNoCase("bin") == 0)
		{
			ss = bin_str(int64, size*8);                           // pass the number of bits to bin_str
		}
		else if (strFormat.Left(3).CompareNoCase("oct") == 0)
		{
			sprintf(disp, "%I64o", int64);
			ss = disp;
		}
		else if (strFormat.Find('%') == -1)
		{
			sprintf(disp, "%I64d", int64);
			ss = disp;
			AddCommas(ss);
		}
		else
		{
			if (strchr("diouxX", *(const char *)strFormat.Right(1)) != NULL)
				strFormat.Insert(strFormat.GetLength()-1, "I64");
			sprintf(disp, strFormat, int64, int64, int64, int64);  // Add 4 times in case strFormat contains multiple format strings
			ss = disp;
		}
		break;
	case TYPE_DATE:
		if (date > -1e30)
		{
			COleDateTime odt;
			odt.m_dt = date;
			odt.m_status = COleDateTime::valid;
			ss = odt.Format(strFormat);
		}
		else
			ss = "##Invalid date##";
		break;

	case TYPE_REAL:
		switch (_fpclass(real64))
		{
		case _FPCLASS_SNAN:
		case _FPCLASS_QNAN:
			ss = "NaN";
			break;
		case _FPCLASS_NINF:
			ss = "-Inf";
			break;
		case _FPCLASS_PINF:
			ss = "+Inf";
			break;
		default:
			if (strFormat.Find('%') != -1)
				ss.Format(strFormat, real64, real64, real64);
			else if (size == 8)
				ss.Format("%.15g", real64);
			else
				ss.Format("%.7g", real64);
			break;
		}
		break;

	case TYPE_STRING:
#ifdef UNICODE_TYPE_STRING
		if (strFormat.Find('%') == -1)
			sw.Format(L"%s", *pstr);
		else
			sw.Format(CStringW(strFormat), *pstr, *pstr);
#else
		if (strFormat.Find('%') == -1)
			ss.Format("%s", *pstr);
		else
			ss.Format(strFormat, *pstr, *pstr);
#endif
		break;
	}

#ifdef UNICODE_TYPE_STRING
	if (!ss.IsEmpty())
	{
		ASSERT(sw.IsEmpty());
		sw = CStringW(ss);
	}
	return sw;
#else
	ASSERT(!ss.IsEmpty());
	return ss;
#endif
}
