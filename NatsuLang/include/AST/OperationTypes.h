#pragma once

namespace NatsuLang::Expression
{
	enum class CastType
	{
		Invalid,
#define CAST_OPERATION(Name) Name,
#include "OperationTypesDef.h"
	};

	constexpr const char* GetCastTypeName(CastType value)
	{
		switch (value)
		{
		default:
			assert(!"Invalid CastType.");
			[[fallthrough]];
		case CastType::Invalid:
			return "Invalid";
#define CAST_OPERATION(Name) case CastType::Name: return #Name;
#include "OperationTypesDef.h"
		}
	}

	enum class BinaryOperationType
	{
		Invalid,
#define BINARY_OPERATION(Name, Spelling) Name,
#include "OperationTypesDef.h"
	};

	constexpr nBool IsBinLogicalOp(BinaryOperationType binOp)
	{
		return binOp == BinaryOperationType::LAnd || binOp == BinaryOperationType::LOr;
	}

	constexpr const char* GetBinaryOperationTypeName(BinaryOperationType value)
	{
		switch (value)
		{
		default:
			assert(!"Invalid BinaryOperationType.");
			[[fallthrough]];
		case BinaryOperationType::Invalid:
			return "Invalid";
#define BINARY_OPERATION(Name, Spelling) case BinaryOperationType::Name: return #Name;
#include "OperationTypesDef.h"
		}
	}

	constexpr const char* GetBinaryOperationTypeSpelling(BinaryOperationType value)
	{
		switch (value)
		{
		default:
			assert(!"Invalid BinaryOperationType.");
			[[fallthrough]];
		case BinaryOperationType::Invalid:
			return "";
#define BINARY_OPERATION(Name, Spelling) case BinaryOperationType::Name: return Spelling;
#include "OperationTypesDef.h"
		}
	}

	enum class UnaryOperationType
	{
		Invalid,
#define UNARY_OPERATION(Name, Spelling) Name,
#include "OperationTypesDef.h"
	};

	constexpr const char* GetUnaryOperationTypeName(UnaryOperationType value)
	{
		switch (value)
		{
		default:
			assert(!"Invalid UnaryOperationType.");
			[[fallthrough]];
		case UnaryOperationType::Invalid:
			return "Invalid";
#define UNARY_OPERATION(Name, Spelling) case UnaryOperationType::Name: return #Name;
#include "OperationTypesDef.h"
		}
	}

	constexpr const char* GetUnaryOperationTypeSpelling(UnaryOperationType value)
	{
		switch (value)
		{
		default:
			assert(!"Invalid UnaryOperationType.");
			[[fallthrough]];
		case UnaryOperationType::Invalid:
			return "";
#define UNARY_OPERATION(Name, Spelling) case UnaryOperationType::Name: return Spelling;
#include "OperationTypesDef.h"
		}
	}
}
