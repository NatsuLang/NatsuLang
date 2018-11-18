#pragma once
#include "AST/Expression.h"
#include "AST/StmtVisitor.h"
#include "AST/DeclVisitor.h"
#include "AST/TypeVisitor.h"
#include "Basic/TextProvider.h"
#include "Basic/SerializationArchive.h"
#include "Sema/CompilerAction.h"
#include "Sema/Sema.h"
#include "Sema/Scope.h"

#include <natBinary.h>

namespace NatsuLang::Serialization
{
	DeclareException(SerializationException, NatsuLib::natException, u8"Exception generated in serialization.");

	class BinarySerializationArchiveReader
		: public NatsuLib::natRefObjImpl<BinarySerializationArchiveReader, ISerializationArchiveReader>
	{
	public:
		explicit BinarySerializationArchiveReader(NatsuLib::natRefPointer<NatsuLib::natBinaryReader> reader);
		~BinarySerializationArchiveReader();

		nBool ReadSourceLocation(nStrView key, SourceLocation& out) override;
		nBool ReadString(nStrView key, nString& out) override;
		nBool ReadInteger(nStrView key, nuLong& out, std::size_t widthHint) override;
		nBool ReadFloat(nStrView key, nDouble& out, std::size_t widthHint) override;
		nBool StartReadingEntry(nStrView key, nBool isArray) override;
		nBool NextReadingElement() override;
		std::size_t GetEntryElementCount() override;
		void EndReadingEntry() override;

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryReader> m_Reader;
		std::vector<std::pair<nBool, std::size_t>> m_EntryElementCount;
	};

	class BinarySerializationArchiveWriter
		: public NatsuLib::natRefObjImpl<BinarySerializationArchiveWriter, ISerializationArchiveWriter>
	{
	public:
		explicit BinarySerializationArchiveWriter(NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> writer);
		~BinarySerializationArchiveWriter();

		void WriteSourceLocation(nStrView key, SourceLocation const& value) override;
		void WriteString(nStrView key, nStrView value) override;
		void WriteInteger(nStrView key, nuLong value, std::size_t widthHint) override;
		void WriteFloat(nStrView key, nDouble value, std::size_t widthHint) override;
		void StartWritingEntry(nStrView key, nBool isArray) override;
		void NextWritingElement() override;
		void EndWritingEntry() override;

	private:
		NatsuLib::natRefPointer<NatsuLib::natBinaryWriter> m_Writer;
		std::vector<std::tuple<nBool, nLen, std::size_t>> m_EntryElementCount;
	};

	class Deserializer
	{
	public:
		Deserializer(Syntax::Parser& parser,
			NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> const& stmtTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> const& declTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> const& typeClassMap = nullptr);
		~Deserializer();

		std::size_t StartDeserialize(NatsuLib::natRefPointer<ISerializationArchiveReader> archive, nBool isImporting = true);
		void EndDeserialize();

		ASTNodePtr Deserialize();
		ASTNodePtr DeserializeDecl();
		ASTNodePtr DeserializeStmt();
		ASTNodePtr DeserializeType();
		ASTNodePtr DeserializeCompilerAction();

	private:
		Syntax::Parser& m_Parser;
		Semantic::Sema& m_Sema;
		NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> m_PseudoTranslationUnit;
		NatsuLib::natRefPointer<ISerializationArchiveReader> m_Archive;
		std::unordered_map<nString, Statement::Stmt::StmtType> m_StmtTypeMap;
		std::unordered_map<nString, Declaration::Decl::DeclType> m_DeclTypeMap;
		std::unordered_map<nString, Type::Type::TypeClass> m_TypeClassMap;

		std::unordered_map<nString, std::unordered_map<ASTNodePtr, std::function<void(NatsuLib::natRefPointer<Declaration::NamedDecl> const&)>>> m_UnresolvedDeclFixers;

		nBool m_IsImporting;

		Identifier::IdPtr getId(nStrView name) const;
		NatsuLib::natRefPointer<Declaration::NamedDecl> parseQualifiedName(nStrView name);
		NatsuLib::natRefPointer<Type::UnresolvedType> getUnresolvedType(nStrView name);

		void tryResolve(NatsuLib::natRefPointer<Declaration::NamedDecl> const& namedDecl);
	};

