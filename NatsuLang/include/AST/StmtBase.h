#pragma once
#include <natRefObj.h>
#include <natLinq.h>
#include "Basic/SourceLocation.h"

namespace NatsuLang::Statement
{
	class Stmt;

	using StmtPtr = NatsuLib::natRefPointer<Stmt>;
	using StmtEnumerable = NatsuLib::Linq<StmtPtr>;

	class Stmt
		: public NatsuLib::natRefObjImpl<Stmt>
	{
	public:
		enum StmtType
		{
			None = 0,
#define STMT(StmtType, Base) StmtType##Class,
#define STMT_RANGE(Base, FIRST, LAST) \
		First##Base##Constant = FIRST##Class, Last##Base##Constant = LAST##Class,
#define LAST_STMT_RANGE(Base, FIRST, LAST) \
		First##Base##Constant = FIRST##Class, Last##Base##Constant = LAST##Class
#define ABSTRACT_STMT(STMT)
#include "Basic/StmtDef.h"
		};

		explicit Stmt(StmtType type, SourceLocation start = {}, SourceLocation end = {}) noexcept
			: m_Type{ type }, m_Start{ start }, m_End{ end }
		{
		}
		~Stmt();

		StmtType GetType() const noexcept
		{
			return m_Type;
		}

		const char* GetTypeName() const noexcept;

		virtual StmtEnumerable GetChildrens();

		virtual SourceLocation GetStartLoc() const noexcept;
		virtual void SetStartLoc(SourceLocation loc) noexcept;
		virtual SourceLocation GetEndLoc() const noexcept;
		virtual void SetEndLoc(SourceLocation loc) noexcept;

	private:
		StmtType m_Type;
		SourceLocation m_Start, m_End;
	};
}
