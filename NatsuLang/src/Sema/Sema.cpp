#include "Sema/Sema.h"
#include "Sema/Scope.h"
#include "Sema/Declarator.h"
#include "Lex/Preprocessor.h"
#include "Lex/LiteralParser.h"
#include "AST/Declaration.h"
#include "AST/ASTConsumer.h"
#include "AST/ASTContext.h"
#include "AST/Expression.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Semantic;

namespace
{
	constexpr NatsuLang::Declaration::IdentifierNamespace chooseIDNS(Sema::LookupNameType lookupNameType) noexcept
	{
		using NatsuLang::Declaration::IdentifierNamespace;
		switch (lookupNameType)
		{
		case Sema::LookupNameType::LookupOrdinaryName:
			return IdentifierNamespace::Ordinary | IdentifierNamespace::Tag | IdentifierNamespace::Member | IdentifierNamespace::Module;
		case Sema::LookupNameType::LookupTagName:
			return IdentifierNamespace::Type;
		case Sema::LookupNameType::LookupLabel:
			return IdentifierNamespace::Label;
		case Sema::LookupNameType::LookupMemberName:
			return IdentifierNamespace::Member | IdentifierNamespace::Tag | IdentifierNamespace::Ordinary;
		case Sema::LookupNameType::LookupModuleName:
			return IdentifierNamespace::Module;
		case Sema::LookupNameType::LookupAnyName:
			return IdentifierNamespace::Ordinary | IdentifierNamespace::Tag | IdentifierNamespace::Member | IdentifierNamespace::Module | IdentifierNamespace::Type;
		default:
			return IdentifierNamespace::None;
		}
	}

	NatsuLang::Expression::CastType getBuiltinCastType(natRefPointer<NatsuLang::Type::BuiltinType> const& fromType, natRefPointer<NatsuLang::Type::BuiltinType> const& toType) noexcept
	{
		using NatsuLang::Expression::CastType;
		using NatsuLang::Type::BuiltinType;

		if (!fromType || (!fromType->IsIntegerType() && !fromType->IsFloatingType()) ||
			!toType || (!toType->IsIntegerType() && !toType->IsFloatingType()))
		{
			return CastType::Invalid;
		}

		if (fromType->GetBuiltinClass() == toType->GetBuiltinClass())
		{
			return CastType::NoOp;
		}

		if (fromType->IsIntegerType())
		{
			if (toType->IsIntegerType())
			{
				return toType->GetBuiltinClass() == BuiltinType::Bool ? CastType::IntegralToBoolean : CastType::IntegralCast;
			}

			// fromType和toType只能是IntegerType或FloatingType
			return CastType::IntegralToFloating;
		}

		// fromType是FloatingType
		if (toType->IsIntegerType())
		{
			return toType->GetBuiltinClass() == BuiltinType::Bool ? CastType::FloatingToBoolean : CastType::FloatingToIntegral;
		}

		return CastType::FloatingCast;
	}

	NatsuLang::Type::TypePtr getCommonType(NatsuLang::Type::TypePtr const& type1, NatsuLang::Type::TypePtr const& type2)
	{
		nat_Throw(NotImplementedException);
	}

	constexpr NatsuLang::Expression::UnaryOperationType getUnaryOperationType(NatsuLang::Lex::TokenType tokenType) noexcept
	{
		using NatsuLang::Expression::UnaryOperationType;

		switch (tokenType)
		{
		case NatsuLang::Lex::TokenType::Plus:
			return UnaryOperationType::Plus;
		case NatsuLang::Lex::TokenType::PlusPlus:
			return UnaryOperationType::PreInc;
		case NatsuLang::Lex::TokenType::Minus:
			return UnaryOperationType::Minus;
		case NatsuLang::Lex::TokenType::MinusMinus:
			return UnaryOperationType::PreDec;
		case NatsuLang::Lex::TokenType::Tilde:
			return UnaryOperationType::Not;
		case NatsuLang::Lex::TokenType::Exclaim:
			return UnaryOperationType::LNot;
		default:
			assert(!"Invalid TokenType for UnaryOperationType.");
			return UnaryOperationType::Invalid;
		}
	}

	constexpr NatsuLang::Expression::BinaryOperationType getBinaryOperationType(NatsuLang::Lex::TokenType tokenType) noexcept
	{
		using NatsuLang::Expression::BinaryOperationType;

		switch (tokenType)
		{
		case NatsuLang::Lex::TokenType::Amp:
			return BinaryOperationType::And;
		case NatsuLang::Lex::TokenType::AmpAmp:
			return BinaryOperationType::LAnd;
		case NatsuLang::Lex::TokenType::AmpEqual:
			return BinaryOperationType::AndAssign;
		case NatsuLang::Lex::TokenType::Star:
			return BinaryOperationType::Mul;
		case NatsuLang::Lex::TokenType::StarEqual:
			return BinaryOperationType::MulAssign;
		case NatsuLang::Lex::TokenType::Plus:
			return BinaryOperationType::Add;
		case NatsuLang::Lex::TokenType::PlusEqual:
			return BinaryOperationType::AddAssign;
		case NatsuLang::Lex::TokenType::Minus:
			return BinaryOperationType::Sub;
		case NatsuLang::Lex::TokenType::MinusEqual:
			return BinaryOperationType::SubAssign;
		case NatsuLang::Lex::TokenType::ExclaimEqual:
			return BinaryOperationType::NE;
		case NatsuLang::Lex::TokenType::Slash:
			return BinaryOperationType::Div;
		case NatsuLang::Lex::TokenType::SlashEqual:
			return BinaryOperationType::DivAssign;
		case NatsuLang::Lex::TokenType::Percent:
			return BinaryOperationType::Mod;
		case NatsuLang::Lex::TokenType::PercentEqual:
			return BinaryOperationType::RemAssign;
		case NatsuLang::Lex::TokenType::Less:
			return BinaryOperationType::LT;
		case NatsuLang::Lex::TokenType::LessLess:
			return BinaryOperationType::Shl;
		case NatsuLang::Lex::TokenType::LessEqual:
			return BinaryOperationType::LE;
		case NatsuLang::Lex::TokenType::LessLessEqual:
			return BinaryOperationType::ShlAssign;
		case NatsuLang::Lex::TokenType::Greater:
			return BinaryOperationType::GT;
		case NatsuLang::Lex::TokenType::GreaterGreater:
			return BinaryOperationType::Shr;
		case NatsuLang::Lex::TokenType::GreaterEqual:
			return BinaryOperationType::GE;
		case NatsuLang::Lex::TokenType::GreaterGreaterEqual:
			return BinaryOperationType::ShrAssign;
		case NatsuLang::Lex::TokenType::Caret:
			return BinaryOperationType::Xor;
		case NatsuLang::Lex::TokenType::CaretEqual:
			return BinaryOperationType::XorAssign;
		case NatsuLang::Lex::TokenType::Pipe:
			return BinaryOperationType::Or;
		case NatsuLang::Lex::TokenType::PipePipe:
			return BinaryOperationType::LOr;
		case NatsuLang::Lex::TokenType::PipeEqual:
			return BinaryOperationType::OrAssign;
		case NatsuLang::Lex::TokenType::Equal:
			return BinaryOperationType::Assign;
		case NatsuLang::Lex::TokenType::EqualEqual:
			return BinaryOperationType::EQ;
		default:
			assert(!"Invalid TokenType for BinaryOperationType.");
			return BinaryOperationType::Invalid;
		}
	}
}

