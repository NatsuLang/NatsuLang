#pragma once
#include <natLinq.h>
#include "DeclBase.h"
#include "Type.h"
#include "Basic/Specifier.h"

namespace NatsuLang
{
	class ASTContext;
}

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
}

namespace NatsuLang::Statement
{
	class Stmt;
	class LabelStmt;
	using StmtPtr = NatsuLib::natRefPointer<Stmt>;
}

namespace NatsuLang::Expression
{
	class Expr;
	using ExprPtr = NatsuLib::natRefPointer<Expr>;
}

namespace NatsuLang::Module
{
	class Module;
}

namespace NatsuLang::Declaration
{
	class TranslationUnitDecl
		: public Decl, public DeclContext
	{
	public:
		explicit TranslationUnitDecl(ASTContext& context);
		~TranslationUnitDecl();

		ASTContext& GetASTContext() const noexcept;

	private:
		ASTContext& m_Context;
	};

	class NamedDecl
		: public Decl
	{
	public:
		using IdPtr = NatsuLib::natRefPointer<Identifier::IdentifierInfo>;

		NamedDecl(DeclType type, DeclContext* context, SourceLocation loc, IdPtr identifierInfo)
			: Decl(type, context, loc), m_IdentifierInfo{ std::move(identifierInfo) }
		{
		}
		~NamedDecl();

		IdPtr GetIdentifierInfo() const noexcept
		{
			return m_IdentifierInfo;
		}

		void SetIdentifierInfo(IdPtr identifierInfo) noexcept
		{
			m_IdentifierInfo = std::move(identifierInfo);
		}

		nStrView GetName() const noexcept;

		nBool HasLinkage() const noexcept
		{
			static_cast<void>(this);
			return true;
		}

	private:
		IdPtr m_IdentifierInfo;
	};

	class LabelDecl
		: public NamedDecl
	{
	public:
		using LabelStmtPtr = NatsuLib::natWeakRefPointer<Statement::LabelStmt>;

		LabelDecl(DeclContext* context, SourceLocation loc, IdPtr identifierInfo, LabelStmtPtr stmt)
			: NamedDecl{ Label, context, loc, std::move(identifierInfo) }, m_Stmt{ std::move(stmt) }
		{
		}

		~LabelDecl()
		{
		}

		LabelStmtPtr GetStmt() const noexcept
		{
			return m_Stmt;
		}

		void SetStmt(LabelStmtPtr stmt) noexcept
		{
			m_Stmt = std::move(stmt);
		}

	private:
		LabelStmtPtr m_Stmt;
	};

	class ModuleDecl
		: public NamedDecl, public DeclContext
	{
	public:
		ModuleDecl(DeclContext* context, SourceLocation idLoc, IdPtr identifierInfo, SourceLocation startLoc)
			: NamedDecl{ Module, context, idLoc, std::move(identifierInfo) }, DeclContext{ Module }, m_Start{ startLoc }
		{
		}

		~ModuleDecl();

		SourceLocation GetStartLoc() const noexcept
		{
			return m_Start;
		}

		void SetStartLoc(SourceLocation value) noexcept
		{
			m_Start = value;
		}

		SourceLocation GetEndLoc() const noexcept
		{
			return m_End;
		}

		void SetEndLoc(SourceLocation value) noexcept
		{
			m_End = value;
		}

	private:
		SourceLocation m_Start, m_End;
	};

	class ValueDecl
		: public NamedDecl
	{
	public:
		ValueDecl(DeclType declType, DeclContext* context, SourceLocation loc, IdPtr identifierInfo, Type::TypePtr valueType)
			: NamedDecl{ declType, context, loc, std::move(identifierInfo) }, m_ValueType{ std::move(valueType) }
		{
		}

		~ValueDecl();

		Type::TypePtr GetValueType() const noexcept
		{
			return m_ValueType;
		}

		void SetValueType(Type::TypePtr value) noexcept
		{
			m_ValueType = std::move(value);
		}

	private:
		Type::TypePtr m_ValueType;
	};

