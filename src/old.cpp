#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_map>
#include "antlr4-runtime.h"
#include "CACTLexer.h"
#include "CACTParser.h"
#include "CACTBaseVisitor.h"

using namespace antlr4;

class SymbolTable {
    std::unordered_map<std::string, std::string> symbols;
public:
    bool addSymbol(const std::string& name, const std::string& type) {
        if (symbols.count(name)) return false;
        symbols[name] = type;
        return true;
    }

    bool contains(const std::string& name) const {
        return symbols.count(name) > 0;
    }

    std::string getType(const std::string& name) const {
        auto it = symbols.find(name);
        return (it != symbols.end()) ? it->second : "unknown";
    }
};


bool isConstLiteralExp(CACTParser::ConstExpContext* ctx) {
        if (!ctx || !ctx->addExp()) return false;
        if (ctx->addExp()->mulExp().size() != 1) return false;
        if (ctx->addExp()->mulExp(0)->unaryExp().size() != 1) return false;
        if (ctx->addExp()->mulExp(0)->unaryExp(0)->children.size() != 1) return false;
        auto primaryExp = ctx->addExp()->mulExp(0)->unaryExp(0)->primaryExp();
        return primaryExp && primaryExp->number();
    }


class CACTVisitorImpl : public CACTBaseVisitor {
    std::vector<SymbolTable> scopeStack;
    std::vector<std::string> errors;
    std::vector<std::string> currentFunctionReturnTypeStack;

    SymbolTable& currentScope() { return scopeStack.back(); }

    std::string processArrayDims(const std::vector<tree::TerminalNode*>& dims) {
        std::string result;
        for (auto dim : dims) result += "[" + dim->getText() + "]";
        return result;
    }

    bool checkConstInitVal(CACTParser::ConstInitValContext* ctx) {
        if (!ctx) return false;
        if (ctx->getText() == "{}") return true;
        if (ctx->constExp()) return isConstLiteralExp(ctx->constExp());
        for (auto child : ctx->constInitVal()) 
            if (!checkConstInitVal(child)) return false;
        return true;
    }

    bool isFlatInitForArray(CACTParser::ConstInitValContext* ctx) {
        // 数组初始化必须采用列表初始化，不允许直接使用常量表达式
        if (ctx->constExp()) return false;
        // 如果为空初始化，大括号中没有内容，也认为是平坦的（根据实际需求，可调整为错误）
        if (ctx->constInitVal().empty()) return true;
        // 遍历大括号内的每个元素，要求每个元素都是常量表达式，而非嵌套的列表
        for (auto child : ctx->constInitVal()) {
            if (!child->constExp()) {
                // 如果检测到嵌套的初始化列表，则返回 false
                return false;
            }
        }
        return true;
    }

    std::string getConstExpType(CACTParser::ConstExpContext* ctx) {
        if (!isConstLiteralExp(ctx)) return "unknown";
        auto number = ctx->addExp()->mulExp(0)->unaryExp(0)->primaryExp()->number();
        if (number->IntConst()) return "int";
        if (number->FloatConst()) return "float";
        if (number->CharConst()) return "char";
        return "unknown";
    }


    // 类型推断系统
    std::string getExpType(CACTParser::ExpContext* ctx) {
        return ctx ? getAddExpType(ctx->addExp()) : "unknown";
    }

    std::string getAddExpType(CACTParser::AddExpContext* ctx) {
        if (ctx->mulExp().size() == 1) return getMulExpType(ctx->mulExp(0));
        
        std::string resultType = "unknown";
        for (auto mulExp : ctx->mulExp()) {
            std::string currentType = getMulExpType(mulExp);
            if (resultType == "unknown") resultType = currentType;
            else if (resultType != currentType) {
                if (resultType == "float" || currentType == "float") resultType = "float";
                else if (resultType == "int" || currentType == "int") resultType = "int";
                else resultType = "unknown";
            }
        }
        return resultType;
    }

    std::string getMulExpType(CACTParser::MulExpContext* ctx) {
        if (ctx->unaryExp().size() == 1) return getUnaryExpType(ctx->unaryExp(0));
        
        std::string resultType = "unknown";
        for (auto unaryExp : ctx->unaryExp()) {
            std::string currentType = getUnaryExpType(unaryExp);
            if (resultType == "unknown") resultType = currentType;
            else if (resultType != currentType) {
                if (resultType == "float" || currentType == "float") resultType = "float";
                else if (resultType == "int" || currentType == "int") resultType = "int";
                else resultType = "unknown";
            }
        }
        return resultType;
    }

