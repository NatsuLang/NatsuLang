#include "Sema/Sema.h"
#include "Sema/Scope.h"
#include "Sema/Declarator.h"
#include "Sema/DefaultActions.h"
#include "Lex/Preprocessor.h"
#include "Lex/LiteralParser.h"
#include "AST/Declaration.h"
#include "AST/ASTConsumer.h"
#include "AST/ASTContext.h"
#include "AST/Expression.h"
#include "Parse/Parser.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace Semantic;

namespace
{
	constexpr Declaration::IdentifierNamespace chooseIDNS(Sema::LookupNameType lookupNameType) noexcept
	{
		using Declaration::IdentifierNamespace;
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

	Expression::CastType getBuiltinCastType(natRefPointer<Type::BuiltinType> const& fromType, natRefPointer<Type::BuiltinType> const& toType) noexcept
	{
		using Expression::CastType;
		using Type::BuiltinType;

		if (!fromType || (!fromType->IsIntegerType() && !fromType->IsFloatingType()) ||
			!toType || (!toType->IsIntegerType() && !toType->IsFloatingType() && toType->GetBuiltinClass() != BuiltinType::Void))
		{
			return CastType::Invalid;
		}

		if (fromType->GetBuiltinClass() == toType->GetBuiltinClass())
		{
			return CastType::NoOp;
		}

		if (toType->GetBuiltinClass() == BuiltinType::Void)
		{
			return CastType::ToVoid;
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

	Type::TypePtr getCommonType(Type::TypePtr const& type1, Type::TypePtr const& type2)
	{
		if (type1 == type2)
		{
			return type1;
		}

		if (const auto builtinType1 = static_cast<natRefPointer<Type::BuiltinType>>(type1), builtinType2 = static_cast<natRefPointer<Type::BuiltinType>>(type2);
			builtinType1 && builtinType2)
		{
			if (builtinType1->IsFloatingType())
			{
				if (builtinType2->IsFloatingType())
				{
					nInt result;
					if (builtinType1->CompareRankTo(builtinType2, result))
					{
						return result > 0 ? builtinType1 : builtinType2;
					}
				}
				else
				{
					return builtinType1;
				}
			}
			else
			{
				if (builtinType2->IsFloatingType())
				{
					return builtinType2;
				}

				nInt result;
				if (builtinType1->CompareRankTo(builtinType2, result))
				{
					return result > 0 ? builtinType1 : builtinType2;
				}
			}
		}

		nat_Throw(NotImplementedException);
	}

	constexpr Expression::UnaryOperationType getUnaryOperationType(Lex::TokenType tokenType) noexcept
	{
		using Expression::UnaryOperationType;

		switch (tokenType)
		{
		case Lex::TokenType::Plus:
			return UnaryOperationType::Plus;
		case Lex::TokenType::PlusPlus:
			return UnaryOperationType::PreInc;
		case Lex::TokenType::Minus:
			return UnaryOperationType::Minus;
		case Lex::TokenType::MinusMinus:
			return UnaryOperationType::PreDec;
		case Lex::TokenType::Tilde:
			return UnaryOperationType::Not;
		case Lex::TokenType::Exclaim:
			return UnaryOperationType::LNot;
		default:
			assert(!"Invalid TokenType for UnaryOperationType.");
			return UnaryOperationType::Invalid;
		}
	}

	constexpr Expression::BinaryOperationType getBinaryOperationType(Lex::TokenType tokenType) noexcept
	{
		using Expression::BinaryOperationType;

		switch (tokenType)
		{
		case Lex::TokenType::Amp:
			return BinaryOperationType::And;
		case Lex::TokenType::AmpAmp:
			return BinaryOperationType::LAnd;
		case Lex::TokenType::AmpEqual:
			return BinaryOperationType::AndAssign;
		case Lex::TokenType::Star:
			return BinaryOperationType::Mul;
		case Lex::TokenType::StarEqual:
			return BinaryOperationType::MulAssign;
		case Lex::TokenType::Plus:
			return BinaryOperationType::Add;
		case Lex::TokenType::PlusEqual:
			return BinaryOperationType::AddAssign;
		case Lex::TokenType::Minus:
			return BinaryOperationType::Sub;
		case Lex::TokenType::MinusEqual:
			return BinaryOperationType::SubAssign;
		case Lex::TokenType::ExclaimEqual:
			return BinaryOperationType::NE;
		case Lex::TokenType::Slash:
			return BinaryOperationType::Div;
		case Lex::TokenType::SlashEqual:
			return BinaryOperationType::DivAssign;
		case Lex::TokenType::Percent:
			return BinaryOperationType::Mod;
		case Lex::TokenType::PercentEqual:
			return BinaryOperationType::RemAssign;
		case Lex::TokenType::Less:
			return BinaryOperationType::LT;
		case Lex::TokenType::LessLess:
			return BinaryOperationType::Shl;
		case Lex::TokenType::LessEqual:
			return BinaryOperationType::LE;
		case Lex::TokenType::LessLessEqual:
			return BinaryOperationType::ShlAssign;
		case Lex::TokenType::Greater:
			return BinaryOperationType::GT;
		case Lex::TokenType::GreaterGreater:
			return BinaryOperationType::Shr;
		case Lex::TokenType::GreaterEqual:
			return BinaryOperationType::GE;
		case Lex::TokenType::GreaterGreaterEqual:
			return BinaryOperationType::ShrAssign;
		case Lex::TokenType::Caret:
			return BinaryOperationType::Xor;
		case Lex::TokenType::CaretEqual:
			return BinaryOperationType::XorAssign;
		case Lex::TokenType::Pipe:
			return BinaryOperationType::Or;
		case Lex::TokenType::PipePipe:
			return BinaryOperationType::LOr;
		case Lex::TokenType::PipeEqual:
			return BinaryOperationType::OrAssign;
		case Lex::TokenType::Equal:
			return BinaryOperationType::Assign;
		case Lex::TokenType::EqualEqual:
			return BinaryOperationType::EQ;
		default:
			assert(!"Invalid TokenType for BinaryOperationType.");
			return BinaryOperationType::Invalid;
		}
	}
}

Sema::Sema(Preprocessor& preprocessor, ASTContext& astContext, natRefPointer<ASTConsumer> astConsumer)
	: m_Preprocessor{ preprocessor }, m_Context{ astContext }, m_Consumer{ std::move(astConsumer) }, m_Diag{ preprocessor.GetDiag() },
	  m_SourceManager{ preprocessor.GetSourceManager() }, m_CurrentPhase{ Phase::Phase1 }, m_TopLevelActionNamespace{ make_ref<CompilerActionNamespace>(u8""_nv) }
{
	prewarming();

	PushScope(ScopeFlags::DeclarableScope);
	ActOnTranslationUnitScope(m_CurrentScope);
}

Sema::~Sema()
{
}

std::vector<Declaration::DeclaratorPtr> const& Sema::GetCachedDeclarators() const noexcept
{
	return m_Declarators;
}

std::vector<Declaration::DeclaratorPtr> Sema::GetAndClearCachedDeclarators() noexcept
{
	return move(m_Declarators);
}

void Sema::ClearCachedDeclarations() noexcept
{
	m_Declarators.clear();
}

void Sema::ActOnPhaseDiverted()
{
	ClearCachedDeclarations();
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

	scope->AddDecl(std::move(decl));
}

void Sema::RemoveFromScopeChains(natRefPointer<Declaration::NamedDecl> const& decl, natRefPointer<Scope> const& scope, nBool removeFromContext)
{
	if (removeFromContext)
	{
		Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get())->RemoveDecl(decl);
	}

	scope->RemoveDecl(decl);
}

natRefPointer<CompilerActionNamespace> Sema::GetTopLevelActionNamespace() noexcept
{
	return m_TopLevelActionNamespace;
}

void Sema::ActOnTranslationUnitScope(natRefPointer<Scope> scope)
{
	m_TranslationUnitScope = std::move(scope);
	PushDeclContext(m_TranslationUnitScope, m_Context.GetTranslationUnit().Get());
}

natRefPointer<Declaration::Decl> Sema::ActOnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path)
{
	nat_Throw(NatsuLib::NotImplementedException);
}

Type::TypePtr Sema::GetTypeName(natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc, natRefPointer<Scope> scope, Type::TypePtr const& objectType)
{
	Declaration::DeclContext* context{};

	if (objectType && objectType->GetType() == Type::Type::Class)
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

Type::TypePtr Sema::BuildFunctionType(Type::TypePtr retType, Linq<Valued<Type::TypePtr>> const& paramType)
{
	return m_Context.GetFunctionType(from(paramType), std::move(retType));
}

Type::TypePtr Sema::CreateUnresolvedType(std::vector<Lex::Token> tokens)
{
	return m_Context.GetUnresolvedType(move(tokens));
}

Declaration::DeclPtr Sema::ActOnStartOfFunctionDef(natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr declarator)
{
	const auto parentScope = scope->GetParent();
	auto decl = static_cast<Declaration::DeclPtr>(HandleDeclarator(parentScope, declarator, declarator->GetDecl()));
	return ActOnStartOfFunctionDef(scope, std::move(decl));
}

Declaration::DeclPtr Sema::ActOnStartOfFunctionDef(natRefPointer<Scope> const& scope, Declaration::DeclPtr decl)
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

Declaration::DeclPtr Sema::ActOnFinishFunctionBody(Declaration::DeclPtr decl, Statement::StmtPtr body)
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
	auto found = false;
	const auto id = result.GetLookupId();
	const auto lookupType = result.GetLookupType();

	for (; scope; scope = scope->GetParent())
	{
		Linq<Valued<natRefPointer<Declaration::NamedDecl>>> query = scope->GetDecls().select([](natRefPointer<Declaration::NamedDecl> const& namedDecl)
		{
			return namedDecl;
		}).where([&id](natRefPointer<Declaration::NamedDecl> const& namedDecl)
		{
			return namedDecl && namedDecl->GetIdentifierInfo() == id;
		});

		switch (lookupType)
		{
		case LookupNameType::LookupTagName:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
			{
				return static_cast<natRefPointer<Declaration::TagDecl>>(decl);
			});
			break;
		case LookupNameType::LookupLabel:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
			{
				return static_cast<natRefPointer<Declaration::LabelDecl>>(decl);
			});
			break;
		case LookupNameType::LookupMemberName:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
			{
				const auto type = decl->GetType();
				return type == Declaration::Decl::Method || type == Declaration::Decl::Field;
			});
			break;
		case LookupNameType::LookupModuleName:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
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
			break;
		}
	}

	result.ResolveResultType();
	return found;
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
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
		{
			return static_cast<natRefPointer<Declaration::TagDecl>>(decl);
		});
		break;
	case LookupNameType::LookupLabel:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
		{
			return static_cast<natRefPointer<Declaration::LabelDecl>>(decl);
		});
		break;
	case LookupNameType::LookupMemberName:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
		{
			const auto type = decl->GetType();
			return type == Declaration::Decl::Method || type == Declaration::Decl::Field;
		});
		break;
	case LookupNameType::LookupModuleName:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
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