	class DeclaratorDecl
		: public ValueDecl
	{
	public:
		DeclaratorDecl(DeclType declType, DeclContext* context, SourceLocation loc, IdPtr identifierInfo, Type::TypePtr valueType, SourceLocation startLoc)
			: ValueDecl{ declType, context, loc, std::move(identifierInfo), std::move(valueType) }, m_StartLoc{ startLoc }
		{
		}

		~DeclaratorDecl();

		SourceLocation GetStartLoc() const noexcept
		{
			return m_StartLoc;
		}

	private:
		SourceLocation m_StartLoc;
	};

	class VarDecl
		: public DeclaratorDecl
	{
	public:
		VarDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: DeclaratorDecl{ declType, context, idLoc, std::move(identifierInfo), std::move(valueType), startLoc }, m_StorageClass{ storageClass }
		{
		}

		~VarDecl();

		Specifier::StorageClass GetStorageClass() const noexcept
		{
			return m_StorageClass;
		}

		void SetStorageClass(Specifier::StorageClass value) noexcept
		{
			m_StorageClass = value;
		}

	private:
		Specifier::StorageClass m_StorageClass;
	};

	class ImplicitParamDecl
		: public VarDecl
	{
	public:
		enum class ImplicitParamType
		{
			This,
			VTT,
			CapturedContext,
			Other
		};

		ImplicitParamDecl(DeclContext* context, SourceLocation loc, IdPtr identifierInfo, Type::TypePtr valueType, ImplicitParamType paramType)
			: VarDecl{ ImplicitParam, context, loc, loc, std::move(identifierInfo), std::move(valueType), Specifier::StorageClass::None }, m_ParamType{ paramType }
		{
		}

		~ImplicitParamDecl();

		ImplicitParamType GetParamType() const noexcept
		{
			return m_ParamType;
		}

	private:
		ImplicitParamType m_ParamType;
	};

