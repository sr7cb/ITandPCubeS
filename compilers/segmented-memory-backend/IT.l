/*--------------------------------------------Prolog------------------------------------------------*/
%{
#include <string.h>
#include <stdio.h>
#include "lib/scanner.h"
#include "lib/parser.h" // for token codes, yylval
#include "src/utils/list.h"

#define TAB_SIZE 8
#define DIM_LENGTH 3
#define DIMENSION_LENGTH 9

/* Global variables
 * ----------------
 * (For shame!) But we need a few to keep track of things that are
 * preserved between calls to yylex or used outside the scanner.
 */
static int curLineNum, curColNum;
List<const char*> savedLines;

static void resetLine();
static void adjustLineAndColumnNo(char *text, char token);
static void DoBeforeEachAction();

#define YY_USER_ACTION DoBeforeEachAction();

%}

/*----------------------------------------Declarations----------------------------------------------*/
/* States
 * CODE: is for the code section of a task
 * PROGRAM: is for the coordinator function of the program
 * FREE_STRING: is for header and libraries of extern code blocks
 * COPY: is for copying the content of a line 	
 */
%s CODE 
%s PROGRAM
%s FREE_STRING
%x COPY
%option stack
%option noyywrap

OPERATOR          ([-+/*%=.,;:!<>()[\]{}&^|])
SINGLE_COMMENT    ("//"[^\n]*)
comment_line      ("//"[^\n]*\n)


/*---------------------------------------------Rules------------------------------------------------*/
%%

{SINGLE_COMMENT}       { /* skip to end of line for // comment */ }

@Extern								return Extern;
@Language							return Language;
@Includes							return Header_Includes;
@Libraries							return Library_Links;

<FREE_STRING>[^,\n[:space:]}{]+					{
								yylval.stringConstant = (char *) malloc(strlen(yytext) + 1);
								strcpy(yylval.stringConstant, yytext);
								return String;
								}

"${"[^\$]+"}$"							{
								adjustLineAndColumnNo(yytext, '{'); 
								char *startPtr = strchr(yytext,'{') + 1;
								char *endPtr = strrchr(yytext, '}');
								int charCount = endPtr - startPtr;
								char *codeBody = (char *) malloc(charCount + 1);
								strncpy(codeBody, startPtr, charCount);
								codeBody[charCount] = '\0'; 
								yylval.stringConstant = codeBody;
								return Native_Code;
								}

<CODE,PROGRAM>[[:space:]]*"{"([[:space:]]|{comment_line})*	adjustLineAndColumnNo(yytext, '{'); return '{';
<CODE,PROGRAM>[[:space:]]*"}"					adjustLineAndColumnNo(yytext, '}'); return '}';	
<CODE,PROGRAM>\\[ \t]*\n					resetLine();
<CODE,PROGRAM,FREE_STRING>\n					resetLine(); return New_Line;
[\n]								resetLine();
[ ]+                   						;		
[\t]                						curColNum += TAB_SIZE - curColNum % TAB_SIZE + 1;
[[:space:]]*":"[[:space:]]*					adjustLineAndColumnNo(yytext, ':'); return ':';				

<PROGRAM>execute						return Execute;
<PROGRAM>new							return New;
<CODE>at							return At;

Program			return Program;
Tuple			return Tuple;
Task			return Task;
Function		return Function;
Define			return S_Define;
Environment		return S_Environment;
Initialize		return S_Initialize;
Compute			return S_Compute;
Partition		return S_Partition;
Arguments		return S_Arguments;
Results			return S_Results;

Integer			return T_Integer;
Character		return T_Character;
Real			return T_Real;
Boolean			return T_Boolean;
Epoch			return T_Epoch;
Index			return T_Index;
Range			return T_Range;
Array			return T_Array;
List			return T_List;
Reduction		return Reduction;

[0-9]+D			{
				int length = strlen(yytext);
				char *dimensionality = (char *) malloc(length);
				strncpy(dimensionality, yytext, length - 1);
				dimensionality[length] = '\0';
				yylval.intConstant = atoi(dimensionality);
				free(dimensionality); 
				return Dimensionality;
			}
dimension[0-9]+		{
				char *dimensionNo = yytext + DIMENSION_LENGTH;
				yylval.intConstant = atoi(dimensionNo);
				return Dimension_No;
			}
dimension		yylval.intConstant = 0; return Dimension_No;

"single-precision"	return Single;
"double-precision"	return Double;
format			return Format;

link			return Link;
create			return Create;
"link-or-create"	return Link_or_Create;	

"<="			return O_LTE;
">="			return O_GTE;
"=="			return O_EQ;
"!="			return O_NE;
">>"			return O_RSH;
"<<"			return O_LSH;
"**"			return O_POWER;
"||"			return O_OR;
"or"			return O_OR;
"&&"			return O_AND;
"and"			return O_AND;
"..."			return O_SB_RANGE;

reduce			return REDUCE;
Repeat			return Repeat;
Activate		return Activate;
do			return Do;
while			return While;
for			return For;
if			return If;
else			return Else;
in			return In;
from			return From;
to			return To;
step			return Step;
foreach			return Foreach;
sequence		return Sequence;
of			return Of;
"sub-partition"		return C_Sub_Partition;
local			return Local;
range			return Range;
index			return Index;

dynamic			return Dynamic;
"Sub-partition"		return P_Sub_Partition;
unordered		return Unordered;
ordered			return Ordered;
replicated		return Replicated;
padding			return Padding;
"relative-to"		return Relative_To;
divides			return Divides;
"sub-partitions"	return Sub_Partitions;
partitions		return Partitions;
"un-partitioned"	return Unpartitioned;
ascends			return Ascends;
descends		return Descends;

Space			return T_Space;
dim[0-9]+		{	
				char *dimensionality = yytext + DIM_LENGTH;
				yylval.intConstant = atoi(dimensionality);
				return V_Dimension;
			}

TRUE			yylval.booleanConstant = true; return Boolean;
FALSE			yylval.booleanConstant = false; return Boolean;
\'.\'			yylval.characterConstant = yytext[1]; return Character;
\"[^\"\n]*\"		{	
				int actualLength = strlen(yytext) - 2;
				yylval.stringConstant = (char *) malloc(actualLength + 1);
                                strncpy(yylval.stringConstant, yytext + 1, actualLength);
				yylval.stringConstant[actualLength] = '\0';	
				return String;
			} 
[0-9]+\.[0-9]+  	{
				int length = strlen(yytext);
				char *dotPosition = strchr(yytext, '.');
				int dotIndex = dotPosition - yytext;
				if (length - dotIndex <= 6) {
					yylval.floatConstant = atof(yytext); return Real_Single;
				} else { 
					yylval.doubleConstant = atof(yytext); return Real_Double;
				}
			}
[0-9]+          	yylval.intConstant = atoi(yytext); return Integer;
[A-Z][0-9a-zA-Z_]+	{
				yylval.stringConstant = (char *) malloc(strlen(yytext) + 1);
			 	strcpy(yylval.stringConstant, yytext);
                  		return Type_Name;
			}
[a-z][0-9a-zA-Z_]*	{	
				yylval.stringConstant = (char *) malloc(strlen(yytext) + 1);
			 	strcpy(yylval.stringConstant, yytext);
                  		return Variable_Name;
			}
[A-Z]			yylval.characterConstant = yytext[0]; return Space_ID;

{OPERATOR}          	return yytext[0];

.               	fprintf(stderr,"Unexpected character %c (%2X)\n",yytext[0],yytext[0]);

%%

/*-------------------------------------------Epilogue----------------------------------------------*/

/* Function: InitScanner
 * ---------------------
 * This function will be called before any calls to yylex().  It is designed
 * to give you an opportunity to do anything that must be done to initialize
 * the scanner (set global variables, configure starting state, etc.). One
 * thing it already does for you is assign the value of the global variable
 * yy_flex_debug that controls whether flex prints debugging information
 * about each token and what rule was matched. If set to false, no information
 * is printed. Setting it to true will give you a running trail that might
 * be helpful when debugging your scanner.
 */
void InitScanner() {
    PrintDebug("lex", "Initializing scanner");
    yy_flex_debug = false;
  //  yy_push_state(COPY); // copy first line at start
    curLineNum = 1;
    curColNum = 1;
}

/* Function: DoBeforeEachAction()
 * ------------------------------
 * This function is installed as the YY_USER_ACTION. This is a place
 * to group code common to all actions.
 * On each match, we fill in the fields to record its location and
 * update our column counter.
 */
static void DoBeforeEachAction() {
   	yylloc.first_line = curLineNum;
   	yylloc.first_column = curColNum;
   	yylloc.last_column = curColNum + yyleng - 1;
   	curColNum += yyleng;
}

/* Function: GetLineNumbered()
 * ---------------------------
 * Returns string with contents of line numbered n or NULL if the
 * contents of that line are not available.  Our scanner copies
 * each line scanned and appends each to a list so we can later
 * retrieve them to report the context for errors.
 */
const char *GetLineNumbered(int num) {
   if (num <= 0 || num > savedLines.NumElements()) return NULL;
   return savedLines.Nth(num-1);
}

/* code section specific parsing functions
 * ------------------------------------
 * We use newlines as expression seperators inside the code sections. In rest of the source newlines are not 
 * important. Furthermore, any string is valid for external headers and libraries but not for other sections
 * of the program.  These functions are provided so that the parser can set up the context for lexical 
 * analyis properly for different sections of the program.
 */
void BeginCode() {BEGIN(CODE);}
void EndCode() {BEGIN(INITIAL);} 	
void BeginProgram() {BEGIN(PROGRAM);}
void EndProgram() {BEGIN(INITIAL);}
void BeginFreeString() { BEGIN(FREE_STRING); }
void EndFreeString() { BEGIN(CODE); } 	

static void resetLine() {
	curColNum = 1; curLineNum++;
}

static void adjustLineAndColumnNo(char *text, char token) {
   	curColNum -= yyleng;
	int length = strlen(text);
	for (int i = 0; i < length; i++) {
		char c = text[i];
		if (c == '\t') {
			curColNum += TAB_SIZE - curColNum % TAB_SIZE + 1;
		} else if (c == '\n') {
			curLineNum++; curColNum = 1;
		} else if (c == token) {
   			yylloc.first_line = curLineNum;
   			yylloc.first_column = curColNum;
   			yylloc.last_column = curColNum;
		}
	}
}
