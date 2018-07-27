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

#undef max
#undef min

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

	Expression::CastType getBuiltinCastType(natRefPointer<Type::BuiltinType> const& fromType,
	                                        natRefPointer<Type::BuiltinType> const& toType) noexcept
	{
		using Expression::CastType;
		using Type::BuiltinType;

		if (!fromType || (!fromType->IsIntegerType() && !fromType->IsFloatingType()) ||
			!toType || (!toType->IsIntegerType() && !toType->IsFloatingType() && toType->GetBuiltinClass() != BuiltinType::Void) ||
			fromType->GetBuiltinClass() == BuiltinType::Null || toType->GetBuiltinClass() == BuiltinType::Null)
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

		if (const auto builtinType1 = type1.Cast<Type::BuiltinType>(), builtinType2 = type2.Cast<Type::BuiltinType>();
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
				else if (builtinType2->IsIntegerType())
				{
					return builtinType1;
				}
				else
				{
					// TODO: 报告错误：无法取得公共类型
					return nullptr;
				}
			}
			else if (builtinType1->IsIntegerType())
			{
				if (builtinType2->IsFloatingType())
				{
					return builtinType2;
				}

				if (builtinType2->IsIntegerType())
				{
					nInt result;
					if (builtinType1->CompareRankTo(builtinType2, result))
					{
						return result > 0 ? builtinType1 : builtinType2;
					}
				}
				else
				{
					// TODO: 报告错误：无法取得公共类型
					return nullptr;
				}
			}
			else
			{
				// TODO: 报告错误：无法取得公共类型
				return nullptr;
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
		case Lex::TokenType::Amp:
			return UnaryOperationType::AddrOf;
		case Lex::TokenType::Star:
			return UnaryOperationType::Deref;
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
			return BinaryOperationType::Rem;
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

	constexpr nBool IsUnsafeOperation(Expression::UnaryOperationType opCode)
	{
		return opCode == Expression::UnaryOperationType::AddrOf || opCode == Expression::UnaryOperationType::Deref;
	}

	class DefaultNameBuilder
		: public natRefObjImpl<DefaultNameBuilder, INameBuilder>
	{
	public:
		nString GetQualifiedName(natRefPointer<Declaration::NamedDecl> const& namedDecl) override
		{
			nString name = namedDecl->GetName();
			auto parent = namedDecl->GetContext();
			while (parent)
			{
				const auto parentDecl = Declaration::Decl::CastFromDeclContext(parent);
				if (const auto parentNamedDecl = dynamic_cast<Declaration::NamedDecl*>(parentDecl))
				{
					name = parentNamedDecl->GetName() + (u8"."_nv + name);
					parent = parentNamedDecl->GetContext();
				}
				else if (dynamic_cast<Declaration::TranslationUnitDecl*>(parentDecl) || dynamic_cast<Declaration::FunctionDecl*>(parent))
				{
					// 翻译单元即顶层声明上下文，函数内部的定义不具有链接性，到此结束即可
					break;
				}
				else
				{
					nat_Throw(natErrException, NatErr::NatErr_InternalErr, u8"Parent is not a namedDecl."_nv);
				}
			}

			return name;
		}

		nString GetTypeName(Type::TypePtr const& type) override
		{
			switch (type->GetType())
			{
			case Type::Type::Builtin:
				return type.UnsafeCast<Type::BuiltinType>()->GetName();
			case Type::Type::Pointer:
			{
				const auto pointerType = type.UnsafeCast<Type::PointerType>();
				const auto pointeeType = Type::Type::GetUnderlyingType(pointerType->GetPointeeType());
				auto pointeeTypeName = GetTypeName(pointeeType);
				mayAddParen(pointeeTypeName, pointeeType);
				pointeeTypeName.Append(u8'*');
				return pointeeTypeName;
			}
			case Type::Type::Array:
			{
				const auto arrayType = type.UnsafeCast<Type::ArrayType>();
				// 不考虑元素为函数的情况，因为不允许元素为函数
				const auto elementType = Type::Type::GetUnderlyingType(arrayType->GetElementType());
				return natUtil::FormatString(u8"{0}[{1}]"_nv, GetTypeName(elementType), arrayType->GetSize());
			}
			case Type::Type::Function:
			{
				const auto functionType = type.UnsafeCast<Type::FunctionType>();
				const auto retType = Type::Type::GetUnderlyingType(functionType->GetResultType());
				auto paramTypeName = functionType->GetParameterTypes().aggregate(u8""_ns, [this](nString const& str, Type::TypePtr const& paramType)
				{
					return str + GetTypeName(Type::Type::GetUnderlyingType(paramType)) + u8", "_nv;
				});

				if (functionType->GetParameterCount())
				{
					// 去除多余的 ", "
					paramTypeName.pop_back(2);
				}

				return natUtil::FormatString(u8"({0}) -> {1}"_nv, paramTypeName, GetTypeName(retType));
			}
			case Type::Type::Class:
			case Type::Type::Enum:
				return GetQualifiedName(type.UnsafeCast<Type::TagType>()->GetDecl());
			default:
				nat_Throw(natErrException, NatErr::NatErr_InvalidArg, u8"Invalid type."_nv);
			}
		}

	private:
		static void mayAddParen(nString& str, Type::TypePtr const& innerType)
		{
			if (innerType->GetType() == Type::Type::Function)
			{
				str = u8"("_nv + str + u8")"_nv;
			}
		}
	};
}

namespace NatsuLang
{
	IAttributeSerializer::~IAttributeSerializer()
	{
	}

	INameBuilder::~INameBuilder()
	{
	}

	class ImportedAttribute
		: public natRefObjImpl<ImportedAttribute, Declaration::IAttribute>
	{
	public:
		nStrView GetName() const noexcept override
		{
			return u8"Imported"_nv;
		}
	};

	class ImportedAttributeSerializer
		: public natRefObjImpl<ImportedAttributeSerializer, IAttributeSerializer>
	{
	public:
		explicit ImportedAttributeSerializer(Sema& sema)
			: m_Sema{ sema }
		{
		}

		void Serialize(natRefPointer<Declaration::IAttribute> const& /*attribute*/,
		               natRefPointer<ISerializationArchiveWriter> const& /*writer*/) override
		{
		}

		natRefPointer<Declaration::IAttribute> Deserialize(natRefPointer<ISerializationArchiveReader> const& /*reader*/) override
		{
			return m_Sema.GetImportedAttribute();
		}

	private:
		Sema& m_Sema;
	};
}

Sema::Sema(Preprocessor& preprocessor, ASTContext& astContext, natRefPointer<ASTConsumer> astConsumer)
	: m_Preprocessor{ preprocessor }, m_Context{ astContext }, m_Consumer{ std::move(astConsumer) },
	  m_Diag{ preprocessor.GetDiag() },
	  m_SourceManager{ preprocessor.GetSourceManager() },
	  m_TopLevelActionNamespace{ make_ref<CompilerActionNamespace>(u8""_nv) },
	  m_CurrentPhase{ Phase::Phase1 }
{
	prewarming();

	PushScope(ScopeFlags::DeclarableScope);
	ActOnTranslationUnitScope(m_CurrentScope);
}

Sema::~Sema()
{
}

void Sema::UseDefaultNameBuilder()
{
	m_NameBuilder = make_ref<DefaultNameBuilder>();
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
	auto declPtr = Declaration::Decl::CastFromDeclContext(dc)->ForkRef();
	assert(declPtr->GetContext() == Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()));
	m_CurrentDeclContext = std::move(declPtr);
	scope->SetEntity(dc);
}

void Sema::PopDeclContext()
{
	assert(m_CurrentDeclContext);
	const auto parentDc = m_CurrentDeclContext->GetContext();
	assert(parentDc);
	m_CurrentDeclContext = Declaration::Decl::CastFromDeclContext(parentDc)->ForkRef();
}

Metadata Sema::CreateMetadata(nBool includeImported) const
{
	Metadata metadata;
	const auto tu = m_Context.GetTranslationUnit();
	const auto decls = tu->GetDecls();
	if (includeImported)
	{
		metadata.AddDecls(decls.select([](Declaration::DeclPtr const& decl)
		{
			decl->DetachAttributes(typeid(ImportedAttribute));
			return decl;
		}));
	}
	else
	{
		metadata.AddDecls(decls.where([](Declaration::DeclPtr const& decl)
		{
			return !decl->GetAttributeCount(typeid(ImportedAttribute));
		}));
	}

	return metadata;
}

void Sema::LoadMetadata(Metadata const& metadata, nBool feedAstConsumer)
{
	for (const auto& decl : metadata.GetDecls())
	{
		if (auto namedDecl = decl.Cast<Declaration::NamedDecl>())
		{
			MarkAsImported(decl);
			PushOnScopeChains(std::move(namedDecl), m_TranslationUnitScope);
		}
	}

	if (feedAstConsumer && m_Consumer)
	{
		m_Consumer->HandleTopLevelDecl(metadata.GetDecls());
	}
}

void Sema::MarkAsImported(Declaration::DeclPtr const& decl) const
{
	decl->AttachAttribute(m_ImportedAttribute);
}

void Sema::UnmarkImported(Declaration::DeclPtr const& decl) const
{
	decl->DetachAttributes(typeid(ImportedAttribute));
}

nBool Sema::IsImported(Declaration::DeclPtr const& decl) const
{
	return decl->GetAttributeCount(typeid(ImportedAttribute));
}

natRefPointer<ImportedAttribute> Sema::GetImportedAttribute() const noexcept
{
	return m_ImportedAttribute;
}

void Sema::SetDeclContext(Declaration::DeclPtr dc) noexcept
{
	m_CurrentDeclContext = std::move(dc);
}

Declaration::DeclPtr Sema::GetDeclContext() const noexcept
{
	return m_CurrentDeclContext;
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

void Sema::PushOnScopeChains(natRefPointer<Declaration::NamedDecl> decl, natRefPointer<Scope> const& scope,
                             nBool addToContext)
{
	assert(decl);

	// 处理覆盖声明的情况

	if (addToContext)
	{
		Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get())->AddDecl(decl);
	}

	scope->AddDecl(std::move(decl));
}

void Sema::RemoveFromScopeChains(natRefPointer<Declaration::NamedDecl> const& decl, natRefPointer<Scope> const& scope,
                                 nBool removeFromContext)
{
	if (removeFromContext)
	{
		Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get())->RemoveDecl(decl);
	}

	scope->RemoveDecl(decl);
}

