grammar CACT;

@header {
    #include <vector>
}

/********** parser ***********/
compUnit
    : (decl | funcDef)(compUnit)? EOF
    ;

decl
    : constDecl
    | varDecl
    ;

constDecl
    : 'const' bType constDef(','constDef)*';' 
    ;

bType
    : 'int'
    | 'float'
    | 'char'
    ;

constDef
    : Ident ('[' IntConst ']')* '=' constInitVal
    ;

constInitVal
    : constExp
    | '{' (constInitVal (',' constInitVal)* )? '}'
    ;

varDecl
    : bType varDef (','varDef)*';' 
    ;

varDef
    : Ident ('[' IntConst ']')+ ('=' constInitVal)?  
    | Ident ('=' constInitVal)?                      
    ;

funcDef
    : funcType Ident '(' (funcParams)? ')' block
    ;

funcType
    : 'void'
    | 'int'
    | 'float'
    | 'char'
    ;

funcParams
    : funcParam (','funcParam)*
    ;

funcParam
    : bType Ident ('['(IntConst)? ']' ('[' IntConst ']')*)?
    ;



block
    : '{'(blockItem)*'}'
    ;

blockItem
    : decl 
    | stmt
    ;

stmt
    : lVal '=' exp ';'
    | (exp)? ';'
    | block
    | 'return' (exp)? ';'
    | 'if' '(' cond ')' stmt ('else' stmt)?
    | 'while' '(' cond ')' stmt
    | 'break' ';'
    | 'continue' ';'
    ;

exp
    : addExp
    ;

constExp
    : addExp
    ;

cond
    : lOrExp
    ;

lVal
    : Ident ( '[' exp ']' )*
    ;


funcRParams
    : exp (',' exp)*
    ;

primaryExp
    : '(' exp ')'
    | lVal
    | number
    ;

unaryExp
    : primaryExp
    | ('+' | '-' | '!') unaryExp
    | Ident '(' (funcRParams)? ')'
    ;

mulExp
    : unaryExp (('*' | '/' | '%') unaryExp)*
    ;


addExp
    : mulExp (('+' | '-') mulExp)*
    ;


relExp
    : addExp (('<' | '>' | '<=' | '>=') addExp)*
    ;

//消除左递归
eqExp
    : relExp (('==' | '!=') relExp)*
    ;


//消除左递归
lAndExp
    : eqExp (('&&') eqExp)*
    ;


//消除左递归
lOrExp
    : lAndExp (('||') lAndExp)*
    ;






/************ lexer(?) ****************/

NewLine: ('\r' '\n'? | '\n') -> skip ; 

WhiteSpace: [ \t]+ ->skip ;

LineComment: '//' ~[\r\n]* -> skip ;

BlockComment: '/*' .*? '*/' -> skip ;
Ident: [a-zA-Z_] [a-zA-Z_0-9]* ;
IntConst: DecimalConst | OctalConst | HexadecConst ;
number
    : IntConst 
    | CharConst 
    | FloatConst
    ;

FloatConst
    : DecimalFloatConst
    | HexadecimalFloatConst
    ;

fragment DecimalFloatConst
    : FractionalConstant ExponentPart? FloatingSuffix
    | DigitSequence ExponentPart FloatingSuffix
    ;

fragment HexadecimalFloatConst
    : HexadecimalPrefix HexadecimalFractionalConstant BinaryExponentPart FloatingSuffix? 
    | HexadecimalPrefix HexadecimalDigitSequence BinaryExponentPart FloatingSuffix?
    ;

fragment FractionalConstant
    : DigitSequence? '.' DigitSequence? 
    | DigitSequence '.'
    ;

fragment ExponentPart
    : ('e' | 'E') ('+' | '-')? DigitSequence
    ;

fragment DigitSequence
    : Digit Digit*
    ;


fragment HexadecimalFractionalConstant
    : HexadecimalDigitSequence? '.' HexadecimalDigitSequence
    | HexadecimalDigitSequence '.'
    ;

fragment BinaryExponentPart
    : ('p' | 'P') ('+' | '-')? DigitSequence
    ;

fragment HexadecimalDigitSequence
    : HexadecimalDigit HexadecimalDigit*
    ;

fragment FloatingSuffix
    : 'f' | 'F' | 'l' | 'L'
    ;
fragment DecimalConst: '0' |  NonzeroDigit Digit* ;
fragment NonzeroDigit: [1-9];
fragment Digit: [0-9];
fragment OctalConst: '0' OctalDigit+ ;
fragment OctalDigit: [0-7];
fragment HexadecConst:  HexadecimalPrefix HexadecimalDigit+;
fragment HexadecimalPrefix: '0x'|'0X';
fragment HexadecimalDigit: [0-9a-fA-F];

CharConst: '\'' Character '\'';
fragment Character:  ~[\r\n'];

