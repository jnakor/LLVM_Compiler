#include "node.h"
#include "codegen.h"
#include "parser.hpp"

using namespace std;

/* Compile the AST into a module */
void CodeGenContext::generateCode(NBlock& root)
{
	std::cout << "Generating code...\n";
	
	/* Create the top level interpreter function to call as entry */
	vector<Type*> argTypes;
	FunctionType *ftype = FunctionType::get(Type::getVoidTy(MyContext), makeArrayRef(argTypes), false);
	mainFunction = Function::Create(ftype, GlobalValue::ExternalLinkage, "main", module);
	BasicBlock *bblock = BasicBlock::Create(MyContext, "entry", mainFunction, 0);
	
	/* Push a new variable/block context */
	pushBlock(bblock);
	root.codeGen(*this); /* emit bytecode for the toplevel block */

	ReturnInst::Create(MyContext, this->currentBlock());//当前块，不再是bblock，块已经分离过了
	popBlock();
	
	/* Print the bytecode in a human-readable format 
	   to see if our program compiled properly
	 */
	std::cout << "Code is generated.\n";
	// module->dump();

	legacy::PassManager pm;
	pm.add(createPrintModulePass(outs()));
	pm.run(*module);
}

/* Executes the AST by running the main function */
GenericValue CodeGenContext::runCode() {
	std::cout << "Running code...\n";
	ExecutionEngine *ee = EngineBuilder( unique_ptr<Module>(module) ).create();
	ee->finalizeObject();
	vector<GenericValue> noargs;
	GenericValue v = ee->runFunction(mainFunction, noargs);
	std::cout << "Code was run.\n";
	return v;
}

/* Returns an LLVM type based on the identifier */
static Type *typeOf(const NIdentifier& type) 
{
	if (type.name.compare("int") == 0) {
		std::cout<<"isint"<<std::endl;
		return Type::getInt64Ty(MyContext);
	}
	else if (type.name.compare("double") == 0) {
		std::cout<<"isdouble"<<std::endl;
		return Type::getDoubleTy(MyContext);
	}
	return Type::getVoidTy(MyContext);
}

/* -- Code Generation -- */

Value* NInteger::codeGen(CodeGenContext& context)
{
	std::cout << "Creating integer: " << value << endl;
	
	return ConstantInt::get(Type::getInt64Ty(MyContext), value, true);
}


Value* NDouble::codeGen(CodeGenContext& context)
{
	std::cout << "Creating double: " << value << endl;
	return ConstantFP::get(Type::getDoubleTy(MyContext), value);
}

Value* NIdentifier::codeGen(CodeGenContext& context)
{
	std::cout << "Creating identifier reference: " << name << endl;
	if (context.locals().find(name) == context.locals().end()) {
		if(context.locals().find(name+"__PASCAL__RET")  == context.locals().end()){
			std::cerr << "undeclared variable " << name << endl;
			return NULL;
		}
		else{
			return new LoadInst(context.locals()[name+"__PASCAL__RET"]->getType(),context.locals()[name], "", false, context.currentBlock());
		}
	}
	else{
		return new LoadInst(context.locals()[name]->getType(),context.locals()[name], name, false, context.currentBlock());
	}
}

Value* NMethodCall::codeGen(CodeGenContext& context)
{
	Function *function = context.module->getFunction(id.name.c_str());
	if (function == NULL) {
		std::cerr << "no such function " << id.name << endl;
	}
	std::vector<Value*> args;
	ExpressionList::const_iterator it;
	for (it = arguments.begin(); it != arguments.end(); it++) {
		args.push_back((**it).codeGen(context));
	}
	CallInst *call = CallInst::Create(function, makeArrayRef(args), "", context.currentBlock());
	std::cout << "Creating method call: " << id.name << endl;
	return call;
}

