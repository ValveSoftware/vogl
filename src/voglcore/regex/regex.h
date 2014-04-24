/**************************************************************************
 *
 * Copyright 2013-2014 RAD Game Tools and Valve Software
 * Copyright 2010-2014 Rich Geldreich and Tenacious Software LLC
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/

// See http://pubs.opengroup.org/onlinepubs/7908799/xbd/re.html
// Or http://en.wikipedia.org/wiki/Regular_expression#POSIX_extended
// Or http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Regular-Expressions.html#Regular-Expressions

/*
	From Wikipedia, because I'm lazy:

	Most formalisms provide the following operations to construct regular expressions.

	Boolean "or"
		A vertical bar separates alternatives. For example, gray|grey can match "gray" or "grey".

	Grouping
		Parentheses are used to define the scope and precedence of the
		operators (among other uses). For example, gray|grey and gr(a|e)y are equivalent
		patterns which both describe the set of "gray" or "grey".

	Quantification
		A quantifier after a token (such as a character) or group specifies how often
		that preceding element is allowed to occur. The most common quantifiers are the
		question mark ?, the asterisk * (derived from the Kleene star), and the plus
		sign + (Kleene cross).

	?	The question mark indicates there is zero or one of the preceding element. For
	example, colou?r matches both "color" and "colour".

	*	The asterisk indicates there is zero or more of the preceding element. For
	example, ab*c matches "ac", "abc", "abbc", "abbbc", and so on.

	+	The plus sign indicates there is one or more of the preceding element. For
	example, ab+c matches "abc", "abbc", "abbbc", and so on, but not "ac".

	These constructions can be combined to form arbitrarily complex expressions,
	much like one can construct arithmetical expressions from numbers and the
	operations +, âˆ’, Ã—, and Ã·. For example, H(ae?|Ã¤)ndel and H(a|ae|Ã¤)ndel are
	both valid patterns which match the same strings as the earlier example,
	H(Ã¤|ae?)ndel.

	POSIX:

	.	Matches any single character (many applications exclude newlines, and exactly
	which characters are considered newlines is flavor-, character-encoding-, and
	platform-specific, but it is safe to assume that the line feed character is
	included). Within POSIX bracket expressions, the dot character matches a literal
	dot. For example, a.c matches "abc", etc., but [a.c] matches only "a", ".", or
	"c".

	[ ]	A bracket expression. Matches a single character that is contained within
	the brackets. For example, [abc] matches "a", "b", or "c". [a-z] specifies a
	range which matches any lowercase letter from "a" to "z". These forms can be
	mixed: [abcx-z] matches "a", "b", "c", "x", "y", or "z", as does [a-cx-z]. The -
	character is treated as a literal character if it is the last or the first
	(after the ^) character within the brackets: [abc-], [-abc]. Note that backslash
	escapes are not allowed. The ] character can be included in a bracket expression
	if it is the first (after the ^) character: []abc].

	[^ ]	Matches a single character that is not contained within the brackets. For
	example, [^abc] matches any character other than "a", "b", or "c". [^a-z]
	matches any single character that is not a lowercase letter from "a" to "z".
	Likewise, literal characters and ranges can be mixed.

	^	Matches the starting position within the string. In line-based tools, it
	matches the starting position of any line.

	$	Matches the ending position of the string or the position just before a
	string-ending newline. In line-based tools, it matches the ending position of
	any line.

	( )	Defines a marked subexpression. The string matched within the parentheses
	can be recalled later (see the next entry, \n). A marked subexpression is also
	called a block or capturing group. BRE mode requires \( \).

	\n	Matches what the nth marked subexpression matched, where n is a digit from 1
	to 9. This construct is vaguely defined in the POSIX.2 standard. Some tools
	allow referencing more than nine capturing groups.

	*	Matches the preceding element zero or more times. For example, ab*c matches
	"ac", "abc", "abbbc", etc. [xyz]* matches "", "x", "y", "z", "zx", "zyx",
	"xyzzy", and so on. (ab)* matches "", "ab", "abab", "ababab", and so on.

	{m,n}	Matches the preceding element at least m and not more than n times. For
	example, a{3,5} matches only "aaa", "aaaa", and "aaaaa". This is not found in a
	few older instances of regular expressions. BRE mode requires \{m,n\}.

	Examples:
	.at matches any three-character string ending with "at", including "hat", "cat", and "bat".
	[hc]at matches "hat" and "cat".
	[^b]at matches all strings matched by .at except "bat".
	[^hc]at matches all strings matched by .at other than "hat" and "cat".
	^[hc]at matches "hat" and "cat", but only at the beginning of the string or line.
	[hc]at$ matches "hat" and "cat", but only at the end of the string or line.
	\[.\] matches any single character surrounded by "[" and "]" since the brackets are escaped, for example: "[a]" and "[b]".

	POSIX extended
	The meaning of metacharacters escaped with a backslash is reversed for some
	characters in the POSIX Extended Regular Expression (ERE) syntax. With this
	syntax, a backslash causes the metacharacter to be treated as a literal
	character. So, for example, \( \) is now ( ) and \{ \} is now { }. Additionally,
	support is removed for \n backreferences and the following metacharacters are
	added:

	?	Matches the preceding element zero or one time. For example, ab?c matches only
	"ac" or "abc".

	+	Matches the preceding element one or more times. For example, ab+c matches
	"abc", "abbc", "abbbc", and so on, but not "ac".

	|	The choice (also known as alternation or set union) operator matches either
	the expression before or the expression after the operator. For example, abc|def
	matches "abc" or "def".

	Examples:
	[hc]+at matches "hat", "cat", "hhat", "chat", "hcat", "cchchat", and so on, but not "at".
	[hc]?at matches "hat", "cat", and "at".
	[hc]*at matches "hat", "cat", "hhat", "chat", "hcat", "cchchat", "at", and so on.
	cat|dog matches "cat" or "dog".

	A bound is `{' followed by an unsigned decimal integer,  possibly  fol-
	 lowed  by  `,'  possibly  followed by another unsigned decimal integer,
	 always followed by `}'.  The integers must lie between 0 and RE_DUP_MAX
	 (255-)  inclusive,  and  if  there  are  two of them, the first may not
	 exceed the second.  An atom followed by a bound containing one  integer
	 i and no comma matches a sequence of exactly i matches of the atom.  An
	 atom followed by a bound containing one integer i and a comma matches a
	 sequence of i or more matches of the atom.  An atom followed by a bound
	 containing two integers i and j matches  a  sequence  of  i  through  j
	 (inclusive) matches of the atom.
*/

