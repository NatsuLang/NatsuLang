#include "Serialization.h"
#include "Basic/Identifier.h"
#include "AST/NestedNameSpecifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Serialization;

namespace
{
	[[noreturn]] void ThrowRejectAst()
	{
		nat_Throw(SerializationException, u8"This ast cannot be serialized."_nv);
	}

	[[noreturn]] void ThrowInvalidData()
	{
		nat_Throw(SerializationException, u8"Invalid data."_nv);
	}
}

ISerializationArchiveReader::~ISerializationArchiveReader()
{
}

nBool ISerializationArchiveReader::ReadBool(nStrView key, nBool& out)
{
	nuLong dummy;
	const auto ret = ReadInteger(key, dummy, 1);
	out = !!dummy;
	return ret;
}

ISerializationArchiveWriter::~ISerializationArchiveWriter()
{
}

void ISerializationArchiveWriter::WriteBool(nStrView key, nBool value)
{
	WriteInteger(key, value, 1);
}

Deserializer::Deserializer(Semantic::Sema& sema, NatsuLib::natRefPointer<ISerializationArchiveReader> archive,
	NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> const& stmtTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> const& declTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> const& typeClassMap)
	: m_Sema{ sema }, m_Archive{ std::move(archive) }
{
	if (stmtTypeMap)
	{
		using UnderlyingType = std::underlying_type_t<Statement::Stmt::StmtType>;
		for (auto i = UnderlyingType{}; i < static_cast<UnderlyingType>(Statement::Stmt::StmtType::LastStmt); ++i)
		{
			const auto stmtType = static_cast<Statement::Stmt::StmtType>(i);
			m_StmtTypeMap.emplace(stmtTypeMap->GetText(stmtType), stmtType);
		}
	}

	if (declTypeMap)
	{
		using UnderlyingType = std::underlying_type_t<Declaration::Decl::DeclType>;
		for (auto i = UnderlyingType{}; i < static_cast<UnderlyingType>(Declaration::Decl::DeclType::LastDecl); ++i)
		{
			const auto declType = static_cast<Declaration::Decl::DeclType>(i);
			m_DeclTypeMap.emplace(declTypeMap->GetText(declType), declType);
		}
	}

	if (typeClassMap)
	{
		using UnderlyingType = std::underlying_type_t<Type::Type::TypeClass>;
		for (auto i = UnderlyingType{}; i < static_cast<UnderlyingType>(Type::Type::TypeClass::TypeLast); ++i)
		{
			const auto typeClass = static_cast<Type::Type::TypeClass>(i);
			m_TypeClassMap.emplace(typeClassMap->GetText(typeClass), typeClass);
		}
	}

	m_Archive->StartEntry(u8"Content");
}

Deserializer::~Deserializer()
{
	m_Archive->EndEntry();
}

void Deserializer::StartDeserialize()
{

}

void Deserializer::EndDeserialize()
{

}

ASTNodePtr Deserializer::Deserialize()
{
	ASTNodeType type;
	if (!m_Archive->ReadNumType(u8"AstNodeType", type))
	{
		ThrowInvalidData();
	}

	switch (type)
	{
	case NatsuLang::ASTNodeType::Declaration:
		return DeserializeDecl();
	case NatsuLang::ASTNodeType::Statement:
		return DeserializeStmt();
	case NatsuLang::ASTNodeType::Type:
		return DeserializeType();
	case NatsuLang::ASTNodeType::CompilerAction:
		return DeserializeCompilerAction();
	default:
		ThrowInvalidData();
	}
}

