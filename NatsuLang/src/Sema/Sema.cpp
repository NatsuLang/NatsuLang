#include "Sema/Sema.h"
#include "Sema/Scope.h"
#include "Sema/Declarator.h"
#include "Lex/Preprocessor.h"
#include "Lex/LiteralParser.h"
#include "AST/Declaration.h"
#include "AST/ASTContext.h"
#include "AST/Expression.h"

using namespace NatsuLib;
using namespace NatsuLang::Semantic;

namespace
{
	constexpr NatsuLang::Declaration::IdentifierNamespace ChooseIDNS(Sema::LookupNameType lookupNameType) noexcept
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
}

Sema::Sema(Preprocessor& preprocessor, ASTContext& astContext)
	: m_Preprocessor{ preprocessor }, m_Context{ astContext }, m_Diag{ preprocessor.GetDiag() }, m_SourceManager{ preprocessor.GetSourceManager() }
{
}

Sema::~Sema()
{
}

natRefPointer<NatsuLang::Declaration::Decl> Sema::OnModuleImport(SourceLocation startLoc, SourceLocation importLoc, ModulePathType const& path)
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

nBool Sema::LookupName(LookupResult& result, natRefPointer<Scope> scope) const
{
	for (; scope; scope = scope->GetParent().Lock())
	{
		const auto context = scope->GetEntity();
		if (LookupQualifiedName(result, context))
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
		auto dc = nns->GetAsDeclContext(m_Context);
		return LookupQualifiedName(result, dc);
	}

	return LookupName(result, scope);
}

NatsuLang::Type::TypePtr Sema::ActOnTypeName(natRefPointer<Scope> const& scope, Declaration::Declarator const& decl)
{

}

natRefPointer<NatsuLang::Declaration::ParmVarDecl> Sema::ActOnParamDeclarator(natRefPointer<Scope> const& scope, Declaration::Declarator const& decl)
{
	
}

natRefPointer<NatsuLang::Declaration::NamedDecl> Sema::HandleDeclarator(natRefPointer<Scope> const& scope, Declaration::Declarator const& decl)
{
	auto id = decl.GetIdentifier();

	if (decl.GetContext() != Declaration::Context::Prototype && !id)
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, decl.GetRange().GetBegin());
		return nullptr;
	}


}

NatsuLang::Expression::ExprPtr Sema::ActOnBooleanLiteral(Token::Token const& token) const
{
	assert(token.IsAnyOf({ Token::TokenType::Kw_true, Token::TokenType::Kw_false }));
	return make_ref<Expression::BooleanLiteral>(token.Is(Token::TokenType::Kw_true), m_Context.GetBuiltinType(Type::BuiltinType::Bool), token.GetLocation());
}

NatsuLang::Expression::ExprPtr Sema::ActOnNumericLiteral(Token::Token const& token) const
{
	assert(token.Is(Token::TokenType::NumericLiteral));

	Lex::NumericLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	Expression::ExprPtr result{};

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

NatsuLang::Expression::ExprPtr Sema::ActOnCharLiteral(Token::Token const& token) const
{

}

NatsuLang::Expression::ExprPtr Sema::ActOnThrow(natRefPointer<Scope> const& scope, SourceLocation loc, Expression::ExprPtr expr)
{
	if (expr)
	{

	}
}

NatsuLang::Expression::ExprPtr Sema::ActOnIdExpression(natRefPointer<Scope> const& scope, natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, nBool hasTraillingLParen)
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

NatsuLang::Expression::ExprPtr Sema::BuildDeclarationNameExpr(natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id, natRefPointer<Declaration::NamedDecl> decl)
{
	auto valueDecl = static_cast<natRefPointer<Declaration::ValueDecl>>(decl);
	if (!valueDecl)
	{
		// 错误，引用的不是值
		return nullptr;
	}

	return BuildDeclRefExpr(std::move(valueDecl), valueDecl->GetValueType(), std::move(id), nns);
}

NatsuLang::Expression::ExprPtr Sema::BuildDeclRefExpr(natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type, Identifier::IdPtr id, natRefPointer<NestedNameSpecifier> const& nns)
{
	static_cast<void>(id);
	return make_ref<Expression::DeclRefExpr>(nns, std::move(decl), SourceLocation{}, std::move(type));
}

LookupResult::LookupResult(Sema& sema, Identifier::IdPtr id, SourceLocation loc, Sema::LookupNameType lookupNameType)
	: m_Sema{ sema }, m_LookupId{ std::move(id) }, m_LookupLoc{ loc }, m_LookupNameType{ lookupNameType }, m_IDNS{ ChooseIDNS(m_LookupNameType) }, m_Result{}, m_AmbiguousType{}
{
}

void LookupResult::AddDecl(natRefPointer<Declaration::NamedDecl> decl)
{
	m_Decls.emplace(std::move(decl));
	m_Result = LookupResultType::Found;
}

void LookupResult::AddDecl(Linq<natRefPointer<Declaration::NamedDecl>> decls)
{
	m_Decls.insert(decls.begin(), decls.begin());
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

Linq<natRefPointer<const NatsuLang::Declaration::NamedDecl>> LookupResult::GetDecls() const noexcept
{
	return from(m_Decls);
}
