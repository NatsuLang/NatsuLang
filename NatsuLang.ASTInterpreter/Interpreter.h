#pragma once
#include <AST/StmtVisitor.h>
#include <AST/ASTConsumer.h>
#include <AST/Expression.h>
#include <Basic/FileManager.h>
#include <Parse/Parser.h>
#include <Sema/Sema.h>
#include <Sema/Scope.h>

#include <natStream.h>
#include <natText.h>
#include <natLog.h>

namespace NatsuLang
{
	DeclareException(InterpreterException, NatsuLib::natException, u8"由解释器生成的异常");

	namespace Detail
	{
		template <typename... ExpectedTypes>
		struct Expected_t
		{
			constexpr Expected_t() = default;
		};

		template <typename... ExpectedTypes>
		constexpr Expected_t<ExpectedTypes...> Expected{};

		template <typename... ExceptedTypes>
		struct Excepted_t
		{
			constexpr Excepted_t() = default;
		};

		template <typename... ExceptedTypes>
		constexpr Excepted_t<ExceptedTypes...> Excepted{};
	}

	class Interpreter final
	{
		class InterpreterDiagIdMap
			: public NatsuLib::natRefObjImpl<InterpreterDiagIdMap, Misc::TextProvider<Diag::DiagnosticsEngine::DiagID>>
		{
		public:
			explicit InterpreterDiagIdMap(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& reader);
			~InterpreterDiagIdMap();

			nString GetText(Diag::DiagnosticsEngine::DiagID id) override;

		private:
			std::unordered_map<Diag::DiagnosticsEngine::DiagID, nString> m_IdMap;
		};

		class InterpreterDiagConsumer
			: public NatsuLib::natRefObjImpl<InterpreterDiagConsumer, Diag::DiagnosticConsumer>
		{
		public:
			explicit InterpreterDiagConsumer(Interpreter& interpreter);
			~InterpreterDiagConsumer();

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
			Interpreter& m_Interpreter;
			nBool m_Errored;
		};

		class InterpreterASTConsumer
			: public NatsuLib::natRefObjImpl<InterpreterASTConsumer, ASTConsumer>
		{
		public:
			explicit InterpreterASTConsumer(Interpreter& interpreter);
			~InterpreterASTConsumer();

			void Initialize(ASTContext& context) override;
			void HandleTranslationUnit(ASTContext& context) override;
			nBool HandleTopLevelDecl(NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> const& decls) override;

		private:
			Interpreter& m_Interpreter;
			std::vector<Declaration::DeclPtr> m_UnnamedDecls;
			std::unordered_map<nStrView, NatsuLib::natRefPointer<Declaration::NamedDecl>> m_NamedDecls;
		};

		class InterpreterExprVisitor
			: public NatsuLib::natRefObjImpl<InterpreterExprVisitor, StmtVisitor>
		{
		public:
			explicit InterpreterExprVisitor(Interpreter& interpreter);
			~InterpreterExprVisitor();

			void Clear() noexcept;
			void PrintExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);
			Expression::ExprPtr GetLastVisitedExpr() const noexcept;

