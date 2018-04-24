#include "Sema/Declarator.h"
#include "Sema/Scope.h"
#include "Sema/Sema.h"
#include "Sema/CompilerAction.h"
#include "AST/NestedNameSpecifier.h"

using namespace NatsuLib;
using namespace NatsuLang;
using namespace NatsuLang::Declaration;

Declarator::Declarator(Context context)
	: m_Context{ context }, m_StorageClass{ Specifier::StorageClass::None },
	  m_Accessibility{ Specifier::Access::None },
	  m_Safety{ Specifier::Safety::None }, m_IsAlias{ false }
{
}

Declarator::~Declarator()
{
}

void Declarator::AttachPostProcessor(natRefPointer<ICompilerAction> action)
{
	m_PostProcessors.emplace(std::move(action));
}

Linq<Valued<natRefPointer<ICompilerAction>>> Declarator::GetPostProcessors() const noexcept
{
	return from(m_PostProcessors);
}

void Declarator::ClearPostProcessors()
{
	m_PostProcessors.clear();
}
