#include "Serialization.h"
#include "Basic/Identifier.h"
#include "AST/NestedNameSpecifier.h"
#include "Lex/Preprocessor.h"
#include "Sema/Declarator.h"
#include "AST/ASTContext.h"
#include "Parse/Parser.h"

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

	nString GetQualifiedName(natRefPointer<Declaration::NamedDecl> const& decl)
	{
		nString name = decl->GetName();
		auto parent = decl->GetContext();
		while (parent)
		{
			const auto parentDecl = Declaration::Decl::CastFromDeclContext(parent);
			if (const auto namedDecl = dynamic_cast<Declaration::NamedDecl*>(parentDecl))
			{
				name = namedDecl->GetName() + (u8"."_nv + name);
				parent = namedDecl->GetContext();
			}
			else if (dynamic_cast<Declaration::TranslationUnitDecl*>(parentDecl) || dynamic_cast<Declaration::FunctionDecl*>(parent))
			{
				// 翻译单元即顶层声明上下文，函数内部的定义不具有链接性，到此结束即可
				break;
			}
			else
			{
				nat_Throw(SerializationException, u8"Parent is not a namedDecl."_nv);
			}
		}

		return name;
	}

	class UnresolvedId
		: public natRefObjImpl<UnresolvedId, ASTNode>
	{
	public:
		explicit UnresolvedId(nString name)
			: m_Name{ std::move(name) }
		{
		}

		nStrView GetName() const noexcept
		{
			return m_Name;
		}

	private:
		nString m_Name;
	};
}

BinarySerializationArchiveReader::BinarySerializationArchiveReader(natRefPointer<natBinaryReader> reader)
	: m_Reader{ std::move(reader) }
{
}

BinarySerializationArchiveReader::~BinarySerializationArchiveReader()
{
}

nBool BinarySerializationArchiveReader::ReadSourceLocation(nStrView key, SourceLocation& out)
{
	nat_Throw(NotImplementedException);
}

nBool BinarySerializationArchiveReader::ReadString(nStrView key, nString& out)
{
	const auto size = m_Reader->ReadPod<nuInt>();
	out.Resize(size);
	return m_Reader->GetUnderlyingStream()->ReadBytes(reinterpret_cast<nData>(out.data()), size) == size;
}

nBool BinarySerializationArchiveReader::ReadInteger(nStrView key, nuLong& out, std::size_t widthHint)
{
	// TODO: 注意端序
	out = 0;
	return m_Reader->GetUnderlyingStream()->ReadBytes(reinterpret_cast<nData>(&out), widthHint);
}

nBool BinarySerializationArchiveReader::ReadFloat(nStrView key, nDouble& out, std::size_t widthHint)
{
	// TODO: 注意端序
	out = 0;
	return m_Reader->GetUnderlyingStream()->ReadBytes(reinterpret_cast<nData>(&out), widthHint);
}

nBool BinarySerializationArchiveReader::StartReadingEntry(nStrView key, nBool isArray)
{
	if (isArray)
	{
		const auto count = m_Reader->ReadPod<nuInt>();
		m_EntryElementCount.emplace_back(true, static_cast<std::size_t>(count));
	}
	else
	{
		m_EntryElementCount.emplace_back(false, 1);
	}

	return true;
}

nBool BinarySerializationArchiveReader::NextReadingElement()
{
	return true;
}

std::size_t BinarySerializationArchiveReader::GetEntryElementCount()
{
	const auto& back = m_EntryElementCount.back();
	return back.second;
}

void BinarySerializationArchiveReader::EndReadingEntry()
{
	m_EntryElementCount.pop_back();
}

BinarySerializationArchiveWriter::BinarySerializationArchiveWriter(natRefPointer<natBinaryWriter> writer)
	: m_Writer{ std::move(writer) }
{
	if (!m_Writer->GetUnderlyingStream()->CanSeek())
	{
		nat_Throw(SerializationException, u8"Stream should be seekable."_nv);
	}
}

BinarySerializationArchiveWriter::~BinarySerializationArchiveWriter()
{
}

void BinarySerializationArchiveWriter::WriteSourceLocation(nStrView key, SourceLocation const& value)
{
	nat_Throw(NotImplementedException);
}

void BinarySerializationArchiveWriter::WriteString(nStrView key, nStrView value)
{
	const auto size = value.size();
	m_Writer->WritePod(static_cast<nuInt>(size));
	m_Writer->GetUnderlyingStream()->WriteBytes(reinterpret_cast<ncData>(value.data()), size);
}

void BinarySerializationArchiveWriter::WriteInteger(nStrView key, nuLong value, std::size_t widthHint)
{
	// TODO: 注意端序
	m_Writer->GetUnderlyingStream()->WriteBytes(reinterpret_cast<ncData>(&value), widthHint);
}

void BinarySerializationArchiveWriter::WriteFloat(nStrView key, nDouble value, std::size_t widthHint)
{
	// TODO: 注意端序
	m_Writer->GetUnderlyingStream()->WriteBytes(reinterpret_cast<ncData>(&value), widthHint);
}

