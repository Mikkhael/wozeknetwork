#pragma once

#include <string_view>

namespace smart_enum_namespace
{
	
	constexpr size_t strlen(const char* str)
	{
		size_t pos = 0;
		while(str[pos] != 0)
			++pos;
		return pos;
	}
	
	constexpr size_t countCommas(const char* str)
	{
		size_t size = strlen(str);
		
		size_t res = 0;
		for(size_t i=0; i<size; i++)
			if(str[i] == ',')
				++res;
		return res;
	}
	
	template <typename Enum, size_t Size>
	struct Names
	{
		std::string_view names[Size];
		constexpr Names(const char* str)
		{
			size_t namesIndex = 0;
			
			size_t beg = 0;
			size_t end = 0;
			
			size_t size = strlen(str);
			
			for(size_t i=0; i<=size; i++)
			{
				char c = str[i];
				if(c == ' ' || c == '\n' || c == '\t')
				{
					if(beg == end)
					{
						beg = end += 1;
					}
				}
				else if(c == ',' || c == '\0')
				{
					names[namesIndex] = std::string_view(str + beg, end - beg);
					beg = end = i+1;
					++namesIndex;
				}
				else
				{
					++end;
				}
			}
		}
	};
}




#define SMARTENUM(name, ...) \
	enum class name { __VA_ARGS__ }; \
	static constexpr auto name##Size = smart_enum_namespace::countCommas( #__VA_ARGS__ ) + 1; \
	static constexpr auto name##Names = smart_enum_namespace::Names< name , name##Size >( #__VA_ARGS__ ); \
	static constexpr auto name##GetName(const size_t n) { assert( n < name##Size ); return name##Names.names[n]; };