void Sema::ActOnCodeComplete(natRefPointer<Scope> const& scope, SourceLocation loc,
                             natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr const& id,
                             Declaration::Context context)
{
	if (!m_CodeCompleter)
	{
		// TODO: 报告错误
		return;
	}

	LookupResult r{ *this, id, loc, LookupNameType::LookupAnyName, true };
	LookupNestedName(r, scope, nns);

	const CodeCompleteResult result{ r.GetDecls() };
	m_CodeCompleter->HandleCodeCompleteResult(result);
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

natRefPointer<Declaration::ModuleDecl> Sema::ActOnModuleDecl(natRefPointer<Scope> scope, SourceLocation startLoc,
                                                             Identifier::IdPtr name, nBool addToContext)
{
	natRefPointer<Declaration::ModuleDecl> moduleDecl;

	LookupResult r{ *this, name, startLoc, LookupNameType::LookupModuleName };
	if (LookupName(r, m_CurrentScope) && r.GetDeclSize() > 0)
	{
		moduleDecl = r.GetDecls().first();
	}
	else
	{
		moduleDecl = make_ref<Declaration::ModuleDecl>(Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()),
		                                               SourceLocation{}, std::move(name), startLoc);
		PushOnScopeChains(moduleDecl, scope, addToContext);
	}

	return moduleDecl;
}

void Sema::ActOnStartModule(natRefPointer<Scope> const& scope, natRefPointer<Declaration::ModuleDecl> const& moduleDecl)
{
	PushDeclContext(scope, moduleDecl.Get());
}

void Sema::ActOnFinishModule()
{
	PopDeclContext();
}

natRefPointer<Declaration::ImportDecl> Sema::ActOnModuleImport(natRefPointer<Scope> const& scope,
                                                               SourceLocation startLoc, SourceLocation importLoc,
                                                               natRefPointer<Declaration::ModuleDecl> const& moduleDecl)
{
	auto importDecl = make_ref<Declaration::ImportDecl>(Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()),
	                                                    importLoc, moduleDecl);
	for (const auto& decl : moduleDecl->GetDecls())
	{
		if (auto namedDecl = decl.Cast<Declaration::NamedDecl>())
		{
			MarkAsImported(namedDecl);
			PushOnScopeChains(std::move(namedDecl), scope, false);
		}
	}

	return importDecl;
}

Type::TypePtr Sema::LookupTypeName(natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc,
                                   natRefPointer<Scope> scope, natRefPointer<NestedNameSpecifier> const& nns)
{
	LookupResult result{ *this, id, nameLoc, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, scope, nns))
	{
		return nullptr;
	}

	switch (result.GetResultType())
	{
	default:
		assert(!"Invalid result type.");[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::FoundOverloaded:
	case LookupResult::LookupResultType::Ambiguous:
		return nullptr;
	case LookupResult::LookupResultType::Found:
	{
		assert(result.GetDeclSize() == 1);
		const auto decl = *result.GetDecls().begin();
		if (const auto typeDecl = decl.Cast<Declaration::TypeDecl>())
		{
			return typeDecl->GetTypeForDecl();
		}
		return nullptr;
	}
	}
}

natRefPointer<Declaration::AliasDecl> Sema::LookupAliasName(
	natRefPointer<Identifier::IdentifierInfo> const& id, SourceLocation nameLoc,
	natRefPointer<Scope> scope, natRefPointer<NestedNameSpecifier> const& nns,
	natRefPointer<Syntax::ResolveContext> const& resolveContext)
{
	assert(id && scope);
	LookupResult result{ *this, id, nameLoc, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, std::move(scope), nns))
	{
		return nullptr;
	}

	switch (result.GetResultType())
	{
	default:
		assert(!"Invalid result type.");[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::FoundOverloaded:
	case LookupResult::LookupResultType::Ambiguous:
		return nullptr;
	case LookupResult::LookupResultType::Found:
	{
		assert(result.GetDeclSize() == 1);
		const auto decl = *result.GetDecls().begin();
		if (auto aliasDecl = decl.Cast<Declaration::AliasDecl>())
		{
			return aliasDecl;
		}

		if (const auto unresolvedDecl = decl.Cast<Declaration::UnresolvedDecl>())
		{
			return ResolveDeclarator(resolveContext, unresolvedDecl);
		}

		return nullptr;
	}
	}
}

natRefPointer<Declaration::ModuleDecl> Sema::LookupModuleName(natRefPointer<Identifier::IdentifierInfo> const& id,
                                                              SourceLocation nameLoc,
                                                              natRefPointer<Scope> scope,
                                                              natRefPointer<NestedNameSpecifier> const& nns)
{
	LookupResult result{ *this, id, nameLoc, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, std::move(scope), nns))
	{
		return nullptr;
	}

	switch (result.GetResultType())
	{
	default:
		assert(!"Invalid result type.");[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::FoundOverloaded:
	case LookupResult::LookupResultType::Ambiguous:
		return nullptr;
	case LookupResult::LookupResultType::Found:
	{
		assert(result.GetDeclSize() == 1);
		const auto decl = result.GetDecls().first();
		if (decl->GetType() == Declaration::Decl::Module)
		{
			return decl.UnsafeCast<Declaration::ModuleDecl>();
		}
		return nullptr;
	}
	}
}

Type::TypePtr Sema::BuildFunctionType(Type::TypePtr retType, Linq<Valued<Type::TypePtr>> const& paramType,
                                      nBool hasVarArg)
{
	return m_Context.GetFunctionType(from(paramType), std::move(retType), hasVarArg);
}

Type::TypePtr Sema::CreateUnresolvedType(std::vector<Lex::Token> tokens)
{
	return m_Context.GetUnresolvedType(move(tokens));
}

Declaration::DeclPtr Sema::ActOnStartOfFunctionDef(natRefPointer<Scope> const& scope,
                                                   const Declaration::DeclaratorPtr& declarator)
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

	auto funcDecl = decl.Cast<Declaration::FunctionDecl>();
	if (!funcDecl)
	{
		return nullptr;
	}

	if (scope)
	{
		PushDeclContext(scope, funcDecl.Get());

		for (auto&& d : funcDecl->GetDecls().select([](Declaration::DeclPtr const& td)
		{
			return td.Cast<Declaration::NamedDecl>();
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
	auto fd = decl.Cast<Declaration::FunctionDecl>();
	if (!fd)
	{
		return nullptr;
	}

	const auto funcType = fd->GetValueType().Cast<Type::FunctionType>();
	assert(funcType);
	if (!funcType->GetResultType()->IsVoid() && !CheckFunctionReturn(body->GetChildrenStmt()))
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrNotAllControlFlowReturnAValue, fd->GetLocation());
	}

	if (const auto method = fd.Cast<Declaration::MethodDecl>())
	{
		const auto classDecl = dynamic_cast<Declaration::ClassDecl*>(Declaration::Decl::CastFromDeclContext(fd->GetContext()));
		assert(classDecl);

		const auto fieldQuery = classDecl->GetFields();

		if (const auto constructor = fd.Cast<Declaration::ConstructorDecl>())
		{
			// TODO: 对未初始化的数据成员进行操作
		}
		else if (const auto destructor = fd.Cast<Declaration::DestructorDecl>())
		{
			// TODO: 析构数据成员，也可以在后端生成
		}
	}

	fd->SetBody(std::move(body));

	PopDeclContext();

	return decl;
}

natRefPointer<Declaration::FunctionDecl> Sema::GetParsingFunction() const noexcept
{
	if (const auto funcScope = m_CurrentScope->GetFunctionParent().Lock())
	{
		if (const auto funcDecl = Declaration::Decl::CastFromDeclContext(funcScope->GetEntity())->ForkRef<Declaration::
			FunctionDecl>())
		{
			return funcDecl;
		}
	}

	return nullptr;
}

natRefPointer<Declaration::TagDecl> Sema::ActOnTag(natRefPointer<Scope> const& scope,
                                                   Type::TagType::TagTypeClass tagTypeClass, SourceLocation kwLoc,
                                                   Specifier::Access accessSpecifier,
                                                   Identifier::IdPtr name, SourceLocation nameLoc,
                                                   Type::TypePtr underlyingType, nBool addToContext)
{
	if (tagTypeClass == Type::TagType::TagTypeClass::Enum && !underlyingType)
	{
		underlyingType = m_Context.GetBuiltinType(Type::BuiltinType::Int);
	}

	LookupResult previous{ *this, name, nameLoc, LookupNameType::LookupTagName };
	if (LookupName(previous, scope) || previous.GetDeclSize() > 0)
	{
		// TODO: 重复定义 Tag，报告错误
		return nullptr;
	}

	if (tagTypeClass == Type::TagType::TagTypeClass::Enum)
	{
		auto enumDecl = make_ref<Declaration::EnumDecl>(Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()),
		                                                nameLoc, name, kwLoc);
		enumDecl->SetTypeForDecl(make_ref<Type::EnumType>(enumDecl));
		enumDecl->SetUnderlyingType(underlyingType);
		PushOnScopeChains(enumDecl, scope, addToContext);
		return enumDecl;
	}

	auto classDecl = make_ref<Declaration::ClassDecl>(Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get()),
	                                                  nameLoc, name, kwLoc);
	classDecl->SetTypeForDecl(make_ref<Type::ClassType>(classDecl));
	PushOnScopeChains(classDecl, scope, addToContext);
	return classDecl;
}

void Sema::ActOnTagStartDefinition(natRefPointer<Scope> const& scope,
                                   natRefPointer<Declaration::TagDecl> const& tagDecl)
{
	PushDeclContext(scope, tagDecl.Get());
}

void Sema::ActOnTagFinishDefinition()
{
	// TODO: 对于没有用户定义构造/析构函数的类型，生成一个默认构造/析构函数

	PopDeclContext();
}

natRefPointer<Declaration::EnumConstantDecl> Sema::ActOnEnumerator(natRefPointer<Scope> const& scope,
                                                                   natRefPointer<Declaration::EnumDecl> const& enumDecl,
                                                                   natRefPointer<Declaration::EnumConstantDecl> const&
                                                                   lastEnumerator, Identifier::IdPtr name,
                                                                   SourceLocation loc, Expression::ExprPtr initializer)
{
	LookupResult r{ *this, name, loc, LookupNameType::LookupOrdinaryName };
	if (LookupName(r, scope) && (r.GetResultType() != LookupResult::LookupResultType::NotFound))
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrDuplicateDeclaration, loc)
		      .AddArgument(name);
		for (const auto& decl : r.GetDecls())
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::NoteSee, decl->GetLocation());
		}
		return nullptr;
	}

	nuLong value;
	if (initializer)
	{
		if (!initializer->EvaluateAsInt(value, m_Context))
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrExpressionCannotEvaluateAsConstant, loc);
			return nullptr;
		}
	}
	else
	{
		value = lastEnumerator ? lastEnumerator->GetValue() + 1 : 0;
	}

	const auto dc = Declaration::Decl::CastToDeclContext(enumDecl.Get());

	auto enumerator = make_ref<Declaration::EnumConstantDecl>(dc, loc, std::move(name), enumDecl->GetTypeForDecl(),
	                                                          std::move(initializer), value);
	PushOnScopeChains(enumerator, scope);
	return enumerator;
}