natRefPointer<Declaration::LabelDecl> Sema::LookupOrCreateLabel(Identifier::IdPtr id, SourceLocation loc)
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

Type::TypePtr Sema::ActOnTypeName(natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr const& decl)
{
	static_cast<void>(scope);
	return decl->GetType();
}

Type::TypePtr Sema::ActOnTypeOfType(natRefPointer<Expression::Expr> expr, Type::TypePtr underlyingType)
{
	return make_ref<Type::TypeOfType>(std::move(expr), std::move(underlyingType));
}

natRefPointer<Declaration::ParmVarDecl> Sema::ActOnParamDeclarator(natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl)
{
	auto id = decl->GetIdentifier();
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
		std::move(id), decl->GetType(), Specifier::StorageClass::None, decl->GetInitializer());

	scope->AddDecl(ret);

	return ret;
}

natRefPointer<Declaration::VarDecl> Sema::ActOnVariableDeclarator(
	natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc)
{
	auto type = Type::Type::GetUnderlyingType(decl->GetType());
	auto id = decl->GetIdentifier();
	auto initExpr = static_cast<natRefPointer<Expression::Expr>>(decl->GetInitializer());

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
	if (initExpr && initExpr->GetExprType() != type)
	{
		initExpr = ImpCastExprToType(std::move(initExpr), type, getCastType(initExpr, type));
	}

	auto varDecl = make_ref<Declaration::VarDecl>(Declaration::Decl::Var, dc, decl->GetRange().GetBegin(),
		SourceLocation{}, std::move(id), std::move(type), decl->GetStorageClass(), decl);

	varDecl->SetInitializer(initExpr);

	return varDecl;
}