void BinarySerializationArchiveWriter::StartWritingEntry(nStrView key, nBool isArray)
{
	m_EntryElementCount.emplace_back(isArray, m_Writer->GetUnderlyingStream()->GetPosition(), std::size_t{});
	if (isArray)
	{
		m_Writer->WritePod(nuInt{});
	}
}

void BinarySerializationArchiveWriter::NextWritingElement()
{
	++std::get<2>(m_EntryElementCount.back());
}

void BinarySerializationArchiveWriter::EndWritingEntry()
{
	const auto& back = m_EntryElementCount.back();
	if (std::get<0>(back))
	{
		const auto pos = m_Writer->GetUnderlyingStream()->GetPosition();
		m_Writer->GetUnderlyingStream()->SetPositionFromBegin(std::get<1>(back));
		m_Writer->WritePod(static_cast<nuInt>(std::get<2>(back)));
		m_Writer->GetUnderlyingStream()->SetPositionFromBegin(pos);
	}

	m_EntryElementCount.pop_back();
}

Deserializer::Deserializer(Syntax::Parser& parser, NatsuLib::natRefPointer<ISerializationArchiveReader> archive,
	NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> const& stmtTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> const& declTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> const& typeClassMap)
	: m_Parser{ parser }, m_Sema{ parser.GetSema() }, m_Archive{ std::move(archive) }, m_IsImporting{ false }
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
}

Deserializer::~Deserializer()
{
}

std::size_t Deserializer::StartDeserialize(nBool isImporting)
{
	m_IsImporting = isImporting;
	m_Sema.PushScope(Semantic::ScopeFlags::DeclarableScope);
	m_PesudoTranslationUnit = make_ref<Declaration::TranslationUnitDecl>(m_Sema.GetASTContext());
	// 假装属于真正的翻译单元，不会真的加进去
	m_PesudoTranslationUnit->SetContext(m_Sema.GetASTContext().GetTranslationUnit().Get());
	m_Sema.PushDeclContext(m_Sema.GetCurrentScope(), m_PesudoTranslationUnit.Get());
	m_Archive->StartReadingEntry(u8"Content", true);
	return m_Archive->GetEntryElementCount();
}

void Deserializer::EndDeserialize()
{
	m_Archive->EndReadingEntry();

	if (!m_UnresolvedDeclFixers.empty())
	{
		ThrowInvalidData();
	}

	m_Sema.PopDeclContext();
	m_Sema.PopScope();
	m_PesudoTranslationUnit->RemoveAllDecl();
	m_PesudoTranslationUnit.Reset();
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
		assert(!"Invalid type.");
		ThrowInvalidData();
	}
}

