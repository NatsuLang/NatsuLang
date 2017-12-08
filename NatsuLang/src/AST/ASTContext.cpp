#include "AST/ASTContext.h"
#include "AST/Expression.h"
#include "Basic/Identifier.h"

using namespace NatsuLib;
using namespace NatsuLang;

ASTContext::ASTContext()
	: m_TUDecl{ make_ref<Declaration::TranslationUnitDecl>(*this) }
{
}

ASTContext::~ASTContext()
{
}

natRefPointer<Type::BuiltinType> ASTContext::GetBuiltinType(Type::BuiltinType::BuiltinClass builtinClass)
{
	decltype(auto) ptr = m_BuiltinTypeMap[builtinClass];
	if (!ptr)
	{
		ptr = make_ref<Type::BuiltinType>(builtinClass);
	}

	return ptr;
}

natRefPointer<Type::ArrayType> ASTContext::GetArrayType(Type::TypePtr elementType, std::size_t arraySize)
{
	// 能否省去此次构造？
	auto ret = make_ref<Type::ArrayType>(std::move(elementType), arraySize);
	const auto iter = m_ArrayTypes.find(ret);
	if (iter != m_ArrayTypes.end())
	{
		return *iter;
	}

	m_ArrayTypes.emplace(ret);
	return ret;
}

natRefPointer<Type::FunctionType> ASTContext::GetFunctionType(Linq<NatsuLib::Valued<Type::TypePtr>> const& params, Type::TypePtr retType)
{
	auto ret = make_ref<Type::FunctionType>(params, std::move(retType));
	const auto iter = m_FunctionTypes.find(ret);
	if (iter != m_FunctionTypes.end())
	{
		return *iter;
	}

	m_FunctionTypes.emplace(ret);
	return ret;
}

natRefPointer<Type::ParenType> ASTContext::GetParenType(Type::TypePtr innerType)
{
	auto ret = make_ref<Type::ParenType>(std::move(innerType));
	const auto iter = m_ParenTypes.find(ret);
	if (iter != m_ParenTypes.end())
	{
		return *iter;
	}

	m_ParenTypes.emplace(ret);
	return ret;
}

natRefPointer<Type::AutoType> ASTContext::GetAutoType(Type::TypePtr deducedAsType)
{
	auto ret = make_ref<Type::AutoType>(std::move(deducedAsType));
	const auto iter = m_AutoTypes.find(ret);
	if (iter != m_AutoTypes.end())
	{
		return *iter;
	}

	m_AutoTypes.emplace(ret);
	return ret;
}

natRefPointer<Type::UnresolvedType> ASTContext::GetUnresolvedType(std::vector<Lex::Token>&& tokens)
{
	auto ret = make_ref<Type::UnresolvedType>(std::move(tokens));
	const auto iter = m_UnresolvedTypes.find(ret);
	if (iter != m_UnresolvedTypes.end())
	{
		return *iter;
	}

	m_UnresolvedTypes.emplace(ret);
	return ret;
}

natRefPointer<Declaration::TranslationUnitDecl> ASTContext::GetTranslationUnit() const noexcept
{
	return m_TUDecl;
}

ASTContext::TypeInfo ASTContext::GetTypeInfo(Type::TypePtr const& type)
{
	auto underlyingType = Type::Type::GetUnderlyingType(type);
	const auto iter = m_CachedTypeInfo.find(underlyingType);
	if (iter != m_CachedTypeInfo.cend())
	{
		return iter->second;
	}

	auto info = getTypeInfoImpl(underlyingType);
	m_CachedTypeInfo.emplace(std::move(underlyingType), info);
	return info;
}

// TODO: 使用编译目标的值
ASTContext::TypeInfo ASTContext::getTypeInfoImpl(Type::TypePtr const& type)
{
	switch (type->GetType())
	{
	case Type::Type::Builtin:
	{
		const auto builtinType = type.Cast<Type::BuiltinType>();
		switch (builtinType->GetBuiltinClass())
		{
		case Type::BuiltinType::Void:
			return { 0, 4 };
		case Type::BuiltinType::Bool:
			return { 1, 4 };
		case Type::BuiltinType::Char:
			return { 1, 4 };
		case Type::BuiltinType::UShort:
		case Type::BuiltinType::Short:
			return { 2, 4 };
		case Type::BuiltinType::UInt:
		case Type::BuiltinType::Int:
			return { 4, 4 };
		case Type::BuiltinType::ULong:
		case Type::BuiltinType::Long:
			return { 8, 8 };
		case Type::BuiltinType::ULongLong:
		case Type::BuiltinType::LongLong:
			return { 16, 16 };
		case Type::BuiltinType::UInt128:
		case Type::BuiltinType::Int128:
			return { 16, 16 };
		case Type::BuiltinType::Float:
			return { 4, 4 };
		case Type::BuiltinType::Double:
			return { 8, 8 };
		case Type::BuiltinType::LongDouble:
			return { 16, 16 };
		case Type::BuiltinType::Float128:
			return { 16, 16 };
		case Type::BuiltinType::Overload:
		case Type::BuiltinType::BoundMember:
		case Type::BuiltinType::BuiltinFn:
		default:
			break;
		}
	}
	case Type::Type::Array:
	{
		const auto arrayType = type.Cast<Type::ArrayType>();
		auto elemInfo = GetTypeInfo(arrayType->GetElementType());
		elemInfo.Size *= arrayType->GetSize();
		return elemInfo;
	}
	case Type::Type::Function:
		return { 0, 0 };
	case Type::Type::Class:
		break;
	case Type::Type::Enum:
	{
		const auto enumType = type.Cast<Type::EnumType>();
		const auto enumDecl = enumType->GetDecl().Cast<Declaration::EnumDecl>();
		assert(enumDecl);
		return GetTypeInfo(enumDecl->GetUnderlyingType());
	}
	default:
		assert(!"Invalid type.");
		std::terminate();
	}

	nat_Throw(NotImplementedException);
}