Sema::Sema(Preprocessor& preprocessor, ASTContext& astContext, natRefPointer<ASTConsumer> astConsumer)
	: m_Preprocessor{ preprocessor }, m_Context{ astContext }, m_Consumer{ std::move(astConsumer) }, m_Diag{ preprocessor.GetDiag() },
	  m_SourceManager{ preprocessor.GetSourceManager() }
{
	PushScope(ScopeFlags::DeclarableScope);
	ActOnTranslationUnitScope(m_CurrentScope);
}

Sema::~Sema()
{
}

void Sema::PushDeclContext(natRefPointer<Scope> const& scope, Declaration::DeclContext* dc)
{
	assert(dc);
	const auto declPtr = Declaration::Decl::CastFromDeclContext(dc)->ForkRef();
	assert(declPtr->GetContext() == Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()));
	m_CurrentDeclContext = declPtr;
	scope->SetEntity(dc);
}

void Sema::PopDeclContext()
{
	assert(m_CurrentDeclContext);
	const auto parentDc = m_CurrentDeclContext->GetContext();
	assert(parentDc);
	m_CurrentDeclContext = Declaration::Decl::CastFromDeclContext(parentDc)->ForkRef();
}

void Sema::PushScope(ScopeFlags flags)
{
	m_CurrentScope = make_ref<Scope>(m_CurrentScope, flags);
}

void Sema::PopScope()
{
	assert(m_CurrentScope);

	m_CurrentScope = m_CurrentScope->GetParent();
}

void Sema::PushOnScopeChains(natRefPointer<Declaration::NamedDecl> decl, natRefPointer<Scope> const& scope, nBool addToContext)
{
	// 处理覆盖声明的情况

	if (addToContext)
	{
		Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get())->AddDecl(decl);
	}

	scope->AddDecl(decl);
}

void Sema::ActOnTranslationUnitScope(natRefPointer<Scope> scope)
{
	m_TranslationUnitScope = scope;
	PushDeclContext(m_TranslationUnitScope, m_Context.GetTranslationUnit().Get());
}

natRefPointer<NatsuLang::Declaration::Decl> Sema::ActOnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path)
{
	nat_Throw(NatsuLib::NotImplementedException);
}

NatsuLang::Type::TypePtr Sema::GetTypeName(natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc, natRefPointer<Scope> scope, Type::TypePtr const& objectType)
{
	Declaration::DeclContext* context{};

	if (objectType && objectType->GetType() == Type::Type::Record)
	{
		const auto tagType = static_cast<natRefPointer<Type::TagType>>(objectType);
		if (tagType)
		{
			context = tagType->GetDecl().Get();
		}
	}

	LookupResult result{ *this, id, nameLoc, LookupNameType::LookupOrdinaryName };
	if (context)
	{
		if (!LookupQualifiedName(result, context))
		{
			LookupName(result, scope);
		}
	}
	else
	{
		LookupName(result, scope);
	}

	switch (result.GetResultType())
	{
	default:
		assert(!"Invalid result type.");
		[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::FoundOverloaded:
	case LookupResult::LookupResultType::Ambiguous:
		return nullptr;
	case LookupResult::LookupResultType::Found:
	{
		assert(result.GetDeclSize() == 1);
		const auto decl = *result.GetDecls().begin();
		if (auto typeDecl = static_cast<natRefPointer<Declaration::TypeDecl>>(decl))
		{
			return typeDecl->GetTypeForDecl();
		}
		return nullptr;
	}
	}
}

NatsuLang::Type::TypePtr Sema::BuildFunctionType(Type::TypePtr retType, Linq<NatsuLib::Valued<Type::TypePtr>> const& paramType)
{
	return m_Context.GetFunctionType(from(paramType), std::move(retType));
}

NatsuLang::Declaration::DeclPtr Sema::ActOnStartOfFunctionDef(natRefPointer<Scope> const& scope, Declaration::Declarator const& declarator)
{
	const auto parentScope = scope->GetParent();
	auto decl = HandleDeclarator(parentScope, declarator);
	return ActOnStartOfFunctionDef(scope, std::move(decl));
}

NatsuLang::Declaration::DeclPtr Sema::ActOnStartOfFunctionDef(natRefPointer<Scope> const& scope, Declaration::DeclPtr decl)
{
	if (!decl)
	{
		return nullptr;
	}

	auto funcDecl = static_cast<natRefPointer<Declaration::FunctionDecl>>(decl);
	if (!funcDecl)
	{
		return nullptr;
	}

	if (scope)
	{
		PushDeclContext(scope, funcDecl.Get());

		for (auto&& d : funcDecl->GetDecls().select([](Declaration::DeclPtr const& td)
			{
				return static_cast<natRefPointer<Declaration::NamedDecl>>(td);
			}).where([](auto&& arg) -> nBool { return arg; }))
		{
			if (d->GetIdentifierInfo())
			{
				PushOnScopeChains(std::move(d), scope, false);
			}
		}

		for (auto&& param : funcDecl->GetParams())
		{
			if (param)
			{
				PushOnScopeChains(param, scope, true);
			}
		}
	}

	return funcDecl;
}

NatsuLang::Declaration::DeclPtr Sema::ActOnFinishFunctionBody(Declaration::DeclPtr decl, Statement::StmtPtr body)
{
	auto fd = static_cast<natRefPointer<Declaration::FunctionDecl>>(decl);
	if (!fd)
	{
		return nullptr;
	}

	fd->SetBody(std::move(body));

	PopDeclContext();
	
	return fd;
}

nBool Sema::LookupName(LookupResult& result, natRefPointer<Scope> scope) const
{
	for (; scope; scope = scope->GetParent())
	{
		const auto context = scope->GetEntity();
		if (context && LookupQualifiedName(result, context))
		{
			return true;
		}
	}

	result.ResolveResultType();
	return false;
}

nBool Sema::LookupQualifiedName(LookupResult& result, Declaration::DeclContext* context) const
{
	const auto id = result.GetLookupId();
	const auto lookupType = result.GetLookupType();
	auto found = false;

	auto query = context->Lookup(id);
	switch (lookupType)
	{
	case LookupNameType::LookupTagName:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			return static_cast<natRefPointer<Declaration::TagDecl>>(decl);
		});
		break;
	case LookupNameType::LookupLabel:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			return static_cast<natRefPointer<Declaration::LabelDecl>>(decl);
		});
		break;
	case LookupNameType::LookupMemberName:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			const auto type = decl->GetType();
			return type == Declaration::Decl::Method || type == Declaration::Decl::Field;
		});
		break;
	case LookupNameType::LookupModuleName:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			return static_cast<natRefPointer<Declaration::ModuleDecl>>(decl);
		});
		break;
	default:
		assert(!"Invalid lookupType");
		[[fallthrough]];
	case LookupNameType::LookupOrdinaryName:
	case LookupNameType::LookupAnyName:
		break;
	}
	auto queryResult{ query.Cast<std::vector<natRefPointer<Declaration::NamedDecl>>>() };
	if (!queryResult.empty())
	{
		result.AddDecl(from(queryResult));
		found = true;
	}

	result.ResolveResultType();
	return found;
}

