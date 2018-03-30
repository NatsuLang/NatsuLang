#pragma once
#include <natLinq.h>
#include "DeclBase.h"
#include "Type.h"
#include "Basic/Specifier.h"

namespace NatsuLang
{
	class ASTContext;

	namespace Declaration
	{
		class ModuleDecl;
	}
}

namespace NatsuLang::Identifier
{
	class IdentifierInfo;
	using IdPtr = NatsuLib::natRefPointer<IdentifierInfo>;
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

namespace NatsuLang::Declaration
{
	class Declarator;

	class TranslationUnitDecl
		: public Decl, public DeclContext
	{
	public:
		explicit TranslationUnitDecl(ASTContext& context);
		~TranslationUnitDecl();

		ASTContext& GetASTContext() const noexcept;

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		ASTContext& m_Context;
	};

	class NamedDecl
		: public Decl
	{
	public:
		NamedDecl(DeclType type, DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo)
			: Decl(type, context, loc), m_IdentifierInfo{ std::move(identifierInfo) }
		{
		}

		~NamedDecl();

		Identifier::IdPtr GetIdentifierInfo() const noexcept
		{
			return m_IdentifierInfo;
		}

		void SetIdentifierInfo(Identifier::IdPtr identifierInfo) noexcept
		{
			m_IdentifierInfo = std::move(identifierInfo);
		}

		nStrView GetName() const noexcept;

		nBool HasLinkage() const noexcept
		{
			static_cast<void>(this);
			return true;
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Identifier::IdPtr m_IdentifierInfo;
	};

	class AliasDecl
		: public NamedDecl
	{
	public:
		AliasDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, ASTNodePtr aliasAsAst)
			: NamedDecl{ Alias, context, loc, std::move(identifierInfo) }, m_AliasAsAst{ std::move(aliasAsAst) }
		{
		}

		~AliasDecl();

		ASTNodePtr GetAliasAsAst() const noexcept
		{
			return m_AliasAsAst;
		}

		void SetAliasAsAst(ASTNodePtr value) noexcept
		{
			m_AliasAsAst = std::move(value);
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		ASTNodePtr m_AliasAsAst;
	};

	class LabelDecl
		: public NamedDecl
	{
	public:
		using LabelStmtPtr = NatsuLib::natWeakRefPointer<Statement::LabelStmt>;

		LabelDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, LabelStmtPtr stmt)
			: NamedDecl{ Label, context, loc, std::move(identifierInfo) }, m_Stmt{ std::move(stmt) }
		{
		}

		~LabelDecl();

		LabelStmtPtr GetStmt() const noexcept
		{
			return m_Stmt;
		}

		void SetStmt(LabelStmtPtr stmt) noexcept
		{
			m_Stmt = std::move(stmt);
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		LabelStmtPtr m_Stmt;
	};

	class ModuleDecl
		: public NamedDecl, public DeclContext
	{
	public:
		ModuleDecl(DeclContext* context, SourceLocation idLoc, Identifier::IdPtr identifierInfo, SourceLocation startLoc)
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		SourceLocation m_Start, m_End;
	};

	class ValueDecl
		: public NamedDecl
	{
	public:
		ValueDecl(DeclType declType, DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo,
		          Type::TypePtr valueType)
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Type::TypePtr m_ValueType;
	};

	class DeclaratorDecl
		: public ValueDecl
	{
	public:
		DeclaratorDecl(DeclType declType, DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo,
		               Type::TypePtr valueType, SourceLocation startLoc)
			: ValueDecl{ declType, context, loc, std::move(identifierInfo), std::move(valueType) }, m_StartLoc{ startLoc }
		{
		}

		~DeclaratorDecl();

		SourceLocation GetStartLoc() const noexcept
		{
			return m_StartLoc;
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		SourceLocation m_StartLoc;
	};

	class UnresolvedDecl
		: public DeclaratorDecl
	{
	public:
		UnresolvedDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, Type::TypePtr valueType,
		               SourceLocation startLoc, NatsuLib::natWeakRefPointer<Declarator> declaratorPtr = nullptr)
			: DeclaratorDecl{ Unresolved, context, loc, std::move(identifierInfo), std::move(valueType), startLoc },
			  m_DeclaratorPtr{ std::move(declaratorPtr) }
		{
		}

		~UnresolvedDecl();

		NatsuLib::natWeakRefPointer<Declarator> GetDeclaratorPtr() const noexcept
		{
			return m_DeclaratorPtr;
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		// 在 resolve 时使用，resolve 完成后将会过期
		NatsuLib::natWeakRefPointer<Declarator> m_DeclaratorPtr;
	};

	class VarDecl
		: public DeclaratorDecl
	{
	public:
		VarDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc,
		        Identifier::IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: DeclaratorDecl{ declType, context, idLoc, std::move(identifierInfo), std::move(valueType), startLoc },
			  m_StorageClass{ storageClass }
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

		Expression::ExprPtr GetInitializer() const noexcept
		{
			return m_Initializer;
		}

		void SetInitializer(Expression::ExprPtr value) noexcept
		{
			m_Initializer = std::move(value);
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Specifier::StorageClass m_StorageClass;
		Expression::ExprPtr m_Initializer;
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

		ImplicitParamDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, Type::TypePtr valueType,
		                  ImplicitParamType paramType)
			: VarDecl{
				ImplicitParam, context, loc, loc, std::move(identifierInfo), std::move(valueType), Specifier::StorageClass::None
			}, m_ParamType{ paramType }
		{
		}

		~ImplicitParamDecl();

		ImplicitParamType GetParamType() const noexcept
		{
			return m_ParamType;
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		ImplicitParamType m_ParamType;
	};

	class ParmVarDecl
		: public VarDecl
	{
	public:
		ParmVarDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc,
		            Identifier::IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass,
		            Expression::ExprPtr defValue)
			: VarDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass }
		{
			SetInitializer(std::move(defValue));
		}

		~ParmVarDecl();

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	class FunctionDecl
		: public VarDecl, public DeclContext
	{
	public:
		FunctionDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc,
		             Identifier::IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: VarDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass },
			  DeclContext{ declType }
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

		std::size_t GetParamCount() const noexcept
		{
			return m_Params.size();
		}

		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<ParmVarDecl>>> GetParams() const noexcept;
		void SetParams(NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<ParmVarDecl>>> value) noexcept;

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		std::vector<NatsuLib::natRefPointer<ParmVarDecl>> m_Params;
		Statement::StmtPtr m_Body;
	};

	class MethodDecl
		: public FunctionDecl
	{
	public:
		MethodDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc,
		           Identifier::IdPtr identifierInfo, Type::TypePtr valueType, Specifier::StorageClass storageClass)
			: FunctionDecl{ declType, context, startLoc, idLoc, std::move(identifierInfo), std::move(valueType), storageClass }
		{
		}

		~MethodDecl();

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	class FieldDecl
		: public DeclaratorDecl
	{
	public:
		FieldDecl(DeclType declType, DeclContext* context, SourceLocation startLoc, SourceLocation idLoc,
		          Identifier::IdPtr identifierInfo, Type::TypePtr valueType)
			: DeclaratorDecl{ declType, context, idLoc, std::move(identifierInfo), std::move(valueType), startLoc }
		{
		}

		~FieldDecl();

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	// TODO: 延迟分析
	class EnumConstantDecl
		: public ValueDecl
	{
	public:
		EnumConstantDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, Type::TypePtr valueType,
		                 Expression::ExprPtr init, nuLong val)
			: ValueDecl{ EnumConstant, context, loc, std::move(identifierInfo), std::move(valueType) },
			  m_Init{ std::move(init) }, m_Value{ val }
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Expression::ExprPtr m_Init;
		nuLong m_Value;
	};

	class TypeDecl
		: public NamedDecl
	{
	public:
		TypeDecl(DeclType type, DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo,
		         SourceLocation startLoc = {})
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Type::TypePtr m_Type;
		SourceLocation m_StartLoc;
	};

	class TagDecl
		: public TypeDecl, public DeclContext
	{
	public:
		TagDecl(DeclType type, Type::TagType::TagTypeClass tagTypeClass, DeclContext* context, SourceLocation loc,
		        Identifier::IdPtr identifierInfo, SourceLocation startLoc)
			: TypeDecl{ type, context, loc, std::move(identifierInfo), startLoc }, DeclContext{ type },
			  m_TagTypeClass{ tagTypeClass }
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Type::TagType::TagTypeClass m_TagTypeClass;
	};

	class EnumDecl
		: public TagDecl
	{
	public:
		EnumDecl(DeclContext* context, SourceLocation loc, Identifier::IdPtr identifierInfo, SourceLocation startLoc)
			: TagDecl{ Enum, Type::TagType::TagTypeClass::Enum, context, loc, std::move(identifierInfo), startLoc }
		{
		}

		~EnumDecl();

		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<EnumConstantDecl>>> GetEnumerators() const noexcept;

		Type::TypePtr GetUnderlyingType() const noexcept
		{
			return m_UnderlyingType;
		}

		void SetUnderlyingType(Type::TypePtr value) noexcept
		{
			m_UnderlyingType = std::move(value);
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		Type::TypePtr m_UnderlyingType;
	};

	class ClassDecl
		: public TagDecl
	{
	public:
		ClassDecl(DeclContext* context, SourceLocation idLoc, Identifier::IdPtr identifierInfo, SourceLocation startLoc)
			: TagDecl{ Class, Type::TagType::TagTypeClass::Class, context, idLoc, std::move(identifierInfo), startLoc }
		{
		}

		~ClassDecl();

		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<FieldDecl>>> GetFields() const noexcept;
		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<MethodDecl>>> GetMethods() const noexcept;
		NatsuLib::Linq<NatsuLib::Valued<NatsuLib::natRefPointer<ClassDecl>>> GetBases() const noexcept;

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	class ImportDecl
		: public Decl
	{
	public:
		ImportDecl(DeclContext* context, SourceLocation loc, NatsuLib::natRefPointer<ModuleDecl> module)
			: Decl{ Import, context, loc }, m_Module{ std::move(module) }
		{
		}

		~ImportDecl();

		NatsuLib::natRefPointer<ModuleDecl> GetModule() const noexcept
		{
			return m_Module;
		}

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;

	private:
		NatsuLib::natRefPointer<ModuleDecl> m_Module;
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

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	class ConstructorDecl
		: public MethodDecl
	{
	public:
		ConstructorDecl(DeclContext* dc, SourceLocation startLoc,
		                Identifier::IdPtr identifierInfo, Type::TypePtr type, Specifier::StorageClass storageClass)
			: MethodDecl{
				Constructor, dc, startLoc, {}, std::move(identifierInfo), std::move(type),
				storageClass
			}
		{
		}

		~ConstructorDecl();

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};

	class DestructorDecl
		: public MethodDecl
	{
	public:
		DestructorDecl(DeclContext* dc, SourceLocation startLoc,
		               Identifier::IdPtr identifierInfo, Type::TypePtr type, Specifier::StorageClass storageClass)
			: MethodDecl{
				Constructor, dc, startLoc, {}, std::move(identifierInfo), std::move(type),
				storageClass
			}
		{
		}

		~DestructorDecl();

		void Accept(NatsuLib::natRefPointer<DeclVisitor> const& visitor) override;
	};
}
