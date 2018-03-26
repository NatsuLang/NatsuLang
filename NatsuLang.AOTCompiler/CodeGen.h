﻿#pragma once
#include <natLog.h>

#include <AST/Expression.h>
#include <AST/ASTConsumer.h>
#include <AST/StmtVisitor.h>
#include <Basic/FileManager.h>
#include <Basic/SourceManager.h>
#include <Parse/Parser.h>
#include <Sema/Sema.h>
#include <Sema/Scope.h>

#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>

namespace llvm
{
	class raw_pwrite_stream;
}

using namespace NatsuLib::StringLiterals;

namespace NatsuLang::Compiler
{
	DeclareException(AotCompilerException, NatsuLib::natException, u8"Exception generated by AotCompiler"_nv);

	class AotCompiler final
	{
		class AotDiagIdMap final
			: public NatsuLib::natRefObjImpl<AotDiagIdMap, Misc::TextProvider<Diag::DiagnosticsEngine::DiagID>>
		{
		public:
			explicit AotDiagIdMap(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& reader);
			~AotDiagIdMap();

			nString GetText(Diag::DiagnosticsEngine::DiagID id) override;

		private:
			std::unordered_map<Diag::DiagnosticsEngine::DiagID, nString> m_IdMap;
		};

		class AotDiagConsumer final
			: public NatsuLib::natRefObjImpl<AotDiagConsumer, Diag::DiagnosticConsumer>
		{
		public:
			explicit AotDiagConsumer(AotCompiler& compiler);
			~AotDiagConsumer();

			void HandleDiagnostic(Diag::DiagnosticsEngine::Level level, Diag::DiagnosticsEngine::Diagnostic const& diag) override;

			nBool IsErrored() const noexcept
			{
				return m_Errored;
			}

			void Reset() noexcept
			{
				m_Errored = false;
			}

		private:
			AotCompiler& m_Compiler;
			nBool m_Errored;
		};

		class AotAstConsumer final
			: public NatsuLib::natRefObjImpl<AotAstConsumer, ASTConsumer>
		{
		public:
			explicit AotAstConsumer(AotCompiler& compiler);
			~AotAstConsumer();

			void Initialize(ASTContext& context) override;
			void HandleTranslationUnit(ASTContext& context) override;
			nBool HandleTopLevelDecl(NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> const& decls) override;

		private:
			AotCompiler& m_Compiler;
		};

