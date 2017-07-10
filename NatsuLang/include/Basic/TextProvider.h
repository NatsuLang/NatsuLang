#pragma once
#include <natRefObj.h>

namespace NatsuLang::Misc
{
	////////////////////////////////////////////////////////////////////////////////
	///	@brief	提供从ID到实际文本的映射的接口抽象
	///	@tparam	IDType	ID的类型
	////////////////////////////////////////////////////////////////////////////////
	template <typename IDType>
	struct TextProvider
		: NatsuLib::natRefObjImpl<TextProvider<IDType>>
	{
		virtual nString GetText(IDType id) = 0;
	};
}