natRefPointer<Declaration::FunctionDecl> Sema::ActOnFunctionDeclarator(
	natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc)
{
	auto id = decl->GetIdentifier();
	if (!id)
	{
		// TODO: 报告错误
		return nullptr;
	}

	// TODO: 处理自动推断函数返回类型的情况
	auto type = static_cast<natRefPointer<Type::FunctionType>>(decl->GetType());
	if (!type)
	{
		// TODO: 报告错误
		return nullptr;
	}

	auto funcDecl = make_ref<Declaration::FunctionDecl>(Declaration::Decl::Function, dc,
		SourceLocation{}, SourceLocation{}, std::move(id), std::move(type), decl->GetStorageClass(), decl);

	for (auto const& param : decl->GetParams())
	{
		param->SetContext(funcDecl.Get());
	}

	funcDecl->SetParams(from(decl->GetParams()));

	return funcDecl;
}

natRefPointer<Declaration::DeclaratorDecl> Sema::ActOnUnresolvedDeclarator(natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc)
{
	assert(m_CurrentPhase == Phase::Phase1);
	assert(!decl->GetType() && !decl->GetInitializer());

	auto id = decl->GetIdentifier();
	if (!id)
	{
		// TODO: 报告错误
		return nullptr;
	}

	auto unresolvedDecl = make_ref<Declaration::DeclaratorDecl>(Declaration::Decl::Declarator, dc, SourceLocation{}, std::move(id), nullptr, SourceLocation{}, decl);
	m_Declarators.emplace_back(std::move(decl));

	return unresolvedDecl;
}

