#include "Interpreter.h"

#undef min
#undef max

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

Interpreter::InterpreterDeclStorage::MemoryLocationDecl::~MemoryLocationDecl()
{
}

Interpreter::InterpreterDeclStorage::ArrayElementAccessor::ArrayElementAccessor(InterpreterDeclStorage& declStorage,
																				natRefPointer<Type::ArrayType> const& arrayType, nData storage)
	: m_DeclStorage{ declStorage }, m_ElementType{ arrayType->GetElementType() },
	m_ElementSize{ declStorage.m_Interpreter.m_AstContext.GetTypeInfo(m_ElementType).Size },
	m_ArrayElementCount{ static_cast<std::size_t>(arrayType->GetSize()) }, m_Storage{ storage }
{
}

Type::TypePtr Interpreter::InterpreterDeclStorage::ArrayElementAccessor::GetElementType() const noexcept
{
	return m_ElementType;
}

natRefPointer<Interpreter::InterpreterDeclStorage::MemoryLocationDecl> Interpreter::InterpreterDeclStorage::ArrayElementAccessor::GetElementDecl(std::size_t i) const
{
	Lex::Token dummy;
	return make_ref<MemoryLocationDecl>(m_ElementType, m_Storage + m_ElementSize * i, m_DeclStorage.m_Interpreter.m_Preprocessor.FindIdentifierInfo(natUtil::FormatString(u8"[{0}]"_nv, i), dummy));
}

nData Interpreter::InterpreterDeclStorage::ArrayElementAccessor::GetStorage() const noexcept
{
	return m_Storage;
}

Interpreter::InterpreterDeclStorage::MemberAccessor::MemberAccessor(InterpreterDeclStorage& declStorage,
																	natRefPointer<Declaration::ClassDecl>
																	classDecl, nData storage)
	: m_DeclStorage{ declStorage }, m_ClassDecl{ std::move(classDecl) }, m_Storage{ storage }
{
	auto&& layout = m_DeclStorage.m_Interpreter.m_AstContext.GetClassLayout(m_ClassDecl);

	auto offsetIter = layout.FieldOffsets.cbegin();
	const auto offsetEnd = layout.FieldOffsets.cend();

	for (; offsetIter != offsetEnd; ++offsetIter)
	{
		if (!offsetIter->first)
		{
			continue;
		}

		m_FieldOffsets.emplace(offsetIter->first, offsetIter->second);
	}
}

std::size_t Interpreter::InterpreterDeclStorage::MemberAccessor::GetFieldCount() const noexcept
{
	return m_FieldOffsets.size();
}

Linq<Valued<natRefPointer<Declaration::FieldDecl>>> Interpreter::InterpreterDeclStorage::MemberAccessor::GetFields() const noexcept
{
	return from(m_FieldOffsets).select([](std::pair<natRefPointer<Declaration::FieldDecl>, std::size_t> const& pair)
	{
		return pair.first;
	});
}

natRefPointer<Interpreter::InterpreterDeclStorage::MemoryLocationDecl> Interpreter::InterpreterDeclStorage::MemberAccessor::GetMemberDecl
	(natRefPointer<Declaration::FieldDecl> const& fieldDecl) const
{
	const auto iter = m_FieldOffsets.find(fieldDecl);
	if (iter == m_FieldOffsets.end())
	{
		return nullptr;
	}

	const auto offset = iter->second;
	return make_ref<MemoryLocationDecl>(fieldDecl->GetValueType(), m_Storage + offset, fieldDecl->GetIdentifierInfo());
}

Interpreter::InterpreterDeclStorage::PointerAccessor::PointerAccessor(nData storage)
	: m_Storage{ storage }
{
}

natRefPointer<Declaration::VarDecl> Interpreter::InterpreterDeclStorage::PointerAccessor::GetReferencedDecl() const noexcept
{
	return natRefPointer<Declaration::VarDecl>{ *reinterpret_cast<Declaration::VarDecl**>(m_Storage) };
}

void Interpreter::InterpreterDeclStorage::PointerAccessor::SetReferencedDecl(natRefPointer<Declaration::VarDecl> const& value) noexcept
{
	// FIXME: 未受引用计数保护的引用
	*reinterpret_cast<Declaration::VarDecl**>(m_Storage) = value.Get();
}

void Interpreter::InterpreterDeclStorage::StorageDeleter::operator()(nData data) const noexcept
{
#ifdef _MSC_VER
#	ifdef NDEBUG
	_aligned_free(data);
#	else
	_aligned_free_dbg(data);
#	endif
#else
	std::free(data);
#endif
}

Interpreter::InterpreterDeclStorage::InterpreterDeclStorage(Interpreter& interpreter)
	: m_Interpreter{ interpreter }
{
	PushStorage();
}