nBool Sema::LookupName(LookupResult& result, natRefPointer<Scope> scope) const
{
	auto found = false;
	const auto id = result.GetLookupId();
	const auto lookupType = result.GetLookupType();

	for (; scope; scope = scope->GetParent())
	{
		Linq<Valued<natRefPointer<Declaration::NamedDecl>>> query = scope->GetDecls().select(
			[](natRefPointer<Declaration::NamedDecl> const& namedDecl)
			{
				return namedDecl;
			}).where(
			[&id, isCodeCompletion = result.IsCodeCompletion()](natRefPointer<Declaration::NamedDecl> const& namedDecl)
			{
				return namedDecl && isCodeCompletion
					       ? !id || namedDecl
					                ->GetIdentifierInfo()->GetName()
					                .Find(id->GetName()) != nString
					       ::npos || id
					                 ->GetName().Find(
						                 namedDecl->GetIdentifierInfo()
						                          ->GetName()) !=
					       nString::npos
					       : namedDecl->GetIdentifierInfo() == id;
			});

		switch (lookupType)
		{
		case LookupNameType::LookupTagName:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
			{
				return decl.Cast<Declaration::TagDecl>();
			});
			break;
		case LookupNameType::LookupLabel:
			query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
			{
				return decl.Cast<Declaration::LabelDecl>();
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
				return decl.Cast<Declaration::ModuleDecl>();
			});
			break;
		default:
			assert(!"Invalid lookupType");[[fallthrough]];
		case LookupNameType::LookupOrdinaryName:
		case LookupNameType::LookupAnyName:
			break;
		}

		if (!query.empty())
		{
			result.AddDecl(query);
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
			return decl.Cast<Declaration::TagDecl>();
		});
		break;
	case LookupNameType::LookupLabel:
		query = query.where([](natRefPointer<Declaration::NamedDecl> const& decl) -> nBool
		{
			return decl.Cast<Declaration::LabelDecl>();
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
			return decl.Cast<Declaration::ModuleDecl>();
		});
		break;
	default:
		assert(!"Invalid lookupType");[[fallthrough]];
	case LookupNameType::LookupOrdinaryName:
	case LookupNameType::LookupAnyName:
		break;
	}

	if (!query.empty())
	{
		result.AddDecl(from(query));
		found = true;
	}

	result.ResolveResultType();
	return found;
}

nBool Sema::LookupNestedName(LookupResult& result, natRefPointer<Scope> scope,
                             natRefPointer<NestedNameSpecifier> const& nns)
{
	if (nns)
	{
		const auto dc = nns->GetAsDeclContext(m_Context);
		return LookupQualifiedName(result, dc);
	}

	return LookupName(result, std::move(scope));
}

nBool Sema::LookupConstructors(LookupResult& result, natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	if (!classDecl)
	{
		result.ResolveResultType();
		return false;
	}

	result.AddDecl(classDecl->GetMethods().where([](natRefPointer<Declaration::MethodDecl> const& method) -> nBool
	{
		return method.Cast<Declaration::ConstructorDecl>();
	}));
	result.ResolveResultType();
	return !result.IsEmpty();
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
		const auto s = m_CurrentScope->GetFunctionParent().Lock();
		PushOnScopeChains(labelDecl, s, true);
	}

	return labelDecl;
}

Type::TypePtr Sema::ActOnArrayType(Type::TypePtr elementType, std::size_t size)
{
	if (elementType->GetType() == Type::Type::Function)
	{
		// TODO: 报告错误：数组的元素不能是函数
	}

	return m_Context.GetArrayType(std::move(elementType), size);
}

Type::TypePtr Sema::ActOnPointerType(natRefPointer<Scope> const& scope, Type::TypePtr pointeeType)
{
	if (!scope->HasFlags(ScopeFlags::UnsafeScope))
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrUnsafeOperationInSafeScope);
	}

	return m_Context.GetPointerType(std::move(pointeeType));
}

natRefPointer<Declaration::ParmVarDecl> Sema::ActOnParamDeclarator(natRefPointer<Scope> const& scope,
                                                                   Declaration::DeclaratorPtr decl)
{
	auto id = decl->GetIdentifier();
	if (id)
	{
		LookupResult r{ *this, id, {}, LookupNameType::LookupOrdinaryName };
		if (LookupName(r, scope) || r.GetDeclSize() > 0)
		{
			// TODO: 报告存在重名的参数
		}
	}

	// 临时放在翻译单元上下文中，在整个函数声明完成后关联到函数
	auto ret = make_ref<Declaration::ParmVarDecl>(Declaration::Decl::ParmVar,
	                                              m_Context.GetTranslationUnit().Get(), SourceLocation{}, SourceLocation{},
	                                              std::move(id), decl->GetType(), Specifier::StorageClass::None,
	                                              decl->GetInitializer());

	PushOnScopeChains(ret, scope, false);

	return ret;
}

natRefPointer<Declaration::VarDecl> Sema::ActOnVariableDeclarator(
	natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc)
{
	auto type = Type::Type::GetUnderlyingType(decl->GetType());
	auto id = decl->GetIdentifier();
	auto initExpr = decl->GetInitializer().Cast<Expression::Expr>();

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

	if (decl->GetStorageClass() == Specifier::StorageClass::Const && !initExpr)
	{
		// TODO: 报告错误：常量必须具有初始化器
		// 考虑使用 0 作为初始化器？
		return nullptr;
	}

	if (const auto initList = initExpr.Cast<Expression::InitListExpr>())
	{
		if (const auto arrayType = type.Cast<Type::ArrayType>())
		{
			if (arrayType->GetSize() < initList->GetInitExprCount())
			{
				// TODO: 报告错误：初始化列表的元素多于数组大小
				return nullptr;
			}

			// TODO: 进行更多检查
		}
	}
	else if (initExpr && Type::Type::GetUnderlyingType(initExpr->GetExprType()) != type)
	{
		const auto castType = getCastType(initExpr, type, true);
		initExpr = ImpCastExprToType(std::move(initExpr), type, castType);
	}

	auto varDecl = make_ref<Declaration::VarDecl>(Declaration::Decl::Var, dc, decl->GetRange().GetBegin(),
	                                              decl->GetIdentifierLocation(), std::move(id), std::move(type),
	                                              decl->GetStorageClass());
	varDecl->SetInitializer(initExpr);

	return varDecl;
}

natRefPointer<Declaration::FieldDecl> Sema::ActOnFieldDeclarator(natRefPointer<Scope> const& scope,
                                                                 Declaration::DeclaratorPtr decl,
                                                                 Declaration::DeclContext* dc)
{
	auto type = Type::Type::GetUnderlyingType(decl->GetType());
	auto id = decl->GetIdentifier();

	if (!id)
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (!type)
	{
		// TODO: 报告错误
		return nullptr;
	}

	auto fieldDecl = make_ref<Declaration::FieldDecl>(Declaration::Decl::Field, dc, decl->GetRange().GetBegin(),
	                                                  decl->GetIdentifierLocation(), std::move(id), std::move(type));
	return fieldDecl;
}

natRefPointer<Declaration::FunctionDecl> Sema::ActOnFunctionDeclarator(
	natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc,
	Identifier::IdPtr asId)
{
	asId = asId ? asId : decl->GetIdentifier();

	// TODO: 处理自动推断函数返回类型的情况
	auto type = decl->GetType().Cast<Type::FunctionType>();
	if (!type)
	{
		// TODO: 报告错误
		return nullptr;
	}

	natRefPointer<Declaration::FunctionDecl> funcDecl;

	if (dc->GetType() == Declaration::Decl::Class && decl->GetStorageClass() != Specifier::StorageClass::Static)
	{
		auto classDecl = Declaration::Decl::CastFromDeclContext(dc)->ForkRef<Declaration::ClassDecl>();
		if (decl->IsConstructor())
		{
			funcDecl = make_ref<Declaration::ConstructorDecl>(dc, decl->GetRange().GetBegin(),
			                                                  std::move(asId), std::move(type), decl->GetStorageClass());
		}
		else if (decl->IsDestructor())
		{
			funcDecl = make_ref<Declaration::DestructorDecl>(dc, decl->GetRange().GetBegin(),
			                                                 std::move(asId), std::move(type), decl->GetStorageClass());
		}
		else
		{
			funcDecl = make_ref<Declaration::MethodDecl>(Declaration::Decl::Method, dc,
			                                             decl->GetRange().GetBegin(), decl->GetIdentifierLocation(),
			                                             std::move(asId), std::move(type),
			                                             decl->GetStorageClass());
		}
	}
	else
	{
		funcDecl = make_ref<Declaration::FunctionDecl>(Declaration::Decl::Function, dc,
		                                               decl->GetRange().GetBegin(), decl->GetIdentifierLocation(),
		                                               std::move(asId), std::move(type),
		                                               decl->GetStorageClass());
	}

	for (auto const& param : decl->GetParams())
	{
		param->SetContext(funcDecl.Get());
	}

	funcDecl->SetParams(from(decl->GetParams()));
	decl->ClearParams();

	return funcDecl;
}

natRefPointer<Declaration::UnresolvedDecl> Sema::ActOnUnresolvedDeclarator(
	natRefPointer<Scope> const& scope, Declaration::DeclaratorPtr decl, Declaration::DeclContext* dc)
{
	assert(m_CurrentPhase == Phase::Phase1);
	assert(!decl->GetType() && !decl->GetInitializer());

	auto unresolvedDecl = make_ref<Declaration::UnresolvedDecl>(dc, decl->GetIdentifierLocation(), decl->GetIdentifier(),
	                                                            nullptr,
	                                                            decl->GetRange().GetBegin(), decl);
	m_Declarators.emplace_back(std::move(decl));

	return unresolvedDecl;
}

natRefPointer<Declaration::UnresolvedDecl> Sema::ActOnCompilerActionIdentifierArgument(Identifier::IdPtr id)
{
	assert(id);
	return make_ref<Declaration::UnresolvedDecl>(nullptr, SourceLocation{}, std::move(id), nullptr, SourceLocation{},
	                                             nullptr);
}

void Sema::RemoveOldUnresolvedDecl(const Declaration::DeclaratorPtr& decl,
                                   Declaration::DeclPtr const& oldUnresolvedDeclPtr)
{
	const auto declScope = decl->GetDeclarationScope();
	const auto declContext = decl->GetDeclarationContext();
	assert(declScope && declContext);
	const auto contextRecoveryScope = make_scope([this, curContext = std::move(m_CurrentDeclContext)]
	{
		m_CurrentDeclContext = curContext;
	});
	m_CurrentDeclContext = declContext;
	RemoveFromScopeChains(oldUnresolvedDeclPtr, declScope);
}

