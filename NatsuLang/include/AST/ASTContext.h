﻿#pragma once
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

		struct IClassLayoutBuilder
			: NatsuLib::natRefObj
		{
			virtual ~IClassLayoutBuilder();

			virtual ClassLayout BuildLayout(ASTContext& context, NatsuLib::natRefPointer<Declaration::ClassDecl> const& classDecl) = 0;
		};

		friend class NestedNameSpecifier;

		ASTContext();
		explicit ASTContext(TargetInfo targetInfo);
		~ASTContext();

		// 禁止调用多次，用户自行检查
		void SetTargetInfo(TargetInfo targetInfo) noexcept;
		TargetInfo GetTargetInfo() const noexcept;

		NatsuLib::natRefPointer<Type::BuiltinType> GetBuiltinType(Type::BuiltinType::BuiltinClass builtinClass);
		NatsuLib::natRefPointer<Type::BuiltinType> GetSizeType();
		NatsuLib::natRefPointer<Type::BuiltinType> GetPtrDiffType();
		Type::BuiltinType::BuiltinClass GetIntegerTypeAtLeast(std::size_t size, std::size_t alignment, nBool exactly = false);
		NatsuLib::natRefPointer<Type::ArrayType> GetArrayType(Type::TypePtr elementType, nuLong arraySize);
		NatsuLib::natRefPointer<Type::PointerType> GetPointerType(Type::TypePtr pointeeType);
		NatsuLib::natRefPointer<Type::FunctionType> GetFunctionType(NatsuLib::Linq<NatsuLib::Valued<Type::TypePtr>> const& params, Type::TypePtr retType, nBool hasVarArg);
		NatsuLib::natRefPointer<Type::ParenType> GetParenType(Type::TypePtr innerType);
		NatsuLib::natRefPointer<Type::AutoType> GetAutoType(Type::TypePtr deducedAsType);
		NatsuLib::natRefPointer<Type::UnresolvedType> GetUnresolvedType(std::vector<Lex::Token>&& tokens);

		// 以下两个方法仅用于修改类型内容后更新缓存
		void EraseType(const Type::TypePtr& type);
		void CacheType(Type::TypePtr type);

		NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> GetTranslationUnit() const noexcept;

		///	@remark	由使用者保证 type 已经经过 Type::GetUnderlyingType
		TypeInfo GetTypeInfo(Type::TypePtr const& type);

		void UseDefaultClassLayoutBuilder();
		void UseCustomClassLayoutBuilder(NatsuLib::natRefPointer<IClassLayoutBuilder> classLayoutBuilder);
		ClassLayout const& GetClassLayout(NatsuLib::natRefPointer<Declaration::ClassDecl> const& classDecl);

	private:
		NatsuLib::UncheckedLazyInit<TargetInfo> m_TargetInfo;

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
		NatsuLib::natRefPointer<IClassLayoutBuilder> m_ClassLayoutBuilder;
		std::unordered_map<NatsuLib::natRefPointer<Declaration::ClassDecl>, ClassLayout> m_CachedClassLayout;
		std::unordered_map<Type::BuiltinType::BuiltinClass, NatsuLib::natRefPointer<Type::BuiltinType>> m_BuiltinTypeMap;
		NatsuLib::natRefPointer<Type::BuiltinType> m_SizeType, m_PtrDiffType;

		TypeInfo getTypeInfoImpl(Type::TypePtr const& type);
	};
}
