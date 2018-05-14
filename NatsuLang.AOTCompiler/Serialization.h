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
		NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> m_PesudoTranslationUnit;
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
		: public NatsuLib::natRefObjImpl<Serializer>, public StmtVisitor, public DeclVisitor, public TypeVisitor
	{
	public:
		explicit Serializer(Semantic::Sema& sema,
			NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> stmtTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> declTypeMap = nullptr,
			NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> typeClassMap = nullptr);
		~Serializer();

		void StartSerialize(NatsuLib::natRefPointer<ISerializationArchiveWriter> archive, nBool isExporting = true);
		void EndSerialize();

		void VisitCatchStmt(NatsuLib::natRefPointer<Statement::CatchStmt> const& stmt) override;
		void VisitTryStmt(NatsuLib::natRefPointer<Statement::TryStmt> const& stmt) override;
		void VisitCompoundStmt(NatsuLib::natRefPointer<Statement::CompoundStmt> const& stmt) override;
		void VisitDeclStmt(NatsuLib::natRefPointer<Statement::DeclStmt> const& stmt) override;
		void VisitDoStmt(NatsuLib::natRefPointer<Statement::DoStmt> const& stmt) override;
		void VisitExpr(NatsuLib::natRefPointer<Expression::Expr> const& expr) override;
		void VisitConditionalOperator(NatsuLib::natRefPointer<Expression::ConditionalOperator> const& expr) override;
		void VisitArraySubscriptExpr(NatsuLib::natRefPointer<Expression::ArraySubscriptExpr> const& expr) override;
		void VisitBinaryOperator(NatsuLib::natRefPointer<Expression::BinaryOperator> const& expr) override;
		void VisitBooleanLiteral(NatsuLib::natRefPointer<Expression::BooleanLiteral> const& expr) override;
		void VisitConstructExpr(NatsuLib::natRefPointer<Expression::ConstructExpr> const& expr) override;
		void VisitDeleteExpr(NatsuLib::natRefPointer<Expression::DeleteExpr> const& expr) override;
		void VisitNewExpr(NatsuLib::natRefPointer<Expression::NewExpr> const& expr) override;
		void VisitThisExpr(NatsuLib::natRefPointer<Expression::ThisExpr> const& expr) override;
		void VisitThrowExpr(NatsuLib::natRefPointer<Expression::ThrowExpr> const& expr) override;
		void VisitCallExpr(NatsuLib::natRefPointer<Expression::CallExpr> const& expr) override;
		void VisitCastExpr(NatsuLib::natRefPointer<Expression::CastExpr> const& expr) override;
		void VisitInitListExpr(NatsuLib::natRefPointer<Expression::InitListExpr> const& expr) override;
		void VisitCharacterLiteral(NatsuLib::natRefPointer<Expression::CharacterLiteral> const& expr) override;
		void VisitDeclRefExpr(NatsuLib::natRefPointer<Expression::DeclRefExpr> const& expr) override;
		void VisitFloatingLiteral(NatsuLib::natRefPointer<Expression::FloatingLiteral> const& expr) override;
		void VisitIntegerLiteral(NatsuLib::natRefPointer<Expression::IntegerLiteral> const& expr) override;
		void VisitMemberExpr(NatsuLib::natRefPointer<Expression::MemberExpr> const& expr) override;
		void VisitParenExpr(NatsuLib::natRefPointer<Expression::ParenExpr> const& expr) override;
		void VisitStmtExpr(NatsuLib::natRefPointer<Expression::StmtExpr> const& expr) override;
		void VisitStringLiteral(NatsuLib::natRefPointer<Expression::StringLiteral> const& expr) override;
		void VisitUnaryOperator(NatsuLib::natRefPointer<Expression::UnaryOperator> const& expr) override;
		void VisitForStmt(NatsuLib::natRefPointer<Statement::ForStmt> const& stmt) override;
		void VisitGotoStmt(NatsuLib::natRefPointer<Statement::GotoStmt> const& stmt) override;
		void VisitIfStmt(NatsuLib::natRefPointer<Statement::IfStmt> const& stmt) override;
		void VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt) override;
		void VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt) override;
		void VisitSwitchCase(NatsuLib::natRefPointer<Statement::SwitchCase> const& stmt) override;
		void VisitCaseStmt(NatsuLib::natRefPointer<Statement::CaseStmt> const& stmt) override;
		void VisitDefaultStmt(NatsuLib::natRefPointer<Statement::DefaultStmt> const& stmt) override;
		void VisitSwitchStmt(NatsuLib::natRefPointer<Statement::SwitchStmt> const& stmt) override;
		void VisitWhileStmt(NatsuLib::natRefPointer<Statement::WhileStmt> const& stmt) override;
		void VisitStmt(NatsuLib::natRefPointer<Statement::Stmt> const& stmt) override;

		void Visit(Statement::StmtPtr const& stmt) override;

		void VisitImportDecl(NatsuLib::natRefPointer<Declaration::ImportDecl> const& decl) override;
		void VisitNamedDecl(NatsuLib::natRefPointer<Declaration::NamedDecl> const& decl) override;
		void VisitAliasDecl(NatsuLib::natRefPointer<Declaration::AliasDecl> const& decl) override;
		void VisitLabelDecl(NatsuLib::natRefPointer<Declaration::LabelDecl> const& decl) override;
		void VisitModuleDecl(NatsuLib::natRefPointer<Declaration::ModuleDecl> const& decl) override;
		void VisitTypeDecl(NatsuLib::natRefPointer<Declaration::TypeDecl> const& decl) override;
		void VisitTagDecl(NatsuLib::natRefPointer<Declaration::TagDecl> const& decl) override;
		void VisitEnumDecl(NatsuLib::natRefPointer<Declaration::EnumDecl> const& decl) override;
		void VisitValueDecl(NatsuLib::natRefPointer<Declaration::ValueDecl> const& decl) override;
		void VisitFunctionDecl(NatsuLib::natRefPointer<Declaration::FunctionDecl> const& decl) override;
		void VisitVarDecl(NatsuLib::natRefPointer<Declaration::VarDecl> const& decl) override;
		void VisitImplicitParamDecl(NatsuLib::natRefPointer<Declaration::ImplicitParamDecl> const& decl) override;
		void VisitEnumConstantDecl(NatsuLib::natRefPointer<Declaration::EnumConstantDecl> const& decl) override;
		void VisitTranslationUnitDecl(NatsuLib::natRefPointer<Declaration::TranslationUnitDecl> const& decl) override;
		void VisitDecl(Declaration::DeclPtr const& decl) override;

		void Visit(Declaration::DeclPtr const& decl) override;

		void VisitBuiltinType(NatsuLib::natRefPointer<Type::BuiltinType> const& type) override;
		void VisitPointerType(NatsuLib::natRefPointer<Type::PointerType> const& type) override;
		void VisitArrayType(NatsuLib::natRefPointer<Type::ArrayType> const& type) override;
		void VisitFunctionType(NatsuLib::natRefPointer<Type::FunctionType> const& type) override;
		void VisitParenType(NatsuLib::natRefPointer<Type::ParenType> const& type) override;
		void VisitTagType(NatsuLib::natRefPointer<Type::TagType> const& type) override;
		void VisitDeducedType(NatsuLib::natRefPointer<Type::DeducedType> const& type) override;
		void VisitUnresolvedType(NatsuLib::natRefPointer<Type::UnresolvedType> const& type) override;
		void VisitType(Type::TypePtr const& type) override;

		void Visit(Type::TypePtr const& type) override;

		void SerializeCompilerAction(NatsuLib::natRefPointer<ICompilerAction> const& action);

		std::size_t GetRefCount() const volatile noexcept override;
		nBool TryIncRef() const volatile override;
		void IncRef() const volatile override;
		nBool DecRef() const volatile override;

	private:
		Semantic::Sema& m_Sema;
		NatsuLib::natRefPointer<ISerializationArchiveWriter> m_Archive;
		NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> m_StmtTypeMap;
		NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> m_DeclTypeMap;
		NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> m_TypeClassMap;

		nBool m_IsExporting;
	};
}
