#ifndef STMT
#define STMT(Type, Base)
#endif

#ifndef ABSTRACT_STMT
#define ABSTRACT_STMT(Type) Type
#endif

#ifndef STMT_RANGE
#define STMT_RANGE(Base, First, Last)
#endif

#ifndef LAST_STMT_RANGE
#define LAST_STMT_RANGE(Base, First, Last) STMT_RANGE(Base, First, Last)
#endif

#ifndef BREAKSTMT
#define BREAKSTMT(Type, Base) STMT(Type, Base)
#endif
BREAKSTMT(BreakStmt, Stmt)
#undef BREAKSTMT

#ifndef CATCHSTMT
#define CATCHSTMT(Type, Base) STMT(Type, Base)
#endif
CATCHSTMT(CatchStmt, Stmt)
#undef CATCHSTMT

#ifndef TRYSTMT
#define TRYSTMT(Type, Base) STMT(Type, Base)
#endif
TRYSTMT(TryStmt, Stmt)
#undef TRYSTMT

#ifndef COMPOUNDSTMT
#define COMPOUNDSTMT(Type, Base) STMT(Type, Base)
#endif
COMPOUNDSTMT(CompoundStmt, Stmt)
#undef COMPOUNDSTMT

#ifndef CONTINUESTMT
#define CONTINUESTMT(Type, Base) STMT(Type, Base)
#endif
CONTINUESTMT(ContinueStmt, Stmt)
#undef CONTINUESTMT

#ifndef DECLSTMT
#define DECLSTMT(Type, Base) STMT(Type, Base)
#endif
DECLSTMT(DeclStmt, Stmt)
#undef DECLSTMT

#ifndef DOSTMT
#define DOSTMT(Type, Base) STMT(Type, Base)
#endif
DOSTMT(DoStmt, Stmt)
#undef DOSTMT

#ifndef EXPR
#define EXPR(Type, Base) STMT(Type, Base)
#endif
ABSTRACT_STMT(EXPR(Expr, Stmt))

#ifndef CONDITIONALOPERATOR
#define CONDITIONALOPERATOR(Type, Base) EXPR(Type, Base)
#endif
CONDITIONALOPERATOR(ConditionalOperator, Expr)
#undef CONDITIONALOPERATOR

#ifndef ARRAYSUBSCRIPTEXPR
#define ARRAYSUBSCRIPTEXPR(Type, Base) EXPR(Type, Base)
#endif
ARRAYSUBSCRIPTEXPR(ArraySubscriptExpr, Expr)
#undef ARRAYSUBSCRIPTEXPR

#ifndef BINARYOPERATOR
#define BINARYOPERATOR(Type, Base) EXPR(Type, Base)
#endif
BINARYOPERATOR(BinaryOperator, Expr)

#ifndef COMPOUNDASSIGNOPERATOR
#define COMPOUNDASSIGNOPERATOR(Type, Base) BINARYOPERATOR(Type, Base)
#endif
COMPOUNDASSIGNOPERATOR(CompoundAssignOperator, BinaryOperator)
#undef COMPOUNDASSIGNOPERATOR

STMT_RANGE(BinaryOperator, BinaryOperator, CompoundAssignOperator)

#undef BINARYOPERATOR

#ifndef BOOLEANLITERAL
#define BOOLEANLITERAL(Type, Base) EXPR(Type, Base)
#endif
BOOLEANLITERAL(BooleanLiteral, Expr)
#undef BOOLEANLITERAL

#ifndef CONSTRUCTEXPR
#define CONSTRUCTEXPR(Type, Base) EXPR(Type, Base)
#endif
CONSTRUCTEXPR(ConstructExpr, Expr)
#undef CONSTRUCTEXPR

#ifndef DELETEEXPR
#define DELETEEXPR(Type, Base) EXPR(Type, Base)
#endif
DELETEEXPR(DeleteExpr, Expr)
#undef DELETEEXPR

/*#ifndef INHERITEDCTORINITEXPR
#define INHERITEDCTORINITEXPR(Type, Base) EXPR(Type, Base)
#endif
INHERITEDCTORINITEXPR(InheritedCtorInitExpr, Expr)
#undef INHERITEDCTORINITEXPR*/

