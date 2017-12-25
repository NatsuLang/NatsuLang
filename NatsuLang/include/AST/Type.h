#pragma once
#include "TypeBase.h"
#include <natLinq.h>
#include "Basic/Token.h"

#define DEFAULT_ACCEPT_DECL void Accept(NatsuLib::natRefPointer<TypeVisitor> const& visitor) override

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Declaration
{
	class TagDecl;
	class ClassDecl;
	class EnumDecl;
}

namespace NatsuLang::Expression
{
	class Expr;
}

namespace NatsuLang::Type
{
	class BuiltinType
		: public Type
	{
	public:
		enum BuiltinClass
		{
			Invalid,
#define BUILTIN_TYPE(Id, Name) Id,
#define LAST_BUILTIN_TYPE(Id) LastType = Id
#include "Basic/BuiltinTypesDef.h"
		};

		explicit BuiltinType(BuiltinClass builtinClass)
			: Type{ Builtin }, m_BuiltinClass{ builtinClass }
		{
			assert(m_BuiltinClass != Invalid);
		}

		~BuiltinType();

		BuiltinClass GetBuiltinClass() const noexcept
		{
			return m_BuiltinClass;
		}

		const char* GetName() const noexcept;

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

		static BuiltinClass GetBuiltinClassFromTokenType(Lex::TokenType type) noexcept;
		static nBool IsIntegerBuiltinClass(BuiltinClass builtinClass) noexcept;
		static nBool IsFloatingBuiltinClass(BuiltinClass builtinClass) noexcept;
		static nBool IsSignedBuiltinClass(BuiltinClass builtinClass) noexcept;
		static BuiltinClass MakeSignedBuiltinClass(BuiltinClass builtinClass) noexcept;
		static BuiltinClass MakeUnsignedBuiltinClass(BuiltinClass builtinClass) noexcept;

		nBool IsIntegerType() const noexcept
		{
			return IsIntegerBuiltinClass(m_BuiltinClass);
		}

		nBool IsFloatingType() const noexcept
		{
			return IsFloatingBuiltinClass(m_BuiltinClass);
		}

		nBool IsSigned() const noexcept
		{
			return IsSignedBuiltinClass(m_BuiltinClass);
		}

		///	@brief	比较两个 BuiltinType 的等级
		///	@param	other	要比较的 BuiltinType
		///	@param	result	比较结果，若等级大于要比较的 BuiltinType 则大于0，若小于则小于0，若等于则等于0，具体值无特殊意义
		///	@return	比较是否有意义
		nBool CompareRankTo(NatsuLib::natRefPointer<BuiltinType> const& other, nInt& result) const noexcept;

	private:
		const BuiltinClass m_BuiltinClass;
	};

