#include "Interpreter.h"

#undef min
#undef max

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

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
	: m_Interpreter{ interpreter }, m_DeclStorage{}
{
	PushStorage();
}

std::pair<nBool, nData> Interpreter::InterpreterDeclStorage::GetOrAddDecl(natRefPointer<Declaration::ValueDecl> decl, Type::TypePtr type)
{
	if (!type)
	{
		type = decl->GetValueType();
	}

	assert(type);
	const auto typeInfo = m_Interpreter.m_AstContext.GetTypeInfo(type);

	auto topestAvailableForCreateStorageIndex = std::numeric_limits<std::size_t>::max();

	for (auto storageIter = m_DeclStorage.rbegin(); storageIter != m_DeclStorage.rend(); ++storageIter)
	{
		if ((storageIter->first & DeclStorageLevelFlag::AvailableForLookup) != DeclStorageLevelFlag::None)
		{
			const auto iter = storageIter->second->find(decl);
			if (iter != storageIter->second->cend())
			{
				return { false, iter->second.get() };
			}
		}

		if (topestAvailableForCreateStorageIndex == std::numeric_limits<std::size_t>::max() &&
			(storageIter->first & DeclStorageLevelFlag::AvailableForCreateStorage) != DeclStorageLevelFlag::None)
		{
			topestAvailableForCreateStorageIndex = std::distance(m_DeclStorage.begin(), storageIter.base()) - 1;
		}

		if ((storageIter->first & DeclStorageLevelFlag::CreateStorageIfNotFound) != DeclStorageLevelFlag::None)
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
		assert(topestAvailableForCreateStorageIndex != std::numeric_limits<std::size_t>::max());
		const auto storageIter = std::next(m_DeclStorage.begin(), topestAvailableForCreateStorageIndex);

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
	assert((flags & DeclStorageLevelFlag::AvailableForCreateStorage) != DeclStorageLevelFlag::None ||
		(flags & DeclStorageLevelFlag::CreateStorageIfNotFound) == DeclStorageLevelFlag::None);

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
			if ((iter->first & DeclStorageLevelFlag::AvailableForCreateStorage) != DeclStorageLevelFlag::None)
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
	assert((flags & DeclStorageLevelFlag::AvailableForCreateStorage) != DeclStorageLevelFlag::None ||
		(flags & DeclStorageLevelFlag::CreateStorageIfNotFound) == DeclStorageLevelFlag::None);

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