std::pair<nBool, nData> Interpreter::InterpreterDeclStorage::GetOrAddDecl(natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type)
{
	if (const auto memoryLocationDecl = decl.Cast<MemoryLocationDecl>())
	{
		return { false, memoryLocationDecl->GetMemoryLocation() };
	}

	if (!type)
	{
		type = decl->GetValueType();
	}

	assert(type);
	const auto typeInfo = m_Interpreter.m_AstContext.GetTypeInfo(type);

	auto topAvailableForCreateStorageIndex = std::numeric_limits<std::size_t>::max();

	for (auto storageIter = m_DeclStorage.rbegin(); storageIter != m_DeclStorage.rend(); ++storageIter)
	{
		if (HasAllFlags(storageIter->first, DeclStorageLevelFlag::AvailableForLookup))
		{
			const auto iter = storageIter->second->find(decl);
			if (iter != storageIter->second->cend())
			{
				return { false, iter->second.get() };
			}
		}

		if (topAvailableForCreateStorageIndex == std::numeric_limits<std::size_t>::max() &&
			HasAllFlags(storageIter->first, DeclStorageLevelFlag::AvailableForCreateStorage))
		{
			topAvailableForCreateStorageIndex = std::distance(m_DeclStorage.begin(), storageIter.base()) - 1;
		}

		if (HasAllFlags(storageIter->first, DeclStorageLevelFlag::CreateStorageIfNotFound))
		{
			break;
		}
	}

	const auto storagePointer =
#ifdef _MSC_VER
#	ifdef NDEBUG
		static_cast<nData>(_aligned_malloc(typeInfo.Size, typeInfo.Align));
#	else
		static_cast<nData>(_aligned_malloc_dbg(typeInfo.Size, typeInfo.Align, __FILE__, __LINE__));
#	endif
#else
		static_cast<nData>(aligned_alloc(typeInfo.Align, typeInfo.Size));
#endif

	if (storagePointer)
	{
		assert(topAvailableForCreateStorageIndex != std::numeric_limits<std::size_t>::max());
		const auto storageIter = std::next(m_DeclStorage.begin(), topAvailableForCreateStorageIndex);

		const auto[iter, succeed] = storageIter->second->try_emplace(std::move(decl), std::unique_ptr<nByte[], StorageDeleter>{ storagePointer });

		if (succeed)
		{
			std::memset(iter->second.get(), 0, typeInfo.Size);
			return { true, iter->second.get() };
		}
	}

	nat_Throw(InterpreterException, u8"无法为此声明创建存储"_nv);
}

void Interpreter::InterpreterDeclStorage::RemoveDecl(natRefPointer<Declaration::ValueDecl> const& decl)
{
	const auto context = decl->GetContext();
	if (context)
	{
		context->RemoveDecl(decl);
	}

	m_DeclStorage.back().second->erase(decl);
}

nBool Interpreter::InterpreterDeclStorage::DoesDeclExist(natRefPointer<Declaration::ValueDecl> const& decl) const noexcept
{
	if (decl.Cast<MemoryLocationDecl>())
	{
		return true;
	}

	for (auto& curStorage : make_range(m_DeclStorage.crbegin(), m_DeclStorage.crend()))
	{
		if (curStorage.second->find(decl) != curStorage.second->cend())
		{
			return true;
		}
	}

	return false;
}

void Interpreter::InterpreterDeclStorage::PushStorage(DeclStorageLevelFlag flags)
{
	assert(HasAllFlags(flags, DeclStorageLevelFlag::AvailableForCreateStorage) ||
		!HasAnyFlags(flags, DeclStorageLevelFlag::CreateStorageIfNotFound));

	m_DeclStorage.emplace_back(flags, std::make_unique<std::unordered_map<natRefPointer<Declaration::ValueDecl>, std::unique_ptr<nByte[], StorageDeleter>>>());
}

void Interpreter::InterpreterDeclStorage::PopStorage()
{
	assert(m_DeclStorage.size() > 1);
	m_DeclStorage.pop_back();
}

void Interpreter::InterpreterDeclStorage::MergeStorage()
{
	if (m_DeclStorage.size() > 2)
	{
		auto iter = m_DeclStorage.rbegin();
		++iter;
		for (; iter != m_DeclStorage.rend(); ++iter)
		{
			if (HasAllFlags(iter->first, DeclStorageLevelFlag::AvailableForCreateStorage))
			{
				break;
			}
		}

		auto& target = *iter->second;

		for (auto&& item : *m_DeclStorage.rbegin()->second)
		{
			target.insert_or_assign(item.first, move(item.second));
		}

		PopStorage();
	}
}

DeclStorageLevelFlag Interpreter::InterpreterDeclStorage::GetTopStorageFlag() const noexcept
{
	return m_DeclStorage.back().first;
}

void Interpreter::InterpreterDeclStorage::SetTopStorageFlag(DeclStorageLevelFlag flags)
{
	assert(HasAllFlags(flags, DeclStorageLevelFlag::AvailableForCreateStorage) ||
		!HasAnyFlags(flags, DeclStorageLevelFlag::CreateStorageIfNotFound));

	m_DeclStorage.back().first = flags;
}

void Interpreter::InterpreterDeclStorage::GarbageCollect()
{
	for (auto& curStorage : make_range(m_DeclStorage.rbegin(), m_DeclStorage.rend()))
	{
		for (auto iter = curStorage.second->cbegin(); iter != curStorage.second->cend();)
		{
			// 没有外部引用了，回收这个声明及占用的存储
			if (iter->first->IsUnique())
			{
				iter = curStorage.second->erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
}

natRefPointer<Declaration::ValueDecl> Interpreter::InterpreterDeclStorage::CreateTemporaryObjectDecl(Type::TypePtr type, SourceLocation loc)
{
	return make_ref<Declaration::ValueDecl>(Declaration::Decl::Var, nullptr, loc, nullptr, type);
}
