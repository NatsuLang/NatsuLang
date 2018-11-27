#pragma once

using namespace NatsuLib::StringLiterals;

namespace NatsuLang
{
	DeclareException(InterpreterException, NatsuLib::natException, u8"由解释器生成的异常"_nv);

	namespace Detail
	{
		template <typename... ExpectedTypes>
		struct ExpectedTag
		{
			constexpr ExpectedTag() = default;
		};

		template <typename... ExpectedTypes>
		constexpr ExpectedTag<ExpectedTypes...> Expected{};

		template <typename... ExceptedTypes>
		struct ExceptedTag
		{
			constexpr ExceptedTag() = default;
		};

		template <typename... ExceptedTypes>
		constexpr ExceptedTag<ExceptedTypes...> Excepted{};

		// 返回值将会被丢弃
		template <typename Callable, typename Arg, typename... ExpectedTypes>
		constexpr nBool InvokeIfSatisfied(Callable&& callableObj, Arg&& arg, ExpectedTag<ExpectedTypes...>)
		{
			if constexpr (!sizeof...(ExpectedTypes) ||
				std::disjunction_v<
					std::is_same<
						std::remove_cv_t<std::remove_reference_t<Arg>>,
						std::remove_cv_t<std::remove_reference_t<ExpectedTypes>>
					>...
				>)
			{
				if constexpr (std::is_invocable_v<Callable&&, Arg&&>)
				{
					static_cast<void>(std::invoke(std::forward<Callable>(callableObj), std::forward<Arg>(arg)));
					return true;
				}
			}

			return false;
		}

		template <typename Callable, typename Arg, typename... ExceptedTypes>
		constexpr nBool InvokeIfSatisfied(Callable&& callableObj, Arg&& arg, ExceptedTag<ExceptedTypes...>)
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
				if constexpr (std::is_invocable_v<Callable&&, Arg&&>)
				{
					static_cast<void>(std::invoke(std::forward<Callable>(callableObj), std::forward<Arg>(arg)));
					return true;
				}
			}

			return false;
		}

#define BUILTIN_TYPE_MAP(OP) \
	OP(Bool, nBool)\
	OP(Char, nByte)\
	OP(Byte, nByte)\
	OP(UShort, nuShort)\
	OP(UInt, nuInt)\
	OP(ULong, nuLong)\
	OP(ULongLong, nuLong)\
	OP(ULong128, nuLong)\
	OP(SByte, nSByte)\
	OP(Short, nShort)\
	OP(Int, nInt)\
	OP(Long, nLong)\
	OP(LongLong, nLong)\
	OP(Long128, nLong)\
	OP(Float, nFloat)\
	OP(Double, nDouble)\
	OP(LongDouble, nDouble)\
	OP(Float128, nDouble)

		template <Type::BuiltinType::BuiltinClass BuiltinClass>
		struct BuiltinTypeMap;

#define BUILTIN_TYPE_MAP_OP(buildinType, mappedType) \
		template <>\
		struct BuiltinTypeMap<Type::BuiltinType::builtinType>\
		{\
			using type = mappedType;\
		};
		BUILTIN_TYPE_MAP(BUILTIN_TYPE_MAP_OP);