	class ParmVarDecl
		: public VarDecl
	{
	public:
		ParmVarDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass, Expression::ExprPtr defValue)
			: VarDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass }, m_DefaultValue{ std::move(defValue) }
		{
		}

		~ParmVarDecl();

		Expression::ExprPtr GetDefaultValue() const noexcept
		{
			return m_DefaultValue;
		}

		void SetDefaultValue(Expression::ExprPtr value) noexcept
		{
			m_DefaultValue = std::move(value);
		}

	private:
		Expression::ExprPtr m_DefaultValue;
	};

	class FunctionDecl
		: public VarDecl, public DeclContext
	{
	public:
		FunctionDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: VarDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass }, DeclContext{ declType }
		{
		}

		~FunctionDecl();

		Statement::StmtPtr GetBody() const noexcept
		{
			return m_Body;
		}

		void SetBody(Statement::StmtPtr value) noexcept
		{
			m_Body = std::move(value);
		}

		NatsuLib::Linq<NatsuLib::natRefPointer<ParmVarDecl>> GetParams() const noexcept;
		void SetParams(NatsuLib::Linq<NatsuLib::natRefPointer<ParmVarDecl>> value) noexcept;

	private:
		std::vector<NatsuLib::natRefPointer<ParmVarDecl>> m_Params;
		Statement::StmtPtr m_Body;
	};

	class MethodDecl
		: public FunctionDecl
	{
	public:
		MethodDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: FunctionDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass }
		{
		}

		~MethodDecl();
	};

	class FieldDecl
		: public DeclaratorDecl
	{
	public:
		FieldDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo, Type::TypePtr valueType)
			: DeclaratorDecl{ declType, context, idLoc, std::move(identifierInfo), std::move(valueType), startLoc }
		{
		}

		~FieldDecl();
	};

	class EnumConstantDecl
		: public ValueDecl
	{
	public:
		EnumConstantDecl(DeclContext* context, SourceLocation loc, IdPtr identifierInfo, Type::TypePtr valueType, Expression::ExprPtr init, nuLong val)
			: ValueDecl{ EnumConstant, context, loc, std::move(identifierInfo), std::move(valueType) }, m_Init{ std::move(init) }, m_Value{ val }
		{
		}

		~EnumConstantDecl();

		Expression::ExprPtr GetInitExpr() const noexcept
		{
			return m_Init;
		}

		void SetInitExpr(Expression::ExprPtr value) noexcept
		{
			m_Init = std::move(value);
		}

		nuLong GetValue() const noexcept
		{
			return m_Value;
		}

		void SetValue(nuLong value) noexcept
		{
			m_Value = value;
		}

	private:
		Expression::ExprPtr m_Init;
		nuLong m_Value;
	};

	class TypeDecl
		: public NamedDecl
	{
	public:
		TypeDecl(DeclType type, DeclContext* context, SourceLocation loc, IdPtr identifierInfo, SourceLocation startLoc = {})
			: NamedDecl{ type, context, loc, std::move(identifierInfo) }, m_StartLoc{ startLoc }
		{
		}

		~TypeDecl();

		Type::TypePtr GetTypeForDecl() const noexcept
		{
			return m_Type;
		}

		void SetTypeForDecl(Type::TypePtr value) noexcept
		{
			m_Type = std::move(value);
		}

		SourceLocation GetStartLoc() const noexcept
		{
			return m_StartLoc;
		}

		void SetStartLoc(SourceLocation value) noexcept
		{
			m_StartLoc = value;
		}

	private:
		Type::TypePtr m_Type;
		SourceLocation m_StartLoc;
	};

	class TagDecl
		: public TypeDecl, public DeclContext
	{
	public:
		TagDecl(DeclType type, Type::TagType::TagTypeClass tagTypeClass, DeclContext* context, SourceLocation loc, IdPtr identifierInfo, SourceLocation startLoc)
			: TypeDecl{ type, context, loc, std::move(identifierInfo), startLoc }, DeclContext{ type }, m_TagTypeClass{ tagTypeClass }
		{
		}

		~TagDecl();

		Type::TagType::TagTypeClass GetTagTypeClass() const noexcept
		{
			return m_TagTypeClass;
		}

		void SetTagTypeClass(Type::TagType::TagTypeClass value) noexcept
		{
			m_TagTypeClass = value;
		}

	private:
		Type::TagType::TagTypeClass m_TagTypeClass;
	};

	class EnumDecl
		: public TagDecl
	{
	public:
		EnumDecl(DeclContext* context, SourceLocation loc, IdPtr identifierInfo, SourceLocation startLoc)
			: TagDecl{ Enum, Type::TagType::TagTypeClass::Enum, context, loc, std::move(identifierInfo), startLoc }
		{
		}

		~EnumDecl();

		NatsuLib::Linq<NatsuLib::natRefPointer<EnumConstantDecl>> GetEnumerators() const noexcept;
	};

	class RecordDecl
		: public TagDecl
	{
	public:
		RecordDecl(DeclType declType, Type::TagType::TagTypeClass tagTypeClass, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc, IdPtr identifierInfo)
			: TagDecl{ declType, tagTypeClass, context, idLoc, std::move(identifierInfo), startLoc }
		{
		}

		~RecordDecl();

		NatsuLib::Linq<NatsuLib::natRefPointer<FieldDecl>> GetFields() const noexcept;
	};

	class ImportDecl
		: public Decl
	{
	public:
		ImportDecl(DeclContext* context, SourceLocation loc, NatsuLib::natRefPointer<Module::Module> module)
			: Decl{ Import, context, loc }, m_Module{ std::move(module) }
		{
		}

		~ImportDecl();

		NatsuLib::natRefPointer<Module::Module> GetModule() const noexcept
		{
			return m_Module;
		}

	private:
		NatsuLib::natRefPointer<Module::Module> m_Module;
	};

	class EmptyDecl
		: public Decl
	{
	public:
		EmptyDecl(DeclContext* context, SourceLocation loc)
			: Decl{ Empty, context, loc }
		{
		}

		~EmptyDecl();
	};

	class ConstructorDecl
		: public MethodDecl
	{
	public:
		ConstructorDecl(NatsuLib::natRefPointer<RecordDecl> recordDecl, SourceLocation startLoc, IdPtr identifierInfo, Type::TypePtr type)
			: MethodDecl{ Constructor, static_cast<DeclContext*>(recordDecl.Get()), startLoc, {}, std::move(identifierInfo), std::move(type), Specifier::StorageClass::None }
		{
		}

		~ConstructorDecl();
	};
}