natRefPointer<Declaration::NamedDecl> Sema::HandleDeclarator(natRefPointer<Scope> scope,
                                                             const Declaration::DeclaratorPtr& decl,
                                                             Declaration::DeclPtr const& oldUnresolvedDeclPtr)
{
	if (auto preparedDecl = decl->GetDecl(); preparedDecl && preparedDecl != oldUnresolvedDeclPtr)
	{
		return preparedDecl;
	}

	auto id = decl->GetIdentifier();

	if (!id)
	{
		if (decl->IsConstructor())
		{
			Lex::Token dummyToken;
			id = m_Preprocessor.FindIdentifierInfo(ConstructorName, dummyToken);
		}
		else if (decl->IsDestructor())
		{
			Lex::Token dummyToken;
			id = m_Preprocessor.FindIdentifierInfo(DestructorName, dummyToken);
		}
		else
		{
			if (decl->GetContext() != Declaration::Context::Prototype)
			{
				m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrExpectedIdentifier, decl->GetRange().GetBegin());
			}

			return nullptr;
		}
	}

	while ((scope->GetFlags() & ScopeFlags::DeclarableScope) == ScopeFlags::None)
	{
		scope = scope->GetParent();
	}

	std::unordered_set<Declaration::AttrPtr> attrs;

	if (oldUnresolvedDeclPtr)
	{
		const auto attrQuery = oldUnresolvedDeclPtr->GetAllAttributes();
		attrs.insert(attrQuery.begin(), attrQuery.end());
		RemoveOldUnresolvedDecl(decl, oldUnresolvedDeclPtr);
	}

	LookupResult previous{ *this, id, {}, LookupNameType::LookupOrdinaryName };
	auto maybeOverload = false;
	if (LookupName(previous, scope) && previous.GetDeclSize())
	{
		// 目前只有函数可以重载，如果发现已有重名的非函数的已解析声明则报告错误
		if (previous.GetDecls().first_or_default(nullptr, [](natRefPointer<Declaration::NamedDecl> const& namedDecl)
		{
			return !namedDecl.Cast<Declaration::UnresolvedDecl>() && !namedDecl.Cast<Declaration::FunctionDecl>();
		}))
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrDuplicateDeclaration, decl->GetIdentifierLocation())
				.AddArgument(id);
			return nullptr;
		}

		maybeOverload = true;
	}

	const auto dc = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());
	const auto type = decl->GetType();
	natRefPointer<Declaration::NamedDecl> retDecl;

	if (decl->IsUnresolved())
	{
		// TODO: 报告错误：只有第一阶段允许未解析的声明符
		assert(m_CurrentPhase == Phase::Phase1);

		retDecl = ActOnUnresolvedDeclarator(scope, decl, dc);
		if (scope->HasFlags(ScopeFlags::UnsafeScope))
		{
			decl->SetSafety(Specifier::Safety::Unsafe);
		}
		decl->SetDeclarationScope(scope);
		decl->SetDeclarationContext(m_CurrentDeclContext);
	}
	else if (const auto funcType = type.Cast<Type::FunctionType>())
	{
		// 检查签名
		if (maybeOverload && !CheckFunctionOverload(funcType, previous.GetDecls()))
		{
			// TODO: 报告错误：无法重载具有相同签名的函数
			return nullptr;
		}

		retDecl = ActOnFunctionDeclarator(scope, decl, dc, id);
	}
	else if (dc->GetType() == Declaration::Decl::Class)
	{
		if (maybeOverload)
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrDuplicateDeclaration, decl->GetIdentifierLocation())
				.AddArgument(id);
			return nullptr;
		}

		retDecl = ActOnFieldDeclarator(scope, decl, dc);
	}
	else
	{
		if (maybeOverload)
		{
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrDuplicateDeclaration, decl->GetIdentifierLocation())
				.AddArgument(id);
			return nullptr;
		}

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

	for (auto attr : attrs)
	{
		retDecl->AttachAttribute(std::move(attr));
	}

	return retDecl;
}

natRefPointer<Declaration::AliasDecl> Sema::ActOnAliasDeclaration(natRefPointer<Scope> scope,
                                                                  SourceLocation loc, Identifier::IdPtr id,
                                                                  SourceLocation idLoc, ASTNodePtr aliasAsAst,
                                                                  nBool addToContext)
{
	while ((scope->GetFlags() & ScopeFlags::DeclarableScope) == ScopeFlags::None)
	{
		scope = scope->GetParent();
	}

	auto dc = scope->GetEntity();
	if (!dc)
	{
		dc = Declaration::Decl::CastToDeclContext(m_CurrentDeclContext.Get());
	}

	LookupResult previous{ *this, id, idLoc, LookupNameType::LookupOrdinaryName };
	if (LookupName(previous, scope) && previous.GetDeclSize())
	{
		// TODO: 处理重载或覆盖的情况
		return nullptr;
	}

	auto aliasDecl = make_ref<Declaration::AliasDecl>(dc, loc, std::move(id), std::move(aliasAsAst));
	PushOnScopeChains(aliasDecl, scope, addToContext);

	return aliasDecl;
}

Statement::StmtPtr Sema::ActOnNullStmt(SourceLocation loc)
{
	return make_ref<Statement::NullStmt>(loc);
}

Statement::StmtPtr Sema::ActOnDeclStmt(Declaration::DeclPtr decl, SourceLocation start,
                                       SourceLocation end)
{
	return make_ref<Statement::DeclStmt>(std::move(decl), start, end);
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

	auto labelStmt = make_ref<Statement::LabelStmt>(labelLoc, labelDecl, std::move(subStmt));
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
	return make_ref<Statement::IfStmt>(ifLoc, ActOnConditionExpr(std::move(condExpr)), std::move(thenStmt), elseLoc,
	                                   std::move(elseStmt));
}

Statement::StmtPtr Sema::ActOnWhileStmt(SourceLocation loc, Expression::ExprPtr cond,
                                        Statement::StmtPtr body)
{
	return make_ref<Statement::WhileStmt>(loc, ActOnConditionExpr(std::move(cond)), std::move(body));
}

