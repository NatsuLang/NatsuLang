#pragma once
#include <natRefObj.h>
#include <natLinq.h>

namespace NatsuLang
{
	namespace Declaration
	{
		class Decl;
		using DeclPtr = NatsuLib::natRefPointer<Decl>;
	}

	class Metadata
	{
	public:
		Metadata();
		~Metadata();

		template <typename Range>
		void AddDecls(Range&& range)
		{
			for (auto decl : range)
			{
				AddDecl(std::move(decl));
			}
		}

		void AddDecl(Declaration::DeclPtr decl);

		std::size_t GetDeclCount() const noexcept;
		NatsuLib::Linq<NatsuLib::Valued<Declaration::DeclPtr>> GetDecls() const;

	private:
		std::vector<Declaration::DeclPtr> m_Decls;
	};
}