    std::string getUnaryExpType(CACTParser::UnaryExpContext* ctx) {
        if (ctx->primaryExp()) {
            auto primary = ctx->primaryExp();
            if (primary->number()) {
                auto num = primary->number();
                if (num->IntConst()) return "int";
                if (num->FloatConst()) return "float";
                if (num->CharConst()) return "char";
            }
            else if (primary->lVal()) {
                std::string varName = primary->lVal()->Ident()->getText();
                for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
                    if (it->contains(varName)) return it->getType(varName);
                }
                errors.push_back("Undeclared variable: " + varName);
            }
            else if (primary->exp()) {
                return getExpType(primary->exp());
            }
        }
        return "unknown";
    }

public:
    std::any visitCompUnit(CACTParser::CompUnitContext *ctx) override {
        scopeStack.push_back(SymbolTable());
        visitChildren(ctx);
        scopeStack.pop_back();
        return nullptr;
    }

    std::any visitFuncDef(CACTParser::FuncDefContext *ctx) override {
        std::string returnType = ctx->funcType()->getText();
        currentFunctionReturnTypeStack.push_back(returnType);

        scopeStack.push_back(SymbolTable());
        if (ctx->funcParams()) {
            for (auto param : ctx->funcParams()->funcParam()) {
                std::string paramType = param->bType()->getText();
                std::string paramName = param->Ident()->getText();
                currentScope().addSymbol(paramName, paramType);
            }
        }
        visitChildren(ctx);
        scopeStack.pop_back();

        currentFunctionReturnTypeStack.pop_back();
        return nullptr;
    }

    std::any visitBlock(CACTParser::BlockContext *ctx) override {
        scopeStack.push_back(SymbolTable());
        visitChildren(ctx);
        scopeStack.pop_back();
        return nullptr;
    }
    std::any visitConstDecl(CACTParser::ConstDeclContext *ctx) override {
        std::string baseType = ctx->bType()->getText();
        for (auto def : ctx->constDef()) {
            std::string name = def->Ident()->getText();
            std::vector<tree::TerminalNode*> dims;
            for (auto dim : def->IntConst()) dims.push_back(dim);
            std::string fullType = baseType + processArrayDims(dims);

            if (!currentScope().addSymbol(name, fullType)) {
                errors.push_back("Redeclaration of const: " + name);
            }

            auto initVal = def->constInitVal();
            if (!dims.empty()) {
                // 对数组常量初始化，禁止使用直接的常量表达式，以及嵌套的初始化器
                if (initVal->constExp()) {
                    errors.push_back("Invalid const array initialization: " + name);
                } else if (!isFlatInitForArray(initVal)) {
                    errors.push_back("Invalid const array initialization: " + name + " (nested initializer not allowed)");
                } else if (!checkConstInitVal(initVal)) {
                    errors.push_back("Invalid const array initialization: " + name);
                } else {
                    std::vector<CACTParser::ConstInitValContext*> stack = { initVal };
                    while (!stack.empty()) {
                        auto current = stack.back();
                        stack.pop_back();
                        if (current->constExp()) {
                            std::string literalType = getConstExpType(current->constExp());
                            if (literalType != baseType) {
                                errors.push_back("Type mismatch in const array element: " + name +
                                                " (expected " + baseType + ", got " + literalType + ")");
                            }
                        } else {
                            for (auto child : current->constInitVal()) {
                                stack.push_back(child);
                            }
                        }
                    }
                }
            } else {
                if (!initVal->constExp()) {
                    errors.push_back("Invalid scalar const initialization: " + name);
                } else if (!isConstLiteralExp(initVal->constExp())) {
                    errors.push_back("Invalid scalar const initialization: " + name);
                } else {
                    std::string literalType = getConstExpType(initVal->constExp());
                    if (literalType != baseType) {
                        errors.push_back("Type mismatch in const initialization: " + name +
                                        " (expected " + baseType + ", got " + literalType + ")");
                    }
                }
            }
        }
        return nullptr;
    }

    std::any visitVarDecl(CACTParser::VarDeclContext *ctx) override {
        std::string baseType = ctx->bType()->getText();
        for (auto def : ctx->varDef()) {
            std::string name = def->Ident()->getText();
            std::string fullType = baseType;
            std::vector<tree::TerminalNode*> dims;
            for (auto dim : def->IntConst()) dims.push_back(dim);
            if (!dims.empty()) fullType += processArrayDims(dims);

            if (!currentScope().addSymbol(name, fullType)) {
                errors.push_back("Redeclaration of variable: " + name);
            }

            if (def->constInitVal()) {
                auto initVal = def->constInitVal();
                if (!dims.empty()) {
                    // 对数组变量的初始化，同样要求必须采用列表初始化，且不能嵌套花括号
                    if (initVal->constExp()) {
                        errors.push_back("Invalid array initialization: " + name + " (only literal constants in list initialization allowed)");
                    } else if (!isFlatInitForArray(initVal)) {
                        errors.push_back("Invalid array initialization: " + name + " (nested initializer not allowed)");
                    } else if (!checkConstInitVal(initVal)) {
                        errors.push_back("Invalid array initialization: " + name + " (only literal constants in list initialization allowed)");
                    }
                } else {
                    // 标量变量必须用字面量常量初始化
                    if (!initVal->constExp()) {
                        errors.push_back("Invalid variable initialization: " + name + " (initialization must be a literal constant)");
                    } else if (!isConstLiteralExp(initVal->constExp())) {
                        errors.push_back("Invalid variable initialization: " + name + " (initialization must be a literal constant)");
                    } else {
                        std::string literalType = getConstExpType(initVal->constExp());
                        if (literalType != baseType) {
                            errors.push_back("Type mismatch in variable initialization: " + name +
                                            " (expected " + baseType + ", got " + literalType + ")");
                        }
                    }
                }
            }
        }
        return nullptr;
    }

    std::any visitStmt(CACTParser::StmtContext *ctx) override {
        // 处理赋值语句
        if (ctx->lVal() && ctx->exp()) {
            std::string name = ctx->lVal()->Ident()->getText();
            std::string lvalType = "unknown";
            for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
                if (it->contains(name)) {
                    lvalType = it->getType(name);
                    break;
                }
            }
            if (lvalType == "unknown") {
                errors.push_back("Undeclared variable in assignment: " + name);
            } else {
                std::string rtype = getExpType(ctx->exp());
                if (rtype != lvalType && rtype != "unknown") {
                    errors.push_back("Type mismatch in assignment to '" + name +
                                    "': expected " + lvalType + ", got " + rtype);
                }
            }
        }

        // 处理 return 语句
        if (ctx->getStart()->getText() == "return") {
            if (currentFunctionReturnTypeStack.empty()) {
                errors.push_back("Return statement outside of function");
                return nullptr;
            }
            std::string expectedType = currentFunctionReturnTypeStack.back();

            if (ctx->exp()) {
                if (expectedType == "void") {
                    errors.push_back("Void function cannot return a value");
                } else {
                    std::string actualType = getExpType(ctx->exp());
                    if (actualType != expectedType) {
                        errors.push_back("Return type mismatch: expected " + expectedType 
                                       + ", got " + actualType);
                    }
                }
            } else {
                if (expectedType != "void") {
                    errors.push_back("Non-void function must return a value");
                }
            }
        }

        return visitChildren(ctx);
    }

    const std::vector<std::string>& getErrors() const { return errors; }
};