Statement::StmtPtr Sema::ActOnForStmt(SourceLocation forLoc, SourceLocation leftParenLoc, Statement::StmtPtr init,
                                      Expression::ExprPtr cond, Expression::ExprPtr third, SourceLocation rightParenLoc,
                                      Statement::StmtPtr body)
{
	return make_ref<Statement::ForStmt>(std::move(init), ActOnConditionExpr(std::move(cond)), std::move(third),
	                                    std::move(body), forLoc, leftParenLoc, rightParenLoc);
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

Statement::StmtPtr Sema::ActOnReturnStmt(SourceLocation loc, Expression::ExprPtr returnedExpr,
                                         natRefPointer<Scope> const& scope)
{
	if (const auto curFunc = GetParsingFunction())
	{
		const auto funcType = curFunc->GetValueType().Cast<Type::FunctionType>();
		if (!funcType)
		{
			return nullptr;
		}

		auto retType = funcType->GetResultType();
		const auto retTypeClass = retType->GetType();
		if (retTypeClass == Type::Type::Auto)
		{
			funcType->SetResultType(returnedExpr
				                        ? returnedExpr->GetExprType()
				                        : static_cast<Type::TypePtr>(m_Context.GetBuiltinType(Type::BuiltinType::Void)));
		}
		else if (retTypeClass == Type::Type::Builtin && returnedExpr)
		{
			const auto castType = getCastType(returnedExpr, retType, true);
			returnedExpr = ImpCastExprToType(std::move(returnedExpr), std::move(retType), castType);
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
	return make_ref<Expression::BooleanLiteral>(token.Is(Lex::TokenType::Kw_true),
	                                            m_Context.GetBuiltinType(Type::BuiltinType::Bool), token.GetLocation());
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

	return make_ref<Expression::CharacterLiteral>(literalParser.GetValue(), nString::UsingStringType,
	                                              m_Context.GetBuiltinType(Type::BuiltinType::Char), token.GetLocation());
}

Expression::ExprPtr Sema::ActOnStringLiteral(Lex::Token const& token)
{
	assert(token.Is(Lex::TokenType::StringLiteral) && token.GetLiteralContent().has_value());

	Lex::StringLiteralParser literalParser{ token.GetLiteralContent().value(), token.GetLocation(), m_Diag };

	if (literalParser.Errored())
	{
		return nullptr;
	}

	auto value = literalParser.GetValue();
	// 多一个 0
	return make_ref<Expression::StringLiteral>(
		value, m_Context.GetArrayType(m_Context.GetBuiltinType(Type::BuiltinType::Char),
		                              static_cast<nuLong>(value.GetSize() + 1)),
		token.GetLocation());
}

Expression::ExprPtr Sema::ActOnNullPointerLiteral(SourceLocation loc) const
{
	return make_ref<Expression::NullPointerLiteral>(m_Context.GetBuiltinType(Type::BuiltinType::Null), loc);
}

Expression::ExprPtr Sema::ActOnConditionExpr(Expression::ExprPtr expr)
{
	if (!expr)
	{
		return nullptr;
	}

	auto boolType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
	const auto castType = getCastType(expr, boolType, true);
	if (castType == Expression::CastType::Invalid)
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrInvalidConvertFromTo, expr->GetStartLoc())
			.AddArgument(GetTypeName(expr->GetExprType()))
			.AddArgument(GetTypeName(boolType));
		return nullptr;
	}
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

Expression::ExprPtr Sema::ActOnInitExpr(Type::TypePtr initType, SourceLocation leftBraceLoc,
                                        std::vector<Expression::ExprPtr> initExprs, SourceLocation rightBraceLoc)
{
	const auto underlyingType = Type::Type::GetUnderlyingType(initType);

	switch (underlyingType->GetType())
	{
	case Type::Type::Builtin:
	case Type::Type::Pointer:
	case Type::Type::Array:
	case Type::Type::Enum:
		return make_ref<Expression::InitListExpr>(std::move(initType), leftBraceLoc, std::move(initExprs), rightBraceLoc);
	case Type::Type::Class:
		return BuildConstructExpr(underlyingType.UnsafeCast<Type::ClassType>(), leftBraceLoc, std::move(initExprs),
		                          rightBraceLoc);
	case Type::Type::Function:
	case Type::Type::Paren:
	case Type::Type::Auto:
	case Type::Type::Unresolved:
	default:
		assert(!"Invalid type");
		return nullptr;
	}
}

natRefPointer<Declaration::NamedDecl> Sema::ResolveDeclarator(
	natRefPointer<Syntax::ResolveContext> const& resolveContext,
	const natRefPointer<Declaration::UnresolvedDecl>& unresolvedDecl)
{
	const auto declarator = unresolvedDecl->GetDeclaratorPtr().Lock();
	assert(declarator && declarator->IsUnresolved());
	if (resolveContext)
	{
		const auto resolvingState = resolveContext->GetDeclaratorResolvingState(declarator);
		switch (resolvingState)
		{
		default:
			assert(!"Invalid resolvingState");
		case Syntax::ResolveContext::ResolvingState::Unknown:
			return resolveContext->GetParser().ResolveDeclarator(declarator);
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

	return nullptr;
}

Expression::ExprPtr Sema::ActOnIdExpr(natRefPointer<Scope> const& scope, natRefPointer<NestedNameSpecifier> const& nns,
                                      Identifier::IdPtr id, SourceLocation idLoc, nBool hasTraillingLParen,
                                      natRefPointer<Syntax::ResolveContext> const& resolveContext)
{
	LookupResult result{ *this, id, idLoc, LookupNameType::LookupOrdinaryName };
	if (!LookupNestedName(result, scope, nns))
	{
		switch (result.GetResultType())
		{
		case LookupResult::LookupResultType::NotFound:
			m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrUndefinedIdentifier, idLoc).AddArgument(id);
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
			if (const auto unresolvedDecl = decl.Cast<Declaration::UnresolvedDecl>())
			{
				decl = ResolveDeclarator(resolveContext, unresolvedDecl);
			}
		}

		if (decl->GetType() == Declaration::Decl::Field)
		{
			// 可能隐含 this
			const auto classDecl = Declaration::Decl::CastFromDeclContext(decl->GetContext())->ForkRef<Declaration::ClassDecl>();
			assert(classDecl);
			if (!classDecl->ContainsDecl(decl))
			{
				// TODO: 报告错误：引用了不属于当前类的字段
				return nullptr;
			}

			// 偷懒了。。。
			return ActOnMemberAccessExpr(
				scope, make_ref<Expression::ThisExpr>(SourceLocation{}, classDecl->GetTypeForDecl(), true), {}, nns, std::move(id));
		}

		// 不可能引用到构造和析构函数，但是还是要注意此处可能的漏判
		if (decl->GetType() == Declaration::Decl::Method)
		{
			// 可能隐含 this
			const auto classDecl = Declaration::Decl::CastFromDeclContext(decl->GetContext())->ForkRef<Declaration::ClassDecl>();
			assert(classDecl);
			if (!classDecl->ContainsDecl(decl))
			{
				// TODO: 报告错误：引用了不属于当前类的方法
				return nullptr;
			}

			// 继续偷懒。。。
			return ActOnMemberAccessExpr(
				scope, make_ref<Expression::ThisExpr>(SourceLocation{}, classDecl->GetTypeForDecl(), true), {}, nns, std::move(id));
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

	if (const auto methodDecl = decl.Cast<Declaration::MethodDecl>())
	{
		const auto recordDecl = dynamic_cast<Declaration::ClassDecl*>(Declaration::Decl::CastFromDeclContext(
			methodDecl->GetContext()));
		assert(recordDecl);
		return make_ref<Expression::ThisExpr>(loc, recordDecl->GetTypeForDecl(), false);
	}

	// TODO: 报告当前上下文不允许使用 this 的错误
	return nullptr;
}

Expression::ExprPtr Sema::ActOnAsTypeExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr exprToCast,
                                          Type::TypePtr type, SourceLocation loc)
{
	const auto castType = getCastType(exprToCast, type, false);
	if (castType == Expression::CastType::Invalid)
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrInvalidConvertFromTo, loc)
			.AddArgument(GetTypeName(exprToCast->GetExprType()))
			.AddArgument(GetTypeName(type));
		return nullptr;
	}

	return make_ref<Expression::AsTypeExpr>(std::move(type), castType, std::move(exprToCast));
}

Expression::ExprPtr Sema::ActOnArraySubscriptExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base,
                                                  SourceLocation lloc, Expression::ExprPtr index, SourceLocation rloc)
{
	// TODO: 屏蔽未使用参数警告，这些参数将会在将来的版本被使用
	static_cast<void>(scope);
	static_cast<void>(lloc);

	// TODO: 当前仅支持对内建数组进行此操作
	const auto baseType = base->GetExprType().Cast<Type::ArrayType>();
	if (!baseType)
	{
		// TODO: 报告基础操作数不是内建数组
		return nullptr;
	}

	// TODO: 当前仅允许下标为内建整数类型
	const auto indexType = index->GetExprType().Cast<Type::BuiltinType>();
	if (!indexType || !indexType->IsIntegerType())
	{
		// TODO: 报告下标操作数不具有内建整数类型
		return nullptr;
	}

	return make_ref<Expression::ArraySubscriptExpr>(base, index, baseType->GetElementType(), rloc);
}

Expression::ExprPtr Sema::ActOnCallExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr func,
                                        SourceLocation lloc, Linq<Valued<Expression::ExprPtr>> argExprs,
                                        SourceLocation rloc)
{
	// TODO: 完成重载部分

	if (!func)
	{
		return nullptr;
	}

	func = func->IgnoreParens();

	const auto exprType = func->GetExprType();
	const auto args = argExprs.Cast<std::vector<Expression::ExprPtr>>();

	// 是函数指针
	if (exprType->GetType() == Type::Type::Pointer)
	{
		const auto pointerType = exprType.UnsafeCast<Type::PointerType>();
		const auto pointeeType = pointerType->GetPointeeType();
		if (!pointeeType || pointeeType->GetType() != Type::Type::Function)
		{
			// TODO: 报告错误
			return nullptr;
		}

		const auto fnType = pointeeType.UnsafeCast<Type::FunctionType>();
		if (!fnType->HasVarArg() && args.size() > fnType->GetParameterCount())
		{
			// TODO: 报告错误
			return nullptr;
		}

		return make_ref<Expression::CallExpr>(std::move(func),
		                                      makeArgsFromType(from(args), args.size(), fnType->GetParameterTypes(),
		                                                       fnType->GetParameterCount()), fnType->GetResultType(), rloc);
	}

	if (auto nonMemberFunc = func.Cast<Expression::DeclRefExpr>())
	{
		const auto refFn = nonMemberFunc->GetDecl().Cast<Declaration::FunctionDecl>();
		if (!refFn)
		{
			return nullptr;
		}

		const auto fnType = refFn->GetValueType().Cast<Type::FunctionType>();
		if (!fnType)
		{
			return nullptr;
		}

		if (!fnType->HasVarArg() && args.size() > refFn->GetParamCount())
		{
			// TODO: 报告错误
			return nullptr;
		}

		return make_ref<Expression::CallExpr>(std::move(nonMemberFunc),
		                                      makeArgsFromDecl(from(args), args.size(), refFn->GetParams(),
		                                                       refFn->GetParamCount()), fnType->GetResultType(), rloc);
	}

	if (auto memberFunc = func.Cast<Expression::MemberExpr>())
	{
		const auto baseObj = memberFunc->GetBase();
		if (!baseObj)
		{
			return nullptr;
		}

		const auto refFn = memberFunc->GetMemberDecl().Cast<Declaration::MethodDecl>();
		if (!refFn)
		{
			return nullptr;
		}

		const auto fnType = refFn->GetValueType().Cast<Type::FunctionType>();
		if (!fnType)
		{
			return nullptr;
		}

		if (!fnType->HasVarArg() && args.size() > refFn->GetParamCount())
		{
			// TODO: 报告错误
			return nullptr;
		}

		return make_ref<Expression::MemberCallExpr>(std::move(memberFunc),
		                                            makeArgsFromDecl(from(args), args.size(), refFn->GetParams(),
		                                                             refFn->GetParamCount()), fnType->GetResultType(), rloc);
	}

	// TODO: 报告错误
	return nullptr;
}

Expression::ExprPtr Sema::ActOnMemberAccessExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr base,
                                                SourceLocation periodLoc, natRefPointer<NestedNameSpecifier> const& nns,
                                                Identifier::IdPtr id)
{
	auto baseType = base->GetExprType();

	LookupResult r{ *this, id, {}, LookupNameType::LookupMemberName };

	if (const auto pointerType = baseType.Cast<Type::PointerType>())
	{
		baseType = pointerType->GetPointeeType();
	}

	const auto classType = baseType.Cast<Type::ClassType>();
	if (classType)
	{
		const auto classDecl = classType->GetDecl();

		Declaration::DeclContext* dc = classDecl.Get();
		if (nns)
		{
			dc = nns->GetAsDeclContext(m_Context);
		}

		// TODO: 对dc的合法性进行检查

		if (!LookupQualifiedName(r, dc) || r.IsEmpty())
		{
			// TODO: 找不到这个成员
		}

		return BuildMemberReferenceExpr(scope, std::move(base), std::move(baseType), periodLoc, nns, r);
	}

	// TODO: 暂时不支持对ClassType及指向ClassType的指针以外的类型进行成员访问操作
	return nullptr;
}

Expression::ExprPtr Sema::ActOnUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType,
                                       Expression::ExprPtr operand)
{
	// TODO: 为将来可能的操作符重载保留
	static_cast<void>(scope);

	const auto opCode = getUnaryOperationType(tokenType);

	if (IsUnsafeOperation(opCode) && !scope->HasFlags(ScopeFlags::UnsafeScope))
	{
		m_Diag.Report(Diag::DiagnosticsEngine::DiagID::ErrUnsafeOperationInSafeScope);
	}

	return CreateBuiltinUnaryOp(loc, opCode, std::move(operand));
}

Expression::ExprPtr Sema::ActOnPostfixUnaryOp(natRefPointer<Scope> const& scope, SourceLocation loc,
                                              Lex::TokenType tokenType, Expression::ExprPtr operand)
{
	// TODO: 为将来可能的操作符重载保留
	static_cast<void>(scope);

	assert(tokenType == Lex::TokenType::PlusPlus || tokenType == Lex::TokenType::MinusMinus);
	return CreateBuiltinUnaryOp(loc,
	                            tokenType == Lex::TokenType::PlusPlus
		                            ? Expression::UnaryOperationType::PostInc
		                            : Expression::UnaryOperationType::PostDec,
	                            std::move(operand));
}