ASTNodePtr Deserializer::DeserializeDecl()
{
	Declaration::Decl::DeclType type;
	if (!m_Archive->ReadNumType(u8"Type", type))
	{
		ThrowInvalidData();
	}

	std::vector<Declaration::AttrPtr> attributes;
	if (!m_Archive->StartReadingEntry(u8"Attributes", true))
	{
		ThrowInvalidData();
	}

	{
		nString attrName;
		const auto count = m_Archive->GetEntryElementCount();
		attributes.reserve(count);
		for (std::size_t i = 0; i < count; ++i)
		{
			if (!m_Archive->ReadString(u8"Name", attrName))
			{
				ThrowInvalidData();
			}

			attributes.emplace_back(m_Sema.DeserializeAttribute(attrName, m_Archive));
			m_Archive->NextReadingElement();
		}
	}

	m_Archive->EndReadingEntry();

	switch (type)
	{
	case NatsuLang::Declaration::Decl::TranslationUnit:
	case NatsuLang::Declaration::Decl::Empty:
	case NatsuLang::Declaration::Decl::Import:
		break;
	default:
		if (type >= Declaration::Decl::FirstNamed && type <= Declaration::Decl::LastNamed)
		{
			nString name;
			if (!m_Archive->ReadString(u8"Name", name))
			{
				ThrowInvalidData();
			}

			auto id = getId(name);

			switch (type)
			{
			case NatsuLang::Declaration::Decl::Alias:
			{
				if (!m_Archive->StartReadingEntry(u8"AliasAs"))
				{
					ThrowInvalidData();
				}

				auto aliasAs = Deserialize();

				m_Archive->EndReadingEntry();

				auto ret = m_Sema.ActOnAliasDeclaration(m_Sema.GetCurrentScope(), {}, std::move(id), aliasAs);

				if (const auto& unresolved = aliasAs.Cast<UnresolvedId>())
				{
					m_UnresolvedDeclFixers.emplace(unresolved->GetName(), [ret](natRefPointer<Declaration::NamedDecl> decl)
					{
						if (const auto tag = decl.Cast<Declaration::TagDecl>())
						{
							ret->SetAliasAsAst(tag->GetTypeForDecl());
						}
						else
						{
							ret->SetAliasAsAst(decl);
						}
					});
				}

				tryResolve(ret);
				for (auto attr : attributes)
				{
					ret->AttachAttribute(std::move(attr));
				}
				return ret;
			}
			case NatsuLang::Declaration::Decl::Label:
				break;
			case NatsuLang::Declaration::Decl::Module:
			{
				auto module = m_Sema.ActOnModuleDecl(m_Sema.GetCurrentScope(), {}, std::move(id));
				{
					Syntax::Parser::ParseScope moduleScope{ &m_Parser, Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::ModuleScope };
					m_Sema.ActOnStartModule(m_Sema.GetCurrentScope(), module);
					if (!m_Archive->StartReadingEntry(u8"Members", true))
					{
						ThrowInvalidData();
					}

					const auto count = m_Archive->GetEntryElementCount();
					for (std::size_t i = 0; i < count; ++i)
					{
						auto ast = Deserialize();
						m_Archive->NextReadingElement();
					}

					m_Archive->EndReadingEntry();
					m_Sema.ActOnFinishModule();
				}

				tryResolve(module);
				for (auto attr : attributes)
				{
					module->AttachAttribute(std::move(attr));
				}
				return module;
			}
			case NatsuLang::Declaration::Decl::Enum:
			case NatsuLang::Declaration::Decl::Class:
			{
				Type::TagType::TagTypeClass tagType;

				if (!m_Archive->ReadNumType(u8"TagType", tagType))
				{
					ThrowInvalidData();
				}

				ASTNodePtr underlyingType;

				if (tagType == Type::TagType::TagTypeClass::Enum)
				{
					if (!m_Archive->StartReadingEntry(u8"UnderlyingType"))
					{
						ThrowInvalidData();
					}
					underlyingType = Deserialize();
					if (!underlyingType)
					{
						ThrowInvalidData();
					}
				}

				auto tagDecl = m_Sema.ActOnTag(m_Sema.GetCurrentScope(), tagType, {}, Specifier::Access::None, std::move(id), {}, underlyingType);
				if (type == Declaration::Decl::Class)
				{
					tagDecl->SetTypeForDecl(make_ref<Type::ClassType>(tagDecl));
				}
				else
				{
					assert(type == Declaration::Decl::Enum);
					if (const auto& unresolved = underlyingType.Cast<UnresolvedId>())
					{
						m_UnresolvedDeclFixers.emplace(unresolved->GetName(), [tagDecl](natRefPointer<Declaration::NamedDecl> const& decl)
						{
							tagDecl->SetTypeForDecl(decl.Cast<Declaration::TagDecl>()->GetTypeForDecl());
						});
					}
					else
					{
						tagDecl->SetTypeForDecl(std::move(underlyingType));
					}
				}

				{
					auto flag = Semantic::ScopeFlags::DeclarableScope;
					if (type == Declaration::Decl::Class)
					{
						flag |= Semantic::ScopeFlags::ClassScope;
					}
					else
					{
						assert(type == Declaration::Decl::Enum);
						flag |= Semantic::ScopeFlags::EnumScope;
					}

					Syntax::Parser::ParseScope tagScope{ &m_Parser, flag };
					m_Sema.ActOnTagStartDefinition(m_Sema.GetCurrentScope(), tagDecl);

					if (!m_Archive->StartReadingEntry(u8"Members", true))
					{
						ThrowInvalidData();
					}

					const auto count = m_Archive->GetEntryElementCount();
					for (std::size_t i = 0; i < count; ++i)
					{
						auto ast = Deserialize();
						m_Archive->NextReadingElement();
					}

					m_Archive->EndReadingEntry();
					m_Sema.ActOnTagFinishDefinition();
				}

				tryResolve(tagDecl);
				for (auto attr : attributes)
				{
					tagDecl->AttachAttribute(std::move(attr));
				}
				return tagDecl;
			}
			case NatsuLang::Declaration::Decl::Unresolved:
				break;
			default:
				if (type >= Declaration::Decl::FirstValue && type <= Declaration::Decl::LastValue)
				{
					if (!m_Archive->StartReadingEntry(u8"DeclType"))
					{
						ThrowInvalidData();
					}
					Type::TypePtr valueType = Deserialize();
					if (!valueType)
					{
						ThrowInvalidData();
					}
					m_Archive->EndReadingEntry();

					auto dc = Declaration::Decl::CastToDeclContext(m_Sema.GetDeclContext().Get());

					switch (type)
					{
					case NatsuLang::Declaration::Decl::Field:
					{
						auto fieldDecl = make_ref<Declaration::FieldDecl>(Declaration::Decl::Field, dc, SourceLocation{},
							SourceLocation{}, std::move(id), std::move(valueType));
						m_Sema.PushOnScopeChains(fieldDecl, m_Sema.GetCurrentScope());
						tryResolve(fieldDecl);
						for (auto attr : attributes)
						{
							fieldDecl->AttachAttribute(std::move(attr));
						}
						return fieldDecl;
					}
					default:
						if (type >= Declaration::Decl::FirstVar && type <= Declaration::Decl::LastVar)
						{
							natRefPointer<Declaration::VarDecl> decl;

							Specifier::StorageClass storageClass;
							if (!m_Archive->ReadNumType(u8"StorageClass", storageClass))
							{
								ThrowInvalidData();
							}

							if (m_IsImporting)
							{
								if (storageClass != Specifier::StorageClass::Static)
								{
									storageClass = Specifier::StorageClass::Extern;
								}
							}

							Expression::ExprPtr initializer;

							nBool hasInitializer;
							if (m_Archive->ReadBool(u8"HasInitializer", hasInitializer))
							{
								if (hasInitializer)
								{
									if (m_Archive->StartReadingEntry(u8"Initializer"))
									{
										initializer = Deserialize();
										if (!initializer)
										{
											ThrowInvalidData();
										}
										m_Archive->EndReadingEntry();
									}
									else
									{
										ThrowInvalidData();
									}
								}
							}

							auto addToContext = true;

							switch (type)
							{
							case NatsuLang::Declaration::Decl::Var:
								decl = make_ref<Declaration::VarDecl>(Declaration::Decl::Var, dc, SourceLocation{},
									SourceLocation{}, std::move(id), std::move(valueType),
									storageClass);
								if (initializer)
								{
									decl->SetInitializer(std::move(initializer));
								}
								break;
							case NatsuLang::Declaration::Decl::ImplicitParam:
								nat_Throw(NotImplementedException);
							case NatsuLang::Declaration::Decl::ParmVar:
								decl = make_ref<Declaration::ParmVarDecl>(Declaration::Decl::ParmVar,
									m_Sema.GetASTContext().GetTranslationUnit().Get(), SourceLocation{}, SourceLocation{},
									std::move(id), std::move(valueType), Specifier::StorageClass::None,
									initializer);
								addToContext = false;
								break;
							case NatsuLang::Declaration::Decl::EnumConstant:
								nat_Throw(NotImplementedException);
							default:
								if (type >= Declaration::Decl::FirstFunction && type <= Declaration::Decl::LastFunction)
								{
									natRefPointer<Declaration::FunctionDecl> funcDecl;

									switch (type)
									{
									case NatsuLang::Declaration::Decl::Function:
										funcDecl = make_ref<Declaration::FunctionDecl>(Declaration::Decl::Function, dc,
											SourceLocation{}, SourceLocation{}, std::move(id), std::move(valueType),
											storageClass);
										break;
									case NatsuLang::Declaration::Decl::Method:
										funcDecl = make_ref<Declaration::MethodDecl>(Declaration::Decl::Method, dc,
											SourceLocation{}, SourceLocation{}, std::move(id), std::move(valueType),
											storageClass);
										break;
									case NatsuLang::Declaration::Decl::Constructor:
										funcDecl = make_ref<Declaration::ConstructorDecl>(dc, SourceLocation{},
											std::move(id), std::move(valueType), storageClass);
										break;
									case NatsuLang::Declaration::Decl::Destructor:
										funcDecl = make_ref<Declaration::DestructorDecl>(dc, SourceLocation{},
											std::move(id), std::move(valueType), storageClass);
										break;
									default:
										ThrowInvalidData();
									}

									if (!m_Archive->StartReadingEntry(u8"Params", true))
									{
										ThrowInvalidData();
									}

									std::vector<natRefPointer<Declaration::ParmVarDecl>> params;

									const auto paramCount = m_Archive->GetEntryElementCount();

									params.reserve(paramCount);

									{
										Syntax::Parser::ParseScope prototypeScope{
											&m_Parser,
											Semantic::ScopeFlags::FunctionDeclarationScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::FunctionPrototypeScope
										};

										for (std::size_t i = 0; i < paramCount; ++i)
										{
											if (auto param = Deserialize().Cast<Declaration::ParmVarDecl>())
											{
												params.emplace_back(std::move(param));
											}
											else
											{
												ThrowInvalidData();
											}
											m_Archive->NextReadingElement();
										}
									}

									funcDecl->SetParams(from(params));

									m_Archive->EndReadingEntry();

									Statement::StmtPtr body;
									nBool hasBody;
									if (m_Archive->ReadBool(u8"HasBody", hasBody))
									{
										if (hasBody)
										{
											if (m_Archive->StartReadingEntry(u8"Body"))
											{
												{
													Syntax::Parser::ParseScope bodyScope{
														&m_Parser,
														Semantic::ScopeFlags::FunctionScope | Semantic::ScopeFlags::DeclarableScope | Semantic::ScopeFlags::CompoundStmtScope
													};
													m_Sema.PushDeclContext(m_Sema.GetCurrentScope(), funcDecl.Get());

													for (auto param : params)
													{
														m_Sema.PushOnScopeChains(std::move(param), m_Sema.GetCurrentScope());
													}

													body = Deserialize().Cast<Statement::Stmt>();
													m_Sema.PopDeclContext();
												}

												if (!body)
												{
													ThrowInvalidData();
												}
												m_Archive->EndReadingEntry();
											}
											else
											{
												ThrowInvalidData();
											}
										}
									}

									funcDecl->SetBody(std::move(body));
									decl = std::move(funcDecl);
									break;
								}

								ThrowInvalidData();
							}

							// 不导入内部链接性的声明
							if (m_IsImporting && storageClass == Specifier::StorageClass::Static)
							{
								return nullptr;
							}

							m_Sema.PushOnScopeChains(decl, m_Sema.GetCurrentScope(), addToContext);
							tryResolve(decl);
							for (auto attr : attributes)
							{
								decl->AttachAttribute(std::move(attr));
							}
							return decl;
						}
						break;
					}
				}
				break;
			}
		}
		break;
	}

	nat_Throw(NotImplementedException);
}