class CACTErrorListener : public BaseErrorListener {
    std::vector<std::string>& errors;
public:
    CACTErrorListener(std::vector<std::string>& errs) : errors(errs) {}
    void syntaxError(Recognizer* recognizer, Token* offendingSymbol,
                    size_t line, size_t charPositionInLine,
                    const std::string& msg, std::exception_ptr e) override {
        errors.push_back("[Syntax Error] Line " + std::to_string(line) + ":" 
                        + std::to_string(charPositionInLine) + " " + msg);
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <CACT_file>" << std::endl;
        return 1;
    }

    std::vector<std::string> allErrors;
    try {
        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
            return 1;
        }

        ANTLRInputStream input(file);
        CACTLexer lexer(&input);
        lexer.removeErrorListeners();
        lexer.addErrorListener(new CACTErrorListener(allErrors));

        CommonTokenStream tokens(&lexer);
        CACTParser parser(&tokens);
        parser.removeErrorListeners();
        parser.addErrorListener(new CACTErrorListener(allErrors));

        CACTParser::CompUnitContext* tree = parser.compUnit();

        if (!allErrors.empty()) {
            for (const auto& err : allErrors) {
                std::cerr << err << std::endl;
            }
            return 1;
        }

        CACTVisitorImpl visitor;
        visitor.visitCompUnit(tree);

        for (const auto& err : visitor.getErrors()) {
            allErrors.push_back("[Semantic Error] " + err);
        }

    } catch (const std::exception& e) {
        allErrors.push_back("[Error] " + std::string(e.what()));
    }

    for (const auto& err : allErrors) {
        std::cerr << err << std::endl;
    }

    return allErrors.empty() ? 0 : 1;
}