natRefPointer<Declaration::NamedDecl> Sema::HandleDeclarator(natRefPointer<Scope> scope, Declaration::DeclaratorPtr decl, Declaration::DeclPtr const& oldUnresolvedDeclPtr)
{
	if (auto preparedDecl = decl->GetDecl(); preparedDecl && preparedDecl != oldUnresolvedDeclPtr)
	{
		return preparedDecl;
	}

	const auto id = decl->GetIdentifier();

	if (!id)
	{
		if (decl->GetContext() != Declaration::Context::Prototype)
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, decl->GetRange().GetBegin());
		}

		return nullptr;
	}

	while ((scope->GetFlags() & ScopeFlags::DeclarableScope) == ScopeFlags::None)
	{
		scope = scope->GetParent();
	}

	if (oldUnresolvedDeclPtr)
	{
		const auto declScope = decl->GetDeclarationScope();
		assert(declScope);
		RemoveFromScopeChains(oldUnresolvedDeclPtr, declScope);
	}

	LookupResult previous{ *this, id, {}, LookupNameType::LookupOrdinaryName };

	LookupName(previous, scope);

	if (previous.GetDeclSize())
	{
		// TODO: 处理重载或覆盖的情况
		return nullptr;
	}

	const auto dc = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());
	const auto type = decl->GetType();
	natRefPointer<Declaration::NamedDecl> retDecl;

	if (!type && !decl->GetInitializer() && m_CurrentPhase == Phase::Phase1)
	{
		retDecl = ActOnUnresolvedDeclarator(scope, decl, dc);
		decl->SetDeclarationScope(scope);
	}
	else if (auto funcType = static_cast<natRefPointer<Type::FunctionType>>(type))
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
		// TODO: 若存在错误则进行报告
	}

	decl->SetDecl(retDecl);

	return retDecl;
}

void Sema::ActOnStartOfClassMemberDeclarations(natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	// TODO
	nat_Throw(NotImplementedException);
}

Statement::StmtPtr Sema::ActOnNullStmt(SourceLocation loc)
{
	return make_ref<Statement::NullStmt>(loc);
}

Statement::StmtPtr Sema::ActOnDeclStmt(std::vector<Declaration::DeclPtr> decls, SourceLocation start,
	SourceLocation end)
{
	return make_ref<Statement::DeclStmt>(move(decls), start, end);
}