Declaration::DeclPtr Deserializer::DeserializeDecl()
{
	Declaration::Decl::DeclType type;
	if (!m_Archive->ReadNumType(u8"Type", type))
	{
		ThrowInvalidData();
	}

	switch (type)
	{
	case NatsuLang::Declaration::Decl::Empty:
		break;
	case NatsuLang::Declaration::Decl::Import:
		break;
	case NatsuLang::Declaration::Decl::Alias:
		break;
	case NatsuLang::Declaration::Decl::Label:
		break;
	case NatsuLang::Declaration::Decl::Module:
		break;
	case NatsuLang::Declaration::Decl::Enum:
		break;
	case NatsuLang::Declaration::Decl::Class:
		break;
	case NatsuLang::Declaration::Decl::Unresolved:
		break;
	case NatsuLang::Declaration::Decl::Field:
		break;
	case NatsuLang::Declaration::Decl::Function:
		break;
	case NatsuLang::Declaration::Decl::Method:
		break;
	case NatsuLang::Declaration::Decl::Constructor:
		break;
	case NatsuLang::Declaration::Decl::Destructor:
		break;
	case NatsuLang::Declaration::Decl::Var:
		break;
	case NatsuLang::Declaration::Decl::ImplicitParam:
		break;
	case NatsuLang::Declaration::Decl::ParmVar:
		break;
	case NatsuLang::Declaration::Decl::EnumConstant:
		break;
	case NatsuLang::Declaration::Decl::TranslationUnit:
		break;
	default:
		break;
	}

	nat_Throw(NotImplementedException);
}

Statement::StmtPtr Deserializer::DeserializeStmt()
{
	Statement::Stmt::StmtType type;
	if (!m_Archive->ReadNumType(u8"Type", type))
	{
		ThrowInvalidData();
	}

	switch (type)
	{
	case NatsuLang::Statement::Stmt::BreakStmtClass:
		break;
	case NatsuLang::Statement::Stmt::CatchStmtClass:
		break;
	case NatsuLang::Statement::Stmt::TryStmtClass:
		break;
	case NatsuLang::Statement::Stmt::CompoundStmtClass:
		break;
	case NatsuLang::Statement::Stmt::ContinueStmtClass:
		break;
	case NatsuLang::Statement::Stmt::DeclStmtClass:
		break;
	case NatsuLang::Statement::Stmt::DoStmtClass:
		break;
	case NatsuLang::Statement::Stmt::ConditionalOperatorClass:
		break;
	case NatsuLang::Statement::Stmt::ArraySubscriptExprClass:
		break;
	case NatsuLang::Statement::Stmt::BinaryOperatorClass:
		break;
	case NatsuLang::Statement::Stmt::CompoundAssignOperatorClass:
		break;
	case NatsuLang::Statement::Stmt::BooleanLiteralClass:
		break;
	case NatsuLang::Statement::Stmt::ConstructExprClass:
		break;
	case NatsuLang::Statement::Stmt::DeleteExprClass:
		break;
	case NatsuLang::Statement::Stmt::NewExprClass:
		break;
	case NatsuLang::Statement::Stmt::ThisExprClass:
		break;
	case NatsuLang::Statement::Stmt::ThrowExprClass:
		break;
	case NatsuLang::Statement::Stmt::CallExprClass:
		break;
	case NatsuLang::Statement::Stmt::MemberCallExprClass:
		break;
	case NatsuLang::Statement::Stmt::AsTypeExprClass:
		break;
	case NatsuLang::Statement::Stmt::ImplicitCastExprClass:
		break;
	case NatsuLang::Statement::Stmt::InitListExprClass:
		break;
	case NatsuLang::Statement::Stmt::CharacterLiteralClass:
		break;
	case NatsuLang::Statement::Stmt::DeclRefExprClass:
		break;
	case NatsuLang::Statement::Stmt::FloatingLiteralClass:
		break;
	case NatsuLang::Statement::Stmt::IntegerLiteralClass:
		break;
	case NatsuLang::Statement::Stmt::MemberExprClass:
		break;
	case NatsuLang::Statement::Stmt::ParenExprClass:
		break;
	case NatsuLang::Statement::Stmt::StmtExprClass:
		break;
	case NatsuLang::Statement::Stmt::StringLiteralClass:
		break;
	case NatsuLang::Statement::Stmt::UnaryExprOrTypeTraitExprClass:
		break;
	case NatsuLang::Statement::Stmt::UnaryOperatorClass:
		break;
	case NatsuLang::Statement::Stmt::ForStmtClass:
		break;
	case NatsuLang::Statement::Stmt::GotoStmtClass:
		break;
	case NatsuLang::Statement::Stmt::IfStmtClass:
		break;
	case NatsuLang::Statement::Stmt::LabelStmtClass:
		break;
	case NatsuLang::Statement::Stmt::NullStmtClass:
		break;
	case NatsuLang::Statement::Stmt::ReturnStmtClass:
		break;
	case NatsuLang::Statement::Stmt::CaseStmtClass:
		break;
	case NatsuLang::Statement::Stmt::DefaultStmtClass:
		break;
	case NatsuLang::Statement::Stmt::SwitchStmtClass:
		break;
	case NatsuLang::Statement::Stmt::WhileStmtClass:
		break;
	default:
		break;
	}

	nat_Throw(NotImplementedException);
}

