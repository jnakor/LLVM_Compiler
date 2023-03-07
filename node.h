#include <iostream>
#include <vector>
#include <llvm/IR/Value.h>

class CodeGenContext;
class NStatement;
class NExpression;
class NVariableDeclaration;
class NVariableDeclarationS;
class NIdentifier;
class NIFStatement;


typedef std::vector<NStatement*> StatementList;
typedef std::vector<NExpression*> ExpressionList;
typedef std::vector<NVariableDeclaration*> VariableList;
typedef std::vector<NIdentifier *> IdentifierList;

class Node {
public:
	virtual ~Node() {}
	virtual llvm::Value* codeGen(CodeGenContext& context) { return NULL; }
};

class NExpression : public Node {
};

class NStatement : public Node {
};

class NInteger : public NExpression {
public:
	long long value;
	NInteger(long long value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NDouble : public NExpression {
public:
	double value;
	NDouble(double value) : value(value) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NIdentifier : public NExpression {
public:
	std::string name;
	NIdentifier(const std::string& name) : name(name) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NMethodCall : public NExpression {
public:
	const NIdentifier& id;
	ExpressionList arguments;
	NMethodCall(const NIdentifier& id, ExpressionList& arguments) :
		id(id), arguments(arguments) { }
	NMethodCall(const NIdentifier& id) : id(id) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NBinaryOperator : public NExpression {
public:
	int op;
	NExpression& lhs;
	NExpression& rhs;
	NBinaryOperator(NExpression& lhs, int op, NExpression& rhs) :
		lhs(lhs), rhs(rhs), op(op) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NArrayAssignment : public NExpression {
	public:
		NIdentifier& lhs;
		NExpression& index;
		NExpression& rhs;
		NArrayAssignment(NIdentifier&lhs, NExpression& index, NExpression& rhs): 
			lhs(lhs), index(index), rhs(rhs) { }
		virtual llvm::Value* codeGen(CodeGenContext& context);
};
class NAssignment : public NExpression {
public:
	NIdentifier& lhs;
	NExpression& rhs;
	NAssignment(NIdentifier& lhs, NExpression& rhs) : 
		lhs(lhs), rhs(rhs) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NBlock : public NExpression {
public:
	StatementList statements;
	NBlock() { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NArrayRef : public NExpression {
public:
	NIdentifier& id;
	NExpression& index;
	NArrayRef(NIdentifier& id, NExpression& index) : id(id), index(index) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NExpressionStatement : public NStatement {
public:
	NExpression& expression;
	NExpressionStatement(NExpression& expression) : 
		expression(expression) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NReturnStatement : public NStatement {
public:
	NExpression& expression;
	NReturnStatement(NExpression& expression) : 
		expression(expression) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NVariableDeclaration : public NStatement {
public:
	const NIdentifier& type;
	NIdentifier& id;
	NExpression *assignmentExpr;
	NVariableDeclaration(const NIdentifier& type, NIdentifier& id) :
		type(type), id(id) { assignmentExpr = NULL; }
	NVariableDeclaration(const NIdentifier& type, NIdentifier& id, NExpression *assignmentExpr) :
		type(type), id(id), assignmentExpr(assignmentExpr) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NExternDeclaration : public NStatement {
public:
    const NIdentifier& type;
    const NIdentifier& id;
    VariableList arguments;
    NExternDeclaration(const NIdentifier& type, const NIdentifier& id,
            const VariableList& arguments) :
        type(type), id(id), arguments(arguments) {}
    virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NFunctionDeclaration : public NStatement {
public:
	const NIdentifier& type;
	const NIdentifier& id;
	VariableList arguments;
	NBlock& block;
	NFunctionDeclaration(const NIdentifier& type, const NIdentifier& id, 
			const VariableList& arguments, NBlock& block) :
		type(type), id(id), arguments(arguments), block(block) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NVariableDeclarationS : public NStatement {
public:
	std::vector<NVariableDeclaration *>VariableDeclarationList;
	NExpression *assignmentExpr;
	NVariableDeclarationS()  { assignmentExpr = NULL; }
	NVariableDeclarationS( std::vector<NVariableDeclaration *>VariableDeclarationList) :
		 VariableDeclarationList(VariableDeclarationList) { assignmentExpr = NULL; }
	NVariableDeclarationS( std::vector<NVariableDeclaration *>VariableDeclarationList, NExpression *assignmentExpr) :
		 VariableDeclarationList(VariableDeclarationList), assignmentExpr(assignmentExpr) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NIFStatement : public NStatement {
public:
	NExpression& condition;
    NExpression& ifStatement;
    NExpression& elseStatement;

    NIFStatement(NExpression& condition, NExpression& ifStatement, NExpression& elseStatement)
        : condition(condition), ifStatement(ifStatement), elseStatement(elseStatement) {}

    virtual llvm::Value* codeGen(CodeGenContext& context);
};

class NArrayDeclaration : public NStatement {
public:
	const NIdentifier& type;
	const NIdentifier& id;
	const NInteger& sz;
	NArrayDeclaration(const NIdentifier& type, const NIdentifier& id, const NInteger& sz) :
		type(type), id(id), sz(sz) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class FORStatement : public NStatement {
public:
	NIdentifier& iter;
	NExpression& condition_start;
	NExpression& condition_end;
	NExpression& for_block;
	FORStatement(NIdentifier& iter,NExpression& condition_start, NExpression& condition_end,NExpression& for_block):
		iter(iter),condition_start(condition_start), condition_end(condition_end),for_block(for_block) { }
	virtual llvm::Value* codeGen(CodeGenContext& context);
};

class FOREACHStatement : public NStatement {
public:
	NIdentifier& iter;
	NIdentifier& id;
	NExpression& for_block;
	FOREACHStatement(NIdentifier& iter, NIdentifier& id, NExpression& for_block):
		iter(iter), id(id), for_block(for_block) {}
	virtual llvm::Value* codeGen(CodeGenContext& context);
};