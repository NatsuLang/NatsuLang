#include "Sema/DefaultActions.h"
#include "AST/Expression.h"
#include "Parse/Parser.h"
#include "Sema/Sema.h"
#include "Sema/Scope.h"

using namespace NatsuLib;
using namespace NatsuLang;

SimpleArgumentRequirement::SimpleArgumentRequirement(std::initializer_list<CompilerActionArgumentType> const& types)
	: m_Types(types.begin(), types.end())
{
}

SimpleArgumentRequirement::SimpleArgumentRequirement(Linq<Valued<CompilerActionArgumentType>> const& types)
	: m_Types(types.begin(), types.end())
{
}

SimpleArgumentRequirement::~SimpleArgumentRequirement()
{
}

CompilerActionArgumentType SimpleArgumentRequirement::GetExpectedArgumentType(std::size_t i)
{
	return i < m_Types.size() ? m_Types[i] : CompilerActionArgumentType::None;
}

SimpleActionContext::SimpleActionContext(natRefPointer<IArgumentRequirement> requirement)
	: m_Requirement{ std::move(requirement) }
{
}

SimpleActionContext::~SimpleActionContext()
{
}

NatsuLib::natRefPointer<IArgumentRequirement> SimpleActionContext::GetArgumentRequirement()
{
	return m_Requirement;
}

void SimpleActionContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	// TODO: 检测类型并报告错误
	switch (GetCategoryPart(m_Requirement->GetExpectedArgumentType(m_ArgumentList.size())))
	{
	case CompilerActionArgumentType::None:
		return;
	case CompilerActionArgumentType::Type:
		break;
	case CompilerActionArgumentType::Declaration:
		break;
	case CompilerActionArgumentType::Statement:
		break;
	case CompilerActionArgumentType::Identifier:
		break;
	case CompilerActionArgumentType::CompilerAction:
		break;
	default:
		break;
	}

	m_ArgumentList.emplace_back(arg);
}

std::vector<ASTNodePtr> const& SimpleActionContext::GetArguments() const noexcept
{
	return m_ArgumentList;
}

const natRefPointer<IArgumentRequirement> ActionDump::s_ArgumentRequirement{ make_ref<ActionDumpArgumentRequirement>() };

ActionDump::ActionDump()
{
}

ActionDump::~ActionDump()
{
}

nStrView ActionDump::GetName() const noexcept
{
	return u8"Dump"_nv;
}

natRefPointer<IActionContext> ActionDump::StartAction(CompilerActionContext const& /*context*/)
{
	return make_ref<SimpleActionContext>(s_ArgumentRequirement);
}

void ActionDump::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	if (output)
	{
		for (auto&& node : context.UnsafeCast<SimpleActionContext>()->GetArguments())
		{
			if (output(node))
			{
				return;
			}
		}
	}
}

ActionDump::ActionDumpArgumentRequirement::~ActionDumpArgumentRequirement()
{
}

CompilerActionArgumentType ActionDump::ActionDumpArgumentRequirement::GetExpectedArgumentType(std::size_t /*i*/)
{
	return CompilerActionArgumentType::Optional |
		CompilerActionArgumentType::Type |
		CompilerActionArgumentType::Declaration |
		CompilerActionArgumentType::Statement;
}

ActionDumpIf::ActionDumpIfContext::~ActionDumpIfContext()
{
}

natRefPointer<IArgumentRequirement> ActionDumpIf::ActionDumpIfContext::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionDumpIf::ActionDumpIfContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	if (!SkipThisNode)
	{
		const auto conditionExpr = static_cast<Expression::ExprPtr>(arg);
		if (!conditionExpr)
		{
			// TODO: 报告错误
			return;
		}

		nuLong result;
		if (!conditionExpr->EvaluateAsInt(result, *Context))
		{
			// TODO: 报告错误
			return;
		}

		SkipThisNode = !result;
	}
	else
	{
		if (!SkipThisNode.value())
		{
			ResultNode = arg;
		}

		SkipThisNode = !SkipThisNode.value();
	}
}

const natRefPointer<IArgumentRequirement> ActionDumpIf::ActionDumpIfContext::s_ArgumentRequirement
{
	make_ref<SimpleArgumentRequirement>(
		std::initializer_list<CompilerActionArgumentType>{
			CompilerActionArgumentType::Statement,
			CompilerActionArgumentType::Type |
			CompilerActionArgumentType::Declaration |
			CompilerActionArgumentType::Statement,
			CompilerActionArgumentType::Optional |
			CompilerActionArgumentType::Type |
			CompilerActionArgumentType::Declaration |
			CompilerActionArgumentType::Statement
		}
	)
};

ActionDumpIf::ActionDumpIf()
{
}

ActionDumpIf::~ActionDumpIf()
{
}

nStrView ActionDumpIf::GetName() const noexcept
{
	return u8"DumpIf"_nv;
}

