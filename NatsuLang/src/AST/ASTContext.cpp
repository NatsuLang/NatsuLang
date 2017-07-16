#include "AST/ASTContext.h"

using namespace NatsuLib;
using namespace NatsuLang;

ASTContext::ASTContext()
{
}

ASTContext::~ASTContext()
{
}

natRefPointer<Type::BuiltinType>& ASTContext::GetBuiltinType(Type::BuiltinType::BuiltinClass builtinClass)
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

natRefPointer<Type::FunctionType> ASTContext::GetFunctionType(Linq<Type::TypePtr> const& params, Type::TypePtr retType)
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