	class PointerType
		: public Type
	{
	public:
		explicit PointerType(TypePtr pointeeType)
			: Type{ Pointer }, m_PointeeType{ std::move(pointeeType) }
		{
		}

		~PointerType();

		TypePtr GetPointeeType() const noexcept
		{
			return m_PointeeType;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		const TypePtr m_PointeeType;
	};

	class ParenType
		: public Type
	{
	public:
		explicit ParenType(TypePtr innerType)
			: Type{ Paren }, m_InnerType{ std::move(innerType) }
		{
		}

		~ParenType();

		TypePtr GetInnerType() const noexcept
		{
			return m_InnerType;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		const TypePtr m_InnerType;
	};

	class ArrayType
		: public Type
	{
	public:
		ArrayType(TypePtr elementType, std::size_t arraySize)
			: Type{ Array }, m_ElementType{ std::move(elementType) }, m_ArraySize{ arraySize }
		{
		}

		~ArrayType();

		TypePtr GetElementType() const noexcept
		{
			return m_ElementType;
		}

		std::size_t GetSize() const noexcept
		{
			return m_ArraySize;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		TypePtr m_ElementType;
		std::size_t m_ArraySize;
	};

	class FunctionType
		: public Type
	{
	public:
		FunctionType(NatsuLib::Linq<NatsuLib::Valued<TypePtr>> const& params, TypePtr resultType)
			: Type{ Function }, m_ParameterTypes{ params.begin(), params.end() }, m_ResultType{ std::move(resultType) }
		{
		}

		~FunctionType();

		TypePtr GetResultType() const noexcept
		{
			return m_ResultType;
		}

		void SetResultType(TypePtr value) noexcept
		{
			m_ResultType = std::move(value);
		}

		NatsuLib::Linq<NatsuLib::Valued<TypePtr>> GetParameterTypes() const noexcept;
		std::size_t GetParameterCount() const noexcept;

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		std::vector<TypePtr> m_ParameterTypes;
		TypePtr m_ResultType;
	};

	class TypeOfType
		: public Type
	{
	public:
		TypeOfType(NatsuLib::natRefPointer<Expression::Expr> expr, TypePtr underlyingType)
			: Type{ TypeOf }, m_Expr{ std::move(expr) }, m_UnderlyingType{ std::move(underlyingType) }
		{
		}

		~TypeOfType();

		NatsuLib::natRefPointer<Expression::Expr> GetExpr() const noexcept
		{
			return m_Expr;
		}

		TypePtr GetUnderlyingType() const noexcept
		{
			return m_UnderlyingType;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		NatsuLib::natRefPointer<Expression::Expr> m_Expr;
		TypePtr m_UnderlyingType;
	};

	class TagType
		: public Type
	{
	protected:
		TagType(TypeClass typeClass, NatsuLib::natRefPointer<Declaration::TagDecl> decl)
			: Type{ typeClass }, m_Decl{ std::move(decl) }
		{
		}

	public:
		enum class TagTypeClass
		{
			Class,
			Enum
		};

		~TagType();

		NatsuLib::natRefPointer<Declaration::TagDecl> GetDecl() const noexcept
		{
			return m_Decl;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		NatsuLib::natRefPointer<Declaration::TagDecl> m_Decl;
	};

	class ClassType
		: public TagType
	{
	public:
		explicit ClassType(NatsuLib::natRefPointer<Declaration::ClassDecl> recordDecl);
		ClassType(TypeClass typeClass, NatsuLib::natRefPointer<Declaration::ClassDecl> recordDecl);
		~ClassType();

		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;
	};

	class EnumType
		: public TagType
	{
	public:
		explicit EnumType(NatsuLib::natRefPointer<Declaration::EnumDecl> decl);

		~EnumType();

		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;
	};

	class DeducedType
		: public Type
	{
	public:
		DeducedType(TypeClass typeClass, TypePtr deducedAsType)
			: Type{ typeClass }, m_DeducedAsType{ std::move(deducedAsType) }
		{
		}

		~DeducedType();

		TypePtr GetDeducedAsType() const noexcept
		{
			return m_DeducedAsType;
		}

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

	private:
		TypePtr m_DeducedAsType;
	};

	class AutoType
		: public DeducedType
	{
	public:
		explicit AutoType(TypePtr deducedAsType)
			: DeducedType{ Auto, std::move(deducedAsType) }
		{
		}

		~AutoType();

		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;
	};

	class UnresolvedType
		: public Type
	{
	public:
		explicit UnresolvedType(std::vector<Lex::Token> tokens)
			: Type{ Unresolved }, m_Tokens{ std::move(tokens) }
		{
		}

		~UnresolvedType();

		std::size_t GetHashCode() const noexcept override;
		nBool EqualTo(TypePtr const& other) const noexcept override;
		DEFAULT_ACCEPT_DECL;

		std::vector<Lex::Token> const& GetTokens() const noexcept
		{
			return m_Tokens;
		}

		std::vector<Lex::Token> GetAndClearTokens() noexcept
		{
			return std::move(m_Tokens);
		}

		void SetTokens(std::vector<Lex::Token> value) noexcept
		{
			m_Tokens = std::move(value);
		}

	private:
		std::vector<Lex::Token> m_Tokens;
	};
}

#undef DEFAULT_ACCEPT_DECL