nBool Sema::LookupNestedName(LookupResult& result, natRefPointer<Scope> scope, natRefPointer<NestedNameSpecifier> const& nns)
{
	if (nns)
	{
		const auto dc = nns->GetAsDeclContext(m_Context);
		return LookupQualifiedName(result, dc);
	}

	return LookupName(result, scope);
}

natRefPointer<NatsuLang::Declaration::LabelDecl> Sema::LookupOrCreateLabel(Identifier::IdPtr id, SourceLocation loc)
{
	LookupResult r{ *this, std::move(id), loc, LookupNameType::LookupLabel };

	if (!LookupName(r, m_CurrentScope) || r.GetDeclSize() != 1)
	{
		// TODO: 找不到这个声明或者找到多于1个声明，报告错误
		return nullptr;
	}

	auto labelDecl = r.GetDecls().first();

	const auto curContext = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());

	if (labelDecl && labelDecl->GetContext() != curContext)
	{
		// TODO: 找到了但是不是当前域下的声明，报告错误
		return nullptr;
	}

	if (!labelDecl)
	{
		labelDecl = make_ref<Declaration::LabelDecl>(curContext, loc, r.GetLookupId(), nullptr);
		auto s = m_CurrentScope->GetFunctionParent().Lock();
		PushOnScopeChains(labelDecl, s, true);
	}

	return labelDecl;
}

NatsuLang::Type::TypePtr Sema::ActOnTypeName(natRefPointer<Scope> const& scope, Declaration::Declarator const& decl)
{
	static_cast<void>(scope);
	return decl.GetType();
}

Type::TypePtr Sema::ActOnTypeOfType(natRefPointer<Expression::Expr> expr, Type::TypePtr underlyingType)
{
	return make_ref<Type::TypeOfType>(std::move(expr), std::move(underlyingType));
}

natRefPointer<NatsuLang::Declaration::ParmVarDecl> Sema::ActOnParamDeclarator(natRefPointer<Scope> const& scope, Declaration::Declarator const& decl)
{
	auto id = decl.GetIdentifier();
	if (id)
	{
		LookupResult r{ *this, id, {}, LookupNameType::LookupOrdinaryName };
		LookupName(r, scope);
		if (r.GetDeclSize() > 0)
		{
			// TODO: 报告存在重名的参数
		}
	}

	// 临时放在翻译单元上下文中，在整个函数声明完成后关联到函数
	auto ret = make_ref<Declaration::ParmVarDecl>(Declaration::Decl::ParmVar,
		m_Context.GetTranslationUnit().Get(), SourceLocation{}, SourceLocation{},
		std::move(id), decl.GetType(), Specifier::StorageClass::None, decl.GetInitializer());

	scope->AddDecl(ret);

	return ret;
}

natRefPointer<NatsuLang::Declaration::VarDecl> Sema::ActOnVariableDeclarator(
	natRefPointer<Scope> const& scope, Declaration::Declarator const& decl, Declaration::DeclContext* dc)
{
	auto type = Type::Type::GetUnderlyingType(decl.GetType());
	auto id = decl.GetIdentifier();
	auto initExpr = static_cast<natRefPointer<Expression::Expr>>(decl.GetInitializer());

	if (!id)
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (!type)
	{
		// 隐含 auto 或者出现错误
		if (!initExpr)
		{
			// TODO: 报告错误
			return nullptr;
		}

		type = initExpr->GetExprType();
	}

	// TODO: 对于嵌套的类型会出现误判
	if (initExpr->GetExprType() != type)
	{
		initExpr = ImpCastExprToType(std::move(initExpr), type, getCastType(initExpr, type));
	}

	auto varDecl = make_ref<Declaration::VarDecl>(Declaration::Decl::Var, dc, decl.GetRange().GetBegin(),
		SourceLocation{}, std::move(id), std::move(type), Specifier::StorageClass::None);

	varDecl->SetInitializer(initExpr);

	return varDecl;
}

