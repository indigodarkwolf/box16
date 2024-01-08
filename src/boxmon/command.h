#pragma once

#include <compare>
#include <functional>
#include <map>

#include "parser.h"

namespace boxmon
{
	class boxmon_command
	{
	public:
		boxmon_command(char const *name, char const *description, std::function<bool(char const *, parser &, bool)> fn);

		std::strong_ordering operator<=>(char const *name) const;
		std::strong_ordering operator<=>(const boxmon_command &cmd) const;
		bool                 run(char const *&input, parser &, bool) const;

		char const *get_name() const;
		char const *get_description() const;

		static const boxmon_command *find(char const *name);
		static void                  for_each(std::function<void(const boxmon_command *cmd)> fn);
		static void                  for_each_partial(char const *name, std::function<void(const boxmon_command *cmd)> fn);

	private:
		char const *m_name;
		char const *m_description;

		std::function<bool(char const *, parser &, bool)> m_run;

		static std::map<const std::string, const boxmon_command *> &get_command_list();
	};

	class boxmon_alias : public boxmon_command
	{
	public:
		boxmon_alias(char const *name, const boxmon_command &cmd);

	private:
		const boxmon_command &m_cmd;
	};
} // namespace boxmon

#define BOXMON_COMMAND(NAME, DESC)                                                                  \
	static bool            boxmon_command_impl_##NAME(char const *input, boxmon::parser &parser, bool help); \
	boxmon::boxmon_command boxmon_command_##NAME(#NAME, DESC, boxmon_command_impl_##NAME);      \
	static bool            boxmon_command_impl_##NAME(char const *input, boxmon::parser &parser, bool help)

#define BOXMON_ALIAS(ALIAS_NAME, ORIGINAL_NAME) \
	boxmon::boxmon_alias boxmon_alias_##ALIAS_NAME(#ALIAS_NAME, boxmon_command_##ORIGINAL_NAME)