Value* NBinaryOperator::codeGen(CodeGenContext& context)
{
	std::cout << "Creating binary operation " << op << endl;
	Instruction::BinaryOps instr;
	int swap_pos=0;
	switch (op) {
		case TPLUS: 	instr = Instruction::Add; goto math;
		case TMINUS: 	instr = Instruction::Sub; goto math;
		case TMUL: 		instr = Instruction::Mul; goto math;
		case TDIV: 		instr = Instruction::SDiv; goto math;
		/* TODO comparison */
		case TCLT:		swap_pos=1;instr = Instruction::Sub; goto math;
		case TCLE:		swap_pos=1;instr = Instruction::Sub; goto math;
		case TCGT:		instr = Instruction::Sub; goto math;
		case TCGE:		instr = Instruction::Sub; goto math;
	}
	
	return NULL;
math:
	if (swap_pos==1){
		return BinaryOperator::Create(instr, rhs.codeGen(context), 
			lhs.codeGen(context), "", context.currentBlock());
	}
	else{
		return BinaryOperator::Create(instr, lhs.codeGen(context), 
		rhs.codeGen(context), "", context.currentBlock());
	}

}

Value* NAssignment::codeGen(CodeGenContext& context)
{
	std::cout << "Creating assignment for " << lhs.name << endl;
	if (context.locals().find(lhs.name) == context.locals().end()) {
		if(context.locals().find(lhs.name+"__PASCAL__RET") ==context.locals().end()){
			std::cerr << "undeclared variable " << lhs.name << endl;
			return NULL;		
		}
		else{
			return new StoreInst(rhs.codeGen(context), context.locals()[lhs.name+"__PASCAL__RET"], false, context.currentBlock());

		}
	}
	else{
		return new StoreInst(rhs.codeGen(context), context.locals()[lhs.name], false, context.currentBlock());
	}
}

Value* NBlock::codeGen(CodeGenContext& context)
{
	StatementList::const_iterator it;
	Value *last = NULL;
	for (it = statements.begin(); it != statements.end(); it++) {
		std::cout << "Generating code for " << typeid(**it).name() << endl;
		last = (**it).codeGen(context);
	}
	std::cout << "Creating block" << endl;
	return last;
}

Value* NExpressionStatement::codeGen(CodeGenContext& context)
{
	std::cout << "Generating code for " << typeid(expression).name() << endl;
	return expression.codeGen(context);
}

Value* NReturnStatement::codeGen(CodeGenContext& context)
{
	std::cout << "Generating return code for " << typeid(expression).name() << endl;
	Value *returnValue = expression.codeGen(context);
	context.setCurrentReturnValue(returnValue);
	return returnValue;
}

Value* NVariableDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Creating variable declaration " << type.name << " " << id.name << endl;
	AllocaInst *alloc = new AllocaInst(typeOf(type),4, id.name.c_str(), context.currentBlock());
	context.locals()[id.name] = alloc;
	if (assignmentExpr != NULL) {
		NAssignment assn(id, *assignmentExpr);
		assn.codeGen(context);
	}
	return alloc;
}

Value* NVariableDeclarationS::codeGen(CodeGenContext& context)
{
	std::cout << "Creating variable declarationS " <<std::endl;
	Value* ret;
	for(int i=0;i<VariableDeclarationList.size();i++){
		ret=VariableDeclarationList[i]->codeGen(context);
	}
	return ret;

}

Value* NExternDeclaration::codeGen(CodeGenContext& context)
{
    vector<Type*> argTypes;
    VariableList::const_iterator it;
    for (it = arguments.begin(); it != arguments.end(); it++) {
        argTypes.push_back(typeOf((**it).type));
    }
    FunctionType *ftype = FunctionType::get(typeOf(type), makeArrayRef(argTypes), false);
    Function *function = Function::Create(ftype, GlobalValue::ExternalLinkage, id.name.c_str(), context.module);
    return function;
}

Value* NFunctionDeclaration::codeGen(CodeGenContext& context)
{
	vector<Type*> argTypes;
	VariableList::const_iterator it;
	for (it = arguments.begin(); it != arguments.end(); it++) {
		argTypes.push_back(typeOf((**it).type));
	}
	FunctionType *ftype = FunctionType::get(typeOf(type), makeArrayRef(argTypes), false);
	Function *function = Function::Create(ftype, GlobalValue::InternalLinkage, id.name.c_str(), context.module);
	BasicBlock *bblock = BasicBlock::Create(MyContext, "entry", function, 0);
	for (auto k: context.locals()) {
		cout<<"kkk:"<<k.first<<endl;
	}
	BasicBlock* topBlock = context.currentBlock();
	context.pushBlock(bblock);
	

	Function::arg_iterator argsValues = function->arg_begin();
    Value* argumentValue;

	for (it = arguments.begin(); it != arguments.end(); it++) {
		(**it).codeGen(context);
		
		argumentValue = &*argsValues++;
		argumentValue->setName((*it)->id.name.c_str());
		StoreInst *inst = new StoreInst(argumentValue, context.locals()[(*it)->id.name], false, bblock);
	}

	//中间构造返回值
	
	NIdentifier* ret_ident=new NIdentifier(id.name + "__PASCAL__RET");
	block.statements.push_back( new NReturnStatement(*ret_ident)); //返回操作加入到block中

	NVariableDeclaration *ret_define=new NVariableDeclaration( type, *ret_ident   );
	ret_define->codeGen(context);

	//返回值构造结束

	block.codeGen(context);
	ReturnInst::Create(MyContext, context.getCurrentReturnValue(), context.currentBlock());
	while(topBlock != context.currentBlock()){
		context.popBlock();
	}
	for (auto k: context.locals()) {
		cout<<"kkk:"<<k.first<<endl;
	}
	std::cout << "Creating function: " << id.name << endl;
	return function;
}