natRefPointer<NatsuLang::Declaration::FunctionDecl> Sema::ActOnFunctionDeclarator(
	natRefPointer<Scope> const& scope, Declaration::Declarator const& decl, Declaration::DeclContext* dc)
{
	auto id = decl.GetIdentifier();
	if (!id)
	{
		// TODO: 报告错误
		return nullptr;
	}

	// TODO: 处理自动推断函数返回类型的情况
	auto type = static_cast<natRefPointer<Type::FunctionType>>(decl.GetType());
	if (!type)
	{
		// TODO: 报告错误
		return nullptr;
	}

	auto funcDecl = make_ref<Declaration::FunctionDecl>(Declaration::Decl::Function, dc,
		SourceLocation{}, SourceLocation{}, std::move(id), std::move(type), Specifier::StorageClass::None);

	for (auto const& param : decl.GetParams())
	{
		param->SetContext(funcDecl.Get());
	}

	funcDecl->SetParams(from(decl.GetParams()));

	return funcDecl;
}

natRefPointer<NatsuLang::Declaration::NamedDecl> Sema::HandleDeclarator(natRefPointer<Scope> scope, Declaration::Declarator const& decl)
{
	if (auto preparedDecl = decl.GetDecl())
	{
		return preparedDecl;
	}

	const auto id = decl.GetIdentifier();

	if (!id)
	{
		if (decl.GetContext() != Declaration::Context::Prototype)
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, decl.GetRange().GetBegin());
		}

		return nullptr;
	}

	while ((scope->GetFlags() & ScopeFlags::DeclarableScope) == ScopeFlags::None)
	{
		scope = scope->GetParent();
	}

	LookupResult previous{ *this, id, {}, LookupNameType::LookupOrdinaryName };

	LookupName(previous, scope);

	if (previous.GetDeclSize())
	{
		// TODO: 处理重载或覆盖的情况
		return nullptr;
	}

	const auto dc = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());
	const auto type = decl.GetType();
	natRefPointer<Declaration::NamedDecl> retDecl;

	if (auto funcType = static_cast<natRefPointer<Type::FunctionType>>(type))
	{
		retDecl = ActOnFunctionDeclarator(scope, decl, dc);
	}
	else
	{
		retDecl = ActOnVariableDeclarator(scope, decl, dc);
	}

	if (retDecl)
	{
		PushOnScopeChains(retDecl, scope);
	}
	else
	{
		// TODO: 报告无法加入声明
	}

	return retDecl;
}

NatsuLang::Statement::StmtPtr Sema::ActOnNullStmt(SourceLocation loc)
{
	return make_ref<Statement::NullStmt>(loc);
}

NatsuLang::Statement::StmtPtr Sema::ActOnDeclStmt(std::vector<Declaration::DeclPtr> decls, SourceLocation start,
	SourceLocation end)
{
	return make_ref<Statement::DeclStmt>(move(decls), start, end);
}

NatsuLang::Statement::StmtPtr Sema::ActOnLabelStmt(SourceLocation labelLoc, natRefPointer<Declaration::LabelDecl> labelDecl,
	SourceLocation colonLoc, Statement::StmtPtr subStmt)
{
	static_cast<void>(colonLoc);

	if (!labelDecl)
	{
		return nullptr;
	}

	if (labelDecl->GetStmt())
	{
		// TODO: 报告标签重定义错误
		return subStmt;
	}

	auto labelStmt = make_ref<Statement::LabelStmt>(labelLoc, std::move(labelDecl), std::move(subStmt));
	labelDecl->SetStmt(labelStmt);

	return labelStmt;
}

NatsuLang::Statement::StmtPtr Sema::ActOnCompoundStmt(std::vector<Statement::StmtPtr> stmtVec, SourceLocation begin,
	SourceLocation end)
{
	return make_ref<Statement::CompoundStmt>(move(stmtVec), begin, end);
}

NatsuLang::Statement::StmtPtr Sema::ActOnIfStmt(SourceLocation ifLoc, Expression::ExprPtr condExpr,
	Statement::StmtPtr thenStmt, SourceLocation elseLoc, Statement::StmtPtr elseStmt)
{
	const auto boolType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
	const auto castType = getCastType(condExpr, boolType);
	return make_ref<Statement::IfStmt>(ifLoc, ImpCastExprToType(std::move(condExpr), std::move(boolType), castType), std::move(thenStmt), elseLoc, std::move(elseStmt));
}

NatsuLang::Statement::StmtPtr Sema::ActOnWhileStmt(SourceLocation loc, Expression::ExprPtr cond,
	Statement::StmtPtr body)
{
	return make_ref<Statement::WhileStmt>(loc, std::move(cond), std::move(body));
}

NatsuLang::Statement::StmtPtr Sema::ActOnContinueStatement(SourceLocation loc, natRefPointer<Scope> const& scope)
{
	if (!scope->GetContinueParent())
	{
		// TODO: 报告错误：当前作用域不可 continue
		return nullptr;
	}

	return make_ref<Statement::ContinueStmt>(loc);
}

NatsuLang::Statement::StmtPtr Sema::ActOnBreakStatement(SourceLocation loc, natRefPointer<Scope> const& scope)
{
	if (!scope->GetBreakParent())
	{
		// TODO: 报告错误：当前作用域不可 break
		return nullptr;
	}

	return make_ref<Statement::BreakStmt>(loc);
}

NatsuLang::Statement::StmtPtr Sema::ActOnReturnStmt(SourceLocation loc, Expression::ExprPtr returnedExpr, natRefPointer<Scope> const& scope)
{
	if (const auto curFunc = scope->GetFunctionParent().Lock())
	{
		const auto entity = Declaration::Decl::CastFromDeclContext(curFunc->GetEntity())->ForkRef<Declaration::FunctionDecl>();
		if (!entity)
		{
			return nullptr;
		}

		const auto funcType = static_cast<natRefPointer<Type::FunctionType>>(entity->GetValueType());
		if (!funcType)
		{
			return nullptr;
		}

		const auto retType = funcType->GetResultType();
		const auto retTypeClass = retType->GetType();
		if (retTypeClass == Type::Type::Auto)
		{
			// TODO: 获得真实返回类型
		}

		if (retTypeClass == Type::Type::Builtin &&
			static_cast<natRefPointer<Type::BuiltinType>>(retType)->GetBuiltinClass() == Type::BuiltinType::Void &&
			returnedExpr)
		{
			returnedExpr = ImpCastExprToType(std::move(returnedExpr), m_Context.GetBuiltinType(Type::BuiltinType::Void), Expression::CastType::ToVoid);
		}

		return make_ref<Statement::ReturnStmt>(loc, std::move(returnedExpr));
	}

	// TODO: 报告错误：只能在函数域内返回
	return nullptr;
}