Statement::StmtPtr Sema::ActOnLabelStmt(SourceLocation labelLoc, natRefPointer<Declaration::LabelDecl> labelDecl,
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

Statement::StmtPtr Sema::ActOnCompoundStmt(std::vector<Statement::StmtPtr> stmtVec, SourceLocation begin,
	SourceLocation end)
{
	return make_ref<Statement::CompoundStmt>(move(stmtVec), begin, end);
}

Statement::StmtPtr Sema::ActOnIfStmt(SourceLocation ifLoc, Expression::ExprPtr condExpr,
	Statement::StmtPtr thenStmt, SourceLocation elseLoc, Statement::StmtPtr elseStmt)
{
	return make_ref<Statement::IfStmt>(ifLoc, ActOnConditionExpr(std::move(condExpr)), std::move(thenStmt), elseLoc, std::move(elseStmt));
}

Statement::StmtPtr Sema::ActOnWhileStmt(SourceLocation loc, Expression::ExprPtr cond,
	Statement::StmtPtr body)
{
	return make_ref<Statement::WhileStmt>(loc, ActOnConditionExpr(std::move(cond)), std::move(body));
}

Statement::StmtPtr Sema::ActOnForStmt(SourceLocation forLoc, SourceLocation leftParenLoc, Statement::StmtPtr init,
	Expression::ExprPtr cond, Expression::ExprPtr third, SourceLocation rightParenLoc, Statement::StmtPtr body)
{
	return make_ref<Statement::ForStmt>(std::move(init), ActOnConditionExpr(std::move(cond)), std::move(third), std::move(body), forLoc, leftParenLoc, rightParenLoc);
}

Statement::StmtPtr Sema::ActOnContinueStmt(SourceLocation loc, natRefPointer<Scope> const& scope)
{
	if (!scope->GetContinueParent())
	{
		// TODO: 报告错误：当前作用域不可 continue
		return nullptr;
	}

	return make_ref<Statement::ContinueStmt>(loc);
}

Statement::StmtPtr Sema::ActOnBreakStmt(SourceLocation loc, natRefPointer<Scope> const& scope)
{
	if (!scope->GetBreakParent())
	{
		// TODO: 报告错误：当前作用域不可 break
		return nullptr;
	}

	return make_ref<Statement::BreakStmt>(loc);
}

Statement::StmtPtr Sema::ActOnReturnStmt(SourceLocation loc, Expression::ExprPtr returnedExpr, natRefPointer<Scope> const& scope)
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

Statement::StmtPtr Sema::ActOnExprStmt(Expression::ExprPtr expr)
{
	return expr;
}

Expression::ExprPtr Sema::ActOnBooleanLiteral(Lex::Token const& token) const
{
	assert(token.IsAnyOf({ Lex::TokenType::Kw_true, Lex::TokenType::Kw_false }));
	return make_ref<Expression::BooleanLiteral>(token.Is(Lex::TokenType::Kw_true), m_Context.GetBuiltinType(Type::BuiltinType::Bool), token.GetLocation());
}

Expression::ExprPtr Sema::ActOnNumericLiteral(Lex::Token const& token) const
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
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::WarnOverflowed, token.GetLocation());
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
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::WarnOverflowed, token.GetLocation());
	}

	return make_ref<Expression::IntegerLiteral>(value, std::move(type), token.GetLocation());
}

Expression::ExprPtr Sema::ActOnCharLiteral(Lex::Token const& token) const
{
	assert(token.Is(Lex::TokenType::CharLiteral) && token.GetLiteralContent().has_value());

	Lex::CharLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	return make_ref<Expression::CharacterLiteral>(literalParser.GetValue(), nString::UsingStringType, m_Context.GetBuiltinType(Type::BuiltinType::Char), token.GetLocation());
}

Expression::ExprPtr Sema::ActOnStringLiteral(Lex::Token const& token)
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

Expression::ExprPtr Sema::ActOnConditionExpr(Expression::ExprPtr expr)
{
	if (!expr)
	{
		return nullptr;
	}

	const auto boolType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
	const auto castType = getCastType(expr, boolType);
	return ImpCastExprToType(std::move(expr), std::move(boolType), castType);
}

Expression::ExprPtr Sema::ActOnThrow(natRefPointer<Scope> const& scope, SourceLocation loc, Expression::ExprPtr expr)
{
	// TODO
	if (expr)
	{

	}

	nat_Throw(NotImplementedException);
}

