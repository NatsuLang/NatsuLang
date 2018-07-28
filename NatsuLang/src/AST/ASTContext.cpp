#include "AST/ASTContext.h"
#include "AST/Expression.h"
#include "Basic/Identifier.h"

#undef max

using namespace NatsuLib;
using namespace NatsuLang;

namespace
{
	constexpr std::size_t AlignTo(std::size_t size, std::size_t alignment) noexcept
	{
		return (size + alignment - 1) / alignment * alignment;
	}

	constexpr ASTContext::TypeInfo GetBuiltinTypeInfo(Type::BuiltinType::BuiltinClass type) noexcept
	{
		switch (type)
		{
		case Type::BuiltinType::Void:
			return { 0, 4 };
		case Type::BuiltinType::Bool:
			return { 1, 1 };
		case Type::BuiltinType::Char:
			return { 1, 1 };
		case Type::BuiltinType::Byte:
		case Type::BuiltinType::SByte:
			return { 1, 1 };
		case Type::BuiltinType::UShort:
		case Type::BuiltinType::Short:
			return { 2, 2 };
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
		default:
			return { 0, 0 };
		}
	}

	class DefaultClassLayoutBuilder
		: public natRefObjImpl<DefaultClassLayoutBuilder, ASTContext::IClassLayoutBuilder>
	{
	public:
		ASTContext::ClassLayout BuildLayout(ASTContext& context, natRefPointer<Declaration::ClassDecl> const& classDecl) override
		{
			// 允许 0 大小对象将会允许对象具有相同的地址
			ASTContext::ClassLayout info{};
			for (auto const& field : classDecl->GetFields())
			{
				const auto fieldInfo = context.GetTypeInfo(field->GetValueType());
				info.Align = std::max(fieldInfo.Align, info.Align);
				const auto fieldOffset = AlignTo(info.Size, fieldInfo.Align);
				if (fieldOffset != info.Size)
				{
					// 插入 padding
					info.FieldOffsets.emplace_back(nullptr, info.Size);
				}
				info.FieldOffsets.emplace_back(field, fieldOffset);
				info.Size = fieldOffset + fieldInfo.Size;
			}

			if (info.Align)
			{
				info.Size = AlignTo(info.Size, info.Align);
			}

			return info;
		}
	};
}

std::optional<std::pair<std::size_t, std::size_t>> ASTContext::ClassLayout::GetFieldInfo(natRefPointer<Declaration::FieldDecl> const& field) const noexcept
{
	const auto iter = std::find_if(FieldOffsets.cbegin(), FieldOffsets.cend(), [&field](std::pair<natRefPointer<Declaration::FieldDecl>, std::size_t> const& pair)
	{
		return pair.first == field;
	});

	if (iter == FieldOffsets.cend())
	{
		return {};
	}

	return std::optional<std::pair<std::size_t, std::size_t>>{ std::in_place, std::distance(FieldOffsets.cbegin(), iter), iter->second };
}

ASTContext::IClassLayoutBuilder::~IClassLayoutBuilder()
{
}

ASTContext::ASTContext()
	: m_TUDecl{ make_ref<Declaration::TranslationUnitDecl>(*this) }
{
}

ASTContext::ASTContext(TargetInfo targetInfo)
	: m_TUDecl{ make_ref<Declaration::TranslationUnitDecl>(*this) }
{
	m_TargetInfo.Init(targetInfo);
}

ASTContext::~ASTContext()
{
}

void ASTContext::SetTargetInfo(TargetInfo targetInfo) noexcept
{
	m_TargetInfo.Init(targetInfo);
}