natRefPointer<IActionContext> ActionDumpIf::StartAction(CompilerActionContext const& context)
{
	auto actionContext = make_ref<ActionDumpIfContext>();
	actionContext->Context = context.GetParser().GetSema().GetASTContext().ForkRef();
	return actionContext;
}

void ActionDumpIf::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	const auto actionContext = context.UnsafeCast<ActionDumpIfContext>();
	if (output)
	{
		output(actionContext->ResultNode);
	}
}

const natRefPointer<IArgumentRequirement> ActionIsDefined::ActionIsDefinedContext::s_ArgumentRequirement{ make_ref<SimpleArgumentRequirement>(std::initializer_list<CompilerActionArgumentType>{ CompilerActionArgumentType::Identifier }) };

ActionIsDefined::ActionIsDefined()
{
}

ActionIsDefined::~ActionIsDefined()
{
}

nStrView ActionIsDefined::GetName() const noexcept
{
	return u8"IsDefined"_nv;
}

natRefPointer<IActionContext> ActionIsDefined::StartAction(CompilerActionContext const& context)
{
	auto actionContext = make_ref<ActionIsDefinedContext>();
	actionContext->Sema = &context.GetParser().GetSema();
	return actionContext;
}

void ActionIsDefined::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	const auto actionContext = context.UnsafeCast<ActionIsDefinedContext>();

	if (output)
	{
		output(actionContext->Result ? make_ref<Expression::BooleanLiteral>(actionContext->Result.value(),
			actionContext->Sema->GetASTContext().GetBuiltinType(Type::BuiltinType::Bool), SourceLocation{}) : nullptr);
	}
}

ActionIsDefined::ActionIsDefinedContext::~ActionIsDefinedContext()
{
}

natRefPointer<IArgumentRequirement> ActionIsDefined::ActionIsDefinedContext::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionIsDefined::ActionIsDefinedContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	const auto name = arg.Cast<Declaration::UnresolvedDecl>();
	if (!name)
	{
		// TODO: 报告错误
		return;
	}

	Lex::Token dummyToken;
	Semantic::LookupResult r{ *Sema, name->GetIdentifierInfo(), {}, Semantic::Sema::LookupNameType::LookupAnyName };
	Result = Sema->LookupName(r, Sema->GetCurrentScope()) && r.GetDeclSize();
}

const natRefPointer<IArgumentRequirement> ActionTypeOf::ActionTypeOfContext::s_ArgumentRequirement{ make_ref<SimpleArgumentRequirement>(std::initializer_list<CompilerActionArgumentType>{ CompilerActionArgumentType::Statement }) };

ActionTypeOf::ActionTypeOf()
{
}

ActionTypeOf::~ActionTypeOf()
{
}

nStrView ActionTypeOf::GetName() const noexcept
{
	return u8"TypeOf"_nv;
}

natRefPointer<IActionContext> ActionTypeOf::StartAction(CompilerActionContext const& /*context*/)
{
	return make_ref<ActionTypeOfContext>();
}

void ActionTypeOf::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	const auto actionContext = context.UnsafeCast<ActionTypeOfContext>();

	if (output)
	{
		output(actionContext->Type);
	}
}

ActionTypeOf::ActionTypeOfContext::~ActionTypeOfContext()
{
}

natRefPointer<IArgumentRequirement> ActionTypeOf::ActionTypeOfContext::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionTypeOf::ActionTypeOfContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	const auto expr = arg.Cast<Expression::Expr>();
	if (!expr)
	{
		// TODO: 报告错误
		return;
	}

	Type = expr->GetExprType();
}

ActionCreateAt::ActionCreateAtArgumentRequirement::ActionCreateAtArgumentRequirement()
{
}

ActionCreateAt::ActionCreateAtArgumentRequirement::~ActionCreateAtArgumentRequirement()
{
}

CompilerActionArgumentType ActionCreateAt::ActionCreateAtArgumentRequirement::GetExpectedArgumentType(std::size_t i)
{
	return i ? CompilerActionArgumentType::Statement | CompilerActionArgumentType::Optional : CompilerActionArgumentType::Type;
}

const natRefPointer<IArgumentRequirement> ActionCreateAt::ActionCreateAtContext::s_ArgumentRequirement{ make_ref<ActionCreateAtArgumentRequirement>() };

ActionCreateAt::ActionCreateAt()
{
}

ActionCreateAt::~ActionCreateAt()
{
}

nStrView ActionCreateAt::GetName() const noexcept
{
	return u8"CreateAt"_nv;
}

natRefPointer<IActionContext> ActionCreateAt::StartAction(CompilerActionContext const& context)
{
	auto actionContext = make_ref<ActionCreateAtContext>();
	actionContext->Sema = &context.GetParser().GetSema();
	return actionContext;
}