NatsuLang::Statement::StmtPtr Sema::ActOnExprStmt(Expression::ExprPtr expr)
{
	return expr;
}

NatsuLang::Expression::ExprPtr Sema::ActOnBooleanLiteral(Lex::Token const& token) const
{
	assert(token.IsAnyOf({ Lex::TokenType::Kw_true, Lex::TokenType::Kw_false }));
	return make_ref<Expression::BooleanLiteral>(token.Is(Lex::TokenType::Kw_true), m_Context.GetBuiltinType(Type::BuiltinType::Bool), token.GetLocation());
}

NatsuLang::Expression::ExprPtr Sema::ActOnNumericLiteral(Lex::Token const& token) const
{
	assert(token.Is(Lex::TokenType::NumericLiteral));

	Lex::NumericLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	if (literalParser.IsFloatingLiteral())
	{
		Type::BuiltinType::BuiltinClass builtinType;
		if (literalParser.IsFloat())
		{
			builtinType = Type::BuiltinType::Float;
		}
		else if (literalParser.IsLong())
		{
			builtinType = Type::BuiltinType::LongDouble;
		}
		else
		{
			builtinType = Type::BuiltinType::Double;
		}

		auto type = m_Context.GetBuiltinType(builtinType);

		nDouble value;
		if (literalParser.GetFloatValue(value))
		{
			// TODO: 报告溢出
		}

		return make_ref<Expression::FloatingLiteral>(value, std::move(type), token.GetLocation());
	}

	// 是整数字面量
	Type::BuiltinType::BuiltinClass builtinType;
	if (literalParser.IsLong())
	{
		builtinType = literalParser.IsUnsigned() ? Type::BuiltinType::ULong : Type::BuiltinType::Long;
	}
	else if (literalParser.IsLongLong())
	{
		builtinType = literalParser.IsUnsigned() ? Type::BuiltinType::ULongLong : Type::BuiltinType::LongLong;
	}
	else
	{
		builtinType = literalParser.IsUnsigned() ? Type::BuiltinType::UInt : Type::BuiltinType::Int;
	}

	auto type = m_Context.GetBuiltinType(builtinType);

	nuLong value;
	if (literalParser.GetIntegerValue(value))
	{
		// TODO: 报告溢出
	}

	return make_ref<Expression::IntegerLiteral>(value, std::move(type), token.GetLocation());
}

NatsuLang::Expression::ExprPtr Sema::ActOnCharLiteral(Lex::Token const& token) const
{
	assert(token.Is(Lex::TokenType::CharLiteral) && token.GetLiteralContent().has_value());

	Lex::CharLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	return make_ref<Expression::CharacterLiteral>(literalParser.GetValue(), nString::UsingStringType, m_Context.GetBuiltinType(Type::BuiltinType::Char), token.GetLocation());
}

NatsuLang::Expression::ExprPtr Sema::ActOnStringLiteral(Lex::Token const& token)
{
	assert(token.Is(Lex::TokenType::StringLiteral) && token.GetLiteralContent().has_value());

	Lex::StringLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	// TODO: 缓存字符串字面量以便重用
	auto value = literalParser.GetValue();
	return make_ref<Expression::StringLiteral>(value, m_Context.GetArrayType(m_Context.GetBuiltinType(Type::BuiltinType::Char), value.GetSize()), token.GetLocation());
}

NatsuLang::Expression::ExprPtr Sema::ActOnThrow(natRefPointer<Scope> const& scope, SourceLocation loc, Expression::ExprPtr expr)
{
	// TODO
	if (expr)
	{

	}

	nat_Throw(NotImplementedException);
}

NatsuLang::Expression::ExprPtr Sema::ActOnIdExpr(natRefPointer<Scope> const& scope, natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, nBool hasTraillingLParen)
{
	LookupResult result{ *this, id, {}, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, scope, nns) || result.GetResultType() == LookupResult::LookupResultType::Ambiguous)
	{
		return nullptr;
	}

	// TODO: 对以函数调用形式引用的标识符采取特殊的处理
	static_cast<void>(hasTraillingLParen);

	if (result.GetDeclSize() == 1)
	{
		return BuildDeclarationNameExpr(nns, std::move(id), result.GetDecls().first());
	}
	
	// TODO: 只有重载函数可以在此找到多个声明，否则报错
	nat_Throw(NotImplementedException);
}

NatsuLang::Expression::ExprPtr Sema::ActOnThis(SourceLocation loc)
{
	assert(m_CurrentDeclContext);
	auto dc = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());
	while (dc->GetType() == Declaration::Decl::Enum)
	{
		dc = Declaration::Decl::CastFromDeclContext(dc)->GetContext();
	}

	const auto decl = Declaration::Decl::CastFromDeclContext(dc)->ForkRef();

	if (const auto methodDecl = static_cast<natRefPointer<Declaration::MethodDecl>>(decl))
	{
		auto recordDecl = Declaration::Decl::CastFromDeclContext(methodDecl->GetContext())->ForkRef<Declaration::RecordDecl>();
		assert(recordDecl);
		return make_ref<Expression::ThisExpr>(loc, recordDecl->GetTypeForDecl(), false);
	}

	// TODO: 报告当前上下文不允许使用 this 的错误
	return nullptr;
}

NatsuLang::Expression::ExprPtr Sema::ActOnAsTypeExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr exprToCast, Type::TypePtr type, SourceLocation loc)
{
	return make_ref<Expression::AsTypeExpr>(std::move(type), getCastType(exprToCast, type), std::move(exprToCast));
}