ASTNodePtr Deserializer::DeserializeStmt()
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

ASTNodePtr Deserializer::DeserializeType()
{
	Type::Type::TypeClass type;
	if (!m_Archive->ReadNumType(u8"Type", type))
	{
		ThrowInvalidData();
	}

	switch (type)
	{
	case NatsuLang::Type::Type::Builtin:
	{
		Type::BuiltinType::BuiltinClass builtinType;
		if (!m_Archive->ReadNumType(u8"BuiltinType", builtinType))
		{
			ThrowInvalidData();
		}

		return m_Sema.GetASTContext().GetBuiltinType(builtinType);
	}
	case NatsuLang::Type::Type::Pointer:
	{
		m_Archive->StartReadingEntry(u8"PointeeType");
		auto pointeeType = Deserialize().Cast<Type::Type>();
		if (!pointeeType)
		{
			ThrowInvalidData();
		}
		m_Archive->EndReadingEntry();
		return m_Sema.GetASTContext().GetPointerType(std::move(pointeeType));
	}
	case NatsuLang::Type::Type::Array:
	{
		m_Archive->StartReadingEntry(u8"ElementType");
		auto elementType = Deserialize().Cast<Type::Type>();
		if (!elementType)
		{
			ThrowInvalidData();
		}
		m_Archive->EndReadingEntry();
		nuLong arraySize;
		if (!m_Archive->ReadNumType(u8"ArraySize", arraySize))
		{
			ThrowInvalidData();
		}

		return m_Sema.GetASTContext().GetArrayType(std::move(elementType), arraySize);
	}
	case NatsuLang::Type::Type::Function:
	{
		if (!m_Archive->StartReadingEntry(u8"ResultType"))
		{
			ThrowInvalidData();
		}
		auto resultType = Deserialize().Cast<Type::Type>();
		if (!resultType)
		{
			ThrowInvalidData();
		}
		m_Archive->EndReadingEntry();

		std::vector<Type::TypePtr> args;
		m_Archive->StartReadingEntry(u8"ArgType", true);
		if (const auto count = m_Archive->GetEntryElementCount())
		{
			args.reserve(count);
			for (std::size_t i = 0; i < count; ++i)
			{
				if (auto argType = Deserialize().Cast<Type::Type>())
				{
					args.emplace_back(std::move(argType));
				}
				m_Archive->NextReadingElement();
			}
		}
		m_Archive->EndReadingEntry();

		nBool hasVarArg;
		if (!m_Archive->ReadBool(u8"HasVarArg", hasVarArg))
		{
			ThrowInvalidData();
		}

		return m_Sema.GetASTContext().GetFunctionType(from(args), std::move(resultType), hasVarArg);
	}
	case NatsuLang::Type::Type::Paren:
	{
		if (!m_Archive->StartReadingEntry(u8"InnerType"))
		{
			ThrowInvalidData();
		}
		auto innerType = Deserialize().Cast<Type::Type>();
		if (!innerType)
		{
			ThrowInvalidData();
		}
		m_Archive->EndReadingEntry();
		return m_Sema.GetASTContext().GetParenType(std::move(innerType));
	}
	case NatsuLang::Type::Type::Class:
	case NatsuLang::Type::Type::Enum:
	{
		nString tagDeclName;
		if (!m_Archive->ReadString(u8"TagDecl", tagDeclName))
		{
			ThrowInvalidData();
		}

		const auto tagDecl = parseQualifiedName(tagDeclName).Cast<Declaration::TagDecl>();
		if (!tagDecl)
		{
			return make_ref<UnresolvedId>(std::move(tagDeclName));
		}

		return tagDecl->GetTypeForDecl();
	}
	case NatsuLang::Type::Type::Auto:
	{
		if (!m_Archive->StartReadingEntry(u8"DeducedAs"))
		{
			ThrowInvalidData();
		}
		auto deducedAsType = Deserialize().Cast<Type::Type>();
		if (!deducedAsType)
		{
			ThrowInvalidData();
		}
		m_Archive->EndReadingEntry();
		return m_Sema.GetASTContext().GetAutoType(std::move(deducedAsType));
	}
	case NatsuLang::Type::Type::Unresolved:
		nat_Throw(NotImplementedException);
	default:
		break;
	}

	nat_Throw(NotImplementedException);
}