Type::TypePtr Deserializer::DeserializeType()
{
	Type::Type::TypeClass type;
	if (!m_Archive->ReadNumType(u8"Type", type))
	{
		ThrowInvalidData();
	}

	switch (type)
	{
	case NatsuLang::Type::Type::Builtin:
		break;
	case NatsuLang::Type::Type::Pointer:
		break;
	case NatsuLang::Type::Type::Array:
		break;
	case NatsuLang::Type::Type::Function:
		break;
	case NatsuLang::Type::Type::Paren:
		break;
	case NatsuLang::Type::Type::Class:
		break;
	case NatsuLang::Type::Type::Enum:
		break;
	case NatsuLang::Type::Type::Auto:
		break;
	case NatsuLang::Type::Type::Unresolved:
		break;
	default:
		break;
	}

	nat_Throw(NotImplementedException);
}

natRefPointer<ICompilerAction> Deserializer::DeserializeCompilerAction()
{
	nString name;
	if (!m_Archive->ReadString(u8"Name", name))
	{
		ThrowInvalidData();
	}

	nat_Throw(NotImplementedException);
}

Serializer::Serializer(NatsuLib::natRefPointer<ISerializationArchiveWriter> archive,
	NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> stmtTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> declTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> typeClassMap)
	: m_Archive{ std::move(archive) }, m_StmtTypeMap{ std::move(stmtTypeMap) }, m_DeclTypeMap{ std::move(declTypeMap) }, m_TypeClassMap{ std::move(typeClassMap) }
{
	m_Archive->StartEntry(u8"Content", true);
}

Serializer::~Serializer()
{
	m_Archive->EndEntry();
}

void Serializer::VisitCatchStmt(natRefPointer<Statement::CatchStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitTryStmt(natRefPointer<Statement::TryStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitCompoundStmt(natRefPointer<Statement::CompoundStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Content", true);
	for (const auto& s : stmt->GetChildrenStmt())
	{
		StmtVisitor::Visit(s);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Decl", true);
	for (const auto& d : stmt->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndEntry();
}

void Serializer::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	VisitStmt(expr);
	m_Archive->StartEntry(u8"ExprType");
	TypeVisitor::Visit(expr->GetExprType());
	m_Archive->EndEntry();
}

void Serializer::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"Cond");
	StmtVisitor::Visit(expr->GetCondition());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndEntry();
}

void Serializer::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndEntry();
}

void Serializer::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"OpCode", expr->GetOpcode());
	m_Archive->StartEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndEntry();
}

void Serializer::VisitBooleanLiteral(natRefPointer<Expression::BooleanLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteBool(u8"Value", expr->GetValue());
}