NatsuLang::Expression::ExprPtr Sema::ActOnArraySubscriptExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation lloc, Expression::ExprPtr index, SourceLocation rloc)
{
	// TODO: 屏蔽未使用参数警告，这些参数将会在将来的版本被使用
	static_cast<void>(scope);
	static_cast<void>(lloc);

	// TODO: 当前仅支持对内建数组进行此操作
	auto baseType = static_cast<natRefPointer<Type::ArrayType>>(base->GetExprType());
	if (!baseType)
	{
		// TODO: 报告基础操作数不是内建数组
		return nullptr;
	}

	// TODO: 当前仅允许下标为内建整数类型
	const auto indexType = static_cast<natRefPointer<Type::BuiltinType>>(index->GetExprType());
	if (!indexType || !indexType->IsIntegerType())
	{
		// TODO: 报告下标操作数不具有内建整数类型
		return nullptr;
	}

	return make_ref<Expression::ArraySubscriptExpr>(base, index, baseType->GetElementType(), rloc);
}

NatsuLang::Expression::ExprPtr Sema::ActOnCallExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr func, SourceLocation lloc, Linq<NatsuLib::Valued<Expression::ExprPtr>> argExprs, SourceLocation rloc)
{
	// TODO: 完成重载部分

	if (!func)
	{
		return nullptr;
	}

	auto fn = static_cast<natRefPointer<Expression::DeclRefExpr>>(func->IgnoreParens());
	if (!fn)
	{
		return nullptr;
	}
	const auto refFn = fn->GetDecl();
	if (!refFn)
	{
		return nullptr;
	}
	const auto fnType = static_cast<natRefPointer<Type::FunctionType>>(refFn->GetValueType());
	if (!fnType)
	{
		return nullptr;
	}

	return make_ref<Expression::CallExpr>(std::move(fn), argExprs, fnType->GetResultType(), rloc);
}

NatsuLang::Expression::ExprPtr Sema::ActOnMemberAccessExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation periodLoc, natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id)
{
	auto baseType = base->GetExprType();

	LookupResult r{ *this, id, {}, LookupNameType::LookupMemberName };
	const auto record = static_cast<natRefPointer<Type::RecordType>>(baseType);
	if (record)
	{
		const auto recordDecl = record->GetDecl();

		Declaration::DeclContext* dc = recordDecl.Get();
		if (nns)
		{
			dc = nns->GetAsDeclContext(m_Context);
		}

		// TODO: 对dc的合法性进行检查

		LookupQualifiedName(r, dc);

		if (r.IsEmpty())
		{
			// TODO: 找不到这个成员
		}

		return BuildMemberReferenceExpr(scope, std::move(base), std::move(baseType), periodLoc, nns, r);
	}

	// TODO: 暂时不支持对RecordType以外的类型进行成员访问操作
	return nullptr;
}

NatsuLang::Expression::ExprPtr Sema::ActOnUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr operand)
{
	// TODO: 为将来可能的操作符重载保留
	static_cast<void>(scope);
	return CreateBuiltinUnaryOp(loc, getUnaryOperationType(tokenType), std::move(operand));
}

NatsuLang::Expression::ExprPtr Sema::ActOnPostfixUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr operand)
{
	// TODO: 为将来可能的操作符重载保留
	static_cast<void>(scope);

	assert(tokenType == Lex::TokenType::PlusPlus || tokenType == Lex::TokenType::MinusMinus);
	return CreateBuiltinUnaryOp(loc,
		tokenType == Lex::TokenType::PlusPlus ?
			Expression::UnaryOperationType::PostInc :
			Expression::UnaryOperationType::PostDec,
		std::move(operand));
}

NatsuLang::Expression::ExprPtr Sema::ActOnBinaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
{
	static_cast<void>(scope);
	return BuildBuiltinBinaryOp(loc, getBinaryOperationType(tokenType), std::move(leftOperand), std::move(rightOperand));
}