ASTNodePtr Deserializer::DeserializeCompilerAction()
{
	nString name;
	if (!m_Archive->ReadString(u8"Name", name))
	{
		ThrowInvalidData();
	}

	const auto scope = make_scope([this, oldLexer = m_Parser.GetPreprocessor().GetLexer()]() mutable
	{
		m_Parser.GetPreprocessor().SetLexer(std::move(oldLexer));
	});

	m_Parser.GetPreprocessor().SetLexer(make_ref<Lex::Lexer>(name, m_Parser.GetPreprocessor()));
	m_Parser.ConsumeToken();
	return m_Parser.ParseCompilerActionName();
}

Identifier::IdPtr Deserializer::getId(nStrView name) const
{
	Lex::Token dummy;
	return m_Sema.GetPreprocessor().FindIdentifierInfo(name, dummy);
}

natRefPointer<Declaration::NamedDecl> Deserializer::parseQualifiedName(nStrView name)
{
	const auto scope = make_scope([this, oldLexer = m_Parser.GetPreprocessor().GetLexer()]() mutable
	{
		m_Parser.GetPreprocessor().SetLexer(std::move(oldLexer));
	});

	m_Parser.GetPreprocessor().SetLexer(make_ref<Lex::Lexer>(name, m_Parser.GetPreprocessor()));
	m_Parser.ConsumeToken();
	auto qualifiedId = m_Parser.ParseMayBeQualifiedId();

	if (const auto type = m_Sema.LookupTypeName(qualifiedId.second, {},
		m_Sema.GetCurrentScope(), qualifiedId.first))
	{
		if (const auto tagType = type.Cast<Type::TagType>())
		{
			return tagType->GetDecl();
		}

		ThrowInvalidData();
	}

	if (auto mayBeIdExpr = m_Sema.ActOnIdExpr(m_Sema.GetCurrentScope(), qualifiedId.first, std::move(qualifiedId.second),
		false, nullptr))
	{
		if (const auto declRefExpr = mayBeIdExpr.Cast<Expression::DeclRefExpr>())
		{
			return declRefExpr->GetDecl();
		}

		if (const auto memberRefExpr = mayBeIdExpr.Cast<Expression::MemberExpr>())
		{
			return memberRefExpr->GetMemberDecl();
		}

		ThrowInvalidData();
	}

	return nullptr;
}

