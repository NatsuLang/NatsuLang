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

	Diag::DiagnosticsEngine diag{ make_ref<IDMap>(), make_ref<JitDiagConsumer>() };
	FileManager fileManager{};
	SourceManager sourceManager{ diag, fileManager };
	Preprocessor pp{ diag, sourceManager };
	pp.SetLexer(make_ref<Lex::Lexer>(testCode, pp));
	ASTContext context;
	const auto consumer = make_ref<TestAstConsumer>();
	ParseAST(pp, context, consumer);

	SECTION("test function \"Increase\"")
	{
		const auto incFunc = static_cast<natRefPointer<Declaration::FunctionDecl>>(consumer->GetNamedDecl(u8"Increase"));
		REQUIRE(incFunc);

		SECTION("test signature")
		{
			auto funcType = static_cast<natRefPointer<Type::FunctionType>>(incFunc->GetValueType());
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
				const auto argRealType = static_cast<natRefPointer<Type::BuiltinType>>(argType);
				REQUIRE(argRealType);
				REQUIRE(argRealType->GetBuiltinClass() == Type::BuiltinType::Int);

				const auto defaultValue = arg->GetDefaultValue();
				REQUIRE(defaultValue);
				nuLong value;
				REQUIRE(defaultValue->EvaluateAsInt(value, context));
				REQUIRE(value == 1);
			}

			SECTION("test result type")
			{
				const auto retType = static_cast<natRefPointer<Type::BuiltinType>>(funcType->GetResultType());
				REQUIRE(retType);
				REQUIRE(retType->GetBuiltinClass() == Type::BuiltinType::Int);
			}
		}

		SECTION("test body")
		{
			const auto body = incFunc->GetBody();
			REQUIRE(body);
		}
	}
}