			void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt) override;
			void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr) override;

			void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr) override;
			void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr) override;
			void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr) override;
			void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr) override;
			void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr) override;
			void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr) override;

			void VisitArraySubscriptExpr(NatsuLib::natRefPointer<Expression::ArraySubscriptExpr> const& expr) override;
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
			void VisitMemberExpr(NatsuLib::natRefPointer<Expression::MemberExpr> const& expr) override;
			void VisitParenExpr(NatsuLib::natRefPointer<Expression::ParenExpr> const& expr) override;
			void VisitStmtExpr(NatsuLib::natRefPointer<Expression::StmtExpr> const& expr) override;
			void VisitUnaryExprOrTypeTraitExpr(NatsuLib::natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr) override;
			void VisitConditionalOperator(NatsuLib::natRefPointer<Expression::ConditionalOperator> const& expr) override;
			void VisitBinaryOperator(NatsuLib::natRefPointer<Expression::BinaryOperator> const& expr) override;
			void VisitCompoundAssignOperator(NatsuLib::natRefPointer<Expression::CompoundAssignOperator> const& expr) override;
			void VisitUnaryOperator(NatsuLib::natRefPointer<Expression::UnaryOperator> const& expr) override;

		private:
			Interpreter& m_Interpreter;
			Expression::ExprPtr m_LastVisitedExpr;
			nBool m_ShouldPrint;
		};

		class InterpreterStmtVisitor
			: public NatsuLib::natRefObjImpl<InterpreterStmtVisitor, StmtVisitor>
		{
		public:
			explicit InterpreterStmtVisitor(Interpreter& interpreter);
			~InterpreterStmtVisitor();

			void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt) override;
			void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr) override;

			void VisitBreakStmt(NatsuLib::natRefPointer<Statement::BreakStmt> const& stmt) override;
			void VisitCatchStmt(NatsuLib::natRefPointer<Statement::CatchStmt> const& stmt) override;
			void VisitTryStmt(NatsuLib::natRefPointer<Statement::TryStmt> const& stmt) override;
			void VisitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& stmt) override;
			void VisitContinueStmt(NatsuLib::natRefPointer<Statement::ContinueStmt> const& stmt) override;
			void VisitDeclStmt(NatsuLib::natRefPointer<Statement::DeclStmt> const& stmt) override;
			void VisitDoStmt(NatsuLib::natRefPointer<Statement::DoStmt> const& stmt) override;
			void VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt) override;
			void VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt) override;
			void VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt) override;
			void VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt) override;
			void VisitNullStmt(NatsuLib::natRefPointer<Statement::NullStmt> const& stmt) override;
			void VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt) override;
			void VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt) override;
			void VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt) override;
			void VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt) override;
			void VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt) override;

		private:
			Interpreter& m_Interpreter;
		};

		class InterpreterDeclStorage
		{
			template <typename Callable, typename RealType, typename... ExpectedTypes>
			nBool VisitInvokeHelper(Callable&& visitor, RealType& realValue, Detail::Expected_t<ExpectedTypes...>)
			{
				if constexpr (!sizeof...(ExpectedTypes) || std::disjunction_v<std::is_same<RealType, ExpectedTypes>...>)
				{
					if constexpr (std::is_invocable_v<decltype(visitor), RealType&>)
					{
						std::invoke(std::forward<Callable>(visitor), realValue);
						return true;
					}
				}

				return false;
			}

			template <typename Callable, typename RealType, typename... ExceptedTypes>
			nBool VisitInvokeHelper(Callable&& visitor, RealType& realValue, Detail::Excepted_t<ExceptedTypes...>)
			{
				if constexpr (!sizeof...(ExceptedTypes) || std::conjunction_v<std::negation<std::is_same<RealType, ExceptedTypes>>...>)
				{
					if constexpr (std::is_invocable_v<decltype(visitor), RealType&>)
					{
						std::invoke(std::forward<Callable>(visitor), realValue);
						return true;
					}
				}

				return false;
			}

		public:
			explicit InterpreterDeclStorage(Interpreter& interpreter);

			// 返回值：是否新增了声明，声明的存储
			// TODO: 不暴露内部实现，返回值不应当使用 std::vector<nByte>&
			std::pair<nBool, std::vector<nByte>&> GetOrAddDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> decl);
			void RemoveDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl);
			nBool DoesDeclExist(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl) const noexcept;

			void GarbageCollect();

			static NatsuLib::natRefPointer<Declaration::ValueDecl> CreateTemporaryObjectDecl(Type::TypePtr type, SourceLocation loc = {});

			// TODO: 设计 MemberAccessor
			struct MemberAccessor
			{

			};

			// TODO: 添加函数

			template <typename Callable, typename ExpectedOrExcepted = Detail::Expected_t<>>
			nBool VisitDeclStorage(NatsuLib::natRefPointer<Declaration::ValueDecl> decl, Callable&& visitor, ExpectedOrExcepted condition = {})
			{
				const auto type = Type::Type::GetUnderlyingType(decl->GetValueType());

				auto [addedDecl, storageVec] = GetOrAddDecl(decl);
				auto& storage = *storageVec.data();
				auto visitSucceed = false;
				const auto scope = NatsuLib::make_scope([this, addedDecl, &visitSucceed, decl = std::move(decl)]
				{
					if (addedDecl && !visitSucceed)
					{
						RemoveDecl(decl);
					}
				});

				switch (type->GetType())
				{
				case Type::Type::Builtin:
				{
					const auto builtinType = static_cast<NatsuLib::natRefPointer<Type::BuiltinType>>(type);
					assert(builtinType);

					switch (builtinType->GetBuiltinClass())
					{
					case Type::BuiltinType::Bool:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nBool&>(storage), condition);
						break;
					case Type::BuiltinType::Char:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nByte&>(storage), condition);
						break;
					case Type::BuiltinType::UShort:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nuShort&>(storage), condition);
						break;
					case Type::BuiltinType::UInt:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nuInt&>(storage), condition);
						break;
					// TODO: 区分Long类型
					case Type::BuiltinType::ULong:
					case Type::BuiltinType::ULongLong:
					case Type::BuiltinType::UInt128:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nuLong&>(storage), condition);
						break;
					case Type::BuiltinType::Short:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nShort&>(storage), condition);
						break;
					case Type::BuiltinType::Int:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nInt&>(storage), condition);
						break;
					case Type::BuiltinType::Long:
					case Type::BuiltinType::LongLong:
					case Type::BuiltinType::Int128:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nLong&>(storage), condition);
						break;
					case Type::BuiltinType::Float:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nFloat&>(storage), condition);
						break;
					case Type::BuiltinType::Double:
						visitSucceed = VisitInvokeHelper(std::forward<Callable>(visitor), reinterpret_cast<nDouble&>(storage), condition);
						break;
					case Type::BuiltinType::LongDouble:
					case Type::BuiltinType::Float128:
						nat_Throw(InterpreterException, u8"此功能尚未实现");
					default:
						assert(!"Invalid type.");
						[[fallthrough]];
					case Type::BuiltinType::Invalid:
					case Type::BuiltinType::Void:
					case Type::BuiltinType::BoundMember:
					case Type::BuiltinType::BuiltinFn:
					case Type::BuiltinType::Overload:
						nat_Throw(InterpreterException, u8"此功能尚未实现");
					}

					break;
				}
				case Type::Type::Array:
					nat_Throw(InterpreterException, u8"此功能尚未实现");
				case Type::Type::Function:
					nat_Throw(InterpreterException, u8"此功能尚未实现");
				case Type::Type::Record:
					nat_Throw(InterpreterException, u8"此功能尚未实现");
				case Type::Type::Enum:
					nat_Throw(InterpreterException, u8"此功能尚未实现");
				default:
					assert(!"Invalid type.");
					[[fallthrough]];
				case Type::Type::Paren:
				case Type::Type::TypeOf:
				case Type::Type::Auto:
					return false;
				}

				return visitSucceed;
			}

		private:
			Interpreter& m_Interpreter;
			std::unordered_map<NatsuLib::natRefPointer<Declaration::ValueDecl>, std::vector<nByte>> m_DeclStorage;
		};

	public:
		Interpreter(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& diagIdMapFile, NatsuLib::natLog& logger);
		~Interpreter();

		void Run(NatsuLib::Uri const& uri);
		void Run(nStrView content);

		NatsuLib::natRefPointer<Semantic::Scope> GetScope() const noexcept;

	private:
		NatsuLib::natRefPointer<InterpreterDiagConsumer> m_DiagConsumer;
		Diag::DiagnosticsEngine m_Diag;
		NatsuLib::natLog& m_Logger;
		FileManager m_FileManager;
		SourceManager m_SourceManager;
		Preprocessor m_Preprocessor;
		ASTContext m_AstContext;
		NatsuLib::natRefPointer<InterpreterASTConsumer> m_Consumer;
		Semantic::Sema m_Sema;
		Syntax::Parser m_Parser;
		NatsuLib::natRefPointer<InterpreterStmtVisitor> m_Visitor;

		NatsuLib::natRefPointer<Semantic::Scope> m_CurrentScope;
		InterpreterDeclStorage m_DeclStorage;
	};
}