void ActionCreateAt::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	const auto actionContext = context.UnsafeCast<ActionCreateAtContext>();

	Semantic::LookupResult r{ *actionContext->Sema, nullptr,{}, Semantic::Sema::LookupNameType::LookupMemberName };
	// 之前已经判断过了
	const auto classType = actionContext->Ptr->GetExprType().UnsafeCast<Type::PointerType>()->GetPointeeType().Cast<Type::ClassType>();
	if (!classType)
	{
		// 不需要任何操作
		if (output)
		{
			output(actionContext->Sema->ActOnNullStmt());
		}
		return;
	}

	if (!actionContext->Sema->LookupConstructors(r, classType->GetDecl().UnsafeCast<Declaration::ClassDecl>()) || r.GetResultType() == Semantic::LookupResult::LookupResultType::NotFound)
	{
		if (!actionContext->Arguments.empty())
		{
			// TODO: 多余的参数
		}

		// TODO: 生成默认构造函数并调用
		if (output)
		{
			output(actionContext->Sema->ActOnNullStmt());
		}
		return;
	}

	// TODO: 处理重载
	assert(r.GetDeclSize() == 1);
	if (output)
	{
		output(actionContext->Sema->ActOnCallExpr(actionContext->Sema->GetCurrentScope(),
			actionContext->Sema->BuildMethodReferenceExpr(actionContext->Ptr, {}, nullptr, r.GetDecls().first(), nullptr), {}, from(actionContext->Arguments), {}));
	}
}

ActionCreateAt::ActionCreateAtContext::~ActionCreateAtContext()
{
}

natRefPointer<IArgumentRequirement> ActionCreateAt::ActionCreateAtContext::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionCreateAt::ActionCreateAtContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	if (!Ptr)
	{
		Ptr = arg;
		if (!Ptr || Type::Type::GetUnderlyingType(Ptr->GetExprType())->GetType() != Type::Type::Pointer)
		{
			// TODO: 报告错误：希望获得指针表达式
		}
	}
	else
	{
		Arguments.emplace_back(arg);
	}
}

const natRefPointer<IArgumentRequirement> ActionDestroyAt::ActionDestroyAtContext::s_ArgumentRequirement{ make_ref<SimpleArgumentRequirement>(std::initializer_list<CompilerActionArgumentType>{ CompilerActionArgumentType::Statement }) };

ActionDestroyAt::ActionDestroyAt()
{
}

ActionDestroyAt::~ActionDestroyAt()
{
}

nStrView ActionDestroyAt::GetName() const noexcept
{
	return u8"DestroyAt"_nv;
}

natRefPointer<IActionContext> ActionDestroyAt::StartAction(CompilerActionContext const& context)
{
	auto actionContext = make_ref<ActionDestroyAtContext>();
	actionContext->Sema = &context.GetParser().GetSema();
	return actionContext;
}

void ActionDestroyAt::EndAction(natRefPointer<IActionContext> const& context, std::function<nBool(natRefPointer<ASTNode>)> const& output)
{
	const auto actionContext = context.UnsafeCast<ActionDestroyAtContext>();

	// 之前已经判断过了
	const auto classType = actionContext->Ptr->GetExprType().UnsafeCast<Type::PointerType>()->GetPointeeType().Cast<Type::ClassType>();
	if (!classType)
	{
		// 不需要任何操作
		if (output)
		{
			output(actionContext->Sema->ActOnNullStmt());
		}
		return;
	}

	const auto classDecl = classType->GetDecl().UnsafeCast<Declaration::ClassDecl>();
	const auto destructor = classDecl->GetDecls().select([](natRefPointer<Declaration::NamedDecl> const& decl)
	{
		return decl.Cast<Declaration::DestructorDecl>();
	}).first_or_default(nullptr, [](natRefPointer<Declaration::DestructorDecl> const& decl) -> nBool
	{
		return decl;
	});

	if (!destructor)
	{
		// TODO: 生成默认析构函数并调用
		if (output)
		{
			output(actionContext->Sema->ActOnNullStmt());
		}
		return;
	}

	if (output)
	{
		output(actionContext->Sema->ActOnCallExpr(actionContext->Sema->GetCurrentScope(),
			actionContext->Sema->BuildMethodReferenceExpr(actionContext->Ptr, {}, nullptr, destructor, nullptr), {}, from_empty<Expression::ExprPtr>(), {}));
	}
}

ActionDestroyAt::ActionDestroyAtContext::~ActionDestroyAtContext()
{
}

natRefPointer<IArgumentRequirement> ActionDestroyAt::ActionDestroyAtContext::GetArgumentRequirement()
{
	return s_ArgumentRequirement;
}

void ActionDestroyAt::ActionDestroyAtContext::AddArgument(natRefPointer<ASTNode> const& arg)
{
	Ptr = arg;
	if (!Ptr || Type::Type::GetUnderlyingType(Ptr->GetExprType())->GetType() != Type::Type::Pointer)
	{
		// TODO: 报告错误：希望获得指针表达式
	}
}