#undef BUILTIN_TYPE_MAP_OP
	}

	enum class DeclStorageLevelFlag
	{
		None = 0x00,

		AvailableForLookup = 0x01,
		AvailableForCreateStorage = 0x02,
		CreateStorageIfNotFound = 0x04,
	};

	MAKE_ENUM_CLASS_BITMASK_TYPE(DeclStorageLevelFlag);

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

			nBool HasError() const noexcept
			{
				return m_HasError;
			}

			void Reset() noexcept
			{
				m_HasError = false;
			}

		private:
			Interpreter& m_Interpreter;
			nBool m_HasError;
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
			: public StmtVisitor<InterpreterExprVisitor>
		{
			template <typename ValueVisitor, typename ExpectedOrExcepted>
			class InterpreterExprEvaluator
				: public StmtVisitor<InterpreterExprEvaluator<ValueVisitor, ExpectedOrExcepted>, nBool>
			{
			public:
				explicit InterpreterExprEvaluator(Interpreter& interpreter, ValueVisitor const& visitor)
					: m_Interpreter{ interpreter }, m_Visitor{ visitor }
				{
				}

				explicit InterpreterExprEvaluator(Interpreter& interpreter, ValueVisitor&& visitor)
					: m_Interpreter{ interpreter }, m_Visitor{ std::move(visitor) }
				{
				}

				~InterpreterExprEvaluator()
				{
				}

				nBool VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& /*stmt*/)
				{
					nat_Throw(InterpreterException, u8"语句无法求值"_nv);
				}

				nBool VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& /*expr*/)
				{
					nat_Throw(InterpreterException, u8"此表达式无法被求值"_nv);
				}

				nBool VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr)
				{
					return Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
				}

				nBool VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr)
				{
					return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nByte>(expr->GetCodePoint()), ExpectedOrExcepted{});
				}

				nBool VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr)
				{
					const auto decl = expr->GetDecl();
					if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && NatsuLib::HasAllFlags(varDecl->GetStorageClass(), Specifier::StorageClass::Const))
					{
						return this->Visit(varDecl->GetInitializer());
					}
					return m_Interpreter.m_DeclStorage.VisitDeclStorage(expr->GetDecl(), m_Visitor, ExpectedOrExcepted{});
				}

				nBool VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr)
				{
					const auto exprType = expr->GetExprType();

					if (const auto builtinType = exprType.Cast<Type::BuiltinType>())
					{
						switch (builtinType->GetBuiltinClass())
						{
						case Type::BuiltinType::Float:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nFloat>(expr->GetValue()), ExpectedOrExcepted{});
						case Type::BuiltinType::Double:
							return Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
						case Type::BuiltinType::LongDouble:
						case Type::BuiltinType::Float128:
							break;
						default:
							nat_Throw(InterpreterException, u8"浮点字面量不应具有此类型"_nv);
						}
					}

					nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
				}

				nBool VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr)
				{
					const auto exprType = expr->GetExprType();

					if (const auto builtinType = exprType.Cast<Type::BuiltinType>())
					{
						switch (builtinType->GetBuiltinClass())
						{
						case Type::BuiltinType::UShort:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nuShort>(expr->GetValue()), ExpectedOrExcepted{});
						case Type::BuiltinType::UInt:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nuInt>(expr->GetValue()), ExpectedOrExcepted{});
						case Type::BuiltinType::ULong:
						case Type::BuiltinType::ULongLong:
						case Type::BuiltinType::UInt128:
							return Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
						case Type::BuiltinType::Short:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nShort>(expr->GetValue()), ExpectedOrExcepted{});
						case Type::BuiltinType::Int:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nInt>(expr->GetValue()), ExpectedOrExcepted{});
						case Type::BuiltinType::Long:
						case Type::BuiltinType::LongLong:
						case Type::BuiltinType::Int128:
							return Detail::InvokeIfSatisfied(m_Visitor, static_cast<nLong>(expr->GetValue()), ExpectedOrExcepted{});
						default:
							nat_Throw(InterpreterException, u8"整数字面量不应具有此类型"_nv);
						}
					}

					nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
				}

				nBool VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr)
				{
					return Detail::InvokeIfSatisfied(m_Visitor, expr->GetValue(), ExpectedOrExcepted{});
				}

			private:
				Interpreter& m_Interpreter;
				ValueVisitor m_Visitor;
			};

		public:
			explicit InterpreterExprVisitor(Interpreter& interpreter);
			~InterpreterExprVisitor();

			void Clear() noexcept;
			void PrintExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);
			Expression::ExprPtr GetLastVisitedExpr() const noexcept;

			template <typename ValueVisitor, typename ExpectedOrExcepted = Detail::ExpectedTag<>>
			[[nodiscard]] nBool Evaluate(NatsuLib::natRefPointer<Expression::Expr> const& expr, ValueVisitor&& visitor, ExpectedOrExcepted = {})
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

			void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt);
			void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);

			void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr);
			void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr);
			void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr);
			void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr);
			void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr);
			void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr);
			void VisitNullPointerLiteral(NatsuLib::natRefPointer<Expression::NullPointerLiteral> const& expr);

			void VisitArraySubscriptExpr(NatsuLib::natRefPointer<Expression::ArraySubscriptExpr> const& expr);
			void VisitConstructExpr(NatsuLib::natRefPointer<Expression::ConstructExpr> const& expr);
			void VisitDeleteExpr(NatsuLib::natRefPointer<Expression::DeleteExpr> const& expr);
			void VisitNewExpr(NatsuLib::natRefPointer<Expression::NewExpr> const& expr);
			void VisitThisExpr(NatsuLib::natRefPointer<Expression::ThisExpr> const& expr);
			void VisitThrowExpr(NatsuLib::natRefPointer<Expression::ThrowExpr> const& expr);
			void VisitCallExpr(NatsuLib::natRefPointer<Expression::CallExpr> const& expr);
			void VisitMemberCallExpr(NatsuLib::natRefPointer<Expression::MemberCallExpr> const& expr);
			void VisitCastExpr(NatsuLib::natRefPointer<Expression::CastExpr> const& expr);
			void VisitAsTypeExpr(NatsuLib::natRefPointer<Expression::AsTypeExpr> const& expr);
			void VisitImplicitCastExpr(NatsuLib::natRefPointer<Expression::ImplicitCastExpr> const& expr);
			void VisitMemberExpr(NatsuLib::natRefPointer<Expression::MemberExpr> const& expr);
			void VisitParenExpr(NatsuLib::natRefPointer<Expression::ParenExpr> const& expr);
			void VisitStmtExpr(NatsuLib::natRefPointer<Expression::StmtExpr> const& expr);
			void VisitConditionalOperator(NatsuLib::natRefPointer<Expression::ConditionalOperator> const& expr);
			void VisitBinaryOperator(NatsuLib::natRefPointer<Expression::BinaryOperator> const& expr);
			void VisitCompoundAssignOperator(NatsuLib::natRefPointer<Expression::CompoundAssignOperator> const& expr);
			void VisitUnaryOperator(NatsuLib::natRefPointer<Expression::UnaryOperator> const& expr);

		private:
			Interpreter& m_Interpreter;
			Expression::ExprPtr m_LastVisitedExpr;
			nBool m_ShouldPrint;
		};

		class InterpreterStmtVisitor
			: public StmtVisitor<InterpreterStmtVisitor>
		{
		public:
			explicit InterpreterStmtVisitor(Interpreter& interpreter);
			~InterpreterStmtVisitor();

			Expression::ExprPtr GetReturnedExpr() const noexcept;
			void ResetReturnedExpr() noexcept;

			void Visit(NatsuLib::natRefPointer<Statement::Stmt> const& stmt);

			void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt);
			void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);

			void VisitBreakStmt(NatsuLib::natRefPointer<Statement::BreakStmt> const& stmt);
			void VisitCatchStmt(NatsuLib::natRefPointer<Statement::CatchStmt> const& stmt);
			void VisitTryStmt(NatsuLib::natRefPointer<Statement::TryStmt> const& stmt);
			void VisitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& stmt);
			void VisitContinueStmt(NatsuLib::natRefPointer<Statement::ContinueStmt> const& stmt);
			void VisitDeclStmt(NatsuLib::natRefPointer<Statement::DeclStmt> const& stmt);
			void VisitDoStmt(NatsuLib::natRefPointer<Statement::DoStmt> const& stmt);
			void VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt);
			void VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt);
			void VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt);
			void VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt);
			void VisitNullStmt(NatsuLib::natRefPointer<Statement::NullStmt> const& stmt);
			void VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt);
			void VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt);
			void VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt);
			void VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt);
			void VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt);

		private:
			Interpreter& m_Interpreter;
			nBool m_Returned;
			Expression::ExprPtr m_ReturnedExpr;

			void initVar(NatsuLib::natRefPointer<Declaration::VarDecl> const& var, Expression::ExprPtr const& initializer);
		};

	public:
		class InterpreterDeclStorage
		{
		public:
			class MemoryLocationDecl
				: public Declaration::VarDecl
			{
			public:
				MemoryLocationDecl(Type::TypePtr type, nData memoryLocation, Identifier::IdPtr name = nullptr, Declaration::DeclPtr fromDecl = nullptr)
					: VarDecl{ Var, nullptr, {}, {}, std::move(name), std::move(type), Specifier::StorageClass::None }, m_MemoryLocation{ memoryLocation }, m_FromDecl{ std::move(fromDecl) }
				{
				}

				~MemoryLocationDecl();

				[[nodiscard]] nData GetMemoryLocation() const noexcept
				{
					return m_MemoryLocation;
				}

				[[nodiscard]] Declaration::DeclPtr const& GetFromDecl() const noexcept
				{
					return m_FromDecl;
				}

			private:
				nData m_MemoryLocation;
				Declaration::DeclPtr m_FromDecl;
			};

			class ArrayElementAccessor
				: NatsuLib::nonmovable
			{
			public:
				ArrayElementAccessor(InterpreterDeclStorage& declStorage, NatsuLib::natRefPointer<Type::ArrayType> const& arrayType, nData storage);

				template <typename Callable, typename ExpectedOrExcepted = Detail::ExpectedTag<>>
				[[nodiscard]] nBool VisitElement(std::size_t i, Callable&& visitor, ExpectedOrExcepted condition = {}) const
				{
					return m_DeclStorage.visitStorage(m_ElementType, m_Storage + m_ElementSize * i, std::forward<Callable>(visitor), condition);
				}

				std::size_t GetSize() const noexcept
				{
					return m_ArrayElementCount;
				}

				Type::TypePtr GetElementType() const noexcept;
				NatsuLib::natRefPointer<MemoryLocationDecl> GetElementDecl(std::size_t i) const;

				nData GetStorage() const noexcept;

			private:
				InterpreterDeclStorage& m_DeclStorage;
				Type::TypePtr m_ElementType;
				std::size_t m_ElementSize;
				std::size_t m_ArrayElementCount;
				nData m_Storage;
			};

			class MemberAccessor
				: NatsuLib::nonmovable
			{
			public:
				MemberAccessor(InterpreterDeclStorage& declStorage, NatsuLib::natRefPointer<Declaration::ClassDecl> classDecl, nData storage);

				template <typename Callable, typename ExpectedOrExcepted = Detail::ExpectedTag<>>
				[[nodiscard]] nBool VisitMember(NatsuLib::natRefPointer<Declaration::FieldDecl> const& fieldDecl, Callable&& visitor, ExpectedOrExcepted condition = {}) const
				{
					const auto iter = m_FieldOffsets.find(fieldDecl);
					if (iter == m_FieldOffsets.end())
					{
						return false;
					}

					const auto offset = iter->second;
					return m_DeclStorage.visitStorage(fieldDecl->GetValueType(), m_Storage + offset, std::forward<Callable>(visitor), condition);
				}

				std::size_t GetFieldCount() const noexcept;
				NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<Declaration::FieldDecl>>> GetFields() const noexcept;

				NatsuLib::natRefPointer<MemoryLocationDecl> GetMemberDecl(NatsuLib::natRefPointer<Declaration::FieldDecl> const& fieldDecl) const;

			private:
				InterpreterDeclStorage& m_DeclStorage;
				NatsuLib::natRefPointer<Declaration::ClassDecl> m_ClassDecl;
				std::unordered_map<NatsuLib::natRefPointer<Declaration::FieldDecl>, std::size_t> m_FieldOffsets;
				nData m_Storage;
			};

			class PointerAccessor
				: NatsuLib::nonmovable
			{
			public:
				explicit PointerAccessor(nData storage);

				NatsuLib::natRefPointer<Declaration::VarDecl> GetReferencedDecl() const noexcept;
				void SetReferencedDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& value) noexcept;

			private:
				nData m_Storage;
			};

		private:
			template <typename Callable, typename ExpectedOrExcepted>
			nBool visitStorage(Type::TypePtr const& type, nData storage, Callable&& visitor, ExpectedOrExcepted condition)
			{
				auto& storageRef = *storage;

				switch (type->GetType())
				{
				case Type::Type::Builtin:
				{
					const auto builtinType = type.UnsafeCast<Type::BuiltinType>();
					assert(builtinType);

					switch (builtinType->GetBuiltinClass())
					{
#define BUILTIN_TYPE(Id, Name)
#define SIGNED_TYPE(Id, Name) \
					case Type::BuiltinType::Id:\
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor),\
							reinterpret_cast<typename Detail::BuiltinTypeMap<Type::BuiltinType::Id>::type&>(storageRef), condition);
#define UNSIGNED_TYPE(Id, Name) \
					case Type::BuiltinType::Id:\
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor),\
							reinterpret_cast<typename Detail::BuiltinTypeMap<Type::BuiltinType::Id>::type&>(storageRef), condition);
