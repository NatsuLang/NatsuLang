#include "TestClasses.h"

TEST_CASE("AST Generation", "[Lexer][Parser][Sema]")
{
	constexpr char testCode[] =
		u8R"(
def Increase : (arg : int = 1) -> int
{
	return 1 + arg;
}
)";

	Diag::DiagnosticsEngine diag{ make_ref<IDMap>(), make_ref<TestDiagConsumer>() };
	FileManager fileManager{};
	SourceManager sourceManager{ diag, fileManager };
	Preprocessor pp{ diag, sourceManager };
	pp.SetLexer(make_ref<Lex::Lexer>(testCode, pp));
	ASTContext context;
	const auto consumer = make_ref<TestAstConsumer>();
	ParseAST(pp, context, consumer);

	SECTION("test function \"Increase\"")
	{
		const auto incFunc = consumer->GetNamedDecl(u8"Increase").Cast<Declaration::FunctionDecl>();
		REQUIRE(incFunc);

		SECTION("test signature")
		{
			auto funcType = incFunc->GetValueType().Cast<Type::FunctionType>();
			REQUIRE(funcType);

			SECTION("test parameters")
			{
				REQUIRE(incFunc->GetParamCount() == 1);
				const auto arg = incFunc->GetParams().first();
				REQUIRE(arg);

				REQUIRE(arg->GetName() == "arg");

				REQUIRE(arg->GetType() == Declaration::Decl::ParmVar);

				const auto argType = arg->GetValueType();
				REQUIRE(argType);
				const auto argRealType = argType.Cast<Type::BuiltinType>();
				REQUIRE(argRealType);
				REQUIRE(argRealType->GetBuiltinClass() == Type::BuiltinType::Int);

				const auto defaultValue = arg->GetInitializer();
				REQUIRE(defaultValue);
				nuLong value;
				REQUIRE(defaultValue->EvaluateAsInt(value, context));
				REQUIRE(value == 1);
			}

			SECTION("test result type")
			{
				const auto retType = funcType->GetResultType().Cast<Type::BuiltinType>();
				REQUIRE(retType);
				REQUIRE(retType->GetBuiltinClass() == Type::BuiltinType::Int);
			}
		}

		SECTION("test body")
		{
			const auto body = incFunc->GetBody().Cast<Statement::CompoundStmt>();
			REQUIRE(body);

			auto content{ body->GetChildrens().Cast<std::vector<Statement::StmtPtr>>() };
			REQUIRE(!content.empty());

			auto retStmt = content[0].Cast<Statement::ReturnStmt>();
			REQUIRE(retStmt);

			auto retValueExpr = retStmt->GetReturnExpr();
			REQUIRE(retValueExpr);
		}
	}
}