void Deserializer::tryResolve(natRefPointer<Declaration::NamedDecl> const& namedDecl)
{
	if (const auto iter = m_UnresolvedDeclFixers.find(GetQualifiedName(namedDecl)); iter != m_UnresolvedDeclFixers.end())
	{
		iter->second(namedDecl);
		m_UnresolvedDeclFixers.erase(iter);
	}
}

Serializer::Serializer(Semantic::Sema& sema, NatsuLib::natRefPointer<ISerializationArchiveWriter> archive,
	NatsuLib::natRefPointer<Misc::TextProvider<Statement::Stmt::StmtType>> stmtTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Declaration::Decl::DeclType>> declTypeMap,
	NatsuLib::natRefPointer<Misc::TextProvider<Type::Type::TypeClass>> typeClassMap)
	: m_Sema{ sema }, m_Archive{ std::move(archive) }, m_StmtTypeMap{ std::move(stmtTypeMap) }, m_DeclTypeMap{ std::move(declTypeMap) },
	  m_TypeClassMap{ std::move(typeClassMap) }, m_IsExporting{ false }
{
}

Serializer::~Serializer()
{
}

void Serializer::StartSerialize(nBool isExporting)
{
	m_IsExporting = isExporting;
	m_Archive->StartWritingEntry(u8"Content", true);
}