void Serializer::VisitConstructExpr(natRefPointer<Expression::ConstructExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitDeleteExpr(natRefPointer<Expression::DeleteExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitNewExpr(natRefPointer<Expression::NewExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitThisExpr(natRefPointer<Expression::ThisExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteBool(u8"IsImplicit", expr->IsImplicit());
}

void Serializer::VisitThrowExpr(natRefPointer<Expression::ThrowExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitCallExpr(natRefPointer<Expression::CallExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"Callee");
	StmtVisitor::Visit(expr->GetCallee());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Args", true);
	for (const auto& arg : expr->GetArgs())
	{
		StmtVisitor::Visit(arg);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"CastType", expr->GetCastType());
	m_Archive->StartEntry(u8"Operand");
	StmtVisitor::Visit(expr->GetOperand());
	m_Archive->EndEntry();
}

void Serializer::VisitInitListExpr(natRefPointer<Expression::InitListExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"InitExprs", true);
	for (const auto& e : expr->GetInitExprs())
	{
		StmtVisitor::Visit(e);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"Value", expr->GetCodePoint());
}

void Serializer::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	VisitExpr(expr);
	// TODO: 输出限定名即可
	nat_Throw(NotImplementedException);
}

void Serializer::VisitFloatingLiteral(natRefPointer<Expression::FloatingLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"Value", expr->GetValue());
}

void Serializer::VisitIntegerLiteral(natRefPointer<Expression::IntegerLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"Value", expr->GetValue());
}

void Serializer::VisitMemberExpr(natRefPointer<Expression::MemberExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"Base");
	StmtVisitor::Visit(expr->GetBase());
	m_Archive->EndEntry();
	m_Archive->WriteString(u8"Name", expr->GetName()->GetName());
	// TODO: 输出限定名即可
	nat_Throw(NotImplementedException);
}

void Serializer::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartEntry(u8"InnerExpr");
	StmtVisitor::Visit(expr->GetInnerExpr());
	m_Archive->EndEntry();
}

void Serializer::VisitStmtExpr(natRefPointer<Expression::StmtExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitStringLiteral(natRefPointer<Expression::StringLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteString(u8"Value", expr->GetValue());
}

void Serializer::VisitUnaryExprOrTypeTraitExpr(natRefPointer<Expression::UnaryExprOrTypeTraitExpr> const& expr)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitUnaryOperator(natRefPointer<Expression::UnaryOperator> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"OpCode", expr->GetOpcode());
	m_Archive->StartEntry(u8"Operand");
	StmtVisitor::Visit(expr->GetOperand());
	m_Archive->EndEntry();
}

void Serializer::VisitForStmt(natRefPointer<Statement::ForStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Init");
	StmtVisitor::Visit(stmt->GetInit());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Inc");
	StmtVisitor::Visit(stmt->GetInc());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndEntry();
}

void Serializer::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Then");
	StmtVisitor::Visit(stmt->GetThen());
	m_Archive->EndEntry();
	if (const auto elseStmt = stmt->GetElse())
	{
		m_Archive->StartEntry(u8"Else");
		StmtVisitor::Visit(elseStmt);
		m_Archive->EndEntry();
	}
}

void Serializer::VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"ReturnExpr");
	StmtVisitor::Visit(stmt->GetReturnExpr());
	m_Archive->EndEntry();
}

void Serializer::VisitSwitchCase(NatsuLib::natRefPointer<Statement::SwitchCase> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndEntry();
}

void Serializer::VisitStmt(natRefPointer<Statement::Stmt> const& stmt)
{
	m_Archive->WriteNumType(u8"AstNodeType", ASTNodeType::Statement);
	if (m_StmtTypeMap)
	{
		m_Archive->WriteString(u8"Type", m_StmtTypeMap->GetText(stmt->GetType()));
	}
	else
	{
		m_Archive->WriteNumType(u8"Type", stmt->GetType());
	}
}

void Serializer::Visit(Statement::StmtPtr const& stmt)
{
	StmtVisitor::Visit(stmt);
	m_Archive->NextElement();
}