llvm::Value* NIFStatement::codeGen(CodeGenContext& context)
{	
    llvm::Value* condValue = condition.codeGen(context);
    if (!condValue)
        return nullptr;
	std::cout<<"0000"<<std::endl;
    llvm::Function* function = context.currentBlock()->getParent();

    // 创建基本块
    llvm::BasicBlock* thenBlock = llvm::BasicBlock::Create(MyContext, "then", function);
    llvm::BasicBlock* elseBlock = llvm::BasicBlock::Create(MyContext, "else", function);
    llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(MyContext, "ifcont", function);
	std::cout<<"1111"<<std::endl;
    // 如果条件为true，则跳转到then块；否则跳转到else块
	llvm::Value* zero = llvm::ConstantInt::get(MyContext, llvm::APInt(32, 0));
	llvm::Value* cmp = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_SGT, condValue, zero, "ifcond", context.currentBlock());
	BranchInst::Create(thenBlock, elseBlock, cmp, context.currentBlock());	
	std::map<std::string, Value*> templocals,templocals_1,templocals_2;
	std::map<std::string, Type*> temptps;
	std::map<std::string, ArrayType*> tempatps;
	templocals=context.locals();
	temptps = context.tps();
	tempatps = context.atps();

	std::cout<<"aaaa"<<std::endl;
    // 生成then块
	context.pushBlock(thenBlock);
	context.setTbs(templocals, temptps, tempatps);
	
	std::cout<<"ttttt"<<std::endl;
	llvm::Value* thenValue = ifStatement.codeGen(context);
    if (!thenValue)
	{
		std::cout<<"nononon"<<std::endl;
        return nullptr;
	}
	std::cout<<"1qqqqq"<<std::endl;
	BranchInst::Create	(mergeBlock, context.currentBlock() );
	context.popBlock();
	std::cout<<"bbbb"<<std::endl;
	context.pushBlock(elseBlock);
	context.setTbs(templocals, temptps, tempatps);
    // 生成else块
    llvm::Value* elseValue = elseStatement.codeGen(context);
    if (!elseValue)
        return nullptr;
    BranchInst::Create	(mergeBlock, context.currentBlock() );

	context.popBlock();

	context.pushBlock(mergeBlock);
	context.setTbs(templocals, temptps, tempatps);
	// phiNode->addIncoming(thenValue, thenBlock);
    // phiNode->addIncoming(elseValue, elseBlock);

    // return phiNode;
	return condValue;
}

Value* NArrayDeclaration::codeGen(CodeGenContext& context)
{
	std::cout << "Creating Array declaration " << type.name << " " << id.name << endl;
	Type* elementType = typeOf(type);
	ArrayType *arrayType = ArrayType::get(elementType, sz.value);
	// GlobalVariable* globalArray = new GlobalVariable(*context.module, arrayType, false, GlobalValue::ExternalLinkage, nullptr, id.name);
	AllocaInst* array = new AllocaInst(arrayType,4, id.name.c_str(), context.currentBlock());
	context.locals()[id.name] = array;
	context.atps()[id.name] = arrayType;
	context.tps()[id.name] = elementType;
	return array;
}