Expression::ExprPtr Sema::ActOnBinaryOp(natRefPointer<Scope> const& scope, SourceLocation loc, Lex::TokenType tokenType,
                                        Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
{
	static_cast<void>(scope);
	return BuildBuiltinBinaryOp(loc, getBinaryOperationType(tokenType), std::move(leftOperand), std::move(rightOperand));
}

Expression::ExprPtr Sema::BuildBuiltinBinaryOp(SourceLocation loc, Expression::BinaryOperationType binOpType,
                                               Expression::ExprPtr leftOperand, Expression::ExprPtr rightOperand)
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
	case Expression::BinaryOperationType::Rem:
	case Expression::BinaryOperationType::Add:
	case Expression::BinaryOperationType::Sub:
	case Expression::BinaryOperationType::Shl:
	case Expression::BinaryOperationType::Shr:
	case Expression::BinaryOperationType::And:
	case Expression::BinaryOperationType::Xor:
	case Expression::BinaryOperationType::Or:
	{
		auto resultType = GetBuiltinBinaryOpType(leftOperand, rightOperand);
		if (resultType->GetType() == Type::Type::Pointer)
		{
			// 至少有一操作数是 pointer，或者存在可能的操作符重载导致结果为 pointer？
			// 暂时不考虑后一种情况
			if (Type::Type::GetUnderlyingType(leftOperand->GetExprType())->GetType() == Type::Type::Pointer)
			{
				if (Type::Type::GetUnderlyingType(rightOperand->GetExprType())->GetType() == Type::Type::Pointer)
				{
					if (binOpType != Expression::BinaryOperationType::Sub)
					{
						// TODO: 报告错误：不支持的操作
						return nullptr;
					}
				}
				else
				{
					if (binOpType != Expression::BinaryOperationType::Add && binOpType != Expression::BinaryOperationType::Sub)
					{
						// TODO: 报告错误：不支持的操作
						return nullptr;
					}
				}
			}
			else
			{
				// 假设此时右操作数类型是指针类型
				assert(Type::Type::GetUnderlyingType(rightOperand->GetExprType())->GetType() == Type::Type::Pointer);
				if (binOpType != Expression::BinaryOperationType::Add)
				{
					// TODO: 报告错误：不支持的操作
					return nullptr;
				}
			}
		}
		return make_ref<Expression::BinaryOperator>(std::move(leftOperand), std::move(rightOperand), binOpType,
		                                            std::move(resultType), loc, Expression::ValueCategory::RValue);
	}
	case Expression::BinaryOperationType::LAnd:
	case Expression::BinaryOperationType::LOr:
	{
		auto boolType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
		// 相当于强制的
		const auto leftCastType = getCastType(leftOperand, boolType, false);
		leftOperand = ImpCastExprToType(std::move(leftOperand), boolType, leftCastType);
		const auto rightCastType = getCastType(rightOperand, boolType, false);
		rightOperand = ImpCastExprToType(std::move(rightOperand), std::move(boolType), rightCastType);
	}[[fallthrough]];
	case Expression::BinaryOperationType::LT:
	case Expression::BinaryOperationType::GT:
	case Expression::BinaryOperationType::LE:
	case Expression::BinaryOperationType::GE:
	case Expression::BinaryOperationType::EQ:
	case Expression::BinaryOperationType::NE:
	{
		auto resultType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
		// 仅转换
		if (Type::Type::GetUnderlyingType(leftOperand->GetExprType())->GetType() == Type::Type::Builtin &&
			Type::Type::GetUnderlyingType(rightOperand->GetExprType())->GetType() == Type::Type::Builtin)
		{
			UsualArithmeticConversions(leftOperand, rightOperand);
		}

		assert(leftOperand->GetExprType() == rightOperand->GetExprType());
		return make_ref<Expression::BinaryOperator>(std::move(leftOperand), std::move(rightOperand), binOpType,
		                                            std::move(resultType), loc, Expression::ValueCategory::RValue);
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
	{
		if (leftOperand->GetValueCategory() != Expression::ValueCategory::LValue)
		{
			// TODO: 报告错误：仅能修改左值
			return nullptr;
		}

		const auto leftType = Type::Type::GetUnderlyingType(leftOperand->GetExprType()), rightType = Type::Type::
			           GetUnderlyingType(rightOperand->GetExprType());

		if (leftType == rightType)
		{
			return make_ref<Expression::CompoundAssignOperator>(std::move(leftOperand), std::move(rightOperand), binOpType,
			                                                    leftType, loc);
		}

		switch (leftType->GetType())
		{
		case Type::Type::Builtin:
		{
			const auto builtinLeftType = leftType.UnsafeCast<Type::BuiltinType>();
			switch (rightType->GetType())
			{
			case Type::Type::Builtin:
			{
				const auto builtinRightType = rightType.UnsafeCast<Type::BuiltinType>();
				Expression::CastType castType;
				if (builtinLeftType->IsIntegerType())
				{
					castType = builtinRightType->IsIntegerType()
						           ? Expression::CastType::IntegralCast
						           : Expression::CastType::FloatingToIntegral;
				}
				else
				{
					castType = builtinRightType->IsIntegerType()
						           ? Expression::CastType::IntegralToFloating
						           : Expression::CastType::FloatingCast;
				}

				return make_ref<Expression::CompoundAssignOperator>(std::move(leftOperand),
				                                                    ImpCastExprToType(std::move(rightOperand), builtinLeftType,
				                                                                      castType), binOpType, builtinLeftType, loc);
			}
			case Type::Type::Pointer:
				break;
			case Type::Type::Array:
				break;
			case Type::Type::Function:
				break;
			case Type::Type::Class:
				break;
			case Type::Type::Enum:
				break;
			default:
				break;
			}
			break;
		}
		case Type::Type::Pointer:
			break;
		case Type::Type::Array:
			break;
		case Type::Type::Function:
			break;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			break;
		}
	}
	}

	// TODO
	nat_Throw(NotImplementedException);
}

Expression::ExprPtr Sema::ActOnConditionalOp(SourceLocation questionLoc, SourceLocation colonLoc,
                                             Expression::ExprPtr condExpr, Expression::ExprPtr leftExpr,
                                             Expression::ExprPtr rightExpr)
{
	const auto leftType = leftExpr->GetExprType(), rightType = rightExpr->GetExprType(), commonType = getCommonType(leftType, rightType);
	const auto leftCastType = getCastType(leftExpr, commonType, true), rightCastType = getCastType(rightExpr, commonType, true);

	const auto valueCategory = leftType == rightType &&
		leftExpr->GetValueCategory() == Expression::ValueCategory::LValue &&
		rightExpr->GetValueCategory() == Expression::ValueCategory::LValue ? Expression::ValueCategory::LValue : Expression::ValueCategory::RValue;

	return make_ref<Expression::ConditionalOperator>(std::move(condExpr), questionLoc,
	                                                 ImpCastExprToType(std::move(leftExpr), commonType, leftCastType),
	                                                 colonLoc,
	                                                 ImpCastExprToType(std::move(rightExpr), commonType, rightCastType),
	                                                 commonType, valueCategory);
}

NatsuLib::natRefPointer<Expression::DeclRefExpr> Sema::BuildDeclarationNameExpr(
	natRefPointer<NestedNameSpecifier> const& nns, Identifier::IdPtr id,
	natRefPointer<Declaration::NamedDecl> decl)
{
	auto valueDecl = decl.Cast<Declaration::ValueDecl>();
	if (!valueDecl)
	{
		// 错误，引用的不是值
		return nullptr;
	}

	auto type = valueDecl->GetValueType();
	return BuildDeclRefExpr(std::move(valueDecl), std::move(type), std::move(id), nns);
}

NatsuLib::natRefPointer<Expression::DeclRefExpr> Sema::BuildDeclRefExpr(natRefPointer<Declaration::ValueDecl> decl,
                                                                        Type::TypePtr type,
                                                                        Identifier::IdPtr id,
                                                                        natRefPointer<NestedNameSpecifier> const& nns)
{
	static_cast<void>(id);

	auto valueCategory = Expression::ValueCategory::LValue;
	if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && HasAllFlags(varDecl->GetStorageClass(), Specifier::StorageClass::Const))
	{
		valueCategory = Expression::ValueCategory::RValue;
	}

	return make_ref<Expression::DeclRefExpr>(nns, std::move(decl), SourceLocation{}, std::move(type), valueCategory);
}

Expression::ExprPtr Sema::BuildMemberReferenceExpr(natRefPointer<Scope> const& scope, Expression::ExprPtr baseExpr,
                                                   Type::TypePtr baseType, SourceLocation opLoc,
                                                   natRefPointer<NestedNameSpecifier> const& nns, LookupResult& r)
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

		if (auto field = decl.Cast<Declaration::FieldDecl>())
		{
			return BuildFieldReferenceExpr(std::move(baseExpr), opLoc, nns, std::move(field), r.GetLookupId());
		}

		if (auto var = decl.Cast<Declaration::VarDecl>())
		{
			return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(var), r.GetLookupId(),
			                                        var->GetValueType(),
													HasAllFlags(var->GetStorageClass(), Specifier::StorageClass::Const) ? Expression::ValueCategory::RValue : Expression::ValueCategory::LValue);
		}

		if (auto method = decl.Cast<Declaration::MethodDecl>())
		{
			return BuildMethodReferenceExpr(std::move(baseExpr), opLoc, nns, std::move(method), r.GetLookupId());
		}

		return nullptr;
	}
	case LookupResult::LookupResultType::FoundOverloaded:
		// TODO: 处理重载的情况
		nat_Throw(NotImplementedException);
	default:
		assert(!"Invalid result type.");[[fallthrough]];
	case LookupResult::LookupResultType::NotFound:
	case LookupResult::LookupResultType::Ambiguous:
		// TODO: 报告错误
		return nullptr;
	}
}

Expression::ExprPtr Sema::BuildFieldReferenceExpr(Expression::ExprPtr baseExpr, SourceLocation opLoc,
                                                  natRefPointer<NestedNameSpecifier> const& nns,
                                                  natRefPointer<Declaration::FieldDecl> field, Identifier::IdPtr id)
{
	static_cast<void>(nns);
	// 字段不能是 const 的，因此总是左值
	return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(field), std::move(id),
	                                        field->GetValueType(), Expression::ValueCategory::LValue);
}

Expression::ExprPtr Sema::BuildMethodReferenceExpr(Expression::ExprPtr baseExpr, SourceLocation opLoc,
                                                   natRefPointer<NestedNameSpecifier> const& nns,
                                                   natRefPointer<Declaration::MethodDecl> method,
                                                   Identifier::IdPtr id)
{
	static_cast<void>(nns);
	return make_ref<Expression::MemberExpr>(std::move(baseExpr), opLoc, std::move(method), std::move(id),
	                                        method->GetValueType(), Expression::ValueCategory::LValue);
}