void Serializer::VisitImportDecl(natRefPointer<Declaration::ImportDecl> const& decl)
{
	VisitDecl(decl);
	m_Archive->StartEntry(u8"ImportedModule");
	DeclVisitor::Visit(decl->GetModule());
	m_Archive->EndEntry();
}

void Serializer::VisitNamedDecl(natRefPointer<Declaration::NamedDecl> const& decl)
{
	VisitDecl(decl);
	m_Archive->WriteString(u8"Name", decl->GetName());
}

void Serializer::VisitAliasDecl(natRefPointer<Declaration::AliasDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartEntry(u8"AliasAs");
	const auto alias = decl->GetAliasAsAst();
	assert(alias);
	if (const auto d = alias.Cast<Declaration::Decl>())
	{
		DeclVisitor::Visit(d);
	}
	else if (const auto s = alias.Cast<Statement::Stmt>())
	{
		StmtVisitor::Visit(s);
	}
	else if (const auto t = alias.Cast<Type::Type>())
	{
		TypeVisitor::Visit(t);
	}
	else
	{
		const auto action = alias.Cast<ICompilerAction>();
		assert(action);
		SerializeCompilerAction(action);
	}
	m_Archive->EndEntry();
}

void Serializer::VisitLabelDecl(NatsuLib::natRefPointer<Declaration::LabelDecl> const& decl)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitModuleDecl(NatsuLib::natRefPointer<Declaration::ModuleDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartEntry(u8"Members", true);
	for (const auto& d : decl->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitTypeDecl(NatsuLib::natRefPointer<Declaration::TypeDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartEntry(u8"DeclaredType");
	TypeVisitor::Visit(decl->GetTypeForDecl());
	m_Archive->EndEntry();
}

void Serializer::VisitTagDecl(NatsuLib::natRefPointer<Declaration::TagDecl> const& decl)
{
	VisitTypeDecl(decl);
	m_Archive->WriteNumType(u8"TagType", decl->GetTagTypeClass());
	m_Archive->StartEntry(u8"Members", true);
	for (const auto& d : decl->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
}

void Serializer::VisitEnumDecl(NatsuLib::natRefPointer<Declaration::EnumDecl> const& decl)
{
	VisitTagDecl(decl);
	m_Archive->StartEntry(u8"UnderlyingType");
	TypeVisitor::Visit(decl->GetUnderlyingType());
	m_Archive->EndEntry();
}

void Serializer::VisitValueDecl(natRefPointer<Declaration::ValueDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartEntry(u8"DeclType");
	TypeVisitor::Visit(decl->GetValueType());
	m_Archive->EndEntry();
}

void Serializer::VisitFunctionDecl(natRefPointer<Declaration::FunctionDecl> const& decl)
{
	VisitVarDecl(decl);

}

void Serializer::VisitVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	VisitDeclaratorDecl(decl);
	m_Archive->WriteNumType(u8"StorageClass", decl->GetStorageClass());
	if (const auto initializer = decl->GetInitializer())
	{
		m_Archive->StartEntry(u8"Initializer");
		StmtVisitor::Visit(initializer);
		m_Archive->EndEntry();
	}
}

void Serializer::VisitImplicitParamDecl(NatsuLib::natRefPointer<Declaration::ImplicitParamDecl> const& decl)
{
	VisitVarDecl(decl);
	m_Archive->WriteNumType(u8"ParamType", decl->GetParamType());
}

void Serializer::VisitEnumConstantDecl(NatsuLib::natRefPointer<Declaration::EnumConstantDecl> const& decl)
{
	VisitValueDecl(decl);
	m_Archive->StartEntry(u8"InitExpr");
	StmtVisitor::Visit(decl->GetInitExpr());
	m_Archive->EndEntry();
}

void Serializer::VisitTranslationUnitDecl(NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> const& decl)
{
	VisitDecl(decl);
}

void Serializer::VisitDecl(Declaration::DeclPtr const& decl)
{
	m_Archive->WriteNumType(u8"AstNodeType", ASTNodeType::Declaration);
	if (m_DeclTypeMap)
	{
		m_Archive->WriteString(u8"Type", m_DeclTypeMap->GetText(decl->GetType()));
	}
	else
	{
		m_Archive->WriteNumType(u8"Type", decl->GetType());
	}
}

void Serializer::Visit(Declaration::DeclPtr const& decl)
{
	DeclVisitor::Visit(decl);
	m_Archive->NextElement();
}

void Serializer::VisitBuiltinType(NatsuLib::natRefPointer<Type::BuiltinType> const& type)
{
	VisitType(type);
	m_Archive->WriteNumType(u8"BuiltinType", type->GetBuiltinClass());
}

void Serializer::VisitPointerType(NatsuLib::natRefPointer<Type::PointerType> const& type)
{
	VisitType(type);
	m_Archive->StartEntry(u8"PointeeType");
	TypeVisitor::Visit(type->GetPointeeType());
	m_Archive->EndEntry();
}

void Serializer::VisitArrayType(NatsuLib::natRefPointer<Type::ArrayType> const& type)
{
	VisitType(type);
	m_Archive->StartEntry(u8"ElementType");
	TypeVisitor::Visit(type->GetElementType());
	m_Archive->EndEntry();
}

void Serializer::VisitFunctionType(NatsuLib::natRefPointer<Type::FunctionType> const& type)
{
	VisitType(type);
	m_Archive->StartEntry(u8"ResultType");
	TypeVisitor::Visit(type->GetResultType());
	m_Archive->EndEntry();
	m_Archive->StartEntry(u8"ArgType", true);
	for (const auto& arg : type->GetParameterTypes())
	{
		TypeVisitor::Visit(arg);
		m_Archive->NextElement();
	}
	m_Archive->EndEntry();
	m_Archive->WriteBool(u8"HasVarArg", type->HasVarArg());
}

void Serializer::VisitParenType(NatsuLib::natRefPointer<Type::ParenType> const& type)
{
	VisitType(type);
	m_Archive->StartEntry(u8"InnerType");
	TypeVisitor::Visit(type->GetInnerType());
	m_Archive->EndEntry();
}

void Serializer::VisitTagType(NatsuLib::natRefPointer<Type::TagType> const& type)
{
	VisitType(type);
	// TODO: 输出限定名即可
	nat_Throw(NotImplementedException);
}

void Serializer::VisitDeducedType(NatsuLib::natRefPointer<Type::DeducedType> const& type)
{
	VisitType(type);
	m_Archive->StartEntry(u8"DeducedAs");
	TypeVisitor::Visit(type->GetDeducedAsType());
	m_Archive->EndEntry();
}

void Serializer::VisitUnresolvedType(natRefPointer<Type::UnresolvedType> const& /*type*/)
{
	ThrowRejectAst();
}

void Serializer::VisitType(Type::TypePtr const& type)
{
	m_Archive->WriteNumType(u8"AstNodeType", ASTNodeType::Type);
	if (m_TypeClassMap)
	{
		m_Archive->WriteString(u8"Type", m_TypeClassMap->GetText(type->GetType()));
	}
	else
	{
		m_Archive->WriteNumType(u8"Type", type->GetType());
	}
}

void Serializer::Visit(Type::TypePtr const& type)
{
	TypeVisitor::Visit(type);
	m_Archive->NextElement();
}

void Serializer::SerializeCompilerAction(natRefPointer<ICompilerAction> const& action)
{
	m_Archive->WriteNumType(u8"AstNodeType", ASTNodeType::CompilerAction);

	nString qualifiedName = action->GetName();
	auto parent = action->GetParent();
	while (parent)
	{
		const auto parentRef = parent.Lock();
		qualifiedName = parentRef->GetName() + (u8"."_nv + qualifiedName);
		parent = parentRef->GetParent();
	}

	m_Archive->WriteString(u8"Name", qualifiedName);
}