#define FLOATING_TYPE(Id, Name) \
					case Type::BuiltinType::Id:\
						return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor),\
							reinterpret_cast<typename Detail::BuiltinTypeMap<Type::BuiltinType::Id>::type&>(storageRef), condition);
#define PLACEHOLDER_TYPE(Id, Name)
#include <Basic/BuiltinTypesDef.h>
					default:
						nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
					}
				}
				case Type::Type::Pointer:
				{
					PointerAccessor accessor{ storage };
					return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), accessor, condition);
				}
				case Type::Type::Array:
				{
					ArrayElementAccessor accessor{ *this, type, storage };
					return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), accessor, condition);
				}
				case Type::Type::Function:
					nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
				case Type::Type::Class:
				{
					const auto classType = type.UnsafeCast<Type::ClassType>();
					auto classDecl = classType->GetDecl().Cast<Declaration::ClassDecl>();
					assert(classDecl);
					// TODO: 能否重用？
					MemberAccessor accessor{ *this, std::move(classDecl), storage };
					return Detail::InvokeIfSatisfied(std::forward<Callable>(visitor), accessor, condition);
				}
				case Type::Type::Enum:
					nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
				default:
					assert(!"Invalid type.");
					[[fallthrough]];
				case Type::Type::Paren:
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
			std::pair<nBool, nData> GetOrAddDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type = nullptr);
			void RemoveDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl);
			nBool DoesDeclExist(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl) const noexcept;

			void PushStorage(DeclStorageLevelFlag flags = DeclStorageLevelFlag::AvailableForLookup | DeclStorageLevelFlag::AvailableForCreateStorage);
			void PopStorage();

			void MergeStorage();

			DeclStorageLevelFlag GetTopStorageFlag() const noexcept;
			void SetTopStorageFlag(DeclStorageLevelFlag flags);

			void GarbageCollect();

			static NatsuLib::natRefPointer<Declaration::ValueDecl> CreateTemporaryObjectDecl(Type::TypePtr type, SourceLocation loc = {});

			template <typename Callable, typename ExpectedOrExcepted = Detail::ExpectedTag<>>
			[[nodiscard]] nBool VisitDeclStorage(NatsuLib::natRefPointer<Declaration::ValueDecl> decl, Callable&& visitor, ExpectedOrExcepted condition = {})
			{
				if (!decl)
				{
					return false;
				}

				const auto type = Type::Type::GetUnderlyingType(decl->GetValueType());

				const auto [addedDecl, storagePointer] = GetOrAddDecl(decl, type);
				auto visitSucceed = false;
				const auto scope = NatsuLib::make_scope([this, addedDecl, &visitSucceed, decl = std::move(decl)]
				{
					if (addedDecl && !visitSucceed)
					{
						RemoveDecl(decl);
					}
				});

				visitSucceed = visitStorage(type, storagePointer, std::forward<Callable>(visitor), condition);
				return visitSucceed;
			}

		private:
			Interpreter& m_Interpreter;
			std::vector<std::pair<DeclStorageLevelFlag, std::unique_ptr<std::unordered_map<NatsuLib::natRefPointer<Declaration::ValueDecl>, std::unique_ptr<nByte[], StorageDeleter>>>>> m_DeclStorage;
		};

		Interpreter(NatsuLib::natRefPointer<NatsuLib::TextReader<NatsuLib::StringType::Utf8>> const& diagIdMapFile, NatsuLib::natLog& logger);
		~Interpreter();

		void Run(NatsuLib::Uri const& uri);
		void Run(nStrView content);

		InterpreterDeclStorage& GetDeclStorage() noexcept;

		using Function = std::function<NatsuLib::natRefPointer<Declaration::ValueDecl>(std::vector<NatsuLib::natRefPointer<Declaration::ValueDecl>> const&)>;

		void RegisterFunction(nStrView name, Type::TypePtr resultType, std::initializer_list<Type::TypePtr> argTypes, Function const& func);

		ASTContext& GetASTContext() noexcept;

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
		InterpreterStmtVisitor m_Visitor;

		InterpreterDeclStorage m_DeclStorage;

		std::unordered_map<NatsuLib::natRefPointer<Declaration::FunctionDecl>, Function> m_FunctionMap;
	};
}