Expression::ExprPtr Sema::BuildConstructExpr(natRefPointer<Type::ClassType> const& classType,
                                             SourceLocation leftBraceLoc, std::vector<Expression::ExprPtr> initExprs,
                                             SourceLocation rightBraceLoc)
{
	const auto classDecl = classType->GetDecl().Cast<Declaration::ClassDecl>();
	assert(classDecl);
	LookupResult r{ *this, nullptr, {}, LookupNameType::LookupMemberName };
	if (!LookupConstructors(r, classDecl))
	{
		if (initExprs.empty())
		{
			// FIXME: 生成一个默认构造函数
			return make_ref<Expression::ConstructExpr>(classType, leftBraceLoc, nullptr, std::move(initExprs));
		}

		// TODO: 报告错误
		return nullptr;
	}

	if (r.GetResultType() != LookupResult::LookupResultType::Found || r.GetDeclSize() != 1)
	{
		// TODO: 实现重载
		nat_Throw(NotImplementedException);
	}

	auto constructor = r.GetDecls().first().Cast<Declaration::ConstructorDecl>();

	// TODO: 使用重载的选择机制来判断是否可调用
	if (constructor->GetParamCount() != initExprs.size())
	{
		// TODO: 报告错误
		return nullptr;
	}

	const auto params = constructor->GetParams();
	auto paramIter = params.begin();
	const auto paramEnd = params.end();

	for (auto initExprIter = initExprs.begin(), initExprEnd = initExprs.end(); initExprIter != initExprEnd && paramIter !=
	     paramEnd; ++initExprIter, static_cast<void>(++paramIter))
	{
		auto paramType = (*paramIter)->GetValueType();
		const auto castType = getCastType(*initExprIter, paramType, true);
		*initExprIter = ImpCastExprToType(*initExprIter, std::move(paramType), castType);
	}

	// TODO: 位置信息缺失
	return make_ref<Expression::ConstructExpr>(classType, leftBraceLoc, std::move(constructor), std::move(initExprs));
}

Expression::ExprPtr Sema::CreateBuiltinUnaryOp(SourceLocation opLoc, Expression::UnaryOperationType opCode,
                                               Expression::ExprPtr operand)
{
	Type::TypePtr resultType;
	auto valueCategory = Expression::ValueCategory::RValue;

	switch (opCode)
	{
	case Expression::UnaryOperationType::PostInc:
	case Expression::UnaryOperationType::PostDec:
	case Expression::UnaryOperationType::PreInc:
	case Expression::UnaryOperationType::PreDec:
		if (operand->GetValueCategory() != Expression::ValueCategory::LValue)
		{
			// TODO: 报告错误：仅能修改左值
			return nullptr;
		}
		[[fallthrough]];
	case Expression::UnaryOperationType::Plus:
	case Expression::UnaryOperationType::Minus:
	case Expression::UnaryOperationType::Not:
		// TODO: 可能的整数提升？
		resultType = operand->GetExprType();
		break;
	case Expression::UnaryOperationType::AddrOf:
		if (operand->GetValueCategory() != Expression::ValueCategory::LValue)
		{
			// TODO: 报告错误：仅能对左值取地址
			return nullptr;
		}
		resultType = m_Context.GetPointerType(operand->GetExprType());
		break;
	case Expression::UnaryOperationType::Deref:
		if (const auto pointerType = operand->GetExprType().Cast<Type::PointerType>())
		{
			resultType = pointerType->GetPointeeType();
		}
		else
		{
			// TODO: 报告错误
			return nullptr;
		}
		valueCategory = Expression::ValueCategory::LValue;
		break;
	case Expression::UnaryOperationType::LNot:
		resultType = m_Context.GetBuiltinType(Type::BuiltinType::Bool);
		break;
	case Expression::UnaryOperationType::Invalid:
	default:
		// TODO: 报告错误
		return nullptr;
	}

	return make_ref<Expression::UnaryOperator>(std::move(operand), opCode, std::move(resultType), opLoc, valueCategory);
}

Type::TypePtr Sema::GetBuiltinBinaryOpType(Expression::ExprPtr& leftOperand, Expression::ExprPtr& rightOperand)
{
	auto leftType = Type::Type::GetUnderlyingType(leftOperand->GetExprType()),
	     rightType = Type::Type::GetUnderlyingType(rightOperand->GetExprType());

	assert(leftType && rightType);

	switch (leftType->GetType())
	{
	case Type::Type::Builtin:
	{
		const auto builtinLeftType = leftType.UnsafeCast<Type::BuiltinType>();
		switch (rightType->GetType())
		{
		case Type::Type::Builtin:
			return UsualArithmeticConversions(leftOperand, rightOperand);
		case Type::Type::Pointer:
			if (!rightType.UnsafeCast<Type::PointerType>()->GetPointeeType().Cast<Type::FunctionType>() && builtinLeftType->
				IsIntegerType())
			{
				return rightType;
			}

			// TODO: 报告错误：不支持的操作
			return nullptr;
		case Type::Type::Array:
		case Type::Type::Function:
			// TODO: 报告错误：不支持的操作
			return nullptr;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			assert(!"Invalid type");
			return nullptr;
		}
		break;
	}
	case Type::Type::Pointer:
	{
		const auto pointerLeftType = leftType.UnsafeCast<Type::PointerType>();
		if (pointerLeftType->GetPointeeType().Cast<Type::FunctionType>())
		{
			// TODO: 报告错误：不支持的操作
			return nullptr;
		}

		switch (rightType->GetType())
		{
		case Type::Type::Builtin:
			if (const auto builtinRightType = rightType.UnsafeCast<Type::BuiltinType>(); builtinRightType->IsIntegerType())
			{
				return leftType;
			}
			// TODO: 报告错误：不支持的操作
			return nullptr;
		case Type::Type::Pointer:
			return m_Context.GetPtrDiffType();
		case Type::Type::Array:
		case Type::Type::Function:
			// TODO: 报告错误：不支持的操作
			return nullptr;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			break;
		}
		break;
	}
	case Type::Type::Array:
	case Type::Type::Function:
		// TODO: 报告错误：不支持的操作
		return nullptr;
	case Type::Type::Class:
		break;
	case Type::Type::Enum:
		break;
	default:
		assert(!"Invalid type");
		return nullptr;
	}

	nat_Throw(NotImplementedException);
}

Type::TypePtr Sema::UsualArithmeticConversions(Expression::ExprPtr& leftOperand, Expression::ExprPtr& rightOperand)
{
	assert(Type::Type::GetUnderlyingType(leftOperand->GetExprType())->GetType() == Type::Type::Builtin &&
		Type::Type::GetUnderlyingType(rightOperand->GetExprType())->GetType() == Type::Type::Builtin);
	auto leftType = Type::Type::GetUnderlyingType(leftOperand->GetExprType()).UnsafeCast<Type::BuiltinType>(),
	     rightType = Type::Type::GetUnderlyingType(rightOperand->GetExprType()).UnsafeCast<Type::BuiltinType>();

	if (leftType == rightType)
	{
		return leftType;
	}

	if (leftType->IsFloatingType() || rightType->IsFloatingType())
	{
		return handleFloatConversion(leftOperand, std::move(leftType), rightOperand, std::move(rightType));
	}

	if (leftType->IsIntegerType() && rightType->IsIntegerType())
	{
		return handleIntegerConversion(leftOperand, std::move(leftType), rightOperand, std::move(rightType));
	}

	// TODO: 报告错误：无法对操作数进行常用算术转换
	return nullptr;
}

Expression::ExprPtr Sema::ImpCastExprToType(Expression::ExprPtr expr, Type::TypePtr type, Expression::CastType castType)
{
	const auto exprType = expr->GetExprType();
	if (exprType == type || castType == Expression::CastType::NoOp)
	{
		return expr;
	}

	if (auto impCastExpr = expr.Cast<Expression::ImplicitCastExpr>())
	{
		if (impCastExpr->GetCastType() == castType)
		{
			impCastExpr->SetExprType(std::move(type));
			return expr;
		}
	}

	return make_ref<Expression::ImplicitCastExpr>(std::move(type), castType, std::move(expr));
}

nBool Sema::CheckFunctionReturn(Statement::StmtEnumerable const& funcBody)
{
	for (auto&& stmt : funcBody)
	{
		if (stmt->GetType() == Statement::Stmt::ReturnStmtClass)
		{
			return true;
		}

		switch (stmt->GetType())
		{
		case Statement::Stmt::CatchStmtClass:
		case Statement::Stmt::TryStmtClass:
		case Statement::Stmt::CompoundStmtClass:
		case Statement::Stmt::DeclStmtClass:
		case Statement::Stmt::DoStmtClass:
		case Statement::Stmt::ForStmtClass:
		case Statement::Stmt::IfStmtClass:
		case Statement::Stmt::LabelStmtClass:
		case Statement::Stmt::CaseStmtClass:
		case Statement::Stmt::DefaultStmtClass:
		case Statement::Stmt::SwitchStmtClass:
		case Statement::Stmt::WhileStmtClass:
		{
			const auto childrenStmt = stmt->GetChildrenStmt();
			if (!childrenStmt.empty() && CheckFunctionReturn(childrenStmt))
			{
				return true;
			}
			break;
		}
		case Statement::Stmt::ReturnStmtClass:
			return true;
		case Statement::Stmt::BreakStmtClass:
		case Statement::Stmt::ContinueStmtClass:
			// TODO
		default:
			break;
		}
	}

	return false;
}

nBool Sema::CheckFunctionOverload(natRefPointer<Type::FunctionType> const& func, Linq<Valued<natRefPointer<Declaration::NamedDecl>>> const& overloadSet)
{
	for (auto const& overloadDecl : overloadSet)
	{
		const auto overloadFunc = overloadDecl.Cast<Declaration::FunctionDecl>();
		assert(overloadFunc);
		const auto overloadFuncType = overloadFunc->GetValueType().UnsafeCast<Type::FunctionType>();
		if (overloadFuncType->HasVarArg() == func->HasVarArg() && overloadFuncType->GetParameterCount() == func->GetParameterCount())
		{
			const auto overloadFuncParams = overloadFuncType->GetParameterTypes();
			const auto funcParams = func->GetParameterTypes();
			if (std::equal(overloadFuncParams.begin(), overloadFuncParams.end(), funcParams.begin(), funcParams.end()))
			{
				return false;
			}
		}
	}

	return true;
}

void Sema::RegisterAttributeSerializer(nString attributeName, natRefPointer<IAttributeSerializer> serializer)
{
	m_AttributeSerializerMap.emplace(std::move(attributeName), std::move(serializer));
}

void Sema::SerializeAttribute(natRefPointer<Declaration::IAttribute> const& attr,
                              natRefPointer<ISerializationArchiveWriter> const& writer)
{
	if (const auto iter = m_AttributeSerializerMap.find(attr->GetName()); iter != m_AttributeSerializerMap.cend())
	{
		iter->second->Serialize(attr, writer);
	}
	else
	{
		nat_Throw(natErrException, NatErr::NatErr_NotFound, u8"No serializer found for attribute \"{0}\""_nv, attr->GetName());
	}
}