Value* NArrayRef::codeGen(CodeGenContext& context) 
{
	std::cout << "Creating ArrayRef declaration " << id.name << endl;
	AllocaInst* arrayVar = reinterpret_cast<AllocaInst*>(context.locals()[id.name]);
	Value* indexValue = index.codeGen(context);
	ArrayType* arrayType = context.atps()[id.name];
  	Type* elementType = context.tps()[id.name];
	Constant* zero = ConstantInt::get(Type::getInt32Ty(MyContext), 0);
	std::vector<Value*> gepIndices;
	gepIndices.push_back(zero);
	gepIndices.push_back(indexValue);
	Value* elementPtr = GetElementPtrInst::CreateInBounds(arrayType, arrayVar, gepIndices, "", context.currentBlock());
	return new LoadInst(elementType, elementPtr, "", false, context.currentBlock());
}

Value* NArrayAssignment::codeGen(CodeGenContext& context) 
{
	AllocaInst* arrayVar = reinterpret_cast<AllocaInst*>(context.locals()[lhs.name]);
	Value* indexValue = index.codeGen(context);
	ArrayType* arrayType = context.atps()[lhs.name];
  	Type* elementType = context.tps()[lhs.name];
	PointerType* elementPtrType = elementType->getPointerTo();
	Constant* zero = ConstantInt::get(elementType, 0);
	std::vector<Value*> gepIndices;
	gepIndices.push_back(zero);
	gepIndices.push_back(indexValue);
	Value* elementPtr = GetElementPtrInst::CreateInBounds(arrayType, arrayVar, gepIndices, "", context.currentBlock());
	return new StoreInst(rhs.codeGen(context), elementPtr, false, context.currentBlock());
}

Value* FORStatement::codeGen(CodeGenContext& context)
{
	
	// //循环变量赋初值
	NAssignment* inintal_assign= new NAssignment( iter,condition_start );
	inintal_assign->codeGen(context);
	// 每次增加的步长

	for(auto k: context.locals()) {
		cout<<"uuuuuuuuuuuu:"<<k.first<<endl;
	}
	NInteger *iter_add_value = new NInteger(1);

	Function *TheFunction=context.currentBlock()->getParent();

	BasicBlock *judgeBB=BasicBlock::Create(MyContext, "judge", TheFunction);
	BasicBlock *workBB=BasicBlock::Create(MyContext, "work", TheFunction);
	BasicBlock *endBB=BasicBlock::Create(MyContext, "end", TheFunction);
	
	BranchInst::Create	(judgeBB, context.currentBlock() );
	cout<<"aaaaaaaaaaaaaaaaaaaaaa"<<endl;
	std::map<std::string, Value*> templocals,templocals_1,templocals_2;
	std::map<std::string, Type*> temptps, temptps_1, temptps_2;
	std::map<std::string, ArrayType*> tempatps, tempatps_1, tempatps_2;
	templocals=context.locals();temptps = context.tps();tempatps = context.atps();
	context.popBlock();
	context.pushBlock(judgeBB);
	context.setTbs(templocals, temptps, tempatps);
	cout<<"bbbbbbbbbbbbbbbbbbbbbbbbbb"<<endl;
	LoadInst *lditer = new LoadInst(context.locals()[iter.name]->getType(), context.locals()[iter.name], iter.name, false, context.currentBlock());
	// ICmpInst *icmp = new ICmpInst(*context.currentBlock(), ICmpInst::ICMP_SLE , lditer, condition_end.codeGen(context) );	

	// 
	cout<<"ccccccccccccccccccccc"<<endl;
	llvm::Value* cmp = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_SLE, lditer, condition_end.codeGen(context), "statisify", context.currentBlock());
	// 
	cout<<"ddddddddddddddddddddd"<<endl;
	BranchInst::Create	(workBB, endBB, cmp ,context.currentBlock());	
	
	cout<<"eeeeeeeeeeeeeeeeeeeeeeeeeeee"<<endl;
	templocals_1=context.locals();temptps_1 = context.tps();tempatps_1 = context.atps();
	context.popBlock();
	context.pushBlock(workBB);
	context.setTbs(templocals_1, temptps_1, tempatps_1);

	for_block.codeGen(context);
	
	NBinaryOperator* additer=new NBinaryOperator(iter, TPLUS,  *iter_add_value );
	NAssignment *add_one_to_iter = new NAssignment(iter,*additer);
	add_one_to_iter->codeGen(context);
	BranchInst::Create	(	judgeBB, context.currentBlock() );

	context.popBlock();
	templocals_2=context.locals();temptps_2 = context.tps();tempatps_2 = context.atps();
	context.pushBlock(endBB);
	context.setTbs(templocals_2, temptps_2, tempatps_2);
	return cmp;
}