Expression::ExprPtr Sema::ActOnIdExpr(natRefPointer<Scope> const& scope, natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, nBool hasTraillingLParen, natRefPointer<Syntax::ResolveContext> const& resolveContext)
{
	LookupResult result{ *this, id, {}, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, scope, nns))
	{
		switch (result.GetResultType())
		{
		case LookupResult::LookupResultType::NotFound:
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrUndefinedIdentifier).AddArgument(id);
			break;
		case LookupResult::LookupResultType::Ambiguous:
			// TODO: 报告二义性
			break;
		default:
			assert(!"Invalid result type.");
			break;
		}
		return nullptr;
	}

	// TODO: 对以函数调用形式引用的标识符采取特殊的处理
	static_cast<void>(hasTraillingLParen);

	const auto size = result.GetDeclSize();
	if (size == 1)
	{
		auto decl = result.GetDecls().first();
		if (m_CurrentPhase == Phase::Phase2)
		{
			if (const auto declaratorDecl = static_cast<natRefPointer<Declaration::DeclaratorDecl>>(decl))
			{
				const auto declarator = declaratorDecl->GetDeclaratorPtr().Lock();
				if (declarator && declarator->IsUnresolved())
				{
					if (resolveContext)
					{
						const auto resolvingState = resolveContext->GetDeclaratorResolvingState(declarator);
						switch (resolvingState)
						{
						default:
							assert(!"Invalid resolvingState");
						case Syntax::ResolveContext::ResolvingState::Unknown:
						{
							const auto oldUnresolvedDecl = declarator->GetDecl();
							const auto declarationScope = declarator->GetDeclarationScope();
							resolveContext->GetParser().ResolveDeclarator(declarator);
							decl = HandleDeclarator(declarationScope, declarator, oldUnresolvedDecl);
							break;
						}
						case Syntax::ResolveContext::ResolvingState::Resolving:
							// TODO: 报告环形依赖
							break;
						case Syntax::ResolveContext::ResolvingState::Resolved:
							assert(!"Should never happen.");
							break;
						}
					}
					else
					{
						// TODO: 报告错误
					}
				}
			}
		}

		return BuildDeclarationNameExpr(nns, std::move(id), decl);
	}

	// TODO: 只有重载函数可以在此找到多个声明，否则报错
	nat_Throw(NotImplementedException);
}

Expression::ExprPtr Sema::ActOnThis(SourceLocation loc)
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
		auto recordDecl = Declaration::Decl::CastFromDeclContext(methodDecl->GetContext())->ForkRef<Declaration::ClassDecl>();
		assert(recordDecl);
		return make_ref<Expression::ThisExpr>(loc, recordDecl->GetTypeForDecl(), false);
	}

	// TODO: 报告当前上下文不允许使用 this 的错误
	return nullptr;
}

Expression::ExprPtr Sema::ActOnAsTypeExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr exprToCast, Type::TypePtr type, SourceLocation loc)
{
	return make_ref<Expression::AsTypeExpr>(std::move(type), getCastType(exprToCast, type), std::move(exprToCast));
}

Expression::ExprPtr Sema::ActOnArraySubscriptExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation lloc, Expression::ExprPtr index, SourceLocation rloc)
{
	// TODO: 屏蔽未使用参数警告，这些参数将会在将来的版本被使用
	static_cast<void>(scope);
	static_cast<void>(lloc);

	// TODO: 当前仅支持对内建数组进行此操作
	const auto baseType = static_cast<natRefPointer<Type::ArrayType>>(base->GetExprType());
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

Expression::ExprPtr Sema::ActOnCallExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr func, SourceLocation lloc, Linq<Valued<Expression::ExprPtr>> argExprs, SourceLocation rloc)
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

	// TODO: 处理有默认参数的情况
	return make_ref<Expression::CallExpr>(std::move(fn), argExprs.zip(fnType->GetParameterTypes()).select([this](std::pair<Expression::ExprPtr, Type::TypePtr> const& pair)
	{
		return ImpCastExprToType(pair.first, pair.second, getCastType(pair.first, pair.second));
	}), fnType->GetResultType(), rloc);
}

Expression::ExprPtr Sema::ActOnMemberAccessExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base, SourceLocation periodLoc, natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id)
{
	auto baseType = base->GetExprType();

	LookupResult r{ *this, id, {}, LookupNameType::LookupMemberName };
	const auto record = static_cast<natRefPointer<Type::ClassType>>(baseType);
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

	// TODO: 暂时不支持对ClassType以外的类型进行成员访问操作
	return nullptr;
}

