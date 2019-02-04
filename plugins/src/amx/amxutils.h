#ifndef AMXUTILS_H_INCLUDED
#define AMXUTILS_H_INCLUDED

#include "sdk/amx/amx.h"
#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>

namespace amx
{
	class Extra
	{
	protected:
		AMX *_amx;

	public:
		Extra(AMX *amx) : _amx(amx)
		{

		}

		virtual ~Extra() = default;
	};

	class Instance
	{
		AMX *_amx;
		std::unordered_map<std::type_index, std::unique_ptr<Extra>> extras;

	public:
		Instance() : _amx(nullptr)
		{

		}

		explicit Instance(AMX *amx) : _amx(amx)
		{

		}

		Instance(const Instance &obj) = delete;
		Instance &operator=(const Instance &obj) = delete;

		template <class ExtraType>
		ExtraType &get_extra()
		{
			std::type_index key = typeid(ExtraType);

			auto it = extras.find(key);
			if(it == extras.end())
			{
				it = extras.insert(std::make_pair(key, std::unique_ptr<ExtraType>(new ExtraType(_amx)))).first;
			}
			return static_cast<ExtraType&>(*it->second);
		}

		template <class ExtraType>
		bool has_extra() const
		{
			std::type_index key = typeid(ExtraType);

			return extras.find(key) != extras.end();
		}

		template <class ExtraType>
		bool remove_extra()
		{
			std::type_index key = typeid(ExtraType);

			auto it = extras.find(key);
			if(it != extras.end())
			{
				extras.erase(it);
				return true;
			}
			return false;
		}

		AMX *get()
		{
			return _amx;
		}

		operator AMX*()
		{
			return _amx;
		}
	};

	const char *StrError(int error);
	std::string GetString(const cell *source, size_t size, bool cstring);
	void SetString(cell *dest, const char *source, size_t len, bool pack);
	bool MemCheck(AMX *amx, size_t size);
	std::shared_ptr<Instance> GetHandle(AMX *amx);
	void RemoveHandle(AMX *amx);
}

#endif