NatsuLang::Expression::ExprPtr Sema::BuildBuiltinBinaryOp(SourceLocation loc, Expression::BinaryOperationType binOpType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
{
	if (!leftOperand || !rightOperand)
	{
		return nullptr;
	}

	switch (binOpType)
	{
	case Expression::BinaryOperationType::Invalid:
	default:
		// TODO: 报告错误
		return nullptr;
	case Expression::BinaryOperationType::Mul:
	case Expression::BinaryOperationType::Div:
	case Expression::BinaryOperationType::Mod:
	case Expression::BinaryOperationType::Add:
	case Expression::BinaryOperationType::Sub:
	case Expression::BinaryOperationType::Shl:
	case Expression::BinaryOperationType::Shr:
	case Expression::BinaryOperationType::And:
	case Expression::BinaryOperationType::Xor:
	case Expression::BinaryOperationType::Or:
	{
		auto resultType = UsualArithmeticConversions(leftOperand, rightOperand);
		return make_ref<Expression::BinaryOperator>(std::move(leftOperand), std::move(rightOperand), binOpType, std::move(resultType), loc);
	}
	case Expression::BinaryOperationType::LT:
	case Expression::BinaryOperationType::GT:
	case Expression::BinaryOperationType::LE:
	case Expression::BinaryOperationType::GE:
	case Expression::BinaryOperationType::EQ:
	case Expression::BinaryOperationType::NE:
	case Expression::BinaryOperationType::LAnd:
	case Expression::BinaryOperationType::LOr:
	{
		auto resultType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
		return make_ref<Expression::BinaryOperator>(std::move(leftOperand), std::move(rightOperand), binOpType, std::move(resultType), loc);
	}
	case Expression::BinaryOperationType::Assign:
	case Expression::BinaryOperationType::MulAssign:
	case Expression::BinaryOperationType::DivAssign:
	case Expression::BinaryOperationType::RemAssign:
	case Expression::BinaryOperationType::AddAssign:
	case Expression::BinaryOperationType::SubAssign:
	case Expression::BinaryOperationType::ShlAssign:
	case Expression::BinaryOperationType::ShrAssign:
	case Expression::BinaryOperationType::AndAssign:
	case Expression::BinaryOperationType::XorAssign:
	case Expression::BinaryOperationType::OrAssign:
		const auto builtinLHSType = static_cast<natRefPointer<Type::BuiltinType>>(leftOperand->GetExprType()),
			builtinRHSType = static_cast<natRefPointer<Type::BuiltinType>>(rightOperand->GetExprType());

		Expression::CastType castType;
		if (builtinLHSType->IsIntegerType())
		{
			castType = builtinRHSType->IsIntegerType() ?
				Expression::CastType::IntegralCast : Expression::CastType::FloatingToIntegral;
		}
		else
		{
			castType = builtinRHSType->IsIntegerType() ?
				Expression::CastType::IntegralToFloating : Expression::CastType::FloatingCast;
		}

		auto leftType = leftOperand->GetExprType(), rightType = rightOperand->GetExprType();
		return make_ref<Expression::CompoundAssignOperator>(std::move(leftOperand), ImpCastExprToType(std::move(rightOperand), std::move(rightType), castType), binOpType, std::move(leftType), loc);
	}
}

NatsuLang::Expression::ExprPtr Sema::ActOnConditionalOp(SourceLocation questionLoc, SourceLocation colonLoc, Expression::ExprPtr condExpr, Expression::ExprPtr leftExpr, Expression::ExprPtr rightExpr)
{
	const auto leftType = leftExpr->GetExprType(), rightType = rightExpr->GetExprType();
	return make_ref<Expression::ConditionalOperator>(std::move(condExpr), questionLoc, std::move(leftExpr), colonLoc, std::move(rightExpr), getCommonType(leftType, rightType));
}

NatsuLang::Expression::ExprPtr Sema::BuildDeclarationNameExpr(natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, natRefPointer<Declaration::NamedDecl> decl)
{
	auto valueDecl = static_cast<natRefPointer<Declaration::ValueDecl>>(decl);
	if (!valueDecl)
	{
		// 错误，引用的不是值
		return nullptr;
	}

	auto type = valueDecl->GetValueType();
	return BuildDeclRefExpr(std::move(valueDecl), std::move(type), std::move(id), nns);
}

NatsuLang::Expression::ExprPtr Sema::BuildDeclRefExpr(natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type, Identifier::IdPtr id, natRefPointer<NestedNameSpecifier> const& nns)
{
	static_cast<void>(id);
	return make_ref<Expression::DeclRefExpr>(nns, std::move(decl), SourceLocation{}, std::move(type));
}

NatsuLang::Expression::ExprPtr Sema::BuildMemberReferenceExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr baseExpr, Type::TypePtr baseType, SourceLocation opLoc, natRefPointer<NestedNameSpecifier> const& nns, LookupResult& r)
{
	r.SetBaseObjectType(baseType);

	switch (r.GetResultType())
	{
	case LookupResult::LookupResultType::Found:
	{
		assert(r.GetDeclSize() == 1);
		auto decl = r.GetDecls().first();
		const auto type = decl->GetType();

		if (!baseExpr)
		{
			// 隐式成员访问

			if (type != Declaration::Decl::Field && type != Declaration::Decl::Method)
			{
				// 访问的是静态成员
				return BuildDeclarationNameExpr(nns, r.GetLookupId(), std::move(decl));
			}

			baseExpr = make_ref<Expression::ThisExpr>(SourceLocation{}, r.GetBaseObjectType(), true);
		}

		if (auto field = static_cast<natRefPointer<Declaration::FieldDecl>>(decl))
		{
			return BuildFieldReferenceExpr(std::move(baseExpr), opLoc, nns, std::move(field), r.GetLookupId());
		}

		if (auto var = static_cast<natRefPointer<Declaration::VarDecl>>(decl))
		{
			return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(var), r.GetLookupId(), var->GetValueType());
		}

		if (auto method = static_cast<natRefPointer<Declaration::MethodDecl>>(decl))
		{
			return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(method), r.GetLookupId(), method->GetValueType());
		}

		return nullptr;
	}
	case LookupResult::LookupResultType::FoundOverloaded:
		// TODO: 处理重载的情况
		nat_Throw(NotImplementedException);
	default:
		assert(!"Invalid result type.");
		[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::Ambiguous:
		// TODO: 报告错误
		return nullptr;
	}
}

NatsuLang::Expression::ExprPtr Sema::BuildFieldReferenceExpr(Expression::ExprPtr baseExpr, SourceLocation opLoc,
	natRefPointer<NestedNameSpecifier> const& nns, natRefPointer<Declaration::FieldDecl> field, Identifier::IdPtr id)
{
	static_cast<void>(nns);
	return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(field), std::move(id), field->GetValueType());
}

NatsuLang::Expression::ExprPtr Sema::CreateBuiltinUnaryOp(SourceLocation opLoc, Expression::UnaryOperationType opCode, Expression::ExprPtr operand)
{
	Type::TypePtr resultType;

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
	case Expression::UnaryOperationType::PostDec:
	case Expression::UnaryOperationType::PreInc:
	case Expression::UnaryOperationType::PreDec:
	case Expression::UnaryOperationType::Plus:
	case Expression::UnaryOperationType::Minus:
	case Expression::UnaryOperationType::Not:
		// TODO: 可能的整数提升？
		resultType = operand->GetExprType();
		break;
	case Expression::UnaryOperationType::LNot:
		resultType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
		break;
	case Expression::UnaryOperationType::Invalid:
	default:
		// TODO: 报告错误
		return nullptr;
	}

	return make_ref<Expression::UnaryOperator>(std::move(operand), opCode, std::move(resultType), opLoc);
}

NatsuLang::Type::TypePtr Sema::UsualArithmeticConversions(Expression::ExprPtr& leftOperand, Expression::ExprPtr& rightOperand)
{
	// TODO: 是否需要对左右操作数进行整数提升？

	auto leftType = static_cast<natRefPointer<Type::BuiltinType>>(leftOperand->GetExprType()), rightType = static_cast<natRefPointer<Type::BuiltinType>>(rightOperand->GetExprType());

	if (leftType == rightType)
	{
		return leftType;
	}

	if (leftType->IsFloatingType() || rightType->IsFloatingType())
	{
		return handleFloatConversion(leftOperand, std::move(leftType), rightOperand, std::move(rightType));
	}

	return handleIntegerConversion(leftOperand, std::move(leftType), rightOperand, std::move(rightType));
}