Expression::ExprPtr Sema::ActOnUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr operand)
{
	// TODO: 为将来可能的操作符重载保留
	static_cast<void>(scope);
	return CreateBuiltinUnaryOp(loc, getUnaryOperationType(tokenType), std::move(operand));
}

Expression::ExprPtr Sema::ActOnPostfixUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr operand)
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

Expression::ExprPtr Sema::ActOnBinaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
{
	static_cast<void>(scope);
	return BuildBuiltinBinaryOp(loc, getBinaryOperationType(tokenType), std::move(leftOperand), std::move(rightOperand));
}

Expression::ExprPtr Sema::BuildBuiltinBinaryOp(SourceLocation loc, Expression::BinaryOperationType binOpType, Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
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

		return make_ref<Expression::CompoundAssignOperator>(std::move(leftOperand), ImpCastExprToType(std::move(rightOperand), builtinLHSType, castType), binOpType, builtinLHSType, loc);
	}
}

Expression::ExprPtr Sema::ActOnConditionalOp(SourceLocation questionLoc, SourceLocation colonLoc, Expression::ExprPtr condExpr, Expression::ExprPtr leftExpr, Expression::ExprPtr rightExpr)
{
	const auto leftType = leftExpr->GetExprType(), rightType = rightExpr->GetExprType(), commonType = getCommonType(leftType, rightType);
	return make_ref<Expression::ConditionalOperator>(std::move(condExpr), questionLoc,
		ImpCastExprToType(leftExpr, commonType, getCastType(leftExpr, commonType)), colonLoc,
		ImpCastExprToType(rightExpr, commonType, getCastType(rightExpr, commonType)), commonType);
}

Expression::ExprPtr Sema::BuildDeclarationNameExpr(natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, natRefPointer<Declaration::NamedDecl> decl)
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

Expression::ExprPtr Sema::BuildDeclRefExpr(natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type, Identifier::IdPtr id, natRefPointer<NestedNameSpecifier> const& nns)
{
	static_cast<void>(id);
	return make_ref<Expression::DeclRefExpr>(nns, std::move(decl), SourceLocation{}, std::move(type));
}

Expression::ExprPtr Sema::BuildMemberReferenceExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr baseExpr, Type::TypePtr baseType, SourceLocation opLoc, natRefPointer<NestedNameSpecifier> const& nns, LookupResult& r)
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

Expression::ExprPtr Sema::BuildFieldReferenceExpr(Expression::ExprPtr baseExpr, SourceLocation opLoc,
	natRefPointer<NestedNameSpecifier> const& nns, natRefPointer<Declaration::FieldDecl> field, Identifier::IdPtr id)
{
	static_cast<void>(nns);
	return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(field), std::move(id), field->GetValueType());
}

Expression::ExprPtr Sema::CreateBuiltinUnaryOp(SourceLocation opLoc, Expression::UnaryOperationType opCode, Expression::ExprPtr operand)
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

Type::TypePtr Sema::UsualArithmeticConversions(Expression::ExprPtr& leftOperand, Expression::ExprPtr& rightOperand)
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

Expression::ExprPtr Sema::ImpCastExprToType(Expression::ExprPtr expr, Type::TypePtr type, Expression::CastType castType)
{
	const auto exprType = expr->GetExprType();
	if (exprType == type || castType == Expression::CastType::NoOp)
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

void Sema::prewarming()
{
	m_TopLevelActionNamespace->RegisterSubNamespace(u8"Compiler"_nv);
	const auto compilerNamespace = m_TopLevelActionNamespace->GetSubNamespace(u8"Compiler"_nv);
	assert(compilerNamespace);
	compilerNamespace->RegisterAction(make_ref<ActionDump>());
	compilerNamespace->RegisterAction(make_ref<ActionDumpIf>());
	compilerNamespace->RegisterAction(make_ref<ActionIsDefined>());
}

Expression::CastType Sema::getCastType(Expression::ExprPtr const& operand, Type::TypePtr toType)
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
		case Type::Type::Class:
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

Type::TypePtr Sema::handleIntegerConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType, Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
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

Type::TypePtr Sema::handleFloatConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType, Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
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

void LookupResult::AddDecl(Linq<Valued<natRefPointer<Declaration::NamedDecl>>> decls)
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

Linq<Valued<natRefPointer<Declaration::NamedDecl>>> LookupResult::GetDecls() const noexcept
{
	return from(m_Decls);
}