void Serializer::EndSerialize()
{
	m_Archive->EndWritingEntry();
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
	m_Archive->StartWritingEntry(u8"Content", true);
	for (const auto& s : stmt->GetChildrenStmt())
	{
		StmtVisitor::Visit(s);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitDeclStmt(natRefPointer<Statement::DeclStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartWritingEntry(u8"Decl", true);
	for (const auto& d : stmt->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitDoStmt(natRefPointer<Statement::DoStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartWritingEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitExpr(natRefPointer<Expression::Expr> const& expr)
{
	VisitStmt(expr);
	m_Archive->StartWritingEntry(u8"ExprType");
	TypeVisitor::Visit(expr->GetExprType());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitConditionalOperator(natRefPointer<Expression::ConditionalOperator> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartWritingEntry(u8"Cond");
	StmtVisitor::Visit(expr->GetCondition());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitArraySubscriptExpr(natRefPointer<Expression::ArraySubscriptExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartWritingEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitBinaryOperator(natRefPointer<Expression::BinaryOperator> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"OpCode", expr->GetOpcode());
	m_Archive->StartWritingEntry(u8"LeftOperand");
	StmtVisitor::Visit(expr->GetLeftOperand());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"RightOperand");
	StmtVisitor::Visit(expr->GetRightOperand());
	m_Archive->EndWritingEntry();
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
	m_Archive->StartWritingEntry(u8"Callee");
	StmtVisitor::Visit(expr->GetCallee());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Args", true);
	for (const auto& arg : expr->GetArgs())
	{
		StmtVisitor::Visit(arg);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitCastExpr(natRefPointer<Expression::CastExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"CastType", expr->GetCastType());
	m_Archive->StartWritingEntry(u8"Operand");
	StmtVisitor::Visit(expr->GetOperand());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitInitListExpr(natRefPointer<Expression::InitListExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartWritingEntry(u8"InitExprs", true);
	for (const auto& e : expr->GetInitExprs())
	{
		StmtVisitor::Visit(e);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitCharacterLiteral(natRefPointer<Expression::CharacterLiteral> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteNumType(u8"Value", expr->GetCodePoint());
}

void Serializer::VisitDeclRefExpr(natRefPointer<Expression::DeclRefExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->WriteString(u8"QualifiedName", GetQualifiedName(expr->GetDecl()));
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
	m_Archive->StartWritingEntry(u8"Base");
	StmtVisitor::Visit(expr->GetBase());
	m_Archive->EndWritingEntry();
	m_Archive->WriteString(u8"Name", expr->GetName()->GetName());
	m_Archive->WriteString(u8"QualifiedName", GetQualifiedName(expr->GetMemberDecl()));
}

void Serializer::VisitParenExpr(natRefPointer<Expression::ParenExpr> const& expr)
{
	VisitExpr(expr);
	m_Archive->StartWritingEntry(u8"InnerExpr");
	StmtVisitor::Visit(expr->GetInnerExpr());
	m_Archive->EndWritingEntry();
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
	m_Archive->StartWritingEntry(u8"Operand");
	StmtVisitor::Visit(expr->GetOperand());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitForStmt(natRefPointer<Statement::ForStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartWritingEntry(u8"Init");
	StmtVisitor::Visit(stmt->GetInit());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Inc");
	StmtVisitor::Visit(stmt->GetInc());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitGotoStmt(natRefPointer<Statement::GotoStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitIfStmt(natRefPointer<Statement::IfStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartWritingEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Then");
	StmtVisitor::Visit(stmt->GetThen());
	m_Archive->EndWritingEntry();
	if (const auto elseStmt = stmt->GetElse())
	{
		m_Archive->WriteBool(u8"HasElse", true);
		m_Archive->StartWritingEntry(u8"Else");
		StmtVisitor::Visit(elseStmt);
		m_Archive->EndWritingEntry();
	}
	else
	{
		m_Archive->WriteBool(u8"HasElse", false);
	}
}

void Serializer::VisitLabelStmt(NatsuLib::natRefPointer<Statement::LabelStmt> const& stmt)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitReturnStmt(NatsuLib::natRefPointer<Statement::ReturnStmt> const& stmt)
{
	VisitStmt(stmt);
	m_Archive->StartWritingEntry(u8"ReturnExpr");
	StmtVisitor::Visit(stmt->GetReturnExpr());
	m_Archive->EndWritingEntry();
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
	m_Archive->StartWritingEntry(u8"Cond");
	StmtVisitor::Visit(stmt->GetCond());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"Body");
	StmtVisitor::Visit(stmt->GetBody());
	m_Archive->EndWritingEntry();
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
	m_Archive->NextWritingElement();
}

void Serializer::VisitImportDecl(natRefPointer<Declaration::ImportDecl> const& decl)
{
	VisitDecl(decl);
	m_Archive->StartWritingEntry(u8"ImportedModule");
	DeclVisitor::Visit(decl->GetModule());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitNamedDecl(natRefPointer<Declaration::NamedDecl> const& decl)
{
	VisitDecl(decl);
	m_Archive->WriteString(u8"Name", decl->GetName());
}

void Serializer::VisitAliasDecl(natRefPointer<Declaration::AliasDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartWritingEntry(u8"AliasAs");
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
	m_Archive->EndWritingEntry();
}

void Serializer::VisitLabelDecl(NatsuLib::natRefPointer<Declaration::LabelDecl> const& decl)
{
	nat_Throw(NotImplementedException);
}

void Serializer::VisitModuleDecl(NatsuLib::natRefPointer<Declaration::ModuleDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartWritingEntry(u8"Members", true);
	for (const auto& d : decl->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitTypeDecl(NatsuLib::natRefPointer<Declaration::TypeDecl> const& decl)
{
	VisitNamedDecl(decl);
}

void Serializer::VisitTagDecl(NatsuLib::natRefPointer<Declaration::TagDecl> const& decl)
{
	VisitTypeDecl(decl);
	m_Archive->WriteNumType(u8"TagType", decl->GetTagTypeClass());
	m_Archive->StartWritingEntry(u8"Members", true);
	for (const auto& d : decl->GetDecls())
	{
		DeclVisitor::Visit(d);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::VisitEnumDecl(NatsuLib::natRefPointer<Declaration::EnumDecl> const& decl)
{
	VisitTagDecl(decl);
	m_Archive->StartWritingEntry(u8"UnderlyingType");
	TypeVisitor::Visit(decl->GetUnderlyingType());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitValueDecl(natRefPointer<Declaration::ValueDecl> const& decl)
{
	VisitNamedDecl(decl);
	m_Archive->StartWritingEntry(u8"DeclType");
	TypeVisitor::Visit(decl->GetValueType());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitFunctionDecl(natRefPointer<Declaration::FunctionDecl> const& decl)
{
	VisitVarDecl(decl);
	m_Archive->StartWritingEntry(u8"Params", true);
	for (const auto& param : decl->GetParams())
	{
		DeclVisitor::Visit(param);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
	if (const auto body = decl->GetBody(); !m_IsExporting && body)
	{
		m_Archive->WriteBool(u8"HasBody", true);
		m_Archive->StartWritingEntry(u8"Body");
		StmtVisitor::Visit(body);
		m_Archive->EndWritingEntry();
	}
	else
	{
		m_Archive->WriteBool(u8"HasBody", false);
	}
}

void Serializer::VisitVarDecl(natRefPointer<Declaration::VarDecl> const& decl)
{
	VisitDeclaratorDecl(decl);
	m_Archive->WriteNumType(u8"StorageClass", decl->GetStorageClass());
	if (const auto initializer = decl->GetInitializer(); !m_IsExporting && initializer)
	{
		m_Archive->WriteBool(u8"HasInitializer", true);
		m_Archive->StartWritingEntry(u8"Initializer");
		StmtVisitor::Visit(initializer);
		m_Archive->EndWritingEntry();
	}
	else
	{
		m_Archive->WriteBool(u8"HasInitializer", false);
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
	m_Archive->StartWritingEntry(u8"InitExpr");
	StmtVisitor::Visit(decl->GetInitExpr());
	m_Archive->EndWritingEntry();
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

	m_Archive->StartWritingEntry(u8"Attributes", true);
	for (const auto& attr : decl->GetAllAttributes())
	{
		m_Archive->WriteString(u8"Name", attr->GetName());
		m_Sema.SerializeAttribute(attr, m_Archive);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
}

void Serializer::Visit(Declaration::DeclPtr const& decl)
{
	DeclVisitor::Visit(decl);
	m_Archive->NextWritingElement();
}

void Serializer::VisitBuiltinType(NatsuLib::natRefPointer<Type::BuiltinType> const& type)
{
	VisitType(type);
	m_Archive->WriteNumType(u8"BuiltinType", type->GetBuiltinClass());
}

void Serializer::VisitPointerType(NatsuLib::natRefPointer<Type::PointerType> const& type)
{
	VisitType(type);
	m_Archive->StartWritingEntry(u8"PointeeType");
	TypeVisitor::Visit(type->GetPointeeType());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitArrayType(NatsuLib::natRefPointer<Type::ArrayType> const& type)
{
	VisitType(type);
	m_Archive->StartWritingEntry(u8"ElementType");
	TypeVisitor::Visit(type->GetElementType());
	m_Archive->EndWritingEntry();
	m_Archive->WriteNumType(u8"ArraySize", type->GetSize());
}

void Serializer::VisitFunctionType(NatsuLib::natRefPointer<Type::FunctionType> const& type)
{
	VisitType(type);
	m_Archive->StartWritingEntry(u8"ResultType");
	TypeVisitor::Visit(type->GetResultType());
	m_Archive->EndWritingEntry();
	m_Archive->StartWritingEntry(u8"ArgType", true);
	for (const auto& arg : type->GetParameterTypes())
	{
		TypeVisitor::Visit(arg);
		m_Archive->NextWritingElement();
	}
	m_Archive->EndWritingEntry();
	m_Archive->WriteBool(u8"HasVarArg", type->HasVarArg());
}

void Serializer::VisitParenType(NatsuLib::natRefPointer<Type::ParenType> const& type)
{
	VisitType(type);
	m_Archive->StartWritingEntry(u8"InnerType");
	TypeVisitor::Visit(type->GetInnerType());
	m_Archive->EndWritingEntry();
}

void Serializer::VisitTagType(NatsuLib::natRefPointer<Type::TagType> const& type)
{
	VisitType(type);
	m_Archive->WriteString(u8"TagDecl", GetQualifiedName(type->GetDecl()));
}

void Serializer::VisitDeducedType(NatsuLib::natRefPointer<Type::DeducedType> const& type)
{
	VisitType(type);
	m_Archive->StartWritingEntry(u8"DeducedAs");
	TypeVisitor::Visit(type->GetDeducedAsType());
	m_Archive->EndWritingEntry();
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
	m_Archive->NextWritingElement();
}

void Serializer::SerializeCompilerAction(natRefPointer<ICompilerAction> const& action)
{
	m_Archive->WriteNumType(u8"AstNodeType", ASTNodeType::CompilerAction);

	nString qualifiedName = action->GetName();
	auto parent = action->GetParent();
	assert(parent);
	while (true)
	{
		const auto parentRef = parent.Lock();
		if (!parentRef->GetParent())
		{
			break;
		}
		qualifiedName = parentRef->GetName() + (u8"."_nv + qualifiedName);
		parent = parentRef->GetParent();
	}

	m_Archive->WriteString(u8"Name", qualifiedName);
}

std::size_t Serializer::GetRefCount() const volatile noexcept
{
	return RefObjImpl::GetRefCount();
}

nBool Serializer::TryAddRef() const volatile
{
	return RefObjImpl::TryAddRef();
}

void Serializer::AddRef() const volatile
{
	return RefObjImpl::AddRef();
}

nBool Serializer::Release() const volatile
{
	return RefObjImpl::Release();
}
