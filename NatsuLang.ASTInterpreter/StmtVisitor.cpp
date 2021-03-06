﻿#include "Interpreter.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Detail;

Interpreter::InterpreterStmtVisitor::InterpreterStmtVisitor(Interpreter& interpreter)
	: m_Interpreter{ interpreter }, m_Returned{ false }
{
}

Interpreter::InterpreterStmtVisitor::~InterpreterStmtVisitor()
{
}

Expression::ExprPtr Interpreter::InterpreterStmtVisitor::GetReturnedExpr() const noexcept
{
	return m_ReturnedExpr;
}

void Interpreter::InterpreterStmtVisitor::ResetReturnedExpr() noexcept
{
	m_Returned = false;
	m_ReturnedExpr.Reset();
}

void Interpreter::InterpreterStmtVisitor::Visit(natRefPointer<Statement::Stmt> const& stmt)
{
	if (m_Returned || m_ReturnedExpr)
	{
		return;
	}

	StmtVisitor::Visit(stmt);
}

void Interpreter::InterpreterStmtVisitor::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此语句无法被访问"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	InterpreterExprVisitor visitor{ m_Interpreter };
	visitor.PrintExpr(expr);
}

void Interpreter::InterpreterStmtVisitor::VisitBreakStmt(natRefPointer<Statement::BreakStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitCatchStmt(natRefPointer<Statement::CatchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitTryStmt(natRefPointer<Statement::TryStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& stmt)
{
	for (auto&& item : stmt->GetChildrenStmt())
	{
		if (m_Returned)
		{
			return;
		}

		Visit(item);
	}
}

void Interpreter::InterpreterStmtVisitor::VisitContinueStmt(natRefPointer<Statement::ContinueStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	const auto decl = stmt->GetDecl();
	if (!decl)
	{
		nat_Throw(InterpreterException, u8"错误的声明"_nv);
	}

	if (const auto varDecl = decl.Cast<Declaration::VarDecl>(); varDecl && !varDecl->IsFunction())
	{
		initVar(varDecl, varDecl->GetInitializer());
	}
}

void Interpreter::InterpreterStmtVisitor::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	InterpreterExprVisitor visitor{ m_Interpreter };

	nBool shouldContinue;
	while (true)
	{
		Visit(stmt->GetBody());
		if (m_Returned)
		{
			return;
		}

		if (!visitor.Evaluate(stmt->GetCond(), [&shouldContinue](nBool value)
		{
			shouldContinue = value;
		}, Expected<nBool>))
		{
			nat_Throw(InterpreterException, u8"条件表达式不能被计算为有效的 bool 值"_nv);
		}

		if (!shouldContinue)
		{
			return;
		}
	}
}

void Interpreter::InterpreterStmtVisitor::VisitForStmt(natRefPointer<Statement::ForStmt> const& stmt)
{
	InterpreterExprVisitor visitor{ m_Interpreter };

	if (const auto init = stmt->GetInit())
	{
		Visit(init);
	}

	const auto cond = stmt->GetCond();
	const auto inc = stmt->GetInc();
	const auto body = stmt->GetBody();

	auto shouldContinue = true;
	while (true)
	{
		if (cond && !visitor.Evaluate(cond, [&shouldContinue](nBool value)
		{
			shouldContinue = value;
		}, Expected<nBool>))
		{
			nat_Throw(InterpreterException, u8"条件表达式不能被计算为有效的 bool 值"_nv);
		}

		if (!shouldContinue)
		{
			return;
		}

		if (body)
		{
			Visit(body);
		}

		if (m_Returned)
		{
			return;
		}

		if (inc)
		{
			visitor.Visit(inc);
		}
	}
}

void Interpreter::InterpreterStmtVisitor::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	InterpreterExprVisitor visitor{ m_Interpreter };
	nBool condition;
	if (!visitor.Evaluate(stmt->GetCond(), [&condition](nBool value)
	{
		condition = value;
	}, Expected<nBool>))
	{
		nat_Throw(InterpreterException, u8"条件表达式不能被计算为有效的 bool 值"_nv);
	}

	if (condition)
	{
		Visit(stmt->GetThen());
	}
	else
	{
		Visit(stmt->GetElse());
	}
}

void Interpreter::InterpreterStmtVisitor::VisitLabelStmt(natRefPointer<Statement::LabelStmt> const& stmt)
{
	Visit(stmt->GetSubStmt());
}

void Interpreter::InterpreterStmtVisitor::VisitNullStmt(natRefPointer<Statement::NullStmt> const& /*stmt*/)
{
}

void Interpreter::InterpreterStmtVisitor::VisitReturnStmt(natRefPointer<Statement::ReturnStmt> const& stmt)
{
	if (auto retExpr = stmt->GetReturnExpr())
	{
		InterpreterExprVisitor visitor{ m_Interpreter };
		visitor.Visit(retExpr);
		retExpr = visitor.GetLastVisitedExpr();
		auto tempObjDecl = InterpreterDeclStorage::CreateTemporaryObjectDecl(retExpr->GetExprType());
		// 禁止当前层创建存储，以保证返回值创建在上层
		m_Interpreter.m_DeclStorage.SetTopStorageFlag(DeclStorageLevelFlag::AvailableForLookup);
		if (!visitor.Evaluate(retExpr, [this, &tempObjDecl](auto value)
		{
			// 由于之前已经访问过，所以不会再创建临时对象及对应的存储
			if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(tempObjDecl, [value](auto& storage)
			{
				storage = value;
			}, Expected<decltype(value)>))
			{
				nat_Throw(InterpreterException, u8"无法访问存储"_nv);
			}
		}, Excepted<nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法对表达式求值"_nv);
		}

		m_ReturnedExpr = make_ref<Expression::DeclRefExpr>(nullptr, tempObjDecl, SourceLocation{}, retExpr->GetExprType(), Expression::ValueCategory::LValue);
	}

	m_Returned = true;
}