#pragma once

/* ========= begin header generated by ./mkh ========= */
#ifdef __cplusplus
extern "C" {
#endif

/* === regex2.h === */
// Don't depend on off_t, it can change depending on which macros where defined before system headers are included.
//typedef off_t regoff_t;
typedef long long regoff_t;
typedef struct
{
    int re_magic;
    size_t re_nsub;       /* number of parenthesized subexpressions */
    const char *re_endp;  /* end pointer for REG_PEND */
    struct re_guts *re_g; /* none of your business :-) */
} regex_t;

typedef struct
{
    regoff_t rm_so; /* start of match */
    regoff_t rm_eo; /* end of match */
} regmatch_t;

/* === regcomp.c === */
extern int vogl_regcomp(regex_t *, const char *, int);
#define REG_BASIC 0000
#define REG_EXTENDED 0001
#define REG_ICASE 0002
#define REG_NOSUB 0004
#define REG_NEWLINE 0010
#define REG_NOSPEC 0020
#define REG_PEND 0040
#define REG_DUMP 0200

/* === regerror.c === */
#define REG_OKAY 0
#define REG_NOMATCH 1
#define REG_BADPAT 2
#define REG_ECOLLATE 3
#define REG_ECTYPE 4
#define REG_EESCAPE 5
#define REG_ESUBREG 6
#define REG_EBRACK 7
#define REG_EPAREN 8
#define REG_EBRACE 9
#define REG_BADBR 10
#define REG_ERANGE 11
#define REG_ESPACE 12
#define REG_BADRPT 13
#define REG_EMPTY 14
#define REG_ASSERT 15
#define REG_INVARG 16
#define REG_ATOI 255  /* convert name to number (!) */
#define REG_ITOA 0400 /* convert number to name (!) */
extern size_t vogl_regerror(int, const regex_t *, char *, size_t);

/* === regexec.c === */
extern int vogl_regexec(const regex_t *, const char *, size_t, regmatch_t[], int);
#define REG_NOTBOL 00001
#define REG_NOTEOL 00002
#define REG_STARTEND 00004
#define REG_TRACE 00400 /* tracing of execution */
#define REG_LARGE 01000 /* force large representation */
#define REG_BACKR 02000 /* force use of backref code */

/* === regfree.c === */
extern void vogl_regfree(regex_t *);

#ifdef __cplusplus
}
#endif
/* ========= end header generated by ./mkh ========= */