NatsuLang::Expression::ExprPtr Sema::ImpCastExprToType(Expression::ExprPtr expr, Type::TypePtr type, Expression::CastType castType)
{
	const auto exprType = expr->GetExprType();
	if (exprType == type)
	{
		return std::move(expr);
	}

	if (auto impCastExpr = static_cast<natRefPointer<Expression::ImplicitCastExpr>>(expr))
	{
		if (impCastExpr->GetCastType() == castType)
		{
			impCastExpr->SetExprType(std::move(type));
			return std::move(expr);
		}
	}

	return make_ref<Expression::ImplicitCastExpr>(std::move(type), castType, std::move(expr));
}

NatsuLang::Expression::CastType Sema::getCastType(Expression::ExprPtr const& operand, Type::TypePtr toType)
{
	toType = Type::Type::GetUnderlyingType(toType);
	auto fromType = Type::Type::GetUnderlyingType(operand->GetExprType());

	assert(operand && toType);
	assert(fromType);

	// TODO
	if (fromType->GetType() == Type::Type::Builtin)
	{
		const auto builtinFromType = static_cast<natRefPointer<Type::BuiltinType>>(fromType);

		switch (toType->GetType())
		{
		case Type::Type::Builtin:
			return getBuiltinCastType(fromType, toType);
		case Type::Type::Enum:
			if (builtinFromType->IsIntegerType())
			{
				return Expression::CastType::IntegralCast;
			}
			if (builtinFromType->IsFloatingType())
			{
				return Expression::CastType::FloatingToIntegral;
			}
			return Expression::CastType::Invalid;
		case Type::Type::Record:
			// TODO: 添加用户定义转换
			return Expression::CastType::Invalid;
		case Type::Type::Auto:
		case Type::Type::Array:
		case Type::Type::Function:
		case Type::Type::TypeOf:
		case Type::Type::Paren:
		default:
			return Expression::CastType::Invalid;
		}
	}

	// TODO
	nat_Throw(NotImplementedException);
}

NatsuLang::Type::TypePtr Sema::handleIntegerConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType, Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
{
	const auto builtinLHSType = static_cast<natRefPointer<Type::BuiltinType>>(leftOperandType), builtinRHSType = static_cast<natRefPointer<Type::BuiltinType>>(rightOperandType);

	if (!builtinLHSType || !builtinRHSType)
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (builtinLHSType == builtinRHSType)
	{
		// 无需转换
		return std::move(leftOperandType);
	}

	nInt compareResult;
	if (!builtinLHSType->CompareRankTo(builtinRHSType, compareResult))
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (!compareResult)
	{
		// 已经进行了相等性比较，只可能是其一为有符号，而另一为无符号
		if (builtinLHSType->IsSigned())
		{
			leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::IntegralCast);
			return std::move(rightOperandType);
		}

		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralCast);
		return std::move(leftOperandType);
	}

	if (compareResult > 0)
	{
		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralCast);
		return std::move(leftOperandType);
	}

	leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::IntegralCast);
	return std::move(rightOperandType);
}

NatsuLang::Type::TypePtr Sema::handleFloatConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType, Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
{
	auto builtinLHSType = static_cast<natRefPointer<Type::BuiltinType>>(leftOperandType), builtinRHSType = static_cast<natRefPointer<Type::BuiltinType>>(rightOperandType);

	if (!builtinLHSType || !builtinRHSType)
	{
		// TODO: 报告错误
		return nullptr;
	}

	const auto lhsFloat = builtinLHSType->IsFloatingType(), rhsFloat = builtinRHSType->IsFloatingType();

	if (lhsFloat && rhsFloat)
	{
		nInt compareResult;
		if (!builtinLHSType->CompareRankTo(builtinRHSType, compareResult))
		{
			// TODO: 报告错误
			return nullptr;
		}

		if (!compareResult)
		{
			// 无需转换
			return builtinLHSType;
		}

		if (compareResult > 0)
		{
			rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::FloatingCast);
			return std::move(leftOperandType);
		}

		leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::FloatingCast);
		return std::move(rightOperandType);
	}

	if (lhsFloat)
	{
		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralToFloating);
		return std::move(leftOperandType);
	}

	assert(rhsFloat);
	leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::IntegralToFloating);
	return std::move(rightOperandType);
}

LookupResult::LookupResult(Sema& sema, Identifier::IdPtr id, SourceLocation loc, Sema::LookupNameType lookupNameType)
	: m_Sema{ sema }, m_LookupId{ std::move(id) }, m_LookupLoc{ loc }, m_LookupNameType{ lookupNameType }, m_IDNS{ chooseIDNS(m_LookupNameType) }, m_Result{}, m_AmbiguousType{}
{
}

void LookupResult::AddDecl(natRefPointer<Declaration::NamedDecl> decl)
{
	m_Decls.emplace(std::move(decl));
	m_Result = LookupResultType::Found;
}

void LookupResult::AddDecl(Linq<NatsuLib::Valued<natRefPointer<Declaration::NamedDecl>>> decls)
{
	m_Decls.insert(decls.begin(), decls.end());
	m_Result = LookupResultType::Found;
}

void LookupResult::ResolveResultType() noexcept
{
	const auto size = m_Decls.size();

	if (size == 0)
	{
		m_Result = LookupResultType::NotFound;
		return;
	}
	
	if (size == 1)
	{
		m_Result = LookupResultType::Found;
		return;
	}

	// 若已经认定为二义性则不需要进一步修改
	if (m_Result == LookupResultType::Ambiguous)
	{
		return;
	}

	// 分析找到多个定义是由于重载还是二义性
	if (from(m_Decls).all([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			return static_cast<natRefPointer<Declaration::FunctionDecl>>(decl);
		}))
	{
		m_Result = LookupResultType::FoundOverloaded;
	}
	else
	{
		// TODO: 不严谨，需要进一步分析
		m_Result = LookupResultType::Ambiguous;
	}
}

Linq<NatsuLib::Valued<natRefPointer<NatsuLang::Declaration::NamedDecl>>> LookupResult::GetDecls() const noexcept
{
	return from(m_Decls);
}
