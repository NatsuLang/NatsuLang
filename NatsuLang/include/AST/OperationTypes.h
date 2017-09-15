#pragma once

namespace NatsuLang::Expression
{
	enum class CastType
	{
		Invalid,
#define CAST_OPERATION(Name) Name,
#include "OperationTypesDef.h"
	};

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

	enum class UnaryOperationType
	{
		Invalid,
#define UNARY_OPERATION(Name, Spelling) Name,
#include "OperationTypesDef.h"
	};
}