		// TODO: 无法处理顶层声明中的初始化器等
		class AotStmtVisitor final
			: public NatsuLib::natRefObjImpl<AotStmtVisitor, StmtVisitor>
		{
			struct ICleanup
				: NatsuLib::natRefObj
			{
				virtual ~ICleanup();

				virtual void Emit(AotStmtVisitor& visitor) = 0;
			};

			class DestructorCleanup
				: public NatsuLib::natRefObjImpl<DestructorCleanup, ICleanup>
			{
			public:
				DestructorCleanup(NatsuLib::natRefPointer<Declaration::DestructorDecl> destructor, llvm::Value* addr);
				~DestructorCleanup();

				void Emit(AotStmtVisitor& visitor) override;

			private:
				NatsuLib::natRefPointer<Declaration::DestructorDecl> m_Destructor;
				llvm::Value* m_Addr;
			};

			class ArrayCleanup
				: public NatsuLib::natRefObjImpl<ArrayCleanup, ICleanup>
			{
			public:
				using CleanupFunction = std::function<void(AotStmtVisitor&, llvm::Value*)>;

				ArrayCleanup(NatsuLib::natRefPointer<Type::ArrayType> type, llvm::Value* addr, CleanupFunction cleanupFunction);
				~ArrayCleanup();

				void Emit(AotStmtVisitor& visitor) override;

			private:
				NatsuLib::natRefPointer<Type::ArrayType> m_Type;
				llvm::Value* m_Addr;
				CleanupFunction m_CleanupFunction;
			};

			class SpecialCleanup
				: public NatsuLib::natRefObjImpl<SpecialCleanup, ICleanup>
			{
			public:
				using SpecialCleanupFunction = std::function<void(AotStmtVisitor&)>;

				explicit SpecialCleanup(SpecialCleanupFunction cleanupFunction);
				~SpecialCleanup();

				void Emit(AotStmtVisitor& visitor) override;

			private:
				SpecialCleanupFunction m_CleanupFunction;
			};

			using CleanupIterator = std::list<std::pair<std::size_t, NatsuLib::natRefPointer<ICleanup>>>::const_iterator;

			class LexicalScope
			{
			public:
				LexicalScope(AotStmtVisitor& visitor, SourceRange range);
				~LexicalScope();

				SourceRange GetRange() const noexcept;

				void AddLabel(NatsuLib::natRefPointer<Declaration::LabelDecl> label);

				void SetBeginIterator(CleanupIterator const& iter) noexcept;

				void ExplicitClean();
				void SetAlreadyCleaned() noexcept;

			private:
				nBool m_AlreadyCleaned;
				CleanupIterator m_BeginIterator;
				AotStmtVisitor& m_Visitor;
				SourceRange m_Range;
				LexicalScope* m_Parent;
				std::vector<NatsuLib::natRefPointer<Declaration::LabelDecl>> m_Labels;
			};

			// 用于存储清理信息
			class JumpDest
			{
			public:
				JumpDest(llvm::BasicBlock* block, CleanupIterator cleanupIterator)
					: m_Block{ block }, m_CleanupIterator{ std::move(cleanupIterator) }
				{
				}

				llvm::BasicBlock* GetBlock() const noexcept
				{
					return m_Block;
				}

				CleanupIterator GetCleanupIterator() const noexcept
				{
					return m_CleanupIterator;
				}

			private:
				llvm::BasicBlock* m_Block;
				CleanupIterator m_CleanupIterator;
			};

		public:
			AotStmtVisitor(AotCompiler& compiler, NatsuLib::natRefPointer<Declaration::FunctionDecl> funcDecl, llvm::Function* funcValue);
			~AotStmtVisitor();

			void VisitInitListExpr(NatsuLib::natRefPointer<Expression::InitListExpr> const& expr) override;
			void VisitBreakStmt(NatsuLib::natRefPointer<Statement::BreakStmt> const& stmt) override;
			void VisitCatchStmt(NatsuLib::natRefPointer<Statement::CatchStmt> const& stmt) override;
			void VisitTryStmt(NatsuLib::natRefPointer<Statement::TryStmt> const& stmt) override;
			void VisitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& stmt) override;
			void VisitContinueStmt(NatsuLib::natRefPointer<Statement::ContinueStmt> const& stmt) override;
			void VisitDeclStmt(NatsuLib::natRefPointer<Statement::DeclStmt> const& stmt) override;
			void VisitDoStmt(NatsuLib::natRefPointer<Statement::DoStmt> const& stmt) override;
			void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr) override;
			void VisitConditionalOperator(NatsuLib::natRefPointer<Expression::ConditionalOperator> const& expr) override;
			void VisitArraySubscriptExpr(NatsuLib::natRefPointer<Expression::ArraySubscriptExpr> const& expr) override;
			void VisitBinaryOperator(NatsuLib::natRefPointer<Expression::BinaryOperator> const& expr) override;
			void VisitCompoundAssignOperator(NatsuLib::natRefPointer<Expression::CompoundAssignOperator> const& expr) override;
			void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr) override;
			void VisitConstructExpr(NatsuLib::natRefPointer<Expression::ConstructExpr> const& expr) override;
			void VisitDeleteExpr(NatsuLib::natRefPointer<Expression::DeleteExpr> const& expr) override;
			void VisitNewExpr(NatsuLib::natRefPointer<Expression::NewExpr> const& expr) override;
			void VisitThisExpr(NatsuLib::natRefPointer<Expression::ThisExpr> const& expr) override;
			void VisitThrowExpr(NatsuLib::natRefPointer<Expression::ThrowExpr> const& expr) override;
			void VisitCallExpr(NatsuLib::natRefPointer<Expression::CallExpr> const& expr) override;
			void VisitMemberCallExpr(NatsuLib::natRefPointer<Expression::MemberCallExpr> const& expr) override;
			void VisitCastExpr(NatsuLib::natRefPointer<Expression::CastExpr> const& expr) override;
			void VisitAsTypeExpr(NatsuLib::natRefPointer<Expression::AsTypeExpr> const& expr) override;
			void VisitImplicitCastExpr(NatsuLib::natRefPointer<Expression::ImplicitCastExpr> const& expr) override;
			void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr) override;
			void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr) override;
			void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr) override;
			void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr) override;
			void VisitMemberExpr(NatsuLib::natRefPointer<Expression::MemberExpr> const& expr) override;
			void VisitParenExpr(NatsuLib::natRefPointer<Expression::ParenExpr> const& expr) override;
			void VisitStmtExpr(NatsuLib::natRefPointer<Expression::StmtExpr> const& expr) override;
			void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr) override;
			void VisitUnaryExprOrTypeTraitExpr(NatsuLib::natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr) override;
			void VisitUnaryOperator(NatsuLib::natRefPointer<Expression::UnaryOperator> const& expr) override;
			void VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt) override;
			void VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt) override;
			void VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt) override;
			void VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt) override;
			void VisitNullStmt(NatsuLib::natRefPointer<Statement::NullStmt> const& stmt) override;
			void VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt) override;
			void VisitSwitchCase(NatsuLib::natRefPointer<Statement::SwitchCase> const& stmt) override;
			void VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt) override;
			void VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt) override;
			void VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt) override;
			void VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt) override;
			void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt) override;

			AotCompiler& GetCompiler() const noexcept
			{
				return m_Compiler;
			}

			void StartVisit();
			void EndVisit();
			llvm::Function* GetFunction() const;

			// TODO: 考虑将 Emit 函数移出 AotStmtVisitor
			void EmitFunctionEpilog();

			void EmitAddressOfVar(NatsuLib::natRefPointer<Declaration::VarDecl> const& varDecl);

			void EmitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& compoundStmt);
			void EmitCompoundStmtWithoutScope(NatsuLib::natRefPointer<Statement::CompoundStmt> const& compoundStmt);

			void EmitBranch(llvm::BasicBlock* target);
			void EmitBranchWithCleanup(JumpDest const& target);
			void EmitBlock(llvm::BasicBlock* block, nBool finished = false);

			llvm::Value* EmitBinOp(llvm::Value* leftOperand, llvm::Value* rightOperand,
				Expression::BinaryOperationType opCode,
				NatsuLib::natRefPointer<Type::BuiltinType> const& commonType,
				NatsuLib::natRefPointer<Type::BuiltinType> const& resultType);

			llvm::Value* EmitIncDec(llvm::Value* operand, NatsuLib::natRefPointer<Type::BuiltinType> const& opType, nBool isInc, nBool isPre);

			llvm::Value* EmitFunctionAddr(NatsuLib::natRefPointer<Declaration::FunctionDecl> const& func);

			void EmitVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);

			void EmitAutoVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);
			llvm::Value* EmitAutoVarAlloc(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);
			void EmitAutoVarInit(Type::TypePtr const& varType, llvm::Value* varPtr, Expression::ExprPtr const& initializer);
			void EmitAutoVarCleanup(Type::TypePtr const& varType, llvm::Value* varPtr);

			void EmitExternVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);

			void EmitStaticVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);

			void EmitDestructorCall(NatsuLib::natRefPointer<Declaration::DestructorDecl> const& destructor, llvm::Value* addr);

			void EvaluateValue(Expression::ExprPtr const& expr);
			void EvaluateAsModifiableValue(Expression::ExprPtr const& expr);
			void EvaluateAsBool(Expression::ExprPtr const& expr);

			llvm::Value* ConvertScalarTo(llvm::Value* from, Type::TypePtr fromType, Type::TypePtr toType);
			llvm::Value* ConvertScalarToBool(llvm::Value* from, NatsuLib::natRefPointer<Type::BuiltinType> const& fromType);

			CleanupIterator GetCleanupStackTop() const noexcept;
			nBool IsCleanupStackEmpty() const noexcept;
			void PushCleanupStack(NatsuLib::natRefPointer<ICleanup> cleanup);
			void InsertCleanupStack(CleanupIterator const& pos, NatsuLib::natRefPointer<ICleanup> cleanup);
			void PopCleanupStack(CleanupIterator const& iter, nBool popStack = true);
			bool CleanupEncloses(CleanupIterator const& a, CleanupIterator const& b) const noexcept;

		private:
			AotCompiler& m_Compiler;
			NatsuLib::natRefPointer<Declaration::FunctionDecl> m_CurrentFunction;
			llvm::Function* m_CurrentFunctionValue;
			llvm::Value* m_This;
			std::unordered_map<NatsuLib::natRefPointer<Declaration::ValueDecl>, llvm::Value*> m_DeclMap;
			llvm::Value* m_LastVisitedValue;
			nBool m_RequiredModifiableValue;
			std::vector<std::pair<JumpDest, JumpDest>> m_BreakContinueStack;
			std::list<std::pair<std::size_t, NatsuLib::natRefPointer<ICleanup>>> m_CleanupStack;
			LexicalScope* m_CurrentLexicalScope;
			JumpDest m_ReturnBlock;
			llvm::Value* m_ReturnValue;
		};

	public:
		AotCompiler(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& diagIdMapFile, NatsuLib::natLog& logger);
		~AotCompiler();

		void Compile(NatsuLib::Uri const& uri, llvm::raw_pwrite_stream& stream);
		void Compile(nStrView const& content, nStrView const& name, llvm::raw_pwrite_stream& stream);

	private:
		NatsuLib::natRefPointer<AotDiagConsumer> m_DiagConsumer;
		Diag::DiagnosticsEngine m_Diag;
		NatsuLib::natLog& m_Logger;
		FileManager m_FileManager;
		SourceManager m_SourceManager;
		Preprocessor m_Preprocessor;
		ASTContext m_AstContext;
		NatsuLib::natRefPointer<AotAstConsumer> m_Consumer;
		Semantic::Sema m_Sema;
		Syntax::Parser m_Parser;

		llvm::LLVMContext m_LLVMContext;
		std::unique_ptr<llvm::Module> m_Module;
		llvm::IRBuilder<> m_IRBuilder;

		std::unordered_map<Type::TypePtr, llvm::Type*> m_TypeMap;
		std::unordered_map<Declaration::DeclPtr, llvm::Type*> m_DeclTypeMap;
		std::unordered_map<NatsuLib::natRefPointer<Declaration::FunctionDecl>, llvm::Function*> m_FunctionMap;
		std::unordered_map<NatsuLib::natRefPointer<Declaration::VarDecl>, llvm::GlobalVariable*> m_GlobalVariableMap;

		std::unordered_map<nString, llvm::GlobalVariable*> m_StringLiteralPool;

		llvm::GlobalVariable* getStringLiteralValue(nStrView literalContent, nStrView literalName = "String");

		llvm::Type* getCorrespondingType(Type::TypePtr const& type);
		llvm::Type* getCorrespondingType(Declaration::DeclPtr const& decl);

		llvm::Type* buildFunctionType(Type::TypePtr const& resultType, NatsuLib::Linq<NatsuLib::Valued<Type::TypePtr>> const& params);
		llvm::Type* buildFunctionType(NatsuLib::natRefPointer<Declaration::FunctionDecl> const& funcDecl);
		llvm::Type* buildFunctionType(NatsuLib::natRefPointer<Declaration::MethodDecl> const& methodDecl);

		llvm::Type* buildClassType(NatsuLib::natRefPointer<Declaration::ClassDecl> const& classDecl);

		NatsuLib::natRefPointer<Type::ArrayType> flattenArray(NatsuLib::natRefPointer<Type::ArrayType> arrayType);
		static NatsuLib::natRefPointer<Declaration::DestructorDecl> findDestructor(NatsuLib::natRefPointer<Declaration::ClassDecl> const& classDecl);
	};
}
