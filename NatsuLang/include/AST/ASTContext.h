#pragma once
#include <unordered_set>
#include <unordered_map>
#include "Declaration.h"
#include "Type.h"

namespace NatsuLang
{
	class ASTContext
		: public NatsuLib::natRefObjImpl<ASTContext>
	{
	public:
		ASTContext();
		~ASTContext();

		NatsuLib::natRefPointer<Type::BuiltinType>& GetBuiltinType(Type::BuiltinType::BuiltinClass builtinClass);
		NatsuLib::natRefPointer<Type::ArrayType> GetArrayType(Type::TypePtr elementType, std::size_t arraySize);
		NatsuLib::natRefPointer<Type::FunctionType> GetFunctionType(NatsuLib::Linq<Type::TypePtr> const& params, Type::TypePtr retType);
		NatsuLib::natRefPointer<Type::ParenType> GetParenType(Type::TypePtr innerType);
		NatsuLib::natRefPointer<Type::AutoType> GetAutoType(Type::TypePtr deducedAsType);

	private:
		struct TypeInfo
		{
			std::size_t Size;
			std::size_t Align;
		};

		template <typename T>
		using TypeSet = std::unordered_set<NatsuLib::natRefPointer<T>, Type::TypeHash, Type::TypeEqualTo>;

		TypeSet<Type::ArrayType> m_ArrayTypes;
		TypeSet<Type::FunctionType> m_FunctionTypes;
		TypeSet<Type::ParenType> m_ParenTypes;
		TypeSet<Type::AutoType> m_AutoTypes;

		std::unordered_map<Type::TypePtr, TypeInfo> m_CachedTypeInfo;
		std::unordered_map<Type::BuiltinType::BuiltinClass, NatsuLib::natRefPointer<Type::BuiltinType>> m_BuiltinTypeMap;
	};
}