	class Serializer
		: public NatsuLib::natRefObjImpl<Serializer>, public StmtVisitor<Serializer>, public DeclVisitor<Serializer>, public TypeVisitor<Serializer>
	{
	public:
		explicit Serializer(Semantic::Sema& sema,
			NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> stmtTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> declTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> typeClassMap = nullptr);
		~Serializer();

		void StartSerialize(NatsuLib::natRefPointer<ISerializationArchiveWriter> archive, nBool isExporting = true);
		void EndSerialize();

		void VisitCatchStmt(NatsuLib::natRefPointer<Statement::CatchStmt> const& stmt);
		void VisitTryStmt(NatsuLib::natRefPointer<Statement::TryStmt> const& stmt);
		void VisitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& stmt);
		void VisitDeclStmt(NatsuLib::natRefPointer<Statement::DeclStmt> const& stmt);
		void VisitDoStmt(NatsuLib::natRefPointer<Statement::DoStmt> const& stmt);
		void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr);
		void VisitConditionalOperator(NatsuLib::natRefPointer<Expression::ConditionalOperator> const& expr);
		void VisitArraySubscriptExpr(NatsuLib::natRefPointer<Expression::ArraySubscriptExpr> const& expr);
		void VisitBinaryOperator(NatsuLib::natRefPointer<Expression::BinaryOperator> const& expr);
		void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr);
		void VisitConstructExpr(NatsuLib::natRefPointer<Expression::ConstructExpr> const& expr);
		void VisitDeleteExpr(NatsuLib::natRefPointer<Expression::DeleteExpr> const& expr);
		void VisitNewExpr(NatsuLib::natRefPointer<Expression::NewExpr> const& expr);
		void VisitThisExpr(NatsuLib::natRefPointer<Expression::ThisExpr> const& expr);
		void VisitThrowExpr(NatsuLib::natRefPointer<Expression::ThrowExpr> const& expr);
		void VisitCallExpr(NatsuLib::natRefPointer<Expression::CallExpr> const& expr);
		void VisitCastExpr(NatsuLib::natRefPointer<Expression::CastExpr> const& expr);
		void VisitInitListExpr(NatsuLib::natRefPointer<Expression::InitListExpr> const& expr);
		void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr);
		void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr);
		void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr);
		void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr);
		void VisitMemberExpr(NatsuLib::natRefPointer<Expression::MemberExpr> const& expr);
		void VisitParenExpr(NatsuLib::natRefPointer<Expression::ParenExpr> const& expr);
		void VisitStmtExpr(NatsuLib::natRefPointer<Expression::StmtExpr> const& expr);
		void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr);
		void VisitUnaryOperator(NatsuLib::natRefPointer<Expression::UnaryOperator> const& expr);
		void VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt);
		void VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt);
		void VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt);
		void VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt);
		void VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt);
		void VisitSwitchCase(NatsuLib::natRefPointer<Statement::SwitchCase> const& stmt);
		void VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt);
		void VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt);
		void VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt);
		void VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt);
		void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt);

		void Visit(Statement::StmtPtr const& stmt);

		void VisitImportDecl(NatsuLib::natRefPointer<Declaration::ImportDecl> const& decl);
		void VisitNamedDecl(NatsuLib::natRefPointer<Declaration::NamedDecl> const& decl);
		void VisitAliasDecl(NatsuLib::natRefPointer<Declaration::AliasDecl> const& decl);
		void VisitLabelDecl(NatsuLib::natRefPointer<Declaration::LabelDecl> const& decl);
		void VisitModuleDecl(NatsuLib::natRefPointer<Declaration::ModuleDecl> const& decl);
		void VisitTypeDecl(NatsuLib::natRefPointer<Declaration::TypeDecl> const& decl);
		void VisitTagDecl(NatsuLib::natRefPointer<Declaration::TagDecl> const& decl);
		void VisitEnumDecl(NatsuLib::natRefPointer<Declaration::EnumDecl> const& decl);
		void VisitValueDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl);
		void VisitFunctionDecl(NatsuLib::natRefPointer<Declaration::FunctionDecl> const& decl);
		void VisitVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl);
		void VisitImplicitParamDecl(NatsuLib::natRefPointer<Declaration::ImplicitParamDecl> const& decl);
		void VisitEnumConstantDecl(NatsuLib::natRefPointer<Declaration::EnumConstantDecl> const& decl);
		void VisitTranslationUnitDecl(NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> const& decl);
		void VisitDecl(Declaration::DeclPtr const& decl);

		void Visit(Declaration::DeclPtr const& decl);

		void VisitBuiltinType(NatsuLib::natRefPointer<Type::BuiltinType> const& type);
		void VisitPointerType(NatsuLib::natRefPointer<Type::PointerType> const& type);
		void VisitArrayType(NatsuLib::natRefPointer<Type::ArrayType> const& type);
		void VisitFunctionType(NatsuLib::natRefPointer<Type::FunctionType> const& type);
		void VisitParenType(NatsuLib::natRefPointer<Type::ParenType> const& type);
		void VisitTagType(NatsuLib::natRefPointer<Type::TagType> const& type);
		void VisitDeducedType(NatsuLib::natRefPointer<Type::DeducedType> const& type);
		void VisitUnresolvedType(NatsuLib::natRefPointer<Type::UnresolvedType> const& type);
		void VisitType(Type::TypePtr const& type);

		void Visit(Type::TypePtr const& type);

		void SerializeCompilerAction(NatsuLib::natRefPointer<ICompilerAction> const& action);

	private:
		Semantic::Sema& m_Sema;
		NatsuLib::natRefPointer<ISerializationArchiveWriter> m_Archive;
		NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> m_StmtTypeMap;
		NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> m_DeclTypeMap;
		NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> m_TypeClassMap;

		nBool m_IsExporting;
	};
}
