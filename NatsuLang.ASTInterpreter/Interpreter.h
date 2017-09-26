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

		// 返回值将会被丢弃
		template <typename Callable, typename Arg, typename... ExpectedTypes>
		constexpr nBool InvokeIfSatisfied(Callable&& callableObj, Arg&& arg, Expected_t<ExpectedTypes...>)
		{
			if constexpr (!sizeof...(ExpectedTypes) ||
				std::disjunction_v<
					std::is_same<
						std::remove_cv_t<std::remove_reference_t<Arg>>,
						std::remove_cv_t<std::remove_reference_t<ExpectedTypes>>
					>...
				>)
			{
				if constexpr (std::is_invocable_v<decltype(callableObj), decltype(arg)>)
				{
					std::invoke(std::forward<Callable>(callableObj), std::forward<Arg>(arg));
					return true;
				}
			}

			return false;
		}

		template <typename Callable, typename Arg, typename... ExceptedTypes>
		constexpr nBool InvokeIfSatisfied(Callable&& callableObj, Arg&& arg, Excepted_t<ExceptedTypes...>)
		{
			if constexpr (!sizeof...(ExceptedTypes) ||
				std::conjunction_v<
					std::negation<
						std::is_same<
							std::remove_cv_t<std::remove_reference_t<Arg>>,
							std::remove_cv_t<std::remove_reference_t<ExceptedTypes>>
						>
					>...
				>)
			{
				if constexpr (std::is_invocable_v<decltype(callableObj), decltype(arg)>)
				{
					std::invoke(std::forward<Callable>(callableObj), std::forward<Arg>(arg));
					return true;
				}
			}

			return false;
		}
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
			template <typename ValueVisitor, typename ExpectedOrExcepted>
			class InterpreterExprEvaluator
				: public natRefObjImpl<InterpreterExprEvaluator<ValueVisitor, ExpectedOrExcepted>, StmtVisitor>
			{
			public:
				explicit InterpreterExprEvaluator(Interpreter& interpreter, ValueVisitor const& visitor)
					: m_Interpreter{ interpreter }, m_Visitor { visitor }, m_LastEvaluationSucceed{ false }
				{
				}

				explicit InterpreterExprEvaluator(Interpreter& interpreter, ValueVisitor&& visitor)
					: m_Interpreter{ interpreter }, m_Visitor{ std::move(visitor) }, m_LastEvaluationSucceed{ false }
				{
				}

				~InterpreterExprEvaluator()
				{
				}

				nBool IsLastEvaluationSucceed() const noexcept
				{
					return m_LastEvaluationSucceed;
				}

				void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& /*stmt*/) override
				{
					nat_Throw(InterpreterException, u8"语句无法求值");
				}

				void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& /*expr*/) override
				{
					nat_Throw(InterpreterException, u8"此表达式无法被求值");
				}

				void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr) override
				{
					m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
				}

				void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr) override
				{
					m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nByte>(expr->GetCodePoint()), ExpectedOrExcepted{});
				}

				void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr) override
				{
					m_LastEvaluationSucceed = m_Interpreter.m_DeclStorage.VisitDeclStorage(expr->GetDecl(), m_Visitor, ExpectedOrExcepted{});
				}

				void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr) override
				{
					const auto exprType = expr->GetExprType();

					if (const auto builtinType = static_cast<NatsuLib::natRefPointer<Type::BuiltinType>>(exprType))
					{
						switch (builtinType->GetBuiltinClass())
						{
						case Type::BuiltinType::Float:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nFloat>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::Double:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::LongDouble:
						case Type::BuiltinType::Float128:
							break;
						default:
							nat_Throw(InterpreterException, u8"浮点字面量不应具有此类型");
						}
					}

					nat_Throw(InterpreterException, u8"此功能尚未实现");
				}

				void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr) override
				{
					const auto exprType = expr->GetExprType();

					if (const auto builtinType = static_cast<NatsuLib::natRefPointer<Type::BuiltinType>>(exprType))
					{
						switch (builtinType->GetBuiltinClass())
						{
						case Type::BuiltinType::UShort:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nuShort>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::UInt:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nuInt>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::ULong:
						case Type::BuiltinType::ULongLong:
						case Type::BuiltinType::UInt128:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::Short:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nShort>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::Int:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nInt>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						case Type::BuiltinType::Long:
						case Type::BuiltinType::LongLong:
						case Type::BuiltinType::Int128:
							m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, static_cast<nLong>(expr->GetValue()), ExpectedOrExcepted{});
							return;
						default:
							nat_Throw(InterpreterException, u8"整数字面量不应具有此类型");
						}
					}

					nat_Throw(InterpreterException, u8"此功能尚未实现");
				}

				void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr) override
				{
					m_LastEvaluationSucceed = Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
				}

			private:
				Interpreter& m_Interpreter;
				ValueVisitor m_Visitor;
				nBool m_LastEvaluationSucceed;
			};

		public:
			explicit InterpreterExprVisitor(Interpreter& interpreter);
			~InterpreterExprVisitor();

			void Clear() noexcept;
			void PrintExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);
			Expression::ExprPtr GetLastVisitedExpr() const noexcept;

			template <typename ValueVisitor, typename ExpectedOrExcepted = Detail::Expected_t<>>
			nBool Evaluate(NatsuLib::natRefPointer<Expression::Expr> const& expr, ValueVisitor&& visitor, ExpectedOrExcepted condition = {})
			{
				if (!expr)
				{
					return false;
				}

				Visit(expr);
				InterpreterExprEvaluator<ValueVisitor, ExpectedOrExcepted> evaluator{ m_Interpreter, std::forward<ValueVisitor>(visitor) };
				evaluator.Visit(m_LastVisitedExpr);
				return evaluator.IsLastEvaluationSucceed();
			}

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
			template <typename Callable, typename ExpectedOrExcepted>
			nBool VisitStorage(Type::TypePtr const& type, nData storage, Callable&& visitor, ExpectedOrExcepted condition)
			{
				auto& storageRef = *storage;

				switch (type->GetType())
				{
				case Type::Type::Builtin:
				{
					const auto builtinType = static_cast<NatsuLib::natRefPointer<Type::BuiltinType>>(type);
					assert(builtinType);

					switch (builtinType->GetBuiltinClass())
					{
					case Type::BuiltinType::Bool:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nBool&>(storageRef), condition);
					case Type::BuiltinType::Char:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nByte&>(storageRef), condition);
					case Type::BuiltinType::UShort:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nuShort&>(storageRef), condition);
					case Type::BuiltinType::UInt:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nuInt&>(storageRef), condition);
						// TODO: 区分Long类型
					case Type::BuiltinType::ULong:
					case Type::BuiltinType::ULongLong:
					case Type::BuiltinType::UInt128:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nuLong&>(storageRef), condition);
					case Type::BuiltinType::Short:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nShort&>(storageRef), condition);
					case Type::BuiltinType::Int:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nInt&>(storageRef), condition);
					case Type::BuiltinType::Long:
					case Type::BuiltinType::LongLong:
					case Type::BuiltinType::Int128:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nLong&>(storageRef), condition);
					case Type::BuiltinType::Float:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nFloat&>(storageRef), condition);
					case Type::BuiltinType::Double:
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), reinterpret_cast<nDouble&>(storageRef), condition);
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
			}

			struct StorageDeleter
			{
				constexpr StorageDeleter() noexcept = default;
				void operator()(nData data) const noexcept;
			};

		public:
			explicit InterpreterDeclStorage(Interpreter& interpreter);

			// 返回值：是否新增了声明，声明的存储
			std::pair<nBool, nData> GetOrAddDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> decl);
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
				if (!decl)
				{
					return false;
				}

				const auto type = Type::Type::GetUnderlyingType(decl->GetValueType());

				const auto [addedDecl, storagePointer] = GetOrAddDecl(decl);
				auto visitSucceed = false;
				const auto scope = NatsuLib::make_scope([this, addedDecl, &visitSucceed, decl = std::move(decl)]
				{
					if (addedDecl && !visitSucceed)
					{
						RemoveDecl(decl);
					}
				});

				visitSucceed = VisitStorage(type, storagePointer, std::forward<Callable>(visitor), condition);
				return visitSucceed;
			}

		private:
			Interpreter& m_Interpreter;
			std::unordered_map<NatsuLib::natRefPointer<Declaration::ValueDecl>, std::unique_ptr<nByte[], StorageDeleter>> m_DeclStorage;
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