#ifndef NEWEXPR
#define NEWEXPR(Type, Base) EXPR(Type, Base)
#endif
NEWEXPR(NewExpr, Expr)
#undef NEWEXPR

#ifndef THISEXPR
#define THISEXPR(Type, Base) EXPR(Type, Base)
#endif
THISEXPR(ThisExpr, Expr)
#undef THISEXPR

#ifndef THROWEXPR
#define THROWEXPR(Type, Base) EXPR(Type, Base)
#endif
THROWEXPR(ThrowExpr, Expr)
#undef THROWEXPR

#ifndef CALLEXPR
#define CALLEXPR(Type, Base) EXPR(Type, Base)
#endif
CALLEXPR(CallExpr, Expr)

#ifndef MEMBERCALLEXPR
#define MEMBERCALLEXPR(Type, Base) CALLEXPR(Type, Base)
#endif
MEMBERCALLEXPR(MemberCallExpr, CallExpr)
#undef MEMBERCALLEXPR

STMT_RANGE(CallExpr, CallExpr, MemberCallExpr)

#undef CALLEXPR

#ifndef CASTEXPR
#define CASTEXPR(Type, Base) EXPR(Type, Base)
#endif
ABSTRACT_STMT(CASTEXPR(CastExpr, Expr))

#ifndef ASTYPEEXPR
#define ASTYPEEXPR(Type, Base) CASTEXPR(Type, Base)
#endif
ASTYPEEXPR(AsTypeExpr, CastExpr)
#undef ASTYPEEXPR

#ifndef IMPLICITCASTEXPR
#define IMPLICITCASTEXPR(Type, Base) CASTEXPR(Type, Base)
#endif
IMPLICITCASTEXPR(ImplicitCastExpr, CastExpr)
#undef IMPLICITCASTEXPR

STMT_RANGE(CastExpr, AsTypeExpr, ImplicitCastExpr)

#undef CASTEXPR

#ifndef CHARACTERLITERAL
#define CHARACTERLITERAL(Type, Base) EXPR(Type, Base)
#endif
CHARACTERLITERAL(CharacterLiteral, Expr)
#undef CHARACTERLITERAL

#ifndef DECLREFEXPR
#define DECLREFEXPR(Type, Base) EXPR(Type, Base)
#endif
DECLREFEXPR(DeclRefExpr, Expr)
#undef DECLREFEXPR

#ifndef FLOATINGLITERAL
#define FLOATINGLITERAL(Type, Base) EXPR(Type, Base)
#endif
FLOATINGLITERAL(FloatingLiteral, Expr)
#undef FLOATINGLITERAL

#ifndef INTEGERLITERAL
#define INTEGERLITERAL(Type, Base) EXPR(Type, Base)
#endif
INTEGERLITERAL(IntegerLiteral, Expr)
#undef INTEGERLITERAL

#ifndef MEMBEREXPR
#define MEMBEREXPR(Type, Base) EXPR(Type, Base)
#endif
MEMBEREXPR(MemberExpr, Expr)
#undef MEMBEREXPR

/*#ifndef OVERLOADEXPR
#define OVERLOADEXPR(Type, Base) EXPR(Type, Base)
#endif
ABSTRACT_STMT(OVERLOADEXPR(OverloadExpr, Expr))
#ifndef UNRESOLVEDLOOKUPEXPR
#define UNRESOLVEDLOOKUPEXPR(Type, Base) OVERLOADEXPR(Type, Base)
#endif
UNRESOLVEDLOOKUPEXPR(UnresolvedLookupExpr, OverloadExpr)
#undef UNRESOLVEDLOOKUPEXPR

#ifndef UNRESOLVEDMEMBEREXPR
#define UNRESOLVEDMEMBEREXPR(Type, Base) OVERLOADEXPR(Type, Base)
#endif
UNRESOLVEDMEMBEREXPR(UnresolvedMemberExpr, OverloadExpr)
#undef UNRESOLVEDMEMBEREXPR

STMT_RANGE(OverloadExpr, UnresolvedLookupExpr, UnresolvedMemberExpr)

#undef OVERLOADEXPR*/