natRefPointer<Declaration::IAttribute> Sema::DeserializeAttribute(nStrView attributeName,
                                                                  natRefPointer<ISerializationArchiveReader> const& reader)
{
	if (const auto iter = m_AttributeSerializerMap.find(attributeName); iter != m_AttributeSerializerMap.cend())
	{
		return iter->second->Deserialize(reader);
	}

	nat_Throw(natErrException, NatErr::NatErr_NotFound, u8"No serializer found for attribute \"{0}\""_nv, attributeName);
}

nBool Sema::IsTypeDefaultConstructible(Type::TypePtr const& type)
{
	const auto underlyingType = Type::Type::GetUnderlyingType(type);

	switch (underlyingType->GetType())
	{
	case Type::Type::Builtin:
	case Type::Type::Pointer:
	case Type::Type::Enum:
		return true;
	case Type::Type::Array:
		return IsTypeDefaultConstructible(underlyingType.UnsafeCast<Type::ArrayType>()->GetElementType());
	case Type::Type::Class:
	{
		const auto classDecl = underlyingType.UnsafeCast<Type::ClassType>()->GetDecl().UnsafeCast<Declaration::ClassDecl>();
		LookupResult r{ *this, nullptr, {}, LookupNameType::LookupMemberName };
		if (!LookupConstructors(r, classDecl) || r.GetResultType() != LookupResult::LookupResultType::Found)
		{
			// TODO: 应该生成默认构造函数，但是没有
			return false;
		}

		return r.GetDecls().select([](natRefPointer<Declaration::NamedDecl> const& decl)
		{
			return decl.UnsafeCast<Declaration::ConstructorDecl>();
		}).first_or_default(nullptr, [](natRefPointer<Declaration::ConstructorDecl> const& constructor)
		{
			return constructor->GetParamCount() == 0;
		});
	}
	default:
		assert(!"Invalid type");[[fallthrough]];
	case Type::Type::Function:
		return false;
	}
}

nString Sema::GetQualifiedName(natRefPointer<Declaration::NamedDecl> const& namedDecl)
{
	assert(namedDecl);

	if (const auto iter = m_DeclQualifiedNameCache.find(namedDecl); iter != m_DeclQualifiedNameCache.cend())
	{
		return iter->second;
	}

	if (!m_NameBuilder)
	{
		UseDefaultNameBuilder();
	}

	auto qualifiedName = m_NameBuilder->GetQualifiedName(namedDecl);
	m_DeclQualifiedNameCache.emplace(namedDecl, qualifiedName);
	return qualifiedName;
}

void Sema::ClearDeclQualifiedNameCache(NatsuLib::natRefPointer<Declaration::NamedDecl> const& namedDecl)
{
	if (namedDecl)
	{
		m_DeclQualifiedNameCache.erase(namedDecl);
	}
	else
	{
		m_DeclQualifiedNameCache.clear();
	}
}

nString Sema::GetTypeName(Type::TypePtr const& type)
{
	const auto realType = Type::Type::GetUnderlyingType(type);
	assert(realType);

	if (const auto iter = m_TypeNameCache.find(realType); iter != m_TypeNameCache.cend())
	{
		return iter->second;
	}

	if (!m_NameBuilder)
	{
		UseDefaultNameBuilder();
	}

	auto typeName = m_NameBuilder->GetTypeName(realType);
	m_TypeNameCache.emplace(realType, typeName);
	return typeName;
}

void Sema::ClearTypeNameCache(Type::TypePtr const& type)
{
	if (type)
	{
		m_TypeNameCache.erase(type);
	}
	else
	{
		m_TypeNameCache.clear();
	}
}

void Sema::prewarming()
{
	m_TopLevelActionNamespace->RegisterSubNamespace(u8"Compiler"_nv);
	const auto compilerNamespace = m_TopLevelActionNamespace->GetSubNamespace(u8"Compiler"_nv);
	assert(compilerNamespace);
	compilerNamespace->RegisterAction(make_ref<ActionDump>());
	compilerNamespace->RegisterAction(make_ref<ActionDumpIf>());
	compilerNamespace->RegisterAction(make_ref<ActionIsDefined>());
	compilerNamespace->RegisterAction(make_ref<ActionTypeOf>());
	compilerNamespace->RegisterAction(make_ref<ActionSizeOf>());
	compilerNamespace->RegisterAction(make_ref<ActionAlignOf>());
	compilerNamespace->RegisterAction(make_ref<ActionCreateAt>());
	compilerNamespace->RegisterAction(make_ref<ActionDestroyAt>());

	m_ImportedAttribute = make_ref<ImportedAttribute>();
	RegisterAttributeSerializer(u8"Imported"_nv, make_ref<ImportedAttributeSerializer>(*this));
}

Expression::CastType Sema::getCastType(Expression::ExprPtr const& operand, Type::TypePtr toType, nBool isImplicit)
{
	toType = Type::Type::GetUnderlyingType(toType);
	auto fromType = Type::Type::GetUnderlyingType(operand->GetExprType());

Begin:

	assert(operand && toType);
	assert(fromType);

	if (fromType == toType)
	{
		return Expression::CastType::NoOp;
	}

	// TODO
	switch (fromType->GetType())
	{
	case Type::Type::Builtin:
	{
		const auto builtinFromType = fromType.UnsafeCast<Type::BuiltinType>();

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
		case Type::Type::Pointer:
			if (builtinFromType->GetBuiltinClass() == Type::BuiltinType::Null)
			{
				// 其实应该是 NoOp，不过后端需要 Convert
				return Expression::CastType::BitCast;
			}
			return !isImplicit && builtinFromType->IsIntegerType()
				       ? Expression::CastType::BitCast
				       : Expression::CastType::Invalid;
		case Type::Type::Auto:
		case Type::Type::Array:
		case Type::Type::Function:
		case Type::Type::Paren:
		default:
			return Expression::CastType::Invalid;
		}
	}
	case Type::Type::Pointer:
		if (isImplicit)
		{
			return Expression::CastType::Invalid;
		}

		// TODO: 转换到 bool 也需要显式转换吗？
		switch (toType->GetType())
		{
		case Type::Type::Builtin:
		{
			const auto builtinToType = toType.UnsafeCast<Type::BuiltinType>();
			return builtinToType->IsIntegerType() ? Expression::CastType::BitCast : Expression::CastType::Invalid;
		}
		case Type::Type::Pointer:
			return Expression::CastType::BitCast;
		case Type::Type::Array:
			break;
		case Type::Type::Function:
			break;
		case Type::Type::Class:
			break;
		case Type::Type::Enum:
			break;
		default:
			break;
		}
		break;
	case Type::Type::Array:
		break;
	case Type::Type::Function:
		break;
	case Type::Type::Class:
		break;
	case Type::Type::Enum:
		fromType = fromType.UnsafeCast<Type::EnumType>()->GetDecl().UnsafeCast<Declaration::EnumDecl>()->GetUnderlyingType();
		goto Begin;
	case Type::Type::Paren:
	case Type::Type::Auto:
	case Type::Type::Unresolved:
	default:
		assert(!"Invalid type");
		return Expression::CastType::Invalid;
	}

	// TODO
	nat_Throw(NotImplementedException);
}

Type::TypePtr Sema::handleIntegerConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType,
                                            Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
{
	const auto builtinLHSType = leftOperandType.Cast<Type::BuiltinType>(), builtinRHSType = rightOperandType.Cast<Type::
		           BuiltinType>();

	if (!builtinLHSType || !builtinRHSType)
	{
		// TODO: 报告错误
		return nullptr;
	}

	if (builtinLHSType == builtinRHSType)
	{
		// 无需转换
		return leftOperandType;
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
			return rightOperandType;
		}

		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralCast);
		return leftOperandType;
	}

	if (compareResult > 0)
	{
		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralCast);
		return leftOperandType;
	}

	leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::IntegralCast);
	return rightOperandType;
}

Type::TypePtr Sema::handleFloatConversion(Expression::ExprPtr& leftOperand, Type::TypePtr leftOperandType,
                                          Expression::ExprPtr& rightOperand, Type::TypePtr rightOperandType)
{
	auto builtinLHSType = leftOperandType.Cast<Type::BuiltinType>(), builtinRHSType = rightOperandType.Cast<Type::
		     BuiltinType>();

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
			return leftOperandType;
		}

		leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::FloatingCast);
		return rightOperandType;
	}

	if (lhsFloat)
	{
		rightOperand = ImpCastExprToType(std::move(rightOperand), leftOperandType, Expression::CastType::IntegralToFloating);
		return leftOperandType;
	}

	assert(rhsFloat);
	leftOperand = ImpCastExprToType(std::move(leftOperand), rightOperandType, Expression::CastType::IntegralToFloating);
	return rightOperandType;
}

Linq<Valued<Expression::ExprPtr>> Sema::makeArgsFromType(
	Linq<Valued<Expression::ExprPtr>> const& argExprs, std::size_t argCount,
	Linq<Valued<Type::TypePtr>> const& paramTypes, std::size_t paramCount)
{
	const auto commonCount = std::min(paramCount, argCount);
	return argExprs.take(commonCount).zip(paramTypes.take(commonCount)).select(
		[this](std::pair<Expression::ExprPtr, Type::TypePtr> const& pair)
		{
			const auto castType = getCastType(pair.first, pair.second, true);
			return ImpCastExprToType(
				pair.first, pair.second, castType);
		}).concat(argExprs.skip(std::min(commonCount, argCount)));
}

Linq<Valued<Expression::ExprPtr>> Sema::makeArgsFromDecl(
	Linq<Valued<Expression::ExprPtr>> const& argExprs, std::size_t argCount,
	Linq<Valued<natRefPointer<Declaration::ParmVarDecl>>> const& paramDecls,
	std::size_t paramCount)
{
	const auto commonCount = std::min(paramCount, argCount);

	return makeArgsFromType(argExprs, argCount, paramDecls.select([](natRefPointer<Declaration::ParmVarDecl> const& param)
	{
		return param->GetValueType();
	}), paramCount).concat(paramDecls.skip(commonCount).select([](natRefPointer<Declaration::ParmVarDecl> const& param)
	{
		return param->GetInitializer();
	}));
}

LookupResult::LookupResult(Sema& sema, Identifier::IdPtr id, SourceLocation loc, Sema::LookupNameType lookupNameType,
                           nBool isCodeCompletion)
	: m_Sema{ sema }, m_IsCodeCompletion{ isCodeCompletion }, m_LookupId{ std::move(id) }, m_LookupLoc{ loc },
	  m_LookupNameType{ lookupNameType },
	  m_IDNS{ chooseIDNS(m_LookupNameType) }, m_Result{}, m_AmbiguousType{}
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
		return decl.Cast<Declaration::FunctionDecl>();
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
