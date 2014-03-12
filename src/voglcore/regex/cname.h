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

/* character-name table */
static struct cname
{
    char *name;
    char code;
} cnames[] =
      {
          { "NUL", '\0' },
          { "SOH", '\001' },
          { "STX", '\002' },
          { "ETX", '\003' },
          { "EOT", '\004' },
          { "ENQ", '\005' },
          { "ACK", '\006' },
          { "BEL", '\007' },
          { "alert", '\007' },
          { "BS", '\010' },
          { "backspace", '\b' },
          { "HT", '\011' },
          { "tab", '\t' },
          { "LF", '\012' },
          { "newline", '\n' },
          { "VT", '\013' },
          { "vertical-tab", '\v' },
          { "FF", '\014' },
          { "form-feed", '\f' },
          { "CR", '\015' },
          { "carriage-return", '\r' },
          { "SO", '\016' },
          { "SI", '\017' },
          { "DLE", '\020' },
          { "DC1", '\021' },
          { "DC2", '\022' },
          { "DC3", '\023' },
          { "DC4", '\024' },
          { "NAK", '\025' },
          { "SYN", '\026' },
          { "ETB", '\027' },
          { "CAN", '\030' },
          { "EM", '\031' },
          { "SUB", '\032' },
          { "ESC", '\033' },
          { "IS4", '\034' },
          { "FS", '\034' },
          { "IS3", '\035' },
          { "GS", '\035' },
          { "IS2", '\036' },
          { "RS", '\036' },
          { "IS1", '\037' },
          { "US", '\037' },
          { "space", ' ' },
          { "exclamation-mark", '!' },
          { "quotation-mark", '"' },
          { "number-sign", '#' },
          { "dollar-sign", '$' },
          { "percent-sign", '%' },
          { "ampersand", '&' },
          { "apostrophe", '\'' },
          { "left-parenthesis", '(' },
          { "right-parenthesis", ')' },
          { "asterisk", '*' },
          { "plus-sign", '+' },
          { "comma", ',' },
          { "hyphen", '-' },
          { "hyphen-minus", '-' },
          { "period", '.' },
          { "full-stop", '.' },
          { "slash", '/' },
          { "solidus", '/' },
          { "zero", '0' },
          { "one", '1' },
          { "two", '2' },
          { "three", '3' },
          { "four", '4' },
          { "five", '5' },
          { "six", '6' },
          { "seven", '7' },
          { "eight", '8' },
          { "nine", '9' },
          { "colon", ':' },
          { "semicolon", ';' },
          { "less-than-sign", '<' },
          { "equals-sign", '=' },
          { "greater-than-sign", '>' },
          { "question-mark", '?' },
          { "commercial-at", '@' },
          { "left-square-bracket", '[' },
          { "backslash", '\\' },
          { "reverse-solidus", '\\' },
          { "right-square-bracket", ']' },
          { "circumflex", '^' },
          { "circumflex-accent", '^' },
          { "underscore", '_' },
          { "low-line", '_' },
          { "grave-accent", '`' },
          { "left-brace", '{' },
          { "left-curly-bracket", '{' },
          { "vertical-line", '|' },
          { "right-brace", '}' },
          { "right-curly-bracket", '}' },
          { "tilde", '~' },
          { "DEL", '\177' },
          { NULL, 0 },
      };