#ifndef PARENEXPR
#define PARENEXPR(Type, Base) EXPR(Type, Base)
#endif
PARENEXPR(ParenExpr, Expr)
#undef PARENEXPR

/*#ifndef PARENLISTEXPR
#define PARENLISTEXPR(Type, Base) EXPR(Type, Base)
#endif
PARENLISTEXPR(ParenListExpr, Expr)
#undef PARENLISTEXPR*/

#ifndef STMTEXPR
#define STMTEXPR(Type, Base) EXPR(Type, Base)
#endif
STMTEXPR(StmtExpr, Expr)
#undef STMTEXPR

#ifndef STRINGLITERAL
#define STRINGLITERAL(Type, Base) EXPR(Type, Base)
#endif
STRINGLITERAL(StringLiteral, Expr)
#undef STRINGLITERAL

#ifndef UNARYEXPRORTYPETRAITEXPR
#define UNARYEXPRORTYPETRAITEXPR(Type, Base) EXPR(Type, Base)
#endif
UNARYEXPRORTYPETRAITEXPR(UnaryExprOrTypeTraitExpr, Expr)
#undef UNARYEXPRORTYPETRAITEXPR

#ifndef UNARYOPERATOR
#define UNARYOPERATOR(Type, Base) EXPR(Type, Base)
#endif
UNARYOPERATOR(UnaryOperator, Expr)
#undef UNARYOPERATOR

STMT_RANGE(Expr, ConditionalOperator, UnaryOperator)

#undef EXPR

#ifndef FORSTMT
#define FORSTMT(Type, Base) STMT(Type, Base)
#endif
FORSTMT(ForStmt, Stmt)
#undef FORSTMT

#ifndef GOTOSTMT
#define GOTOSTMT(Type, Base) STMT(Type, Base)
#endif
GOTOSTMT(GotoStmt, Stmt)
#undef GOTOSTMT

#ifndef IFSTMT
#define IFSTMT(Type, Base) STMT(Type, Base)
#endif
IFSTMT(IfStmt, Stmt)
#undef IFSTMT

#ifndef LABELSTMT
#define LABELSTMT(Type, Base) STMT(Type, Base)
#endif
LABELSTMT(LabelStmt, Stmt)
#undef LABELSTMT

#ifndef NULLSTMT
#define NULLSTMT(Type, Base) STMT(Type, Base)
#endif
NULLSTMT(NullStmt, Stmt)
#undef NULLSTMT

#ifndef RETURNSTMT
#define RETURNSTMT(Type, Base) STMT(Type, Base)
#endif
RETURNSTMT(ReturnStmt, Stmt)
#undef RETURNSTMT

#ifndef SWITCHCASE
#define SWITCHCASE(Type, Base) STMT(Type, Base)
#endif
ABSTRACT_STMT(SWITCHCASE(SwitchCase, Stmt))
#ifndef CASESTMT
#define CASESTMT(Type, Base) SWITCHCASE(Type, Base)
#endif
CASESTMT(CaseStmt, SwitchCase)
#undef CASESTMT

#ifndef DEFAULTSTMT
#define DEFAULTSTMT(Type, Base) SWITCHCASE(Type, Base)
#endif
DEFAULTSTMT(DefaultStmt, SwitchCase)
#undef DEFAULTSTMT

STMT_RANGE(SwitchCase, CaseStmt, DefaultStmt)

#undef SWITCHCASE

#ifndef SWITCHSTMT
#define SWITCHSTMT(Type, Base) STMT(Type, Base)
#endif
SWITCHSTMT(SwitchStmt, Stmt)
#undef SWITCHSTMT

#ifndef WHILESTMT
#define WHILESTMT(Type, Base) STMT(Type, Base)
#endif
WHILESTMT(WhileStmt, Stmt)
#undef WHILESTMT

LAST_STMT_RANGE(Stmt, BreakStmt, WhileStmt)

#undef STMT
#undef STMT_RANGE
#undef LAST_STMT_RANGE
#undef ABSTRACT_STMT