Value* FOREACHStatement::codeGen(CodeGenContext& context)
{
	
	// //循环变量赋初值
	AllocaInst *alloc_index = new AllocaInst(Type::getInt64Ty(MyContext),4, "index", context.currentBlock());
	context.locals()["index"] = alloc_index;
	// 每次增加的步长
	NInteger *iter_add_value = new NInteger(1);

	Function *TheFunction=context.currentBlock()->getParent();

	BasicBlock *judgeBB=BasicBlock::Create(MyContext, "judge", TheFunction);
	BasicBlock *workBB=BasicBlock::Create(MyContext, "work", TheFunction);
	BasicBlock *endBB=BasicBlock::Create(MyContext, "end", TheFunction);
	
	BranchInst::Create	(judgeBB, context.currentBlock() );
	cout<<"aaaaaaaaaaaaaaaaaaaaaa"<<endl;
	std::map<std::string, Value*> templocals,templocals_1,templocals_2;
	std::map<std::string, Type*> temptps, temptps_1, temptps_2;
	std::map<std::string, ArrayType*> tempatps, tempatps_1, tempatps_2;
	templocals=context.locals();temptps = context.tps();tempatps = context.atps();
	context.popBlock();
	context.pushBlock(judgeBB);
	context.setTbs(templocals, temptps, tempatps);
	cout<<"bbbbbbbbbbbbbbbbbbbbbbbbbb"<<endl;
	// LoadInst *lditer = new LoadInst(context.locals()[iter.name]->getType(), context.locals()[iter.name], iter.name, false, context.currentBlock());
	// ICmpInst *icmp = new ICmpInst(*context.currentBlock(), ICmpInst::ICMP_SLE , lditer, condition_end.codeGen(context) );	

	// 
	cout<<"ccccccccccccccccccccc"<<endl;
	llvm::Value* cmp = llvm::CmpInst::Create(llvm::Instruction::ICmp, llvm::CmpInst::ICMP_SLE, alloc_index, NInteger(5).codeGen(context), "statisify", context.currentBlock());
	// 
	cout<<"ddddddddddddddddddddd"<<endl;
	BranchInst::Create(workBB, endBB, cmp ,context.currentBlock());	
	
	// cout<<"eeeeeeeeeeeeeeeeeeeeeeeeeeee"<<endl;
	templocals_1=context.locals();temptps_1 = context.tps();tempatps_1 = context.atps();
	context.popBlock();
	context.pushBlock(workBB);
	context.setTbs(templocals_1, temptps_1, tempatps_1);
	AllocaInst *alloc_iter = new AllocaInst(context.tps()[id.name],4, "iter", context.currentBlock());
	context.locals()["iter"] = alloc_iter;

	AllocaInst* arrayVar = reinterpret_cast<AllocaInst*>(context.locals()[id.name]);
	ArrayType* arrayType = context.atps()[id.name];
  	Type* elementType = context.tps()[id.name];
	Constant* zero = ConstantInt::get(Type::getInt32Ty(MyContext), 0);
	std::vector<Value*> gepIndices;
	gepIndices.push_back(zero);
	gepIndices.push_back(alloc_index);
	Value* elementPtr = GetElementPtrInst::CreateInBounds(arrayType, arrayVar, gepIndices, "", context.currentBlock());
	Value* ldit = new LoadInst(elementType, elementPtr, "", false, context.currentBlock());
	Value* stit = new StoreInst(ldit, alloc_iter, false, context.currentBlock());
	// NIdentifier tpit = NIdentifier("iter");

	// Value* stit = new StoreInst(new NArrayRef(tpit, NInteger()), alloc_iter, false, context.currentBlock());

	// for_block.codeGen(context);
	
	// NBinaryOperator* additer=new NBinaryOperator(iter_index, TPLUS,  *iter_add_value );
	// NAssignment *add_one_to_iter = new NAssignment(iter_index, *additer);
	// add_one_to_iter->codeGen(context);
	// BranchInst::Create	(	judgeBB, context.currentBlock() );

	// context.popBlock();
	// templocals_2=context.locals();temptps_2 = context.tps();tempatps_2 = context.atps();
	// context.pushBlock(endBB);
	// context.setTbs(templocals_2, temptps_2, tempatps_2);
	return cmp;
}