void Interpreter::InterpreterStmtVisitor::VisitCaseStmt(natRefPointer<Statement::CaseStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitDefaultStmt(natRefPointer<Statement::DefaultStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitSwitchStmt(natRefPointer<Statement::SwitchStmt> const& stmt)
{
	nat_Throw(InterpreterException, u8"此功能尚未实现"_nv);
}

void Interpreter::InterpreterStmtVisitor::VisitWhileStmt(natRefPointer<Statement::WhileStmt> const& stmt)
{
	InterpreterExprVisitor visitor{ m_Interpreter };

	nBool shouldContinue;
	while (true)
	{
		if (!visitor.Evaluate(stmt->GetCond(), [&shouldContinue](nBool value)
		{
			shouldContinue = value;
		}, Expected<nBool>))
		{
			nat_Throw(InterpreterException, u8"条件表达式不能被计算为有效的 bool 值"_nv);
		}

		if (!shouldContinue)
		{
			return;
		}

		Visit(stmt->GetBody());
		if (m_Returned)
		{
			return;
		}
	}
}

void Interpreter::InterpreterStmtVisitor::initVar(natRefPointer<Declaration::VarDecl> const& var, Expression::ExprPtr const& initializer)
{
	if (!initializer)
	{
		m_Interpreter.m_DeclStorage.GetOrAddDecl(var, Type::Type::GetUnderlyingType(var->GetValueType()));
		return;
	}

	const auto varType = Type::Type::GetUnderlyingType(var->GetValueType());

	if (const auto initListExpr = initializer.Cast<Expression::InitListExpr>())
	{
		if (const auto arrayType = varType.Cast<Type::ArrayType>())
		{
			if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(var, [this, &initListExpr](InterpreterDeclStorage::ArrayElementAccessor& accessor)
			{
				const auto initExprs = initListExpr->GetInitExprs();
				auto initExprIter = initExprs.begin();
				const auto initExprEnd = initExprs.end();

				InterpreterExprVisitor evaluator{ m_Interpreter };

				for (std::size_t i = 0; initExprIter != initExprEnd; ++i, static_cast<void>(++initExprIter))
				{
					initVar(accessor.GetElementDecl(i), *initExprIter);
				}
			}, Expected<InterpreterDeclStorage::ArrayElementAccessor>))
			{
				nat_Throw(InterpreterException, u8"无法创建声明的存储"_nv);
			}
		}
		else if (varType->GetType() == Type::Type::Builtin)
		{
			const auto count = initListExpr->GetInitExprCount();
			if (count == 0)
			{
				m_Interpreter.m_DeclStorage.GetOrAddDecl(var);
			}
			else if (count == 1)
			{
				initVar(var, initListExpr->GetInitExprs().first());
			}
			else
			{
				// TODO: 报告错误
			}
		}
		else if (varType->GetType() == Type::Type::Pointer)
		{
			// TODO: 初始化指针
		}
	}
	else if (const auto constructExpr = initializer.Cast<Expression::ConstructExpr>())
	{
		// TODO: 构造函数初始化
	}
	else if (const auto stringLiteral = initializer.Cast<Expression::StringLiteral>())
	{
		// 一定是用来初始化数组类型，若是用户自定义类型将会是构造函数初始化
		if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(var, [stringValue = stringLiteral->GetValue()](InterpreterDeclStorage::ArrayElementAccessor& accessor)
		{
			// 由前端保证存储够用
			std::memmove(accessor.GetStorage(), stringValue.data(), stringValue.size());
		}, Expected<InterpreterDeclStorage::ArrayElementAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法创建声明的存储"_nv);
		}
	}
	else
	{
		auto succeed = false;
		InterpreterExprVisitor evaluator{ m_Interpreter };
		if (!evaluator.Evaluate(initializer, [this, &succeed, &var](auto value)
		{
			succeed = m_Interpreter.m_DeclStorage.VisitDeclStorage(var, [value](auto& storage)
			{
				storage = value;
			}, Expected<decltype(value)>);
		}, Excepted<nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>))
		{
			nat_Throw(InterpreterException, u8"无法创建声明的存储，或者对初始化器的求值失败"_nv);
		}

		/*if (!m_Interpreter.m_DeclStorage.VisitDeclStorage(var, [this, &initializer, &succeed](auto& storage)
		{
			InterpreterExprVisitor evaluator{ m_Interpreter };
			succeed = evaluator.Evaluate(initializer, [&storage](auto value)
			{
				storage = value;
			}, Expected<decltype(storage)>);
		}, Excepted<nStrView, InterpreterDeclStorage::ArrayElementAccessor, InterpreterDeclStorage::MemberAccessor, InterpreterDeclStorage::PointerAccessor>) || !succeed)
		{
			nat_Throw(InterpreterException, u8"无法创建声明的存储，或者对初始化器的求值失败"_nv);
		}*/
	}
}