TargetInfo ASTContext::GetTargetInfo() const noexcept
{
	return m_TargetInfo.Get();
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

natRefPointer<Type::BuiltinType> ASTContext::GetSizeType()
{
	if (m_SizeType)
	{
		return m_SizeType;
	}

	const auto ptrSize = m_TargetInfo.Get().GetPointerSize(), ptrAlign = m_TargetInfo.Get().GetPointerAlign();

	const auto builtinClass = GetIntegerTypeAtLeast(ptrSize, ptrAlign, false);

	if (builtinClass == Type::BuiltinType::Invalid)
	{
		assert(!"Not found");
		return nullptr;
	}

	m_SizeType = GetBuiltinType(Type::BuiltinType::MakeUnsignedBuiltinClass(builtinClass));
	return m_SizeType;
}

natRefPointer<Type::BuiltinType> ASTContext::GetPtrDiffType()
{
	if (m_PtrDiffType)
	{
		return m_PtrDiffType;
	}

	const auto ptrSize = m_TargetInfo.Get().GetPointerSize(), ptrAlign = m_TargetInfo.Get().GetPointerAlign();

	const auto builtinClass = GetIntegerTypeAtLeast(ptrSize, ptrAlign, false);

	if (builtinClass == Type::BuiltinType::Invalid)
	{
		assert(!"Not found");
		return nullptr;
	}

	m_PtrDiffType = GetBuiltinType(Type::BuiltinType::MakeSignedBuiltinClass(builtinClass));
	return m_PtrDiffType;
}

Type::BuiltinType::BuiltinClass ASTContext::GetIntegerTypeAtLeast(std::size_t size, std::size_t alignment, nBool exactly)
{
	using BuiltinType = std::underlying_type_t<Type::BuiltinType::BuiltinClass>;

	for (auto i = static_cast<BuiltinType>(Type::BuiltinType::Invalid) + 1; i < static_cast<BuiltinType>(Type::BuiltinType::BuiltinClass::LastType); ++i)
	{
		const auto typeInfo = GetBuiltinTypeInfo(static_cast<Type::BuiltinType::BuiltinClass>(i));
		if (exactly ? typeInfo.Size == size && typeInfo.Align == alignment : typeInfo.Size >= size && typeInfo.Align >= alignment)
		{
			return static_cast<Type::BuiltinType::BuiltinClass>(i);
		}
	}

	return Type::BuiltinType::Invalid;
}

natRefPointer<Type::ArrayType> ASTContext::GetArrayType(Type::TypePtr elementType, nuLong arraySize)
{
	// 能否省去此次构造？
	// 对于 set 直接 emplace 即可，若换成 map 则恢复之前的写法
	const auto ret = m_ArrayTypes.emplace(make_ref<Type::ArrayType>(std::move(elementType), arraySize));
	return *ret.first;
}

natRefPointer<Type::PointerType> ASTContext::GetPointerType(Type::TypePtr pointeeType)
{
	const auto ret = m_PointerTypes.emplace(make_ref<Type::PointerType>(std::move(pointeeType)));
	return *ret.first;
}

natRefPointer<Type::FunctionType> ASTContext::GetFunctionType(Linq<Valued<Type::TypePtr>> const& params, Type::TypePtr retType, nBool hasVarArg)
{
	const auto ret = m_FunctionTypes.emplace(make_ref<Type::FunctionType>(params, std::move(retType), hasVarArg));
	return *ret.first;
}

natRefPointer<Type::ParenType> ASTContext::GetParenType(Type::TypePtr innerType)
{
	const auto ret = m_ParenTypes.emplace(make_ref<Type::ParenType>(std::move(innerType)));
	return *ret.first;
}

natRefPointer<Type::AutoType> ASTContext::GetAutoType(Type::TypePtr deducedAsType)
{
	const auto ret = m_AutoTypes.emplace(make_ref<Type::AutoType>(std::move(deducedAsType)));
	return *ret.first;
}

natRefPointer<Type::UnresolvedType> ASTContext::GetUnresolvedType(std::vector<Lex::Token>&& tokens)
{
	const auto ret = m_UnresolvedTypes.emplace(make_ref<Type::UnresolvedType>(std::move(tokens)));
	return *ret.first;
}

void ASTContext::EraseType(const Type::TypePtr& type)
{
	switch (type->GetType())
	{
	case Type::Type::Pointer:
		m_PointerTypes.erase(type);
		break;
	case Type::Type::Array:
		m_ArrayTypes.erase(type);
		break;
	case Type::Type::Function:
		m_FunctionTypes.erase(type);
		break;
	case Type::Type::Paren:
		m_ParenTypes.erase(type);
		break;
	case Type::Type::Auto:
		m_AutoTypes.erase(type);
		break;
	case Type::Type::Unresolved:
		m_UnresolvedTypes.erase(type);
		break;
	case Type::Type::Builtin:
	case Type::Type::Class:
	case Type::Type::Enum:
		assert(!"These types will not need update.");
		[[fallthrough]];
	default:
		break;
	}
}

void ASTContext::CacheType(Type::TypePtr type)
{
	switch (type->GetType())
	{
	case Type::Type::Pointer:
		m_PointerTypes.emplace(std::move(type));
		break;
	case Type::Type::Array:
		m_ArrayTypes.emplace(std::move(type));
		break;
	case Type::Type::Function:
		m_FunctionTypes.emplace(std::move(type));
		break;
	case Type::Type::Paren:
		m_ParenTypes.emplace(std::move(type));
		break;
	case Type::Type::Auto:
		m_AutoTypes.emplace(std::move(type));
		break;
	case Type::Type::Unresolved:
		m_UnresolvedTypes.emplace(std::move(type));
		break;
	case Type::Type::Builtin:
	case Type::Type::Class:
	case Type::Type::Enum:
		assert(!"These types will not need update.");
		[[fallthrough]];
	default:
		break;
	}
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

void ASTContext::UseDefaultClassLayoutBuilder()
{
	m_ClassLayoutBuilder = make_ref<DefaultClassLayoutBuilder>();
}

void ASTContext::UseCustomClassLayoutBuilder(natRefPointer<IClassLayoutBuilder> classLayoutBuilder)
{
	m_ClassLayoutBuilder = std::move(classLayoutBuilder);
}

ASTContext::ClassLayout const& ASTContext::GetClassLayout(natRefPointer<Declaration::ClassDecl> const& classDecl)
{
	assert(classDecl);

	const auto layoutIter = m_CachedClassLayout.find(classDecl);
	if (layoutIter != m_CachedClassLayout.end())
	{
		return layoutIter->second;
	}

	if (!m_ClassLayoutBuilder)
	{
		UseDefaultClassLayoutBuilder();
	}

	const auto ret = m_CachedClassLayout.emplace(classDecl, m_ClassLayoutBuilder->BuildLayout(*this, classDecl));
	if (!ret.second)
	{
		nat_Throw(natErrException, NatErr::NatErr_InternalErr, u8"Cannot insert class layout"_nv);
	}

	return ret.first->second;
}

// TODO: 使用编译目标的值
ASTContext::TypeInfo ASTContext::getTypeInfoImpl(Type::TypePtr const& type)
{
	switch (type->GetType())
	{
	case Type::Type::Builtin:
		return GetBuiltinTypeInfo(type.UnsafeCast<Type::BuiltinType>()->GetBuiltinClass());
	case Type::Type::Pointer:
		return { m_TargetInfo.Get().GetPointerSize(), m_TargetInfo.Get().GetPointerAlign() };
	case Type::Type::Array:
	{
		const auto arrayType = type.UnsafeCast<Type::ArrayType>();
		auto elemInfo = GetTypeInfo(arrayType->GetElementType());
		elemInfo.Size *= static_cast<nuLong>(arrayType->GetSize());
		return elemInfo;
	}
	case Type::Type::Function:
		return { 0, 0 };
	case Type::Type::Class:
	{
		const auto classLayout = GetClassLayout(type.UnsafeCast<Type::ClassType>()->GetDecl());
		return { classLayout.Size, classLayout.Align };
	}
	case Type::Type::Enum:
	{
		const auto enumType = type.UnsafeCast<Type::EnumType>();
		const auto enumDecl = enumType->GetDecl().Cast<Declaration::EnumDecl>();
		assert(enumDecl);
		return GetTypeInfo(enumDecl->GetUnderlyingType());
	}
	default:
		assert(!"Invalid type.");
		std::terminate();
	}
}
