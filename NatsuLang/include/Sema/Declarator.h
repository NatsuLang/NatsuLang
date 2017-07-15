#pragma once
#include <natRefObj.h>
#include <natLinq.h>
#include <variant>
#include "Basic/SourceLocation.h"

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
}

namespace NatsuLang::Type
{
	class Type;
	using TypePtr = NatsuLib::natRefPointer<Type>;
}

namespace NatsuLang::Expression
{
	class Expr;
	using ExprPtr = NatsuLib::natRefPointer<Expr>;
}

namespace NatsuLang::Declaration
{
	class Decl;
	using DeclPtr = NatsuLib::natRefPointer<Decl>;

	class DeclaratorChunk
	{
	public:
		enum class ChunkType
		{
			Array,
			Function,
			Paren
		};

		struct ArrayTypeInfo
		{
			Type::TypePtr ElementType;
			Expression::ExprPtr SizeExpr;

			ArrayTypeInfo(Type::TypePtr elementType, Expression::ExprPtr sizeExpr)
				: ElementType{ std::move(elementType) }, SizeExpr{ std::move(sizeExpr) }
			{
			}
		};

		struct ParamInfo
		{
			Identifier::IdPtr Id;
			SourceLocation Location;
			DeclPtr Param;
		};

		struct FunctionTypeInfo
		{
			std::vector<ParamInfo> Params;
			Type::TypePtr ReturnType;

			FunctionTypeInfo(std::vector<ParamInfo> params, Type::TypePtr returnType)
				: Params{ move(params) }, ReturnType{ std::move(returnType) }
			{
			}
		};

		struct ParenTypeInfo
		{
			constexpr ParenTypeInfo() = default;
		};

		static constexpr ParenTypeInfo ParenTypeInfoPlaceHolder{};

		DeclaratorChunk(Type::TypePtr elementType, Expression::ExprPtr sizeExpr, SourceLocation begin, SourceLocation end)
			: m_Location{ begin }, m_EndLocation{ end }, m_Type{ ChunkType::Array }, m_TypeInfo{ std::in_place_index<0>, std::move(elementType), std::move(sizeExpr) }
		{
		}

		DeclaratorChunk(std::vector<ParamInfo> params, Type::TypePtr returnType, SourceLocation begin, SourceLocation end)
			: m_Location{ begin }, m_EndLocation{ end }, m_Type{ ChunkType::Function }, m_TypeInfo{ std::in_place_index<1>, move(params), std::move(returnType) }
		{
		}

		DeclaratorChunk(ParenTypeInfo, SourceLocation begin, SourceLocation end)
			: m_Location{ begin }, m_EndLocation{ end }, m_Type{ ChunkType::Paren }, m_TypeInfo{ std::in_place_index<2> }
		{
		}

		ArrayTypeInfo* GetArrayTypeInfo() noexcept
		{
			if (m_Type == ChunkType::Array)
			{
				return &std::get<0>(m_TypeInfo);
			}

			return nullptr;
		}

		FunctionTypeInfo* GetFunctionTypeInfo() noexcept
		{
			if (m_Type == ChunkType::Function)
			{
				return &std::get<1>(m_TypeInfo);
			}

			return nullptr;
		}

		ChunkType GetType() const noexcept
		{
			return m_Type;
		}

		SourceRange GetRange() const noexcept
		{
			if (m_EndLocation.IsValid())
			{
				return { m_Location, m_EndLocation };
			}

			return { m_Location, m_Location };
		}

	private:
		SourceLocation m_Location, m_EndLocation;
		ChunkType m_Type{};

		std::variant<ArrayTypeInfo, FunctionTypeInfo, ParenTypeInfo> m_TypeInfo;
	};

	class Declarator
	{
	public:
		enum class Context
		{
			File,
			Prototype,
			TypeName,
			Member,
			Block,
			For,
			New,
			Catch,
		};

		explicit Declarator(Context context)
			: m_Context{ context }
		{
		}

		Identifier::IdPtr GetIdentifier() const noexcept
		{
			return m_Identifier;
		}

		void SetIdentifier(Identifier::IdPtr idPtr) noexcept
		{
			m_Identifier = std::move(idPtr);
		}

		SourceRange GetRange() const noexcept
		{
			return m_Range;
		}

		void SetRange(SourceRange range) noexcept
		{
			m_Range = range;
		}

		Context GetContext() const noexcept
		{
			return m_Context;
		}

		void SetContext(Context context) noexcept
		{
			m_Context = context;
		}

		void AddTypeInfo(DeclaratorChunk const& chunk, SourceLocation endLoc)
		{
			m_TypeInfos.emplace_back(chunk);
			if (!endLoc.IsValid())
			{
				m_Range.SetEnd(endLoc);
			}
		}

		void AddTypeInfo(DeclaratorChunk && chunk, SourceLocation endLoc)
		{
			m_TypeInfos.emplace_back(std::move(chunk));
			if (!endLoc.IsValid())
			{
				m_Range.SetEnd(endLoc);
			}
		}

		NatsuLib::Linq<DeclaratorChunk> GetTypeInfos() noexcept
		{
			return NatsuLib::from(m_TypeInfos);
		}

		NatsuLib::Linq<const DeclaratorChunk> GetTypeInfos() const noexcept
		{
			return NatsuLib::from(m_TypeInfos);
		}

		std::size_t GetTypeInfoCount() const noexcept
		{
			return m_TypeInfos.size();
		}

		DeclaratorChunk& GetTypeInfo(std::size_t index) noexcept
		{
			return m_TypeInfos[index];
		}

		DeclaratorChunk const& GetTypeInfo(std::size_t index) const noexcept
		{
			return m_TypeInfos[index];
		}

	private:
		Identifier::IdPtr m_Identifier;
		SourceRange m_Range;
		Context m_Context;
		std::vector<DeclaratorChunk> m_TypeInfos;
	};
}
