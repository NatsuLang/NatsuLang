#pragma once
#include <unordered_set>
#include <unordered_map>
#include "Declaration.h"
#include "Type.h"
#include "NestedNameSpecifier.h"
#include "Basic/TargetInfo.h"

namespace NatsuLang
{
	class ASTContext
		: public NatsuLib::natRefObjImpl<ASTContext>
	{
	public:
		struct TypeInfo
		{
			std::size_t Size;
			std::size_t Align;
		};

		struct ClassLayout
		{
			std::size_t Size;
			std::size_t Align;
			// 若 first 是 null 则表示的是 padding
			std::vector<std::pair<NatsuLib::natRefPointer<Declaration::FieldDecl>, std::size_t>> FieldOffsets;

			// 返回值：字段索引，字段偏移
			std::optional<std::pair<std::size_t, std::size_t>> GetFieldInfo(NatsuLib::natRefPointer<Declaration::FieldDecl> const& field) const noexcept;
		};

		friend class NestedNameSpecifier;

		explicit ASTContext(TargetInfo targetInfo);
		~ASTContext();

		NatsuLib::natRefPointer<Type::BuiltinType> GetBuiltinType(Type::BuiltinType::BuiltinClass builtinClass);
		NatsuLib::natRefPointer<Type::BuiltinType> GetSizeType();
		NatsuLib::natRefPointer<Type::BuiltinType> GetPtrDiffType();
		NatsuLib::natRefPointer<Type::ArrayType> GetArrayType(Type::TypePtr elementType, std::size_t arraySize);
		NatsuLib::natRefPointer<Type::PointerType> GetPointerType(Type::TypePtr pointeeType);
		NatsuLib::natRefPointer<Type::FunctionType> GetFunctionType(NatsuLib::Linq<NatsuLib::Valued<Type::TypePtr>> const& params, Type::TypePtr retType, nBool hasVarArg);
		NatsuLib::natRefPointer<Type::ParenType> GetParenType(Type::TypePtr innerType);
		NatsuLib::natRefPointer<Type::AutoType> GetAutoType(Type::TypePtr deducedAsType);
		NatsuLib::natRefPointer<Type::UnresolvedType> GetUnresolvedType(std::vector<Lex::Token>&& tokens);

		NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> GetTranslationUnit() const noexcept;

		TypeInfo GetTypeInfo(Type::TypePtr const& type);
		ClassLayout const& GetClassLayout(NatsuLib::natRefPointer<Declaration::ClassDecl> const& classDecl);

	private:
		TargetInfo m_TargetInfo;

		NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> m_TUDecl;

		template <typename T>
		using TypeSet = std::unordered_set<NatsuLib::natRefPointer<T>, Type::TypeHash, Type::TypeEqualTo>;

		TypeSet<Type::ArrayType> m_ArrayTypes;
		TypeSet<Type::PointerType> m_PointerTypes;
		TypeSet<Type::FunctionType> m_FunctionTypes;
		TypeSet<Type::ParenType> m_ParenTypes;
		TypeSet<Type::AutoType> m_AutoTypes;
		TypeSet<Type::UnresolvedType> m_UnresolvedTypes;

		mutable std::unordered_set<NatsuLib::natRefPointer<NestedNameSpecifier>, NestedNameSpecifier::Hash, NestedNameSpecifier::EqualTo> m_NestedNameSpecifiers;

		std::unordered_map<Type::TypePtr, TypeInfo> m_CachedTypeInfo;
		std::unordered_map<NatsuLib::natRefPointer<Declaration::ClassDecl>, ClassLayout> m_CachedClassLayout;
		std::unordered_map<Type::BuiltinType::BuiltinClass, NatsuLib::natRefPointer<Type::BuiltinType>> m_BuiltinTypeMap;
		NatsuLib::natRefPointer<Type::BuiltinType> m_SizeType, m_PtrDiffType;

		TypeInfo getTypeInfoImpl(Type::TypePtr const& type);
	};
}
